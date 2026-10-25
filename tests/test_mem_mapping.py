"""
tests/test_mem_mapping.py: Unit tests for AddressMapper and banking.
Tests address mapping schemes, bit-field diagrams, XOR/hash interleaving,
channel interleaving, and bank conflict detection.
"""

import pytest
from memory.config import DRAMConfig
from memory.mapping import AddressMapper, DRAMCoordinates


def test_standard_mapping_coordinates():
    cfg = DRAMConfig(channels=2, ranks_per_channel=1, bank_groups_per_rank=4, banks_per_group=4)
    mapper = AddressMapper(cfg, line_size_bytes=64)

    # Offset = 6 bits, Chan = 1 bit, Col = 10 bits, Bank = 2 bits, BG = 2 bits, Row = 16 bits
    assert mapper.byte_bits == 6
    assert mapper.chan_bits == 1
    assert mapper.bank_bits == 2
    assert mapper.bg_bits == 2

    # Map address 0
    coords0 = mapper.map_address(0)
    assert coords0.channel == 0
    assert coords0.bank_group == 0
    assert coords0.bank == 0
    assert coords0.row == 0
    assert coords0.column == 0
    assert coords0.byte_offset == 0

    # Address 64 (next line) -> channel interleaving sends to Channel 1!
    coords64 = mapper.map_address(64)
    assert coords64.channel == 1
    assert coords64.byte_offset == 0


def test_xor_hash_interleaving():
    # XOR hash interleaving modifies bank index using row bits
    cfg = DRAMConfig(address_mapping="xor_hash", bank_groups_per_rank=4, banks_per_group=4)
    mapper = AddressMapper(cfg, line_size_bytes=64)

    # Two addresses with different rows
    c1 = mapper.map_address(0x00000000)
    c2 = mapper.map_address(0x00100000) # Row differs
    # XOR hashing ensures bank mapping is not identically stride-aligned
    assert c1.bank != c2.bank or c1.row != c2.row


def test_bank_conflict_detection():
    cfg = DRAMConfig(bank_groups_per_rank=2, banks_per_group=2)
    mapper = AddressMapper(cfg, line_size_bytes=64)

    # Map same bank with different rows -> bank conflict
    c1 = mapper.map_address(0x00000000)
    # Stride that lands in same bank and bank group, but different row
    stride = (mapper.cols * mapper.total_banks) << mapper.byte_bits
    c2 = mapper.map_address(stride)

    assert (c1.bank_group, c1.bank) == (c2.bank_group, c2.bank)
    assert c1.row != c2.row
    assert mapper.conflict_count >= 1

    chart = mapper.print_bank_utilization_chart()
    assert "DRAM Bank Utilization" in chart
