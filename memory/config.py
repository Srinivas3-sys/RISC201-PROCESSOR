"""
memory/config.py: Unified configuration schema for the RISC201 memory hierarchy.
Supports dataclass serialization to/from JSON and YAML with standard presets.
"""

from __future__ import annotations
import json
from dataclasses import dataclass, field, asdict
from typing import Optional, Dict, Any

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class CacheConfig:
    name: str = "L1D"
    size_bytes: int = 32 * 1024           # 32 KB default
    line_size_bytes: int = 64             # 64 B default
    associativity: int = 8                # 8-way default
    hit_latency_cycles: int = 4           # 4-cycle hit default
    replacement_policy: str = "lru"       # lru, fifo, random, plru, srrip
    insertion_policy: str = "mru"         # mru, lru, bip
    write_policy: str = "wb_wa"           # wb_wa (write-back + write-allocate) or wt_nwa (write-through)
    indexing_mode: str = "vipt"           # pipt, vipt, vivt
    mshr_entries: int = 8                 # Number of MSHRs
    write_buffer_entries: int = 16
    victim_cache_entries: int = 0         # 0 = disabled
    srrip_bits: int = 2                   # For SRRIP (2 bits = 0..3)
    bip_epsilon: float = 0.03125          # 1/32 probability to insert at MRU for BIP


@dataclass
class DRAMTimingConfig:
    # Clock and basic timings in DRAM clock cycles
    tCK_ns: float = 0.625                 # DDR4-3200 has 1600 MHz clock => 0.625 ns
    tRCD: int = 22                        # Row to Column Delay
    tRP: int = 22                         # Row Precharge
    tCL: int = 22                         # CAS Latency
    tRAS: int = 52                        # Row Active Time (tRC = tRAS + tRP = 74)
    tRC: int = 74
    tWR: int = 24                         # Write Recovery
    tWTR_S: int = 4                       # Write to Read (short, diff bank group)
    tWTR_L: int = 12                      # Write to Read (long, same bank group)
    tRTP: int = 12                        # Read to Precharge
    tFAW: int = 34                        # Four Activate Window
    tRRD_S: int = 6                       # Activate to Activate (diff bank group)
    tRRD_L: int = 8                       # Activate to Activate (same bank group)
    tCCD_S: int = 4                       # Column to Column (diff bank group)
    tCCD_L: int = 8                       # Column to Column (same bank group)
    burst_length: int = 8                 # BL8 default
    tBURST: int = 4                       # BL8 / 2 = 4 DRAM cycles
    tREFI: int = 12480                    # Refresh interval (~7.8 us / 0.625 ns)
    tRFC: int = 560                       # Refresh cycle time


@dataclass
class DRAMConfig:
    dram_type: str = "DDR4-3200"
    channels: int = 1
    ranks_per_channel: int = 1
    bank_groups_per_rank: int = 4
    banks_per_group: int = 4
    rows_per_bank: int = 65536
    cols_per_row: int = 1024
    bus_width_bytes: int = 8              # 64-bit channel
    subchannels_per_channel: int = 1      # 1 for DDR4, 2 for DDR5
    page_policy: str = "open"             # open or closed
    scheduler: str = "frfcfs"             # fcfs or frfcfs (row-hit first)
    address_mapping: str = "row_bg_bank_col_chan_byte"  # row_bg_bank_col_chan_byte, row_col_bank_chan, xor_hash
    queue_capacity: int = 32
    timings: DRAMTimingConfig = field(default_factory=DRAMTimingConfig)


@dataclass
class PrefetcherConfig:
    enabled: bool = True
    level: str = "L1D"                    # Target cache: L1D or L2
    prefetcher_type: str = "stream"       # none, next_line, stride, stream
    degree: int = 2                       # Number of lines to prefetch
    distance: int = 4                     # Lookahead distance
    stream_table_entries: int = 16
    stride_table_entries: int = 32
    cross_page_boundary: bool = False     # Guard against crossing 4KB page bounds


@dataclass
class TLBConfig:
    name: str = "TLB"
    entries: int = 64
    associativity: int = 4
    hit_latency_cycles: int = 1
    replacement_policy: str = "lru"


@dataclass
class VMConfig:
    enabled: bool = True
    va_bits: int = 48
    pa_bits: int = 32
    page_size_bytes: int = 4096           # 4 KB default
    pt_levels: int = 4                    # 1, 2, or 4
    huge_pages_enabled: bool = False
    huge_page_size_bytes: int = 2 * 1024 * 1024  # 2 MB
    
    itlb: TLBConfig = field(default_factory=lambda: TLBConfig("ITLB", 64, 4, 1))
    dtlb: TLBConfig = field(default_factory=lambda: TLBConfig("DTLB", 64, 4, 1))
    stlb: TLBConfig = field(default_factory=lambda: TLBConfig("STLB", 1024, 8, 7))
    stlb_enabled: bool = True
    
    pwc_enabled: bool = True              # Page-Walk Cache
    pwc_entries: int = 16
    asid_enabled: bool = True
    
    # Physical Frame Management and Demand Paging
    phys_mem_size_bytes: int = 1024 * 1024  # 1 MB default physical memory
    demand_paging: bool = True
    page_replacement: str = "lru"         # fifo, lru, clock, opt
    
    # Fault Latency Modeling (in CPU cycles)
    minor_fault_latency_cycles: int = 1500     # Page allocated / zeroed
    major_fault_latency_cycles: int = 200000   # Swap read from SSD/Disk (~0.1ms @ 2GHz)
    ssd_latency_ns: float = 100000.0          # ~0.1 ms
    disk_latency_ns: float = 3000000.0        # ~3.0 ms


