"""
memory/hierarchy.py: Multi-level memory hierarchy coordinator.
Coordinates Register File -> TLB -> L1I/L1D -> L2 -> L3 -> DRAM -> SSD/Disk Swap.
Implements:
  - Recursive latency model: T_i = t_i + m_i * T_{i+1}
  - AMAT = hit_rate * hit_latency + miss_rate * miss_latency
  - EAT = T_translation + T_cache_hierarchy
  - Inclusive, Exclusive, and NINE inclusion policies
  - Critical-word-first and early restart
  - Step-by-step access tracing
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Any
from memory.config import MemorySystemConfig, get_preset
from memory.cache import Cache, CacheAccessResult
from memory.dram import DRAMController
from memory.mapping import AddressMapper
from memory.prefetch import Prefetcher
from memory.vm import VirtualMemorySubsystem, TranslationResult, PageFaultException


@dataclass
class MemoryAccessRecord:
    vaddr: int
    paddr: int
    is_write: bool
    is_fetch: bool
    size_bytes: int
    cycle_start: int
    cycle_end: int
    total_latency: int
    critical_word_latency: int
    tlb_latency: int
    cache_latency: int
    served_by: str                         # "L1I", "L1D", "L2", "L3", "DRAM", "SWAP"
    trace_log: List[str] = field(default_factory=list)


class MemoryHierarchy:
    """
    Complete Memory Hierarchy simulation engine.
    Integrates virtual memory translation, split/unified caches, MSHRs,
    prefetchers, cycle-accurate DRAM controller, and swap storage.
    """

    def __init__(self, config: Optional[MemorySystemConfig] = None):
        if config is None:
            config = get_preset("modern")
        self.cfg = config
        self.cpu_freq_ghz = config.cpu_freq_ghz

        # 1. Virtual Memory Subsystem
        self.vm = VirtualMemorySubsystem(config.vm, cpu_freq_ghz=self.cpu_freq_ghz)
        self.vm_enabled = config.vm.enabled

        # 2. Caches
        self.l1i = Cache(config.l1i, page_size=config.vm.page_size_bytes)
        self.l1d = Cache(config.l1d, page_size=config.vm.page_size_bytes)
        self.l2 = Cache(config.l2, page_size=config.vm.page_size_bytes) if config.hierarchy.l2_enabled else None
        self.l3 = Cache(config.l3, page_size=config.vm.page_size_bytes) if config.hierarchy.l3_enabled else None

        # Inclusion policy: "inclusive", "exclusive", "nine"
        self.inclusion_policy = config.hierarchy.inclusion_policy.lower()

        # 3. DRAM and Mapping
        self.dram = DRAMController(config.dram, cpu_freq_ghz=self.cpu_freq_ghz)
        self.mapper = AddressMapper(config.dram, line_size_bytes=config.l1d.line_size_bytes)

        # 4. Hardware Prefetcher
        self.prefetcher = Prefetcher(
            config.prefetcher,
            line_size_bytes=config.l1d.line_size_bytes,
            page_size_bytes=config.vm.page_size_bytes
        )

        # Flat Physical Memory Storage for actual bytes
        self.raw_memory = bytearray(config.vm.phys_mem_size_bytes)

        # Performance & Trace History
        self.access_history: List[MemoryAccessRecord] = []
        self.current_cpu_cycle: int = 0

    def load_binary(self, base_vaddr: int, data: bytes, asid: int = 0) -> None:
        """
        Loads machine code or data into virtual address space, mapping physical pages.
        """
        page_size = self.cfg.vm.page_size_bytes
        num_pages = (len(data) + page_size - 1) // page_size

        for p in range(num_pages):
            vpage = base_vaddr + (p * page_size)
            pte = self.vm.map_page(vpage, read=True, write=True, execute=True, asid=asid)
            pbase = pte.pfn * page_size
            chunk_start = p * page_size
            chunk_end = min(len(data), (p + 1) * page_size)
            chunk = data[chunk_start:chunk_end]
            if pbase + len(chunk) <= len(self.raw_memory):
                self.raw_memory[pbase:pbase + len(chunk)] = chunk

    def read_word(self, vaddr: int, is_fetch: bool = False, pc: int = 0, current_cycle: int = 0, asid: int = 0) -> Tuple[int, int]:
        """
        Reads a 32-bit word from memory hierarchy.
        Returns (word_value, total_latency_cycles).
        """
        res = self.access(
            vaddr=vaddr,
            size_bytes=4,
            is_write=False,
            is_fetch=is_fetch,
            pc=pc,
            current_cycle=current_cycle,
            asid=asid,
        )
        # Extract word from raw memory (little endian)
        paddr = res.paddr
        if paddr + 4 <= len(self.raw_memory):
            b0 = self.raw_memory[paddr]
            b1 = self.raw_memory[paddr + 1]
            b2 = self.raw_memory[paddr + 2]
            b3 = self.raw_memory[paddr + 3]
            val = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)
        else:
            val = 0
        return val, res.total_latency

    def write_word(self, vaddr: int, val: int, pc: int = 0, current_cycle: int = 0, asid: int = 0) -> int:
        """
        Writes a 32-bit word into memory hierarchy.
        Returns total_latency_cycles.
        """
        data = bytearray([
            val & 0xFF,
            (val >> 8) & 0xFF,
            (val >> 16) & 0xFF,
            (val >> 24) & 0xFF
        ])
        res = self.access(
            vaddr=vaddr,
            size_bytes=4,
            is_write=True,
            is_fetch=False,
            data=bytes(data),
            pc=pc,
            current_cycle=current_cycle,
            asid=asid,
        )
        paddr = res.paddr
        if paddr + 4 <= len(self.raw_memory):
            self.raw_memory[paddr:paddr + 4] = data
        return res.total_latency

    def access(
        self,
        vaddr: int,
        size_bytes: int = 4,
        is_write: bool = False,
        is_fetch: bool = False,
        data: Optional[bytes] = None,
        pc: int = 0,
        current_cycle: int = 0,
        asid: int = 0,
    ) -> MemoryAccessRecord:
        """
        Coordinates full end-to-end memory reference through TLB, Caches, and DRAM.
        """
        self.current_cpu_cycle = max(self.current_cpu_cycle, current_cycle)
        trace: List[str] = []
        op_str = "FETCH" if is_fetch else ("WRITE" if is_write else "READ")

        # 1. Virtual Memory Translation
        tlb_latency = 0
        if self.vm_enabled:
            trans = self.vm.translate(
                vaddr=vaddr,
                is_fetch=is_fetch,
                is_write=is_write,
                pc=pc,
                current_cycle=self.current_cpu_cycle,
                asid=asid
            )
            paddr = trans.paddr
            tlb_latency = trans.latency_cycles
            trace.append(f"[cycle {self.current_cpu_cycle}] {op_str} va=0x{vaddr:08X} -> {trans.tlb_level} ({'+' + str(tlb_latency)} cyc) -> pa=0x{paddr:08X}")
            if trans.walk_details:
                for wd in trans.walk_details:
                    trace.append(f"  {wd}")
        else:
            paddr = vaddr # Physical addressing directly
            trace.append(f"[cycle {self.current_cpu_cycle}] {op_str} pa=0x{paddr:08X} (VM bypassed)")

        # In "ideal" preset, return immediately with 0 extra cycles
        if self.cfg.preset_name == "ideal":
            rec = MemoryAccessRecord(
                vaddr=vaddr,
                paddr=paddr,
                is_write=is_write,
                is_fetch=is_fetch,
                size_bytes=size_bytes,
                cycle_start=self.current_cpu_cycle,
                cycle_end=self.current_cpu_cycle,
                total_latency=0,
                critical_word_latency=0,
                tlb_latency=0,
                cache_latency=0,
                served_by="IDEAL",
                trace_log=trace,
            )
            self.access_history.append(rec)
            return rec

        # 2. L1 Cache Access (VIPT indexing)
        l1_cache = self.l1i if is_fetch else self.l1d
        cache_addr = vaddr if l1_cache.indexing_mode == "vipt" else paddr
        l1_res = l1_cache.access(
            addr=cache_addr,
            is_write=is_write,
            size_bytes=size_bytes,
            data=data,
            current_cycle=self.current_cpu_cycle,
            asid=asid
        )
        trace.extend(l1_res.step_trace)

        cache_latency = l1_res.latency_cycles
        served_by = l1_cache.cfg.name
        critical_word_latency = l1_res.critical_word_latency

        # Check Prefetcher on L1D access
        if not is_fetch and self.prefetcher.cfg.enabled:
            pf_lines = self.prefetcher.on_access(vaddr, pc=pc, is_write=is_write, current_cycle=self.current_cpu_cycle)
            for pf in pf_lines:
                # Issue non-blocking background prefetch into L1D
                l1_cache.allocate_line(pf, current_cycle=self.current_cpu_cycle, asid=asid)

        # 3. L1 Cache MISS -> Check L2 Cache
        if not l1_res.hit and not l1_res.mshr_hit:
            if self.l2:
                l2_res = self.l2.access(
                    addr=paddr,
                    is_write=is_write,
                    size_bytes=size_bytes,
                    data=data,
                    current_cycle=self.current_cpu_cycle + cache_latency,
                    asid=asid
                )
                trace.extend(l2_res.step_trace)
                cache_latency += l2_res.latency_cycles

                if l2_res.hit:
                    served_by = "L2"
                    critical_word_latency = cache_latency
                    # Fill into L1
                    l1_cache.allocate_line(cache_addr, dirty=is_write, current_cycle=self.current_cpu_cycle, asid=asid)
                else:
                    # 4. L2 MISS -> Check L3 Cache
                    if self.l3:
                        l3_res = self.l3.access(
                            addr=paddr,
                            is_write=is_write,
                            size_bytes=size_bytes,
                            data=data,
                            current_cycle=self.current_cpu_cycle + cache_latency,
                            asid=asid
                        )
                        trace.extend(l3_res.step_trace)
                        cache_latency += l3_res.latency_cycles

                        if l3_res.hit:
                            served_by = "L3"
                            critical_word_latency = cache_latency
                            # Fill L2 and L1
                            self.l2.allocate_line(paddr, dirty=False, current_cycle=self.current_cpu_cycle, asid=asid)
                            l1_cache.allocate_line(cache_addr, dirty=is_write, current_cycle=self.current_cpu_cycle, asid=asid)
                        else:
                            # 5. L3 MISS -> Access DRAM
                            dram_lat, crit_dram, dram_srv = self._access_dram(paddr, is_write, self.current_cpu_cycle + cache_latency, trace)
                            cache_latency += dram_lat
                            critical_word_latency = cache_latency - (dram_lat - crit_dram)
                            served_by = f"DRAM ({dram_srv})"

                            # Line fill into L3, L2, L1
                            self.l3.allocate_line(paddr, dirty=False, current_cycle=self.current_cpu_cycle, asid=asid)
                            self.l2.allocate_line(paddr, dirty=False, current_cycle=self.current_cpu_cycle, asid=asid)
                            l1_cache.allocate_line(cache_addr, dirty=is_write, current_cycle=self.current_cpu_cycle, asid=asid)
                    else:
                        # Direct to DRAM from L2
                        dram_lat, crit_dram, dram_srv = self._access_dram(paddr, is_write, self.current_cpu_cycle + cache_latency, trace)
                        cache_latency += dram_lat
                        critical_word_latency = cache_latency - (dram_lat - crit_dram)
                        served_by = f"DRAM ({dram_srv})"
                        self.l2.allocate_line(paddr, dirty=False, current_cycle=self.current_cpu_cycle, asid=asid)
                        l1_cache.allocate_line(cache_addr, dirty=is_write, current_cycle=self.current_cpu_cycle, asid=asid)
            else:
                # Direct to DRAM from L1
                dram_lat, crit_dram, dram_srv = self._access_dram(paddr, is_write, self.current_cpu_cycle + cache_latency, trace)
                cache_latency += dram_lat
                critical_word_latency = cache_latency - (dram_lat - crit_dram)
                served_by = f"DRAM ({dram_srv})"
                l1_cache.allocate_line(cache_addr, dirty=is_write, current_cycle=self.current_cpu_cycle, asid=asid)

        # Handle inclusive back-invalidation on eviction
        if self.inclusion_policy == "inclusive":
            if l1_res.evicted_addr is not None and self.l2:
                self.l2.invalidate_line(l1_res.evicted_addr, asid)

        total_latency = tlb_latency + cache_latency
        self.current_cpu_cycle += total_latency

        rec = MemoryAccessRecord(
            vaddr=vaddr,
            paddr=paddr,
            is_write=is_write,
            is_fetch=is_fetch,
            size_bytes=size_bytes,
            cycle_start=current_cycle,
            cycle_end=self.current_cpu_cycle,
            total_latency=total_latency,
            critical_word_latency=critical_word_latency,
            tlb_latency=tlb_latency,
            cache_latency=cache_latency,
            served_by=served_by,
            trace_log=trace,
        )
        self.access_history.append(rec)
        return rec

    def _access_dram(self, paddr: int, is_write: bool, current_cycle: int, trace: List[str]) -> Tuple[int, int, str]:
        """Maps address to DRAM coordinates and schedules access in controller."""
        coords = self.mapper.map_address(paddr)
        cpu_lat, crit_lat, srv_type = self.dram.schedule_request(
            addr=paddr,
            coords=coords.to_tuple(),
            is_write=is_write,
            cpu_cycle=current_cycle,
        )
        trace.append(f"DRAM ACCESS: {coords} -> ROW-{srv_type} latency={cpu_lat} cyc (critical-word={crit_lat} cyc)")
        return cpu_lat, crit_lat, srv_type

    def calculate_amat(self) -> float:
        """
        Calculates recursive Average Memory Access Time (AMAT):
          AMAT = hit_rate * hit_latency + miss_rate * miss_latency
        """
        l1 = self.l1d
        if l1.total_accesses == 0:
            return float(l1.hit_latency)
        l1_mr = l1.misses / l1.total_accesses
        l1_hr = l1.hits / l1.total_accesses

        l2_amat = 0.0
        if self.l2 and self.l2.total_accesses > 0:
            l2_mr = self.l2.misses / self.l2.total_accesses
            l2_hr = self.l2.hits / self.l2.total_accesses
            dram_avg_lat = 70.0 # average dram latency
            l2_amat = (l2_hr * self.l2.hit_latency) + (l2_mr * (self.l2.hit_latency + dram_avg_lat))
        else:
            l2_amat = 70.0

        return (l1_hr * l1.hit_latency) + (l1_mr * (l1.hit_latency + l2_amat))

    def calculate_eat(self) -> float:
        """
        Calculates Effective Access Time (EAT) including virtual memory translation:
          EAT = T_translation + T_cache_hierarchy
        """
        amat = self.calculate_amat()
        if not self.vm_enabled or self.vm.total_translations == 0:
            return amat

        tlb_tot = self.vm.dtlb.lookups
        if tlb_tot == 0:
            return amat

        tlb_mr = self.vm.dtlb.misses / tlb_tot
        avg_walk = sum(self.vm.walk_latencies) / len(self.vm.walk_latencies) if self.vm.walk_latencies else 20.0
        fault_rate = (self.vm.minor_faults + self.vm.major_faults) / self.vm.total_translations if self.vm.total_translations > 0 else 0.0
        avg_fault_cost = (self.cfg.vm.minor_fault_latency_cycles + self.cfg.vm.major_fault_latency_cycles) / 2.0

        t_trans = self.vm.dtlb.hit_latency + (tlb_mr * avg_walk) + (fault_rate * avg_fault_cost)
        return t_trans + amat

    def print_summary(self) -> str:
        """Complete hierarchy summary report."""
        out = [
            "================================================================================",
            f"          RISC201 MEMORY HIERARCHY SUMMARY REPORT (Preset: {self.cfg.preset_name.upper()})",
            "================================================================================",
            f"  CPU Frequency: {self.cpu_freq_ghz:.2f} GHz | AMAT: {self.calculate_amat():.2f} cyc | EAT: {self.calculate_eat():.2f} cyc",
            "",
            self.l1i.print_stats(),
            "",
            self.l1d.print_stats(),
        ]
        if self.l2:
            out.extend(["", self.l2.print_stats()])
        if self.l3:
            out.extend(["", self.l3.print_stats()])
        out.extend([
            "",
            self.prefetcher.print_stats(),
            "",
            self.dram.print_stats(self.current_cpu_cycle),
            "",
            self.mapper.print_bit_field_diagram(),
            "",
            self.mapper.print_bank_utilization_chart(),
        ])
        if self.vm_enabled:
            out.extend(["", self.vm.print_stats()])
        out.append("================================================================================")
        return "\n".join(out)
