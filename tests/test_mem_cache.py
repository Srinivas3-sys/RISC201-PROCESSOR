"""
tests/test_mem_cache.py: Comprehensive unit tests for Cache module.
Tests address splitting (lecture & modern), replacement policies,
insertion policies, write policies, 3C classification, MSHRs,
line-straddling, VIPT aliasing, and cross-checks against a reference cache.
"""

import pytest
from memory.config import CacheConfig
from memory.cache import Cache, CacheLine, MSHREntry


class ReferenceCache:
    """Tiny independent reference direct-mapped cache for cross-checking."""
    def __init__(self, size: int, line_size: int):
        self.num_lines = size // line_size
        self.line_size = line_size
        self.tags = [-1] * self.num_lines
        self.valid = [False] * self.num_lines
        self.hits = 0
        self.misses = 0

    def access(self, addr: int) -> bool:
        block = addr // self.line_size
        idx = block % self.num_lines
        tag = block // self.num_lines
        if self.valid[idx] and self.tags[idx] == tag:
            self.hits += 1
            return True
        else:
            self.misses += 1
            self.valid[idx] = True
            self.tags[idx] = tag
            return False


def test_lecture_address_split():
    # Lecture example: 8-bit address, 8-byte blocks, 64 B direct-mapped
    # Total lines = 64 / 8 = 8 lines => 3 offset bits, 3 index bits, 2 tag bits
    cfg = CacheConfig(size_bytes=64, line_size_bytes=8, associativity=1)
    cache = Cache(cfg)
    assert cache.offset_bits == 3
    assert cache.index_bits == 3
    assert cache.num_sets == 8

    # Address 0b10_011_101 = 0x9D (Tag=2, Set=3, Offset=5)
    tag, s_idx, off = cache.get_address_split(0x9D)
    assert tag == 2
    assert s_idx == 3
    assert off == 5


def test_modern_address_split():
    # Modern example: 48-bit address, 32 KB, 8-way, 64 B line
    # Total lines = 32768 / 64 = 512 lines. Num sets = 512 / 8 = 64 sets
    # Offset = 6 bits, Index = 6 bits, Tag = 48 - 12 = 36 bits
    cfg = CacheConfig(size_bytes=32 * 1024, line_size_bytes=64, associativity=8)
    cache = Cache(cfg)
    assert cache.offset_bits == 6
    assert cache.index_bits == 6
    assert cache.num_sets == 64

    # Address 0x00007FFF1040
    tag, s_idx, off = cache.get_address_split(0x7FFF1040)
    assert off == (0x7FFF1040 & 0x3F)
    assert s_idx == ((0x7FFF1040 >> 6) & 0x3F)
    assert tag == (0x7FFF1040 >> 12)


def test_direct_mapped_conflict_abab():
    # Direct-mapped cache (1-way), 2 sets, 64B lines
    # Addresses A = 0x00, B = 0x80 map to same set 0 (both have set_idx = 0)
    cfg = CacheConfig(size_bytes=128, line_size_bytes=64, associativity=1)
    cache = Cache(cfg)

    # Sequence: A, B, A, B
    res_a1 = cache.access(0x00, current_cycle=1)
    res_b1 = cache.access(0x80, current_cycle=2)
    res_a2 = cache.access(0x00, current_cycle=3)
    res_b2 = cache.access(0x80, current_cycle=4)

    assert not res_a1.hit
    assert not res_b1.hit
    assert not res_a2.hit  # Thrashing conflict miss!
    assert not res_b2.hit
    assert cache.hits == 0
    assert cache.misses == 4
    assert res_a2.miss_category == "conflict"

    # Now same pattern on 2-way associative cache: A and B both fit!
    cfg2 = CacheConfig(size_bytes=128, line_size_bytes=64, associativity=2)
    cache2 = Cache(cfg2)
    cache2.access(0x00, current_cycle=1)
    cache2.allocate_line(0x00, current_cycle=1)
    cache2.access(0x80, current_cycle=2)
    cache2.allocate_line(0x80, current_cycle=2)

    res_a2_fixed = cache2.access(0x00, current_cycle=3)
    res_b2_fixed = cache2.access(0x80, current_cycle=4)
    assert res_a2_fixed.hit
    assert res_b2_fixed.hit
    assert cache2.hits == 2


