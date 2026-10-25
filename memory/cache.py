"""
memory/cache.py: Generic cycle-accurate set-associative cache with MSHRs,
3C miss classification (Compulsory, Capacity, Conflict), VIPT aliasing detection,
multiple replacement/insertion policies, and line-straddling support.
"""

from __future__ import annotations
import math
import random
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Set, Any
from memory.config import CacheConfig


@dataclass
class CacheLine:
    valid: bool = False
    dirty: bool = False
    tag: int = 0
    data: bytearray = field(default_factory=bytearray)
    last_access_cycle: int = 0
    fifo_insert_cycle: int = 0
    rrpv: int = 3                          # For SRRIP (0=near, 3=distant for 2-bit)
    asid: int = 0                          # For VIVT indexing


@dataclass
class MSHREntry:
    line_addr: int                         # Block-aligned physical/virtual address
    allocated_cycle: int
    ready_cycle: int
    demand_targets: List[Tuple[int, str]] = field(default_factory=list) # (cpu_cycle, req_type)
    is_prefetch: bool = False
    critical_word_ready_cycle: int = 0


@dataclass
class CacheAccessResult:
    hit: bool
    latency_cycles: int
    miss_category: Optional[str] = None    # "compulsory", "capacity", "conflict"
    mshr_hit: bool = False                 # Merged into active MSHR
    evicted_line: Optional[CacheLine] = None
    evicted_addr: Optional[int] = None
    step_trace: List[str] = field(default_factory=list)
    critical_word_latency: int = 0
    straddled: bool = False


class CacheSet:
    def __init__(self, ways: int, line_size: int):
        self.ways = ways
        self.line_size = line_size
        self.lines: List[CacheLine] = [
            CacheLine(valid=False, dirty=False, tag=0, data=bytearray(line_size))
            for _ in range(ways)
        ]
        # Tree-PLRU bits: (ways - 1) tree nodes for power-of-2 ways
        self.plru_tree: List[int] = [0] * max(1, ways)

    def find_way_by_tag(self, tag: int, asid: int = 0, is_vivt: bool = False) -> int:
        for idx, line in enumerate(self.lines):
            if line.valid and line.tag == tag:
                if not is_vivt or line.asid == asid:
                    return idx
        return -1

    def find_invalid_way(self) -> int:
        for idx, line in enumerate(self.lines):
            if not line.valid:
                return idx
        return -1


