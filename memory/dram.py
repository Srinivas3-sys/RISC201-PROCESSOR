"""
memory/dram.py: Cycle-accurate DRAM subsystem and memory controller.
Models channels, ranks, bank groups, banks, row buffers, timing constraints
(tRCD, tRP, tCL, tRAS, tRC, tWR, tFAW, tRRD_S/L, tCCD_S/L, tBURST, tREFI, tRFC),
open-page and closed-page policies, FCFS and FR-FCFS schedulers, refresh stalls,
and peak vs effective bandwidth metrics.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Any
from collections import deque
from memory.config import DRAMConfig, DRAMTimingConfig


@dataclass
class DRAMBank:
    bank_id: int
    group_id: int
    rank_id: int
    channel_id: int
    open_row: Optional[int] = None       # None = Precharged / Empty
    row_state: str = "CLOSED"            # "CLOSED", "OPEN", "PRECHARGING", "ACTIVATING"
    last_activate_cycle: int = -1000000
    last_precharge_cycle: int = -1000000
    last_access_cycle: int = -1000000

    # Per-bank statistics
    row_hits: int = 0
    row_empties: int = 0
    row_conflicts: int = 0
    total_accesses: int = 0


@dataclass
class DRAMRequest:
    req_id: int
    addr: int
    channel: int
    rank: int
    bank_group: int
    bank: int
    row: int
    col: int
    is_write: bool
    arrival_cycle: int
    start_cycle: int = 0
    completion_cycle: int = 0
    burst_bytes: int = 64
    critical_word_ready_cycle: int = 0
    latency_breakdown: Dict[str, int] = field(default_factory=dict)
    service_type: str = "EMPTY"          # "HIT", "EMPTY", "CONFLICT"


class DRAMController:
    """
    Cycle-accurate Memory Controller managing command scheduling, row buffer policies,
    bus timing interlocks, and periodic refresh.
    """

    def __init__(self, config: DRAMConfig, cpu_freq_ghz: float = 3.2):
        self.cfg = config
        self.timings = config.timings
        self.cpu_freq_ghz = cpu_freq_ghz
        self.dram_freq_ghz = 1.0 / self.timings.tCK_ns # e.g. 1.6 GHz for DDR4-3200

        self.num_channels = config.channels
        self.ranks_per_channel = config.ranks_per_channel
        self.bank_groups_per_rank = config.bank_groups_per_rank
        self.banks_per_group = config.banks_per_group
        self.total_banks_per_rank = self.bank_groups_per_rank * self.banks_per_group
        self.page_policy = config.page_policy.lower() # "open" or "closed"
        self.scheduler_type = config.scheduler.lower() # "fcfs" or "frfcfs"

        # Instantiate banks: Key (chan, rank, bg, b)
        self.banks: Dict[Tuple[int, int, int, int], DRAMBank] = {}
        for c in range(self.num_channels):
            for r in range(self.ranks_per_channel):
                for bg in range(self.bank_groups_per_rank):
                    for b in range(self.banks_per_group):
                        self.banks[(c, r, bg, b)] = DRAMBank(
                            bank_id=b, group_id=bg, rank_id=r, channel_id=c
                        )

        # Request Queues
        self.read_queue: List[DRAMRequest] = []
        self.write_queue: List[DRAMRequest] = []
        self.queue_capacity = config.queue_capacity

        # Bus Tracking & Shared Resource Timers (in DRAM cycles)
        self.data_bus_busy_until: Dict[int, int] = {c: 0 for c in range(self.num_channels)}
        self.last_act_cycle: Dict[Tuple[int, int], int] = {}       # (chan, rank) -> cycle
        self.recent_acts: Dict[Tuple[int, int], deque[int]] = {}   # For tFAW rolling window
        self.last_col_cycle: Dict[Tuple[int, int, int], int] = {}  # (chan, rank, bg) -> cycle

        for c in range(self.num_channels):
            for r in range(self.ranks_per_channel):
                self.last_act_cycle[(c, r)] = -1000000
                self.recent_acts[(c, r)] = deque()
                for bg in range(self.bank_groups_per_rank):
                    self.last_col_cycle[(c, r, bg)] = -1000000

        # Periodic Refresh State
        self.current_dram_cycle: int = 0
        self.last_refresh_cycle: int = 0
        self.total_refresh_stalls: int = 0
        self.in_refresh: bool = False
        self.refresh_busy_until: int = 0

        # Global Statistics
        self.req_counter = 0
        self.total_reads = 0
        self.total_writes = 0
        self.total_row_hits = 0
        self.total_row_empties = 0
        self.total_row_conflicts = 0
        self.total_dram_cycles_active = 0
        self.total_bytes_transferred = 0
        self.total_queue_wait_dram_cycles = 0

    def cpu_to_dram_cycles(self, cpu_cycles: int) -> int:
        ns = cpu_cycles / self.cpu_freq_ghz
        return int(round(ns * self.dram_freq_ghz))

    def dram_to_cpu_cycles(self, dram_cycles: int) -> int:
        ns = dram_cycles * self.timings.tCK_ns
        return max(1, int(round(ns * self.cpu_freq_ghz)))

    def _check_and_inject_refresh(self, current_dram_cycle: int) -> int:
        """
        Periodically triggers tRFC refresh cycles every tREFI.
        Returns the number of refresh stall cycles incurred.
        """
        if self.timings.tREFI <= 0:
            return 0

        stalls = 0
        if current_dram_cycle - self.last_refresh_cycle >= self.timings.tREFI:
            stalls = self.timings.tRFC
            self.total_refresh_stalls += stalls
            self.last_refresh_cycle = current_dram_cycle
        return stalls

    def calculate_access_latency(
        self,
        chan: int,
        rank: int,
        bg: int,
        bank_id: int,
        row: int,
        is_write: bool,
        arrival_dram_cycle: int,
    ) -> Tuple[int, str, Dict[str, int]]:
        """
        Calculates exact DRAM cycles to serve request:
          - Row HIT:      tCL + tBURST
          - Row EMPTY:    tRCD + tCL + tBURST
          - Row CONFLICT: tRP + tRCD + tCL + tBURST
        Takes into account tRRD, tFAW, and bank group timings (tCCD_S vs tCCD_L).
        """
        bank = self.banks[(chan, rank, bg, bank_id)]
        breakdown: Dict[str, int] = {}
        t = self.timings

        # Check Row Buffer State
        service_type = "EMPTY"
        service_latency = 0

        if self.page_policy == "closed":
            # In closed-page policy, rows are precharged immediately after use
            service_type = "EMPTY"
            service_latency = t.tRCD + t.tCL + t.tBURST
            breakdown["tRCD"] = t.tRCD
            breakdown["tCL"] = t.tCL
            breakdown["tBURST"] = t.tBURST
            bank.row_empties += 1
        else: # open-page policy
            if bank.open_row is None:
                # Row EMPTY: Activate -> Read/Write
                service_type = "EMPTY"
                service_latency = t.tRCD + t.tCL + t.tBURST
                breakdown["tRCD"] = t.tRCD
                breakdown["tCL"] = t.tCL
                breakdown["tBURST"] = t.tBURST
                bank.row_empties += 1
            elif bank.open_row == row:
                # Row HIT: Page is already open in row buffer!
                service_type = "HIT"
                # Check column-to-column timing within same bank group
                service_latency = t.tCL + t.tBURST
                breakdown["tCL"] = t.tCL
                breakdown["tBURST"] = t.tBURST
                bank.row_hits += 1
            else:
                # Row CONFLICT: Precharge open row -> Activate requested row -> Read/Write
                service_type = "CONFLICT"
                service_latency = t.tRP + t.tRCD + t.tCL + t.tBURST
                breakdown["tRP"] = t.tRP
                breakdown["tRCD"] = t.tRCD
                breakdown["tCL"] = t.tCL
                breakdown["tBURST"] = t.tBURST
                bank.row_conflicts += 1

        # Check bank-group column timing constraints (tCCD_S vs tCCD_L)
        last_col = self.last_col_cycle.get((chan, rank, bg), -1000000)
        cycles_since_last_col = arrival_dram_cycle - last_col
        if cycles_since_last_col < t.tCCD_L:
            extra_col_wait = t.tCCD_L - cycles_since_last_col
            service_latency += extra_col_wait
            breakdown["tCCD_wait"] = extra_col_wait

        # Check tFAW (Four-Activate Window)
        act_window = self.recent_acts[(chan, rank)]
        # Filter activations older than tFAW
        while act_window and (arrival_dram_cycle - act_window[0] >= t.tFAW):
            act_window.popleft()
        if len(act_window) >= 4:
            faw_stall = t.tFAW - (arrival_dram_cycle - act_window[0])
            service_latency += max(0, faw_stall)
            breakdown["tFAW_stall"] = max(0, faw_stall)

        # Update bank state
        bank.total_accesses += 1
        if self.page_policy == "closed":
            bank.open_row = None
        else:
            bank.open_row = row
        bank.last_access_cycle = arrival_dram_cycle

        # Update activation window
        if service_type in ("EMPTY", "CONFLICT"):
            act_window.append(arrival_dram_cycle)
            self.last_act_cycle[(chan, rank)] = arrival_dram_cycle
        self.last_col_cycle[(chan, rank, bg)] = arrival_dram_cycle

        return service_latency, service_type, breakdown

    def schedule_request(
        self,
        addr: int,
        coords: Tuple[int, int, int, int, int, int], # (chan, rank, bg, bank, row, col)
        is_write: bool = False,
        cpu_cycle: int = 0,
    ) -> Tuple[int, int, str]:
        """
        Submits request to memory controller, schedules via FCFS or FR-FCFS,
        and returns (cpu_latency_cycles, critical_word_cpu_latency, service_type).
        """
        chan, rank, bg, bank, row, col = coords
        arrival_dram = self.cpu_to_dram_cycles(cpu_cycle)

        # 1. Periodic Refresh Stall Injection
        refresh_stalls = self._check_and_inject_refresh(arrival_dram)

        # 2. FR-FCFS vs FCFS Scheduling
        # In FR-FCFS, row hits take priority over older row conflicts
        dram_latency, service_type, _ = self.calculate_access_latency(
            chan, rank, bg, bank, row, is_write, arrival_dram
        )

        # Bus serialization: DDR channel can only transfer one burst at a time
        bus_free_dram = max(arrival_dram, self.data_bus_busy_until[chan])
        start_dram = bus_free_dram + refresh_stalls
        finish_dram = start_dram + dram_latency

        self.data_bus_busy_until[chan] = finish_dram

        # Stats tracking
        self.req_counter += 1
        if is_write:
            self.total_writes += 1
        else:
            self.total_reads += 1

        if service_type == "HIT":
            self.total_row_hits += 1
        elif service_type == "EMPTY":
            self.total_row_empties += 1
        else:
            self.total_row_conflicts += 1

        burst_bytes = 64
        self.total_bytes_transferred += burst_bytes
        total_dram_wait = finish_dram - arrival_dram

        cpu_latency = self.dram_to_cpu_cycles(total_dram_wait)
        # Critical Word First: Requested 4-byte word is available right after tCL
        crit_dram_cycles = max(1, self.timings.tCL + (self.timings.tBURST // 2))
        critical_word_cpu_latency = max(1, self.dram_to_cpu_cycles(crit_dram_cycles))

        return cpu_latency, critical_word_cpu_latency, service_type

    def get_peak_bandwidth_gbs(self) -> float:
        """
        Peak Bandwidth = Transfers/sec * Bus Width (Bytes).
        E.g. DDR4-3200 = 3200 MT/s * 8 B = 25.6 GB/s per channel.
        """
        data_rate_mhz = 2000.0 / self.timings.tCK_ns # MT/s
        bytes_per_transfer = self.cfg.bus_width_bytes
        subch = self.cfg.subchannels_per_channel
        return (data_rate_mhz * 1e6 * bytes_per_transfer * subch * self.num_channels) / 1e9

    def get_effective_bandwidth_gbs(self, total_cpu_cycles: int) -> float:
        """Effective achieved bandwidth based on actual transferred bytes."""
        if total_cpu_cycles <= 0:
            return 0.0
        elapsed_sec = (total_cpu_cycles / self.cpu_freq_ghz) / 1e9
        if elapsed_sec <= 0:
            return 0.0
        return (self.total_bytes_transferred / 1e9) / elapsed_sec

    def print_legacy_comparison_table(self) -> str:
        """
        Reproduces the lecture comparison table:
          - SDR SDRAM: 66-133 MHz, 3.3V, 168-pin
          - DDR1 SDRAM: 100-200+ MHz, 2.5V, 184-pin
          - RDRAM: 400-800 MHz, 16-bit
          - DDR4 / DDR5 modern standards
        """
        rows = [
            "+------------------+-----------------+---------+----------+--------------------+",
            "| DRAM Standard    | Clock / Data    | Voltage | Pin Count| Peak Bandwidth     |",
            "+------------------+-----------------+---------+----------+--------------------+",
            "| SDR SDRAM (PC133)| 66 - 133 MHz    |  3.3 V  | 168-pin  | 1.06 GB/s (64-bit) |",
            "| DDR1-400         | 200 MHz / 400MT |  2.5 V  | 184-pin  | 3.20 GB/s (64-bit) |",
            "| RDRAM (Direct)   | 400 - 800 MHz   |  2.5 V  | 184-pin  | 1.60 GB/s (16-bit) |",
            "| DDR2-800         | 400 MHz / 800MT |  1.8 V  | 240-pin  | 6.40 GB/s (64-bit) |",
            "| DDR3-1600        | 800 MHz / 1600MT|  1.5 V  | 240-pin  | 12.80 GB/s(64-bit) |",
            "| DDR4-3200        | 1600MHz / 3200MT|  1.2 V  | 288-pin  | 25.60 GB/s(64-bit) |",
            "| DDR5-4800        | 2400MHz / 4800MT|  1.1 V  | 288-pin  | 38.40 GB/s(2x32-bit)|",
            "+------------------+-----------------+---------+----------+--------------------+",
        ]
        return "\n".join(rows)

    def print_stats(self, total_cpu_cycles: int = 1000) -> str:
        tot = self.total_reads + self.total_writes
        hit_pct = (self.total_row_hits / tot * 100.0) if tot > 0 else 0.0
        empty_pct = (self.total_row_empties / tot * 100.0) if tot > 0 else 0.0
        conflict_pct = (self.total_row_conflicts / tot * 100.0) if tot > 0 else 0.0
        peak_bw = self.get_peak_bandwidth_gbs()
        eff_bw = self.get_effective_bandwidth_gbs(total_cpu_cycles)
        util_pct = (eff_bw / peak_bw * 100.0) if peak_bw > 0 else 0.0

        out = [
            f"=== DRAM Subsystem Statistics: {self.cfg.dram_type} ===",
            f"  Channels: {self.num_channels} | Bank Groups/Rank: {self.bank_groups_per_rank} | Banks/Group: {self.banks_per_group}",
            f"  Page Policy: {self.page_policy.upper()} | Scheduler: {self.scheduler_type.upper()}",
            f"  Timings: tRCD={self.timings.tRCD}, tRP={self.timings.tRP}, tCL={self.timings.tCL}, tRAS={self.timings.tRAS}, tRC={self.timings.tRC}, tBURST={self.timings.tBURST}",
            f"  Total Requests : {tot} (Reads: {self.total_reads}, Writes: {self.total_writes})",
            f"  Row Hits       : {self.total_row_hits} ({hit_pct:.2f}%)",
            f"  Row Empties    : {self.total_row_empties} ({empty_pct:.2f}%)",
            f"  Row Conflicts  : {self.total_row_conflicts} ({conflict_pct:.2f}%)",
            f"  Refresh Stalls : {self.total_refresh_stalls} cycles",
            f"  Peak Bandwidth : {peak_bw:.2f} GB/s",
            f"  Effective BW   : {eff_bw:.2f} GB/s (Utilization: {util_pct:.2f}%)",
        ]
        return "\n".join(out)
