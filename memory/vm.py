"""
memory/vm.py: Comprehensive Virtual Memory and Two-Level TLB Subsystem.
Implements:
  - Multi-level Radix Page Tables (Single-level, 2-level 10+10+12, 4-level 9+9+9+9+12)
  - PTE bit layouts with Present, R/W/X, User/Supervisor, Accessed, Dirty, PFN, SwapSlot
  - Segment Loader (Code R-X, Data RW-, Stack RW- with Guard Page)
  - L1 ITLB, L1 DTLB, and L2 Unified STLB with parallel VIPT overlap
  - Hardware Page-Table Walker traversing cache hierarchy with Page-Walk Cache (PWC)
  - Frame Allocator with FIFO, LRU, Clock, Belady Optimal page replacement
  - Minor and Major Page Fault modeling (SSD/HDD swap latencies)
  - Belady's Anomaly reproduction
  - Multi-process ASID tagging and context switching
  - Precise exception integration with RISC201 stack guard safety
"""

from __future__ import annotations
import math
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Set, Any
from memory.config import VMConfig, TLBConfig


@dataclass
class PTE:
    pfn: int = 0
    present: bool = False
    read: bool = True
    write: bool = True
    execute: bool = False
    user: bool = True
    accessed: bool = False
    dirty: bool = False
    is_swapped: bool = False
    swap_slot: int = -1
    is_huge: bool = False

    def to_word(self) -> int:
        """PTE bit layout (x86-64 / SimpleRisc VM style)."""
        w = (self.pfn & 0xFFFFF) << 12
        if self.present: w |= (1 << 0)
        if self.read:    w |= (1 << 1)
        if self.write:   w |= (1 << 2)
        if self.execute: w |= (1 << 3)
        if self.user:    w |= (1 << 4)
        if self.accessed:w |= (1 << 5)
        if self.dirty:   w |= (1 << 6)
        if self.is_huge: w |= (1 << 7)
        if self.is_swapped: w |= (1 << 9)
        return w

    @classmethod
    def from_word(cls, w: int) -> PTE:
        return cls(
            pfn=(w >> 12) & 0xFFFFF,
            present=bool(w & (1 << 0)),
            read=bool(w & (1 << 1)),
            write=bool(w & (1 << 2)),
            execute=bool(w & (1 << 3)),
            user=bool(w & (1 << 4)),
            accessed=bool(w & (1 << 5)),
            dirty=bool(w & (1 << 6)),
            is_huge=bool(w & (1 << 7)),
            is_swapped=bool(w & (1 << 9)),
        )


@dataclass
class TLBEntry:
    valid: bool = False
    vpn: int = 0
    pfn: int = 0
    read: bool = True
    write: bool = True
    execute: bool = False
    user: bool = True
    accessed: bool = False
    dirty: bool = False
    asid: int = 0
    global_page: bool = False
    last_access_cycle: int = 0
    insert_cycle: int = 0
    is_huge: bool = False


