"""
memory/mapping.py: Physical address to DRAM coordinates mapping, banking,
channel interleaving, XOR/hash bank interleaving, and Bank-Level Parallelism (BLP).
"""

from __future__ import annotations
import math
from dataclasses import dataclass
from typing import Tuple, Dict, List, Optional
from memory.config import DRAMConfig


@dataclass
class DRAMCoordinates:
    channel: int
    rank: int
    bank_group: int
    bank: int
    row: int
    column: int
    byte_offset: int

    def to_tuple(self) -> Tuple[int, int, int, int, int, int]:
        return (self.channel, self.rank, self.bank_group, self.bank, self.row, self.column)

    def __str__(self) -> str:
        return f"Chan={self.channel} Rank={self.rank} BG={self.bank_group} Bank={self.bank} Row=0x{self.row:04X} Col=0x{self.column:03X} Offset={self.byte_offset}"


class AddressMapper:
    """
    Translates physical memory addresses to DRAM channel, rank, bank-group, bank, row, and column.
    Supports standard linear bit-slice schemes and XOR/hash interleaving.
    """

    def __init__(self, config: DRAMConfig, line_size_bytes: int = 64):
        self.cfg = config
        self.line_size = line_size_bytes

        # Dimensions
        self.channels = config.channels
        self.ranks = config.ranks_per_channel
        self.bank_groups = config.bank_groups_per_rank
        self.banks_per_group = config.banks_per_group
        self.total_banks = self.bank_groups * self.banks_per_group
        self.rows = config.rows_per_bank
        self.cols = config.cols_per_row

        # Bit widths
        self.byte_bits = int(math.log2(self.line_size))       # 6 bits for 64B line
        self.chan_bits = int(math.log2(self.channels)) if self.channels > 1 else 0
        self.rank_bits = int(math.log2(self.ranks)) if self.ranks > 1 else 0
        self.bg_bits = int(math.log2(self.bank_groups)) if self.bank_groups > 1 else 0
        self.bank_bits = int(math.log2(self.banks_per_group)) if self.banks_per_group > 1 else 0
        self.col_bits = int(math.log2(self.cols))
        self.row_bits = int(math.log2(self.rows))

        self.mapping_scheme = config.address_mapping.lower()

        # BLP and Bank Conflict tracking
        self.bank_access_counts: Dict[Tuple[int, int], int] = {} # (bg, b) -> count
        self.conflict_count = 0
        self.last_accessed_row: Dict[Tuple[int, int], int] = {}

    def map_address(self, paddr: int) -> DRAMCoordinates:
        """
        Extracts DRAM coordinates from a physical address.
        """
        curr = paddr
        byte_offset = curr & ((1 << self.byte_bits) - 1)
        curr >>= self.byte_bits

        if self.mapping_scheme == "xor_hash":
            # XOR-based bank interleaving:
            # channel -> col_low -> (bank XOR row_slice) -> bg -> row -> col_high
            chan = 0
            if self.chan_bits > 0:
                chan = curr & ((1 << self.chan_bits) - 1)
                curr >>= self.chan_bits

            col_low = curr & 0x7 # 3 bits of column
            curr >>= 3

            raw_bank = curr & ((1 << self.bank_bits) - 1)
            curr >>= self.bank_bits

            raw_bg = curr & ((1 << self.bg_bits) - 1) if self.bg_bits > 0 else 0
            if self.bg_bits > 0:
                curr >>= self.bg_bits

            row = curr & ((1 << self.row_bits) - 1)
            curr >>= self.row_bits

            col_high = curr & ((1 << (self.col_bits - 3)) - 1)
            col = (col_high << 3) | col_low

            # XOR hash bank with lower row bits to disperse power-of-2 strides
            bank = (raw_bank ^ (row & ((1 << self.bank_bits) - 1)))
            bg = (raw_bg ^ ((row >> self.bank_bits) & ((1 << self.bg_bits) - 1))) if self.bg_bits > 0 else 0
            rank = 0

        elif self.mapping_scheme == "row_col_bank_chan":
            # Channel -> Bank -> BankGroup -> Column -> Row
            chan = 0
            if self.chan_bits > 0:
                chan = curr & ((1 << self.chan_bits) - 1)
                curr >>= self.chan_bits

            bank = curr & ((1 << self.bank_bits) - 1) if self.bank_bits > 0 else 0
            if self.bank_bits > 0:
                curr >>= self.bank_bits

            bg = curr & ((1 << self.bg_bits) - 1) if self.bg_bits > 0 else 0
            if self.bg_bits > 0:
                curr >>= self.bg_bits

            rank = curr & ((1 << self.rank_bits) - 1) if self.rank_bits > 0 else 0
            if self.rank_bits > 0:
                curr >>= self.rank_bits

            col = curr & ((1 << self.col_bits) - 1)
            curr >>= self.col_bits

            row = curr & ((1 << self.row_bits) - 1)

        else: # default: row_bg_bank_col_chan_byte
            # Channel -> Column -> Bank -> BankGroup -> Rank -> Row
            chan = 0
            if self.chan_bits > 0:
                chan = curr & ((1 << self.chan_bits) - 1)
                curr >>= self.chan_bits

            col = curr & ((1 << self.col_bits) - 1)
            curr >>= self.col_bits

            bank = curr & ((1 << self.bank_bits) - 1) if self.bank_bits > 0 else 0
            if self.bank_bits > 0:
                curr >>= self.bank_bits

            bg = curr & ((1 << self.bg_bits) - 1) if self.bg_bits > 0 else 0
            if self.bg_bits > 0:
                curr >>= self.bg_bits

            rank = curr & ((1 << self.rank_bits) - 1) if self.rank_bits > 0 else 0
            if self.rank_bits > 0:
                curr >>= self.rank_bits

            row = curr & ((1 << self.row_bits) - 1)

        # Track bank conflict
        bank_key = (bg, bank)
        self.bank_access_counts[bank_key] = self.bank_access_counts.get(bank_key, 0) + 1
        if bank_key in self.last_accessed_row and self.last_accessed_row[bank_key] != row:
            self.conflict_count += 1
        self.last_accessed_row[bank_key] = row

        return DRAMCoordinates(
            channel=chan,
            rank=rank,
            bank_group=bg,
            bank=bank,
            row=row,
            column=col,
            byte_offset=byte_offset,
        )

    def print_bit_field_diagram(self) -> str:
        """Returns ASCII bit-allocation layout diagram for current address mapping."""
        total_bits = self.row_bits + self.rank_bits + self.bg_bits + self.bank_bits + self.col_bits + self.chan_bits + self.byte_bits
        high = total_bits - 1

        parts = []
        if self.mapping_scheme == "xor_hash":
            parts = [
                f"[Row: {self.row_bits}b]",
                f"[Col_High: {self.col_bits - 3}b]",
                f"[BG^Row: {self.bg_bits}b]",
                f"[Bank^Row: {self.bank_bits}b]",
                f"[Col_Low: 3b]",
                f"[Chan: {self.chan_bits}b]" if self.chan_bits > 0 else "",
                f"[Byte: {self.byte_bits}b]",
            ]
        elif self.mapping_scheme == "row_col_bank_chan":
            parts = [
                f"[Row: {self.row_bits}b]",
                f"[Col: {self.col_bits}b]",
                f"[Rank: {self.rank_bits}b]" if self.rank_bits > 0 else "",
                f"[BG: {self.bg_bits}b]",
                f"[Bank: {self.bank_bits}b]",
                f"[Chan: {self.chan_bits}b]" if self.chan_bits > 0 else "",
                f"[Byte: {self.byte_bits}b]",
            ]
        else: # row_bg_bank_col_chan_byte
            parts = [
                f"[Row: {self.row_bits}b]",
                f"[Rank: {self.rank_bits}b]" if self.rank_bits > 0 else "",
                f"[BG: {self.bg_bits}b]",
                f"[Bank: {self.bank_bits}b]",
                f"[Col: {self.col_bits}b]",
                f"[Chan: {self.chan_bits}b]" if self.chan_bits > 0 else "",
                f"[Byte: {self.byte_bits}b]",
            ]
        filtered = [p for p in parts if p]
        diagram = " -> ".join(filtered)
        return f"Address Mapping ({self.mapping_scheme.upper()}):\nMSB [{high}:0] LSB: {diagram}"

    def print_bank_utilization_chart(self) -> str:
        """Returns ASCII bar chart of per-bank utilization."""
        out = ["=== DRAM Bank Utilization & Conflict Profile ==="]
        max_accesses = max(self.bank_access_counts.values()) if self.bank_access_counts else 1
        for bg in range(self.bank_groups):
            for b in range(self.banks_per_group):
                cnt = self.bank_access_counts.get((bg, b), 0)
                bar_len = int((cnt / max_accesses) * 30) if max_accesses > 0 else 0
                bar = "#" * bar_len
                out.append(f"  Bank [BG {bg}, B {b}] : {cnt:4d} accesses | {bar}")
        out.append(f"Total Bank Conflicts Detected: {self.conflict_count}")
        return "\n".join(out)