@dataclass
class HierarchyConfig:
    inclusion_policy: str = "nine"        # inclusive, exclusive, nine
    l2_enabled: bool = True
    l3_enabled: bool = True
    split_l1: bool = True                 # True: L1I and L1D separated


@dataclass
class MemorySystemConfig:
    preset_name: str = "modern"
    cpu_freq_ghz: float = 3.2             # CPU clock 3.2 GHz
    
    # Cache levels
    l1i: CacheConfig = field(default_factory=lambda: CacheConfig("L1I", 32 * 1024, 64, 8, 4, "lru", "mru", "wb_wa", "vipt", 8))
    l1d: CacheConfig = field(default_factory=lambda: CacheConfig("L1D", 32 * 1024, 64, 8, 4, "lru", "mru", "wb_wa", "vipt", 8))
    l2: CacheConfig = field(default_factory=lambda: CacheConfig("L2", 512 * 1024, 64, 8, 12, "lru", "mru", "wb_wa", "pipt", 16))
    l3: CacheConfig = field(default_factory=lambda: CacheConfig("L3", 8 * 1024 * 1024, 64, 16, 38, "lru", "mru", "wb_wa", "pipt", 32))
    
    hierarchy: HierarchyConfig = field(default_factory=HierarchyConfig)
    dram: DRAMConfig = field(default_factory=DRAMConfig)
    prefetcher: PrefetcherConfig = field(default_factory=PrefetcherConfig)
    vm: VMConfig = field(default_factory=VMConfig)

    def cpu_ns_to_cycles(self, ns: float) -> int:
        """Convert nanoseconds to CPU cycles based on CPU frequency."""
        return max(1, int(round(ns * self.cpu_freq_ghz)))

    def dram_cycles_to_cpu_cycles(self, dram_cycles: int) -> int:
        """Convert DRAM cycles to CPU cycles."""
        ns = dram_cycles * self.dram.timings.tCK_ns
        return self.cpu_ns_to_cycles(ns)

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent)

    def to_yaml(self) -> str:
        if not HAS_YAML:
            return self.to_json()
        return yaml.dump(self.to_dict(), default_flow_style=False)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> MemorySystemConfig:
        cfg = cls()
        if "preset_name" in data:
            cfg.preset_name = data["preset_name"]
        if "cpu_freq_ghz" in data:
            cfg.cpu_freq_ghz = data["cpu_freq_ghz"]
        if "l1i" in data:
            cfg.l1i = CacheConfig(**data["l1i"])
        if "l1d" in data:
            cfg.l1d = CacheConfig(**data["l1d"])
        if "l2" in data:
            cfg.l2 = CacheConfig(**data["l2"])
        if "l3" in data:
            cfg.l3 = CacheConfig(**data["l3"])
        if "hierarchy" in data:
            cfg.hierarchy = HierarchyConfig(**data["hierarchy"])
        if "dram" in data:
            dram_data = dict(data["dram"])
            if "timings" in dram_data:
                dram_data["timings"] = DRAMTimingConfig(**dram_data["timings"])
            cfg.dram = DRAMConfig(**dram_data)
        if "prefetcher" in data:
            cfg.prefetcher = PrefetcherConfig(**data["prefetcher"])
        if "vm" in data:
            vm_data = dict(data["vm"])
            if "itlb" in vm_data:
                vm_data["itlb"] = TLBConfig(**vm_data["itlb"])
            if "dtlb" in vm_data:
                vm_data["dtlb"] = TLBConfig(**vm_data["dtlb"])
            if "stlb" in vm_data:
                vm_data["stlb"] = TLBConfig(**vm_data["stlb"])
            cfg.vm = VMConfig(**vm_data)
        return cfg

    @classmethod
    def from_json(cls, json_str: str) -> MemorySystemConfig:
        return cls.from_dict(json.loads(json_str))

    @classmethod
    def from_yaml(cls, yaml_str: str) -> MemorySystemConfig:
        if not HAS_YAML:
            return cls.from_json(yaml_str)
        return cls.from_dict(yaml.safe_load(yaml_str))


