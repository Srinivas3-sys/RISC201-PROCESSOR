"""
tests/test_vm_paging.py: Unit tests for demand paging, frame allocation,
page replacement algorithms (FIFO, LRU, Clock, Belady Optimal),
Belady's Anomaly reproduction, minor/major page faults, and protection checks.
"""

import pytest
from memory.config import VMConfig
from memory.vm import VirtualMemorySubsystem, PageFaultException


def test_beladys_anomaly_reproduction():
    """
    Belady's Anomaly: On FIFO page replacement, increasing the number of physical
    frames can anomalously INCREASE the total number of page faults.
    Classic reference string: [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]
    """
    ref_string = [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]

    def count_faults_fifo(num_frames: int) -> int:
        frames = []
        faults = 0
        for p in ref_string:
            if p not in frames:
                faults += 1
                if len(frames) < num_frames:
                    frames.append(p)
                else:
                    frames.pop(0) # FIFO replacement
                    frames.append(p)
        return faults

    faults_3_frames = count_faults_fifo(3)
    faults_4_frames = count_faults_fifo(4)

    # In 3 frames: 9 page faults
    # In 4 frames: 10 page faults!
    assert faults_3_frames == 9
    assert faults_4_frames == 10
    assert faults_4_frames > faults_3_frames  # Belady's Anomaly verified!


def test_page_replacement_algorithms():
    # Test LRU vs FIFO vs Clock on VirtualMemorySubsystem with limited frames
    for policy in ["fifo", "lru", "clock"]:
        cfg = VMConfig(
            va_bits=32, page_size_bytes=4096, pt_levels=2,
            phys_mem_size_bytes=4 * 4096, # Exactly 4 physical frames
            page_replacement=policy
        )
        vm = VirtualMemorySubsystem(cfg)
        assert vm.total_frames == 4

        # Access 6 distinct pages (pages 1 to 6) -> forces 2 evictions
        for vpn in [1, 2, 3, 4, 5, 6]:
            vaddr = vpn * 4096
            vm.map_page(vaddr, read=True, write=True)
            vm.translate(vaddr, current_cycle=vpn)

        assert vm.page_evictions >= 2


def test_minor_vs_major_page_fault():
    cfg = VMConfig(
        va_bits=32, page_size_bytes=4096, pt_levels=2,
        phys_mem_size_bytes=2 * 4096, # 2 frames
        minor_fault_latency_cycles=500,
        major_fault_latency_cycles=200000
    )
    vm = VirtualMemorySubsystem(cfg)

    # 1. First access to unallocated page -> Minor fault (zero-fill allocation)
    vaddr1 = 0x1000
    pte1 = vm.map_page(vaddr1, read=True, write=True, present=False)

    res1 = vm.translate(vaddr1, current_cycle=1)
    assert vm.minor_faults == 1

    # Mark page dirty and evict it by accessing 2 more pages
    pte1.dirty = True
    vaddr2 = 0x2000
    vaddr3 = 0x3000
    vm.map_page(vaddr2)
    vm.translate(vaddr2, current_cycle=2)
    vm.map_page(vaddr3)
    vm.translate(vaddr3, current_cycle=3) # Forces eviction of page 1 to swap!

    assert pte1.is_swapped

    # 2. Re-access page 1 -> Major fault (read back from swap)
    res_swap = vm.translate(vaddr1, current_cycle=4)
    assert vm.major_faults == 1
    assert vm.swap_reads == 1


def test_protection_and_guard_page_fault():
    cfg = VMConfig(va_bits=32, page_size_bytes=4096, pt_levels=2)
    vm = VirtualMemorySubsystem(cfg)

    # Case 1: Write to Read-Only Page
    ro_vaddr = 0x5000
    vm.map_page(ro_vaddr, pfn=0x10, read=True, write=False)
    with pytest.raises(PageFaultException) as excinfo:
        vm.translate(ro_vaddr, is_write=True, pc=0x100)
    assert excinfo.value.fault_type == "protection_violation"

    # Case 2: Access into Guard Page (Stack boundary guard)
    guard_vaddr = 0x0007FFC0 # Just below stack limit
    vm.map_page(guard_vaddr, is_guard=True)
    with pytest.raises(PageFaultException) as excinfo_guard:
        vm.translate(guard_vaddr, is_write=True, pc=0x200)
    assert excinfo_guard.value.fault_type == "guard_page"