def test_replacement_policies():
    # 2-way associative cache with 1 set
    for policy in ["lru", "fifo", "random", "plru", "srrip"]:
        cfg = CacheConfig(size_bytes=128, line_size_bytes=64, associativity=2, replacement_policy=policy)
        c = Cache(cfg, seed=123)
        # Fill way 0 and way 1
        c.access(0x00, current_cycle=1)
        c.allocate_line(0x00, current_cycle=1)
        c.access(0x40, current_cycle=2)
        c.allocate_line(0x40, current_cycle=2)

        # Access 0x00 again to update LRU/PLRU/SRRIP recency
        c.access(0x00, current_cycle=3)

        # Access new line 0x80 -> triggers eviction
        res = c.access(0x80, current_cycle=4)
        assert not res.hit


def test_write_back_dirty_eviction():
    cfg = CacheConfig(size_bytes=128, line_size_bytes=64, associativity=1, write_policy="wb_wa")
    cache = Cache(cfg)

    # 1. Write miss to 0x00 -> allocate and mark dirty
    cache.access(0x00, is_write=True, current_cycle=1)
    cache.allocate_line(0x00, dirty=True, current_cycle=1)

    # 2. Access 0x80 (same set 0) -> evicts dirty line 0x00
    res = cache.access(0x80, is_write=False, current_cycle=2)
    assert res.evicted_line is not None
    assert res.evicted_line.dirty
    assert res.evicted_addr == 0x00
    assert cache.writebacks == 1


def test_line_straddling():
    cfg = CacheConfig(size_bytes=1024, line_size_bytes=64, associativity=2)
    cache = Cache(cfg)

    # 4-byte read at offset 60 does not straddle (bytes 60..63 fit in 64B line)
    assert not cache.is_line_straddled(60, 4)

    # 4-byte read at offset 62 straddles (bytes 62..65 cross into next line)
    assert cache.is_line_straddled(62, 4)

    res = cache.access(62, size_bytes=4)
    assert res.straddled
    assert cache.line_straddle_count == 1


def test_mshr_merging():
    cfg = CacheConfig(size_bytes=1024, line_size_bytes=64, associativity=2, mshr_entries=4)
    cache = Cache(cfg)

    # First access to 0x100 misses -> allocate MSHR
    res1 = cache.access(0x100, current_cycle=10)
    assert not res1.hit
    assert not res1.mshr_hit
    cache.mshrs[0x100] = MSHREntry(line_addr=0x100, allocated_cycle=10, ready_cycle=50)

    # Second access to 0x110 (same line) -> merges into active MSHR
    res2 = cache.access(0x110, current_cycle=12)
    assert not res2.hit
    assert res2.mshr_hit
    assert cache.mshr_hits == 1


def test_vipt_aliasing_detection():
    # If bytes_per_way (CacheSize / Assoc) > PageSize, VIPT synonym aliasing occurs!
    # Case 1: Safe VIPT: 32 KB 8-way => 4 KB per way == 4 KB page size (NO aliasing)
    cfg_safe = CacheConfig(size_bytes=32 * 1024, associativity=8, indexing_mode="vipt")
    c_safe = Cache(cfg_safe, page_size=4096)
    assert not c_safe.vipt_aliasing_detected

    # Case 2: Unsafe VIPT: 64 KB 4-way => 16 KB per way > 4 KB page size (ALIASING!)
    cfg_unsafe = CacheConfig(size_bytes=64 * 1024, associativity=4, indexing_mode="vipt")
    c_unsafe = Cache(cfg_unsafe, page_size=4096)
    assert c_unsafe.vipt_aliasing_detected


def test_cross_check_reference_cache():
    # Cross check direct-mapped cache against reference implementation
    cfg = CacheConfig(size_bytes=512, line_size_bytes=64, associativity=1)
    cache = Cache(cfg)
    ref = ReferenceCache(size=512, line_size=64)

    trace = [0, 64, 128, 0, 64, 512, 0, 64, 128, 192, 256, 320, 384, 448, 512]
    for cycle, addr in enumerate(trace):
        r_hit = ref.access(addr)
        c_res = cache.access(addr, current_cycle=cycle)
        if not c_res.hit:
            cache.allocate_line(addr, current_cycle=cycle)
        assert c_res.hit == r_hit

    assert cache.hits == ref.hits
    assert cache.misses == ref.misses