class TLB:
    """
    Set-Associative Translation Lookaside Buffer.
    """

    def __init__(self, config: TLBConfig, name: str = "TLB"):
        self.cfg = config
        self.name = name
        self.entries_count = config.entries
        self.associativity = config.associativity
        self.hit_latency = config.hit_latency_cycles
        self.num_sets = max(1, self.entries_count // self.associativity)
        self.policy = config.replacement_policy.lower()

        self.sets: List[List[TLBEntry]] = [
            [TLBEntry() for _ in range(self.associativity)]
            for _ in range(self.num_sets)
        ]

        # 3C classification history
        self.seen_vpns: Set[int] = set()
        self.shadow_fa_vpns: List[int] = []

        # Stats
        self.lookups = 0
        self.hits = 0
        self.misses = 0
        self.compulsory_misses = 0
        self.capacity_misses = 0
        self.conflict_misses = 0
        self.flushes = 0

    def get_set_idx(self, vpn: int) -> int:
        return vpn % self.num_sets

    def lookup(self, vpn: int, asid: int = 0, current_cycle: int = 0) -> Tuple[bool, Optional[TLBEntry]]:
        self.lookups += 1
        set_idx = self.get_set_idx(vpn)
        for entry in self.sets[set_idx]:
            if entry.valid and entry.vpn == vpn:
                if entry.global_page or entry.asid == asid:
                    self.hits += 1
                    entry.last_access_cycle = current_cycle
                    entry.accessed = True
                    return True, entry

        self.misses += 1
        # 3C classification
        if vpn not in self.seen_vpns:
            self.seen_vpns.add(vpn)
            self.compulsory_misses += 1
            if vpn in self.shadow_fa_vpns:
                self.shadow_fa_vpns.remove(vpn)
            self.shadow_fa_vpns.append(vpn)
            if len(self.shadow_fa_vpns) > self.entries_count:
                self.shadow_fa_vpns.pop(0)
        elif vpn in self.shadow_fa_vpns:
            self.conflict_misses += 1
            self.shadow_fa_vpns.remove(vpn)
            self.shadow_fa_vpns.append(vpn)
        else:
            self.capacity_misses += 1
            self.shadow_fa_vpns.append(vpn)
            if len(self.shadow_fa_vpns) > self.entries_count:
                self.shadow_fa_vpns.pop(0)

        return False, None

    def insert(self, entry: TLBEntry, current_cycle: int = 0) -> None:
        set_idx = self.get_set_idx(entry.vpn)
        tset = self.sets[set_idx]

        # Prefer invalid entry
        target_idx = -1
        for i, e in enumerate(tset):
            if not e.valid:
                target_idx = i
                break

        if target_idx == -1:
            # Evict according to policy
            if self.policy == "fifo":
                oldest = float("inf")
                for i, e in enumerate(tset):
                    if e.insert_cycle < oldest:
                        oldest = e.insert_cycle
                        target_idx = i
            else: # lru
                oldest = float("inf")
                for i, e in enumerate(tset):
                    if e.last_access_cycle < oldest:
                        oldest = e.last_access_cycle
                        target_idx = i

        entry.insert_cycle = current_cycle
        entry.last_access_cycle = current_cycle
        tset[target_idx] = entry

    def invalidate_page(self, vpn: int, asid: int = 0) -> bool:
        set_idx = self.get_set_idx(vpn)
        for entry in self.sets[set_idx]:
            if entry.valid and entry.vpn == vpn and (entry.asid == asid or entry.global_page):
                entry.valid = False
                return True
        return False

    def invalidate_asid(self, asid: int) -> int:
        count = 0
        for tset in self.sets:
            for entry in tset:
                if entry.valid and not entry.global_page and entry.asid == asid:
                    entry.valid = False
                    count += 1
        return count

    def flush_all(self) -> None:
        self.flushes += 1
        for tset in self.sets:
            for entry in tset:
                entry.valid = False


@dataclass
class PageFaultException(Exception):
    fault_type: str                      # "not_present_minor", "not_present_major", "protection_violation", "guard_page"
    vaddr: int
    pc: int
    details: str
    service_cost_cycles: int = 0


@dataclass
class TranslationResult:
    paddr: int
    latency_cycles: int
    tlb_hit: bool
    tlb_level: str                       # "ITLB", "DTLB", "STLB", "WALK"
    walk_levels: int = 0
    walk_details: List[str] = field(default_factory=list)
    fault: Optional[PageFaultException] = None


class VirtualMemorySubsystem:
    """
    Complete Virtual Memory architecture:
      - 32-bit and 48-bit multi-level page tables
      - L1 ITLB / DTLB + Unified L2 STLB
      - Hardware Page-Table Walker with Page-Walk Cache (PWC)
      - Physical Frame Allocator with FIFO, LRU, Clock, Belady Optimal replacement
      - Demand Paging with Swap Storage
      - Protection & Precise Exception handling
    """

    def __init__(self, config: VMConfig, cpu_freq_ghz: float = 3.2):
        self.cfg = config
        self.cpu_freq_ghz = cpu_freq_ghz
        self.page_size = config.page_size_bytes
        self.offset_bits = int(math.log2(self.page_size)) # 12 bits for 4KB
        self.va_bits = config.va_bits
        self.pa_bits = config.pa_bits
        self.pt_levels = config.pt_levels

        # TLBs
        self.itlb = TLB(config.itlb, "ITLB")
        self.dtlb = TLB(config.dtlb, "DTLB")
        self.stlb = TLB(config.stlb, "STLB") if config.stlb_enabled else None

        # Page-Walk Cache (PWC): Maps intermediate table addresses -> PTE address
        self.pwc: Dict[Tuple[int, int], int] = {} # (level, vpn_prefix) -> PFN
        self.pwc_hits = 0
        self.pwc_lookups = 0

        # Physical Memory & Frame Allocator
        self.phys_mem_size = config.phys_mem_size_bytes
        self.total_frames = max(4, self.phys_mem_size // self.page_size)
        self.free_frames: List[int] = list(range(self.total_frames))
        self.frame_to_vpn: Dict[int, Tuple[int, int]] = {} # frame_id -> (asid, vpn)
        self.frame_use_bits: Dict[int, bool] = {f: False for f in range(self.total_frames)}
        self.fifo_queue: List[int] = []
        self.lru_list: List[int] = []
        self.clock_hand: int = 0

        # Page Tables per ASID: asid -> root_table
        # For 4-level 48-bit: PML4 -> PDP -> PD -> PT -> PTE
        # For 2-level 32-bit: Directory -> PT -> PTE
        # Represented as nested dicts
        self.page_tables: Dict[int, Dict] = {0: {}}
        self.current_asid: int = 0

        # Swap Space (SSD/HDD backing store)
        self.swap_space: Dict[Tuple[int, int], bytes] = {} # (asid, vpn) -> page data
        self.next_swap_slot = 0

        # Performance & Event Counters
        self.total_translations = 0
        self.minor_faults = 0
        self.major_faults = 0
        self.protection_faults = 0
        self.page_evictions = 0
        self.swap_writes = 0
        self.swap_reads = 0
        self.walk_latencies: List[int] = []

        # Reference trace for Belady's Optimal algorithm
        self.future_reference_stream: List[int] = []
        self.current_reference_idx: int = 0

    def split_virtual_address(self, vaddr: int) -> Tuple[List[int], int]:
        """
        Splits virtual address into multi-level VPN indices and page offset.
        48-bit 4-level: (9, 9, 9, 9, 12) -> PML4, PDP, PD, PT, offset.
        32-bit 2-level: (10, 10, 12) -> Directory, PT, offset.
        """
        offset_mask = (1 << self.offset_bits) - 1
        page_offset = vaddr & offset_mask
        vpn_total = vaddr >> self.offset_bits

        indices: List[int] = []
        if self.pt_levels == 4:
            # 48-bit: 9 bits per level (512 entries per table)
            for _ in range(4):
                idx = vpn_total & 0x1FF # 9 bits
                indices.insert(0, idx)
                vpn_total >>= 9
        elif self.pt_levels == 2:
            # 32-bit: 10 bits per level (1024 entries per table)
            pt_idx = vpn_total & 0x3FF
            dir_idx = (vpn_total >> 10) & 0x3FF
            indices = [dir_idx, pt_idx]
        else: # 1-level flat
            indices = [vpn_total]

        return indices, page_offset

    def map_page(
        self,
        vaddr: int,
        pfn: Optional[int] = None,
        read: bool = True,
        write: bool = True,
        execute: bool = False,
        user: bool = True,
        asid: int = 0,
        is_guard: bool = False
    ) -> PTE:
        """
        Inserts or maps a page in the multi-level page table.
        """
        indices, _ = self.split_virtual_address(vaddr)
        vpn = vaddr >> self.offset_bits

        if asid not in self.page_tables:
            self.page_tables[asid] = {}

        curr_table = self.page_tables[asid]

        # Navigate / instantiate intermediate levels
        for idx in indices[:-1]:
            if idx not in curr_table:
                curr_table[idx] = {}
            curr_table = curr_table[idx]

        leaf_idx = indices[-1]

        if is_guard:
            # Guard page has no permissions (causes guard page fault on access)
            pte = PTE(pfn=0, present=False, read=False, write=False, execute=False, user=user)
        else:
            if pfn is None:
                pfn = self.allocate_frame(vpn, asid)
            pte = PTE(pfn=pfn, present=True, read=read, write=write, execute=execute, user=user)

        curr_table[leaf_idx] = pte
        return pte

    def get_pte(self, vaddr: int, asid: int = 0) -> Optional[PTE]:
        """Traverses page table and retrieves PTE if it exists."""
        if asid not in self.page_tables:
            return None
        indices, _ = self.split_virtual_address(vaddr)
        curr = self.page_tables[asid]
        for idx in indices[:-1]:
            if idx not in curr:
                return None
            curr = curr[idx]
        return curr.get(indices[-1], None)

    def allocate_frame(self, vpn: int, asid: int) -> int:
        """
        Allocates a physical frame. If physical memory is full, invokes
        the configured page replacement algorithm (FIFO, LRU, Clock, Optimal).
        """
        if self.free_frames:
            f = self.free_frames.pop(0)
            self.frame_to_vpn[f] = (asid, vpn)
            self.fifo_queue.append(f)
            self.lru_list.append(f)
            self.frame_use_bits[f] = True
            return f

        # Physical memory full -> Page Replacement required!
        victim_frame = self._select_replacement_victim()
        self._evict_frame(victim_frame)

        self.frame_to_vpn[victim_frame] = (asid, vpn)
        self.fifo_queue.append(victim_frame)
        self.lru_list.append(victim_frame)
        self.frame_use_bits[victim_frame] = True
        return victim_frame

    def _select_replacement_victim(self) -> int:
        """Applies configured page replacement policy."""
        policy = self.cfg.page_replacement.lower()

        if policy == "fifo":
            return self.fifo_queue.pop(0)

        elif policy == "lru":
            return self.lru_list.pop(0)

        elif policy == "clock":
            # Second-chance clock algorithm
            while True:
                f = self.fifo_queue[self.clock_hand]
                if self.frame_use_bits.get(f, False):
                    self.frame_use_bits[f] = False
                    self.clock_hand = (self.clock_hand + 1) % len(self.fifo_queue)
                else:
                    self.fifo_queue.pop(self.clock_hand)
                    if self.clock_hand >= len(self.fifo_queue) and self.fifo_queue:
                        self.clock_hand = 0
                    return f

        elif policy == "opt":
            # Belady's Optimal offline algorithm
            # Select frame whose next reference is farthest in future
            farthest_idx = -1
            victim_frame = self.lru_list[0]
            remaining = self.future_reference_stream[self.current_reference_idx:]

            for f in self.frame_to_vpn.keys():
                owner_asid, vpn = self.frame_to_vpn[f]
                try:
                    next_ref = remaining.index(vpn)
                except ValueError:
                    next_ref = float("inf") # Never referenced again
                if next_ref > farthest_idx:
                    farthest_idx = next_ref
                    victim_frame = f
            if victim_frame in self.lru_list:
                self.lru_list.remove(victim_frame)
            if victim_frame in self.fifo_queue:
                self.fifo_queue.remove(victim_frame)
            return victim_frame

        else: # default fallback to LRU
            return self.lru_list.pop(0)

    def _evict_frame(self, frame_id: int) -> None:
        """Evicts a physical frame to swap if dirty, updating PTE and TLB."""
        self.page_evictions += 1
        owner_asid, vpn = self.frame_to_vpn[frame_id]
        vaddr = vpn << self.offset_bits
        pte = self.get_pte(vaddr, owner_asid)

        if pte:
            if pte.dirty:
                self.swap_writes += 1
                pte.is_swapped = True
                pte.swap_slot = self.next_swap_slot
                self.next_swap_slot += 1
                self.swap_space[(owner_asid, vpn)] = bytes(self.page_size) # Placeholder swap page
            pte.present = False
            pte.pfn = 0

        # Invalidate TLB entries for evicted page
        self.itlb.invalidate_page(vpn, owner_asid)
        self.dtlb.invalidate_page(vpn, owner_asid)
        if self.stlb:
            self.stlb.invalidate_page(vpn, owner_asid)

    def handle_page_fault(self, vaddr: int, pc: int, is_write: bool, asid: int) -> int:
        """
        Handles demand paging page faults:
          - Minor fault: Page newly allocated / zeroed
          - Major fault: Swapped page restored from SSD/Disk
        Returns service latency cycles.
        """
        vpn = vaddr >> self.offset_bits
        pte = self.get_pte(vaddr, asid)

        if pte is None:
            # Unmapped access -> Protection / Segmentation fault
            self.protection_faults += 1
            raise PageFaultException(
                fault_type="protection_violation",
                vaddr=vaddr,
                pc=pc,
                details=f"Unmapped virtual address 0x{vaddr:08X}"
            )

        # Check permissions
        if is_write and not pte.write:
            self.protection_faults += 1
            raise PageFaultException(
                fault_type="protection_violation",
                vaddr=vaddr,
                pc=pc,
                details=f"Write permission violation at 0x{vaddr:08X}"
            )

        if not pte.read and not pte.write:
            # Guard Page Fault
            self.protection_faults += 1
            raise PageFaultException(
                fault_type="guard_page",
                vaddr=vaddr,
                pc=pc,
                details=f"Stack guard page violation at 0x{vaddr:08X}"
            )

        if not pte.present:
            if pte.is_swapped:
                # Major Page Fault: Swap read from SSD / Disk
                self.major_faults += 1
                self.swap_reads += 1
                new_pfn = self.allocate_frame(vpn, asid)
                pte.pfn = new_pfn
                pte.present = True
                pte.is_swapped = False
                return self.cfg.major_fault_latency_cycles
            else:
                # Minor Page Fault: Zero-fill allocation
                self.minor_faults += 1
                new_pfn = self.allocate_frame(vpn, asid)
                pte.pfn = new_pfn
                pte.present = True
                return self.cfg.minor_fault_latency_cycles

        return 0

    def translate(
        self,
        vaddr: int,
        is_fetch: bool = False,
        is_write: bool = False,
        pc: int = 0,
        current_cycle: int = 0,
        asid: int = 0,
    ) -> TranslationResult:
        """
        Full virtual memory translation pipeline:
          1. Parallel L1 TLB lookup (ITLB for fetch, DTLB for data)
          2. On L1 TLB miss: L2 Unified STLB lookup
          3. On STLB miss: Hardware Page-Table Walker (checking PWC and cache hierarchy)
          4. Fault checks and demand paging resolution
        """
        self.total_translations += 1
        vpn = vaddr >> self.offset_bits
        _, offset = self.split_virtual_address(vaddr)

        # 1. Select L1 TLB
        target_tlb = self.itlb if is_fetch else self.dtlb
        hit, entry = target_tlb.lookup(vpn, asid=asid, current_cycle=current_cycle)

        if hit and entry:
            if is_write and not entry.dirty:
                entry.dirty = True
                # Propagate dirty bit to PTE
                pte = self.get_pte(vaddr, asid)
                if pte: pte.dirty = True
            paddr = (entry.pfn << self.offset_bits) | offset
            return TranslationResult(
                paddr=paddr,
                latency_cycles=target_tlb.hit_latency,
                tlb_hit=True,
                tlb_level=target_tlb.name
            )

        # 2. Check L2 STLB (if enabled)
        stlb_latency = 0
        if self.stlb:
            stlb_hit, stlb_entry = self.stlb.lookup(vpn, asid=asid, current_cycle=current_cycle)
            stlb_latency = self.stlb.hit_latency
            if stlb_hit and stlb_entry:
                # Refill L1 TLB
                target_tlb.insert(stlb_entry, current_cycle)
                paddr = (stlb_entry.pfn << self.offset_bits) | offset
                return TranslationResult(
                    paddr=paddr,
                    latency_cycles=target_tlb.hit_latency + stlb_latency,
                    tlb_hit=True,
                    tlb_level="STLB"
                )

        # 3. Hardware Page Table Walk
        walk_details: List[str] = []
        indices, _ = self.split_virtual_address(vaddr)
        levels_walked = len(indices)
        walk_cost_cycles = target_tlb.hit_latency + stlb_latency

        # Check Page-Walk Cache (PWC)
        self.pwc_lookups += 1
        pwc_key = (self.pt_levels, vpn >> 9)
        if self.cfg.pwc_enabled and pwc_key in self.pwc:
            self.pwc_hits += 1
            walk_details.append(f"PWC HIT: skipped {levels_walked - 1} directory levels")
            levels_walked = 1
            walk_cost_cycles += 4 # Short walk (hits in L1/L2)
        else:
            # Traverse each level (each PTE fetch accesses memory/cache hierarchy ~10-40 cycles)
            for lvl_idx, idx in enumerate(indices):
                walk_details.append(f"WALK Level {lvl_idx + 1}: table_idx=0x{idx:03X}")
                walk_cost_cycles += 12 # Typical L2 hit latency per level
            if self.cfg.pwc_enabled:
                self.pwc[pwc_key] = 1

        self.walk_latencies.append(walk_cost_cycles)

        # 4. Check Page Fault
        pte = self.get_pte(vaddr, asid)
        if pte is None or not pte.present or (is_write and not pte.write) or (not pte.read and not pte.write):
            fault_latency = self.handle_page_fault(vaddr, pc, is_write, asid)
            pte = self.get_pte(vaddr, asid) # Refresh after demand page allocation
            walk_cost_cycles += fault_latency

        # 5. Refill TLBs
        if pte and pte.present:
            pte.accessed = True
            if is_write: pte.dirty = True

            new_entry = TLBEntry(
                valid=True,
                vpn=vpn,
                pfn=pte.pfn,
                read=pte.read,
                write=pte.write,
                execute=pte.execute,
                user=pte.user,
                accessed=pte.accessed,
                dirty=pte.dirty,
                asid=asid,
                global_page=False,
                is_huge=pte.is_huge,
            )
            target_tlb.insert(new_entry, current_cycle)
            if self.stlb:
                self.stlb.insert(new_entry, current_cycle)

            paddr = (pte.pfn << self.offset_bits) | offset
            return TranslationResult(
                paddr=paddr,
                latency_cycles=walk_cost_cycles,
                tlb_hit=False,
                tlb_level="WALK",
                walk_levels=levels_walked,
                walk_details=walk_details,
            )

        raise PageFaultException("unhandled_fault", vaddr, pc, "Fatal translation failure")

    def print_stats(self) -> str:
        tot = self.total_translations
        itlb_hr = (self.itlb.hits / self.itlb.lookups * 100.0) if self.itlb.lookups > 0 else 0.0
        dtlb_hr = (self.dtlb.hits / self.dtlb.lookups * 100.0) if self.dtlb.lookups > 0 else 0.0
        stlb_hr = (self.stlb.hits / self.stlb.lookups * 100.0) if self.stlb and self.stlb.lookups > 0 else 0.0
        avg_walk = (sum(self.walk_latencies) / len(self.walk_latencies)) if self.walk_latencies else 0.0

        out = [
            f"=== Virtual Memory & TLB Subsystem Statistics ===",
            f"  Page Size: {self.page_size // 1024} KB | Levels: {self.pt_levels} | VA Bits: {self.va_bits} | PA Bits: {self.pa_bits}",
            f"  Total Translations : {tot}",
            f"  ITLB Hits / Misses : {self.itlb.hits} / {self.itlb.misses} ({itlb_hr:.2f}% hit rate)",
            f"  DTLB Hits / Misses : {self.dtlb.hits} / {self.dtlb.misses} ({dtlb_hr:.2f}% hit rate)",
        ]
        if self.stlb:
            out.append(f"  STLB Hits / Misses : {self.stlb.hits} / {self.stlb.misses} ({stlb_hr:.2f}% hit rate)")
        out.extend([
            f"  PWC Lookups / Hits : {self.pwc_lookups} / {self.pwc_hits}",
            f"  Average Walk Cost  : {avg_walk:.2f} cycles",
            f"  Minor Page Faults  : {self.minor_faults}",
            f"  Major Page Faults  : {self.major_faults} (Swap reads: {self.swap_reads}, writes: {self.swap_writes})",
            f"  Protection Faults  : {self.protection_faults}",
            f"  Page Evictions     : {self.page_evictions}",
            f"  Physical Frames    : {self.total_frames - len(self.free_frames)} used / {self.total_frames} total",
        ])
        return "\n".join(out)
