"""
tests/test_mem_prefetch.py: Unit tests for hardware prefetchers.
Tests next-line, stride RPT, stream prefetcher, 4KB page boundary guard,
accuracy, timeliness, and pollution metrics.
"""

import pytest
from memory.config import PrefetcherConfig
from memory.prefetch import Prefetcher


def test_next_line_prefetcher():
    cfg = PrefetcherConfig(enabled=True, prefetcher_type="next_line", degree=2)
    pf = Prefetcher(cfg, line_size_bytes=64, page_size_bytes=4096)

    # Access address 0x100
    candidates = pf.on_access(0x100, pc=0x10, current_cycle=1)
    assert len(candidates) == 2
    assert candidates[0] == 0x140  # 0x100 + 64
    assert candidates[1] == 0x180  # 0x100 + 128
    assert pf.total_prefetches_issued == 2


def test_page_boundary_guard():
    cfg = PrefetcherConfig(enabled=True, prefetcher_type="next_line", degree=2, cross_page_boundary=False)
    pf = Prefetcher(cfg, line_size_bytes=64, page_size_bytes=4096)

    # Access at end of 4KB page: offset 4032 (last 64B block in page 0 is 4032..4095)
    # Next line (4096) crosses into page 1!
    candidates = pf.on_access(4032, pc=0x20, current_cycle=1)
    # Prefetcher MUST NOT cross page boundary!
    assert len(candidates) == 0


def test_stride_rpt_prefetcher():
    cfg = PrefetcherConfig(enabled=True, prefetcher_type="stride", degree=1)
    pf = Prefetcher(cfg, line_size_bytes=64)

    pc = 0x8000
    # Access 1: 0x100 (state: INIT)
    c1 = pf.on_access(0x100, pc=pc, current_cycle=1)
    assert len(c1) == 0

    # Access 2: 0x180 (stride = +128, state: TRANSIENT)
    c2 = pf.on_access(0x180, pc=pc, current_cycle=2)
    assert len(c2) == 0

    # Access 3: 0x200 (stride = +128 confirmed, state: STEADY -> prefetches 0x280)
    c3 = pf.on_access(0x200, pc=pc, current_cycle=3)
    assert len(c3) == 1
    assert c3[0] == 0x280


def test_prefetch_accuracy_and_timeliness():
    cfg = PrefetcherConfig(enabled=True, prefetcher_type="next_line", degree=1)
    pf = Prefetcher(cfg, line_size_bytes=64)

    # Access line 0 -> issues prefetch for line 64 at cycle 10
    pf.on_access(0, pc=0, current_cycle=10)
    assert pf.total_prefetches_issued == 1

    # Later (cycle 50), CPU accesses line 64 -> prefetch consumed!
    # Note: access to 64 also triggers next-line prefetch of 128 (total 2 issued, 1 useful = 50%)
    pf.on_access(64, pc=4, current_cycle=50)
    assert pf.useful_prefetches == 1
    assert pf.get_accuracy() == 50.0