def get_preset(preset: str) -> MemorySystemConfig:
    """
    Factory for standard architecture presets:
      - 'ideal': 0 extra latency, 1-cycle access, no VM (baseline matching original CPU)
      - 'simple': L1 (32KB, 4-cycle) + DRAM (flat 50ns), VM off
      - 'modern': L1I/L1D + L2 + L3 + DDR4-3200 + stream prefetcher + VM/TLBs
      - 'modern-ddr5': L1I/L1D + L2 + L3 + DDR5-4800 (subchannels, BL16) + stream prefetcher + VM/TLBs
      - 'lecture': Latencies matching Prof. Sarangi / lecture slides
    """
    preset = preset.lower().strip()
    
    if preset == "ideal":
        cfg = MemorySystemConfig(preset_name="ideal", cpu_freq_ghz=3.0)
        cfg.l1i.hit_latency_cycles = 0
        cfg.l1d.hit_latency_cycles = 0
        cfg.l2.hit_latency_cycles = 0
        cfg.l3.hit_latency_cycles = 0
        cfg.hierarchy.l2_enabled = False
        cfg.hierarchy.l3_enabled = False
        cfg.prefetcher.enabled = False
        cfg.vm.enabled = False
        # DRAM responds with 0 extra cycles
        cfg.dram.timings.tRCD = 0
        cfg.dram.timings.tRP = 0
        cfg.dram.timings.tCL = 0
        cfg.dram.timings.tBURST = 0
        return cfg

    elif preset == "simple":
        cfg = MemorySystemConfig(preset_name="simple", cpu_freq_ghz=2.0)
        cfg.l1i.size_bytes = 32 * 1024
        cfg.l1d.size_bytes = 32 * 1024
        cfg.l1i.hit_latency_cycles = 4
        cfg.l1d.hit_latency_cycles = 4
        cfg.hierarchy.l2_enabled = False
        cfg.hierarchy.l3_enabled = False
        cfg.prefetcher.enabled = False
        cfg.vm.enabled = False
        cfg.dram.dram_type = "SimpleDRAM"
        cfg.dram.page_policy = "closed"
        cfg.dram.timings.tCK_ns = 1.0
        cfg.dram.timings.tRCD = 15
        cfg.dram.timings.tRP = 15
        cfg.dram.timings.tCL = 15
        cfg.dram.timings.tBURST = 4
        return cfg

    elif preset == "modern":
        cfg = MemorySystemConfig(preset_name="modern", cpu_freq_ghz=3.2)
        # Default is modern DDR4-3200
        return cfg

    elif preset == "modern-ddr5":
        cfg = MemorySystemConfig(preset_name="modern-ddr5", cpu_freq_ghz=4.0)
        cfg.dram.dram_type = "DDR5-4800"
        cfg.dram.channels = 1
        cfg.dram.subchannels_per_channel = 2  # 2 independent 32-bit subchannels
        cfg.dram.bus_width_bytes = 4         # 32 bits per subchannel
        cfg.dram.bank_groups_per_rank = 8     # DDR5 doubles bank groups
        cfg.dram.banks_per_group = 4
        cfg.dram.timings.tCK_ns = 0.416       # 2400 MHz bus clock
        cfg.dram.timings.tRCD = 34
        cfg.dram.timings.tRP = 34
        cfg.dram.timings.tCL = 40
        cfg.dram.timings.tRAS = 77
        cfg.dram.timings.tRC = 111
        cfg.dram.timings.burst_length = 16    # BL16 for DDR5
        cfg.dram.timings.tBURST = 8
        cfg.dram.timings.tRRD_S = 8
        cfg.dram.timings.tRRD_L = 12
        cfg.dram.timings.tCCD_S = 8
        cfg.dram.timings.tCCD_L = 16
        return cfg

    elif preset == "lecture":
        # Lecture slide preset:
        # RF: 0.3ns (~1 cycle @ 3.3GHz)
        # L1: 1.1ns (~4 cycles)
        # L2: 3.3ns (~11 cycles)
        # L3: 12.8ns (~42 cycles)
        # DRAM: 62.9ns (~207 cycles)
        # SSD: 0.1ms (~330,000 cycles)
        # Disk: 3.0ms (~9,900,000 cycles)
        cfg = MemorySystemConfig(preset_name="lecture", cpu_freq_ghz=3.3)
        cfg.l1i.hit_latency_cycles = 4
        cfg.l1d.hit_latency_cycles = 4
        cfg.l2.hit_latency_cycles = 11
        cfg.l3.hit_latency_cycles = 42
        cfg.dram.dram_type = "LectureDRAM"
        cfg.dram.timings.tCK_ns = 1.0
        cfg.dram.timings.tRCD = 20
        cfg.dram.timings.tRP = 20
        cfg.dram.timings.tCL = 20
        cfg.dram.timings.tBURST = 4
        cfg.vm.va_bits = 32
        cfg.vm.pt_levels = 2  # 10 + 10 + 12
        cfg.vm.minor_fault_latency_cycles = 1500
        cfg.vm.major_fault_latency_cycles = 330000
        return cfg

    else:
        raise ValueError(f"Unknown preset: '{preset}'. Choose from: 'ideal', 'simple', 'modern', 'modern-ddr5', 'lecture'.")
