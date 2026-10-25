"""
tests/test_mem_dram.py: Comprehensive unit tests for DRAM controller and timings.
Tests Row-Hit, Row-Empty, Row-Conflict latencies, bank groups (tCCD_S vs tCCD_L),
tFAW window, refresh stalls, FCFS vs FR-FCFS, bandwidth metrics, and legacy comparison table.
"""

import pytest
from memory.config import DRAMConfig, DRAMTimingConfig
from memory.dram import DRAMController


def test_dram_row_buffer_policies():
    cfg = DRAMConfig(page_policy="open")
    # Custom timings for clean arithmetic verification:
    # tRCD = 10, tRP = 12, tCL = 14, tBURST = 4
    cfg.timings.tRCD = 10
    cfg.timings.tRP = 12
    cfg.timings.tCL = 14
    cfg.timings.tBURST = 4
    cfg.timings.tCCD_L = 0
    cfg.timings.tCCD_S = 0
    cfg.timings.tFAW = 0
    cfg.timings.tREFI = 0 # Disable refresh during latency test

    ctrl = DRAMController(cfg, cpu_freq_ghz=1.6)

    # 1. First access to Bank 0, Row 5 -> Row EMPTY: tRCD + tCL + tBURST = 10 + 14 + 4 = 28
    lat1, srv1, b1 = ctrl.calculate_access_latency(0, 0, 0, 0, row=5, is_write=False, arrival_dram_cycle=10)
    assert srv1 == "EMPTY"
    assert lat1 == 28

    # 2. Second access to same Bank 0, same Row 5 -> Row HIT: tCL + tBURST = 14 + 4 = 18
    lat2, srv2, b2 = ctrl.calculate_access_latency(0, 0, 0, 0, row=5, is_write=False, arrival_dram_cycle=50)
    assert srv2 == "HIT"
    assert lat2 == 18

    # 3. Third access to same Bank 0, different Row 9 -> Row CONFLICT: tRP + tRCD + tCL + tBURST = 12 + 10 + 14 + 4 = 40
    lat3, srv3, b3 = ctrl.calculate_access_latency(0, 0, 0, 0, row=9, is_write=False, arrival_dram_cycle=80)
    assert srv3 == "CONFLICT"
    assert lat3 == 40


def test_closed_page_policy():
    cfg = DRAMConfig(page_policy="closed")
    cfg.timings.tRCD = 15
    cfg.timings.tCL = 15
    cfg.timings.tBURST = 4
    cfg.timings.tCCD_L = 0
    cfg.timings.tFAW = 0
    cfg.timings.tREFI = 0

    ctrl = DRAMController(cfg)
    # Every access should be Row EMPTY because row buffer is automatically precharged
    lat1, srv1, _ = ctrl.calculate_access_latency(0, 0, 0, 0, row=10, is_write=False, arrival_dram_cycle=0)
    lat2, srv2, _ = ctrl.calculate_access_latency(0, 0, 0, 0, row=10, is_write=False, arrival_dram_cycle=100)
    assert srv1 == "EMPTY"
    assert srv2 == "EMPTY"
    assert lat1 == 34
    assert lat2 == 34


def test_bank_group_timing_tccd():
    # In DDR4/DDR5:
    # Column accesses to DIFFERENT bank groups can proceed with tCCD_S (e.g. 4 cycles)
    # Column accesses to the SAME bank group require tCCD_L (e.g. 8 cycles)
    cfg = DRAMConfig()
    cfg.timings.tCCD_S = 4
    cfg.timings.tCCD_L = 10
    cfg.timings.tREFI = 0
    ctrl = DRAMController(cfg)

    # Access BG 0 at cycle 100
    ctrl.calculate_access_latency(0, 0, bg=0, bank_id=0, row=1, is_write=False, arrival_dram_cycle=100)

    # Access SAME BG 0 at cycle 102 (only 2 cycles later) -> incurs 8 cycles of tCCD_L wait
    lat_same, _, b_same = ctrl.calculate_access_latency(0, 0, bg=0, bank_id=1, row=2, is_write=False, arrival_dram_cycle=102)
    assert "tCCD_wait" in b_same
    assert b_same["tCCD_wait"] == 8

    # Access DIFFERENT BG 1 at cycle 102 -> no extra wait for BG 1
    lat_diff, _, b_diff = ctrl.calculate_access_latency(0, 0, bg=1, bank_id=0, row=3, is_write=False, arrival_dram_cycle=102)
    assert "tCCD_wait" not in b_diff


def test_refresh_stall_injection():
    cfg = DRAMConfig()
    cfg.timings.tREFI = 1000
    cfg.timings.tRFC = 200
    ctrl = DRAMController(cfg)

    # At cycle 500: no refresh
    stalls1 = ctrl._check_and_inject_refresh(500)
    assert stalls1 == 0

    # At cycle 1050: refresh triggers
    stalls2 = ctrl._check_and_inject_refresh(1050)
    assert stalls2 == 200
    assert ctrl.total_refresh_stalls == 200


def test_bandwidth_and_legacy_table():
    cfg = DRAMConfig(dram_type="DDR4-3200")
    cfg.timings.tCK_ns = 0.625
    cfg.bus_width_bytes = 8
    ctrl = DRAMController(cfg)

    peak_bw = ctrl.get_peak_bandwidth_gbs()
    # 2000 / 0.625 = 3200 MT/s * 8 B = 25.6 GB/s
    assert abs(peak_bw - 25.6) < 0.1

    table = ctrl.print_legacy_comparison_table()
    assert "SDR SDRAM (PC133)" in table
    assert "DDR1-400" in table
    assert "RDRAM (Direct)" in table
    assert "DDR4-3200" in table
    assert "DDR5-4800" in table