class Cache:
    """
    Highly parameterized Set-Associative Cache.
    Supports:
      - Line sizes: 32, 64, 128 bytes
      - Associativity: Direct-mapped (1-way), N-way, Fully-associative, non-power-of-2
      - Replacement: LRU, FIFO, Random, Tree-PLRU, SRRIP
      - Insertion: MRU, LRU, BIP (Bimodal Insertion Policy)
      - Write policies: Write-Back + Write-Allocate (wb_wa), Write-Through + No-Write-Allocate (wt_nwa)
      - Indexing: PIPT, VIPT (with synonym detection), VIVT
      - 3C Miss Classification via shadow fully-associative cache
      - MSHRs with hit-under-miss, miss-under-miss, merging, and MLP tracking
      - Step-by-step cycle-accurate access tracing
    """

    def __init__(self, config: CacheConfig, page_size: int = 4096, seed: int = 42):
        self.cfg = config
        self.page_size = page_size
        self.rng = random.Random(seed)

        self.size_bytes = config.size_bytes
        self.line_size = config.line_size_bytes
        self.associativity = config.associativity
        self.hit_latency = config.hit_latency_cycles
        self.write_policy = config.write_policy.lower()
        self.replacement_policy = config.replacement_policy.lower()
        self.insertion_policy = config.insertion_policy.lower()
        self.indexing_mode = config.indexing_mode.lower()

        # Geometry calculations
        self.total_lines = self.size_bytes // self.line_size
        self.num_sets = max(1, self.total_lines // self.associativity)
        self.is_power_of_two_sets = (self.num_sets & (self.num_sets - 1)) == 0
        self.offset_bits = int(math.log2(self.line_size))
        self.index_bits = int(math.log2(self.num_sets)) if self.is_power_of_two_sets else 0

        # VIPT Aliasing / Synonym Check:
        # If (CacheSize / Associativity) > PageSize, then index bits overlap with VPN bits
        self.vipt_aliasing_detected = False
        self.bytes_per_way = self.size_bytes // self.associativity
        if self.indexing_mode == "vipt" and self.bytes_per_way > self.page_size:
            self.vipt_aliasing_detected = True

        # Cache Sets
        self.sets: List[CacheSet] = [
            CacheSet(self.associativity, self.line_size) for _ in range(self.num_sets)
        ]

        # MSHR Structure
        self.mshr_entries_max = config.mshr_entries
        self.mshrs: Dict[int, MSHREntry] = {}  # Key: line_addr -> MSHREntry

        # Victim Cache (optional)
        self.victim_cache_max = config.victim_cache_entries
        self.victim_cache: List[Tuple[int, CacheLine]] = [] # [(line_addr, line)]

        # 3C Miss Classification: Shadow Fully-Associative Cache & Global History
        self.global_seen_lines: Set[int] = set()
        self.shadow_fa_lines: List[int] = [] # LRU list of line addresses for capacity tracking
        self.shadow_fa_capacity = self.total_lines

        # Statistics & Performance Counters
        self.total_accesses = 0
        self.read_accesses = 0
        self.write_accesses = 0
        self.hits = 0
        self.misses = 0
        self.mshr_hits = 0
        self.evictions = 0
        self.writebacks = 0
        self.compulsory_misses = 0
        self.capacity_misses = 0
        self.conflict_misses = 0
        self.line_straddle_count = 0
        self.victim_cache_hits = 0

        # Memory Level Parallelism (MLP) tracking
        self.mlp_samples: List[int] = []

    def reset_stats(self) -> None:
        self.total_accesses = 0
        self.read_accesses = 0
        self.write_accesses = 0
        self.hits = 0
        self.misses = 0
        self.mshr_hits = 0
        self.evictions = 0
        self.writebacks = 0
        self.compulsory_misses = 0
        self.capacity_misses = 0
        self.conflict_misses = 0
        self.line_straddle_count = 0
        self.victim_cache_hits = 0
        self.mlp_samples.clear()

    def get_address_split(self, addr: int) -> Tuple[int, int, int]:
        """
        Splits address into (tag, set_index, byte_offset).
        Supports power-of-2 and arbitrary non-power-of-2 set counts.
        """
        offset_mask = (1 << self.offset_bits) - 1
        byte_offset = addr & offset_mask
        block_addr = addr >> self.offset_bits

        if self.is_power_of_two_sets:
            index_mask = (1 << self.index_bits) - 1
            set_index = block_addr & index_mask
            tag = block_addr >> self.index_bits
        else:
            set_index = block_addr % self.num_sets
            tag = block_addr // self.num_sets

        return tag, set_index, byte_offset

    def is_line_straddled(self, addr: int, size: int) -> bool:
        """Checks if a multi-byte access crosses a 64-byte cache line boundary."""
        offset = addr & ((1 << self.offset_bits) - 1)
        return (offset + size) > self.line_size

    def _update_plru_on_access(self, cset: CacheSet, way: int) -> None:
        """Update pseudo-LRU binary tree bits on way access."""
        # For an N-way cache, tree depth = log2(N).
        # We navigate from root (0) down to leaf.
        # Bits point to the MORE recently used direction, or inverted.
        idx = 0
        ways = cset.ways
        left = 0
        right = ways - 1
        while left < right:
            mid = (left + right) // 2
            if way <= mid:
                cset.plru_tree[idx] = 1 # Point to right (LRU is right)
                idx = 2 * idx + 1
                right = mid
            else:
                cset.plru_tree[idx] = 0 # Point to left (LRU is left)
                idx = 2 * idx + 2
                left = mid + 1
            if idx >= len(cset.plru_tree):
                break

    def _get_plru_victim(self, cset: CacheSet) -> int:
        """Find victim using Tree-PLRU bits."""
        idx = 0
        left = 0
        right = cset.ways - 1
        while left < right:
            mid = (left + right) // 2
            bit = cset.plru_tree[idx] if idx < len(cset.plru_tree) else 0
            if bit == 0:
                # Left was not recently accessed -> choose left
                idx = 2 * idx + 1
                right = mid
            else:
                # Right was not recently accessed -> choose right
                idx = 2 * idx + 2
                left = mid + 1
        return min(left, cset.ways - 1)

    def _choose_victim_way(self, cset: CacheSet, current_cycle: int) -> int:
        """Selects a victim way according to the configured replacement policy."""
        # 1. Prefer invalid lines first
        inv = cset.find_invalid_way()
        if inv != -1:
            return inv

        # 2. Apply policy
        if self.replacement_policy == "lru":
            oldest_cycle = float("inf")
            victim = 0
            for i, line in enumerate(cset.lines):
                if line.last_access_cycle < oldest_cycle:
                    oldest_cycle = line.last_access_cycle
                    victim = i
            return victim

        elif self.replacement_policy == "fifo":
            oldest_cycle = float("inf")
            victim = 0
            for i, line in enumerate(cset.lines):
                if line.fifo_insert_cycle < oldest_cycle:
                    oldest_cycle = line.fifo_insert_cycle
                    victim = i
            return victim

        elif self.replacement_policy == "random":
            return self.rng.randint(0, self.associativity - 1)

        elif self.replacement_policy == "plru":
            return self._get_plru_victim(cset)

        elif self.replacement_policy == "srrip":
            # Static RRIP: Search for line with max RRPV (3 for 2-bit)
            max_rrpv = (1 << self.cfg.srrip_bits) - 1
            while True:
                for i, line in enumerate(cset.lines):
                    if line.rrpv >= max_rrpv:
                        return i
                # Age all lines (increment RRPV) until one hits max
                for line in cset.lines:
                    line.rrpv = min(max_rrpv, line.rrpv + 1)

        else:
            # Default fallback to LRU
            return 0

    def _classify_miss(self, block_addr: int) -> str:
        """
        Classifies miss according to the 3C model:
          - Compulsory: First time line was ever requested.
          - Capacity: Miss would still occur in a fully-associative cache of same capacity.
          - Conflict: Miss would hit in a fully-associative cache, but misses due to set indexing conflict.
        """
        if block_addr not in self.global_seen_lines:
            self.global_seen_lines.add(block_addr)
            # Update shadow FA
            if block_addr in self.shadow_fa_lines:
                self.shadow_fa_lines.remove(block_addr)
            self.shadow_fa_lines.append(block_addr)
            if len(self.shadow_fa_lines) > self.shadow_fa_capacity:
                self.shadow_fa_lines.pop(0)
            return "compulsory"

        # Check if line is present in shadow fully-associative cache
        if block_addr in self.shadow_fa_lines:
            # It hit in shadow FA cache, but missed in this set! => Conflict miss
            self.shadow_fa_lines.remove(block_addr)
            self.shadow_fa_lines.append(block_addr)
            return "conflict"
        else:
            # Even a fully-associative cache of this capacity evicts it => Capacity miss
            self.shadow_fa_lines.append(block_addr)
            if len(self.shadow_fa_lines) > self.shadow_fa_capacity:
                self.shadow_fa_lines.pop(0)
            return "capacity"

    def access(
        self,
        addr: int,
        is_write: bool = False,
        size_bytes: int = 4,
        data: Optional[bytes] = None,
        current_cycle: int = 0,
        asid: int = 0,
        is_virtual: bool = False,
    ) -> CacheAccessResult:
        """
        Cycle-accurate access into the cache.
        Returns a CacheAccessResult with exact latency, hit/miss, and trace.
        """
        self.total_accesses += 1
        if is_write:
            self.write_accesses += 1
        else:
            self.read_accesses += 1

        trace: List[str] = []
        tag, set_idx, offset = self.get_address_split(addr)
        block_addr = addr >> self.offset_bits
        line_aligned_addr = block_addr << self.offset_bits

        trace.append(f"ADDR 0x{addr:08X} -> block=0x{block_addr:06X} tag=0x{tag:08X} set={set_idx} offset={offset}")

        # Check for line-straddling
        straddled = self.is_line_straddled(addr, size_bytes)
        if straddled:
            self.line_straddle_count += 1
            trace.append(f"STRADDLE: Request ({size_bytes}B) crosses 64B cache line boundary at offset {offset}")

        cset = self.sets[set_idx]
        way = cset.find_way_by_tag(tag, asid=asid, is_vivt=(self.indexing_mode == "vivt"))

        # Track active MSHR count for MLP
        active_mshrs = len(self.mshrs)
        if active_mshrs > 0:
            self.mlp_samples.append(active_mshrs)

        # 1. Cache HIT
        if way != -1:
            self.hits += 1
            line = cset.lines[way]
            line.last_access_cycle = current_cycle

            # Replacement policy updates on hit
            if self.replacement_policy == "plru":
                self._update_plru_on_access(cset, way)
            elif self.replacement_policy == "srrip":
                line.rrpv = 0 # Promote to near-immediate

            if is_write:
                if self.write_policy == "wb_wa":
                    line.dirty = True
                    trace.append(f"HIT [SET {set_idx}, WAY {way}] WRITE (dirty set) latency={self.hit_latency}")
                else: # wt_nwa
                    self.writebacks += 1 # Write through to next level
                    trace.append(f"HIT [SET {set_idx}, WAY {way}] WRITE-THROUGH latency={self.hit_latency}")
            else:
                trace.append(f"HIT [SET {set_idx}, WAY {way}] READ latency={self.hit_latency}")

            return CacheAccessResult(
                hit=True,
                latency_cycles=self.hit_latency,
                step_trace=trace,
                critical_word_latency=self.hit_latency,
                straddled=straddled,
            )

        # 2. Check Victim Cache (if enabled)
        if self.victim_cache_max > 0:
            for v_idx, (v_addr, v_line) in enumerate(self.victim_cache):
                if v_addr == line_aligned_addr:
                    self.victim_cache_hits += 1
                    trace.append(f"VICTIM-CACHE HIT for line 0x{line_aligned_addr:08X} (rescued)")
                    # Swap victim line with set victim
                    victim_way = self._choose_victim_way(cset, current_cycle)
                    old_line = cset.lines[victim_way]
                    old_line_addr = (old_line.tag << (self.index_bits + self.offset_bits)) | (set_idx << self.offset_bits)

                    self.victim_cache.pop(v_idx)
                    if old_line.valid:
                        self.victim_cache.append((old_line_addr, old_line))

                    cset.lines[victim_way] = v_line
                    v_line.last_access_cycle = current_cycle
                    return CacheAccessResult(
                        hit=True,
                        latency_cycles=self.hit_latency + 1, # Small 1-cycle VC penalty
                        step_trace=trace,
                        critical_word_latency=self.hit_latency + 1,
                        straddled=straddled,
                    )

        # 3. Cache MISS
        self.misses += 1
        miss_cat = self._classify_miss(block_addr)
        if miss_cat == "compulsory":
            self.compulsory_misses += 1
        elif miss_cat == "capacity":
            self.capacity_misses += 1
        else:
            self.conflict_misses += 1

        trace.append(f"MISS [SET {set_idx}] ({miss_cat.upper()} miss)")

        # 4. Check MSHR for hit-under-miss / request merging
        mshr_hit = False
        if line_aligned_addr in self.mshrs:
            mshr = self.mshrs[line_aligned_addr]
            mshr.demand_targets.append((current_cycle, "write" if is_write else "read"))
            mshr_hit = True
            self.mshr_hits += 1
            trace.append(f"MSHR MERGE: secondary miss merged with active request 0x{line_aligned_addr:08X}")
            return CacheAccessResult(
                hit=False,
                latency_cycles=self.hit_latency,
                miss_category=miss_cat,
                mshr_hit=True,
                step_trace=trace,
                critical_word_latency=self.hit_latency,
                straddled=straddled,
            )

        # 5. Eviction Check on allocate
        evicted_line: Optional[CacheLine] = None
        evicted_addr: Optional[int] = None

        if not is_write or (is_write and self.write_policy == "wb_wa"):
            victim_way = self._choose_victim_way(cset, current_cycle)
            candidate = cset.lines[victim_way]

            if candidate.valid:
                self.evictions += 1
                evicted_line = CacheLine(
                    valid=candidate.valid,
                    dirty=candidate.dirty,
                    tag=candidate.tag,
                    data=bytearray(candidate.data),
                    asid=candidate.asid
                )
                if self.is_power_of_two_sets:
                    evicted_addr = (candidate.tag << (self.index_bits + self.offset_bits)) | (set_idx << self.offset_bits)
                else:
                    evicted_addr = (candidate.tag * self.num_sets + set_idx) << self.offset_bits

                if candidate.dirty:
                    self.writebacks += 1
                    trace.append(f"EVICT DIRTY [WAY {victim_way}]: Writeback to next level (addr=0x{evicted_addr:08X})")
                else:
                    trace.append(f"EVICT CLEAN [WAY {victim_way}]: Discard line (addr=0x{evicted_addr:08X})")

                # Put in victim cache if configured
                if self.victim_cache_max > 0:
                    if len(self.victim_cache) >= self.victim_cache_max:
                        self.victim_cache.pop(0)
                    self.victim_cache.append((evicted_addr, evicted_line))

        return CacheAccessResult(
            hit=False,
            latency_cycles=self.hit_latency,
            miss_category=miss_cat,
            mshr_hit=False,
            evicted_line=evicted_line,
            evicted_addr=evicted_addr,
            step_trace=trace,
            critical_word_latency=self.hit_latency,
            straddled=straddled,
        )

    def allocate_line(
        self,
        addr: int,
        data: Optional[bytes] = None,
        dirty: bool = False,
        current_cycle: int = 0,
        asid: int = 0
    ) -> int:
        """
        Fills a line into the cache after fetch from next hierarchy level.
        Applies insertion policy (MRU, LRU, BIP) and returns way index.
        """
        tag, set_idx, _ = self.get_address_split(addr)
        cset = self.sets[set_idx]

        # Select victim way
        way = self._choose_victim_way(cset, current_cycle)
        line = cset.lines[way]

        line.valid = True
        line.dirty = dirty
        line.tag = tag
        line.asid = asid
        line.fifo_insert_cycle = current_cycle

        # Insertion policy
        if self.insertion_policy == "lru":
            line.last_access_cycle = 0 # Make it look oldest
        elif self.insertion_policy == "bip":
            if self.rng.random() < self.cfg.bip_epsilon:
                line.last_access_cycle = current_cycle # MRU insert
            else:
                line.last_access_cycle = 0 # LRU insert
        else: # mru default
            line.last_access_cycle = current_cycle

        # SRRIP insertion RRPV
        if self.replacement_policy == "srrip":
            line.rrpv = 2 # Insert at distance 2 (long, not distant)

        if self.replacement_policy == "plru":
            self._update_plru_on_access(cset, way)

        if data:
            line.data[:len(data)] = data

        return way

    def invalidate_line(self, addr: int, asid: int = 0) -> bool:
        """Invalidate a single line (used for back-invalidation in inclusive hierarchies)."""
        tag, set_idx, _ = self.get_address_split(addr)
        cset = self.sets[set_idx]
        way = cset.find_way_by_tag(tag, asid=asid, is_vivt=(self.indexing_mode == "vivt"))
        if way != -1:
            cset.lines[way].valid = False
            cset.lines[way].dirty = False
            return True
        return False

    def flush(self) -> int:
        """Flush entire cache; returns number of dirty writebacks."""
        wb_count = 0
        for cset in self.sets:
            for line in cset.lines:
                if line.valid and line.dirty:
                    wb_count += 1
                line.valid = False
                line.dirty = False
        return wb_count

    def get_average_mlp(self) -> float:
        """Calculates average Memory-Level Parallelism (MLP)."""
        if not self.mlp_samples:
            return 1.0
        return sum(self.mlp_samples) / len(self.mlp_samples)

    def print_stats(self) -> str:
        """Formatted cache performance statistics."""
        hr = (self.hits / self.total_accesses * 100.0) if self.total_accesses > 0 else 0.0
        mr = (self.misses / self.total_accesses * 100.0) if self.total_accesses > 0 else 0.0
        comp_pct = (self.compulsory_misses / self.misses * 100.0) if self.misses > 0 else 0.0
        cap_pct = (self.capacity_misses / self.misses * 100.0) if self.misses > 0 else 0.0
        conf_pct = (self.conflict_misses / self.misses * 100.0) if self.misses > 0 else 0.0

        out = [
            f"=== Cache Statistics: {self.cfg.name} ===",
            f"  Capacity: {self.size_bytes // 1024} KB | Line: {self.line_size} B | Assoc: {self.associativity}-way",
            f"  Sets: {self.num_sets} | Indexing: {self.indexing_mode.upper()} | Write Policy: {self.write_policy.upper()}",
            f"  Total Accesses : {self.total_accesses} (Reads: {self.read_accesses}, Writes: {self.write_accesses})",
            f"  Hits           : {self.hits} ({hr:.2f}%)",
            f"  Misses         : {self.misses} ({mr:.2f}%)",
            f"    - Compulsory : {self.compulsory_misses} ({comp_pct:.2f}%)",
            f"    - Capacity   : {self.capacity_misses} ({cap_pct:.2f}%)",
            f"    - Conflict   : {self.conflict_misses} ({conf_pct:.2f}%)",
            f"  MSHR Merges    : {self.mshr_hits}",
            f"  Evictions      : {self.evictions} (Dirty Writebacks: {self.writebacks})",
            f"  Average MLP    : {self.get_average_mlp():.2f}",
        ]
        if self.victim_cache_max > 0:
            out.append(f"  Victim Cache Hits: {self.victim_cache_hits}")
        if self.vipt_aliasing_detected:
            out.append(f"  [WARNING] VIPT Synonym Aliasing: bytes_per_way ({self.bytes_per_way}) > page_size ({self.page_size})!")
        return "\n".join(out)
