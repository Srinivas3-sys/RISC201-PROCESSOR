"""
tests/test_vm_translation.py: Unit tests for Virtual Memory address translation,
multi-level page tables, TLBs, PWC, ASID isolation, and reference model cross-checks.
"""

import pytest
from memory.config import VMConfig, TLBConfig
from memory.vm import VirtualMemorySubsystem, PTE, TLB, TLBEntry, PageFaultException


class ReferenceTLB:
    """Tiny independent reference fully-associative TLB for cross-checking."""
    def __init__(self, entries: int):
        self.entries = entries
        self.table: dict[int, int] = {}
        self.hits = 0
        self.misses = 0

    def lookup(self, vpn: int) -> bool:
        if vpn in self.table:
            self.hits += 1
            self.table.pop(vpn)
            self.table[vpn] = 1
            return True
        else:
            self.misses += 1
            if len(self.table) >= self.entries:
                self.table.pop(next(iter(self.table)))
            self.table[vpn] = 1
            return False


def test_vpn_offset_splits():
    # 4 KB page (12 bits offset)
    cfg4k = VMConfig(va_bits=48, page_size_bytes=4096, pt_levels=4)
    vm4k = VirtualMemorySubsystem(cfg4k)
    indices, off = vm4k.split_virtual_address(0x00007FFF1040)
    assert off == 0x040
    assert len(indices) == 4

    # 2 MB huge page (21 bits offset)
    cfg2m = VMConfig(va_bits=48, page_size_bytes=2 * 1024 * 1024, pt_levels=4)
    vm2m = VirtualMemorySubsystem(cfg2m)
    indices_m, off_m = vm2m.split_virtual_address(0x00007FFF1040)
    assert off_m == (0x00007FFF1040 & ((1 << 21) - 1))


def test_two_level_page_walk():
    # 32-bit address, 2-level page table: 10 bits dir, 10 bits pt, 12 bits offset
    cfg = VMConfig(va_bits=32, page_size_bytes=4096, pt_levels=2)
    vm = VirtualMemorySubsystem(cfg)

    # Hand-computed address: 0x00401020
    # vpn = 0x00401 (1025). dir_idx = 1025 >> 10 = 1. pt_idx = 1025 & 0x3FF = 1. offset = 0x020
    vaddr = 0x00401020
    vm.map_page(vaddr, pfn=0x55, read=True, write=True)

    indices, off = vm.split_virtual_address(vaddr)
    assert indices == [1, 1]
    assert off == 0x020

    res = vm.translate(vaddr, is_fetch=False, is_write=False, current_cycle=1)
    # Translated physical address: (0x55 << 12) | 0x020 = 0x00055020
    assert res.paddr == 0x00055020
    assert not res.tlb_hit # First access misses in TLB, walks page table!

    # Second access must HIT in DTLB!
    res2 = vm.translate(vaddr, is_fetch=False, is_write=False, current_cycle=2)
    assert res2.tlb_hit
    assert res2.tlb_level == "DTLB"
    assert res2.paddr == 0x00055020


def test_four_level_radix_walk():
    cfg = VMConfig(va_bits=48, page_size_bytes=4096, pt_levels=4)
    vm = VirtualMemorySubsystem(cfg)

    vaddr = 0x0000123456781234
    vm.map_page(vaddr, pfn=0x99, read=True, write=True)

    res = vm.translate(vaddr, is_fetch=False, current_cycle=1)
    assert res.paddr == ((0x99 << 12) | (vaddr & 0xFFF))
    assert res.walk_levels == 4


def test_asid_isolation():
    cfg = VMConfig(va_bits=32, page_size_bytes=4096, pt_levels=2, asid_enabled=True)
    vm = VirtualMemorySubsystem(cfg)

    vaddr = 0x10000 # Same virtual address in both processes
    # Process 0 maps vaddr -> PFN 0x10
    vm.map_page(vaddr, pfn=0x10, asid=0)
    # Process 1 maps vaddr -> PFN 0x20
    vm.map_page(vaddr, pfn=0x20, asid=1)

    # Translate in ASID 0
    res0 = vm.translate(vaddr, asid=0, current_cycle=1)
    assert res0.paddr == (0x10 << 12)

    # Translate in ASID 1 -> MUST NOT hit ASID 0's TLB entry!
    res1 = vm.translate(vaddr, asid=1, current_cycle=2)
    assert res1.paddr == (0x20 << 12)


def test_dirty_accessed_bit_updates():
    cfg = VMConfig(va_bits=32, page_size_bytes=4096, pt_levels=2)
    vm = VirtualMemorySubsystem(cfg)

    vaddr = 0x20000
    pte = vm.map_page(vaddr, pfn=0x30, read=True, write=True)
    assert not pte.accessed
    assert not pte.dirty

    # 1. Read access -> sets accessed bit
    vm.translate(vaddr, is_write=False, current_cycle=1)
    assert pte.accessed
    assert not pte.dirty

    # 2. Write access -> sets dirty bit
    vm.translate(vaddr, is_write=True, current_cycle=2)
    assert pte.dirty


def test_page_walk_cache_hit():
    cfg = VMConfig(va_bits=48, pt_levels=4, pwc_enabled=True)
    vm = VirtualMemorySubsystem(cfg)

    vaddr1 = 0x000010000000
    vaddr2 = 0x000010001000 # Same upper directory prefix
    vm.map_page(vaddr1, pfn=0x40)
    vm.map_page(vaddr2, pfn=0x41)

    # First translation populates PWC
    vm.translate(vaddr1, current_cycle=1)

    # Second translation to nearby page should hit in PWC, skipping levels
    res2 = vm.translate(vaddr2, current_cycle=2)
    assert vm.pwc_hits >= 1


def test_cross_check_reference_tlb():
    cfg = TLBConfig(entries=8, associativity=8) # fully associative TLB
    tlb = TLB(cfg)
    ref = ReferenceTLB(entries=8)

    vpn_stream = [1, 2, 3, 4, 1, 2, 5, 6, 7, 8, 9, 1, 2]
    for cycle, vpn in enumerate(vpn_stream):
        ref_hit = ref.lookup(vpn)
        t_hit, entry = tlb.lookup(vpn, current_cycle=cycle)
        if not t_hit:
            tlb.insert(TLBEntry(valid=True, vpn=vpn, pfn=vpn * 2), current_cycle=cycle)
        assert t_hit == ref_hit
    assert tlb.hits == ref.hits
    assert tlb.misses == ref.misses
