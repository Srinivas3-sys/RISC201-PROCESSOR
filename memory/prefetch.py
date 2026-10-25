"""
memory/prefetch.py: Hardware data prefetchers (Next-Line, Stride RPT, Stream).
Tracks coverage, accuracy, timeliness, pollution, and bandwidth overhead.
Enforces 4KB page boundary guard.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Set
from memory.config import PrefetcherConfig


@dataclass
class StrideEntry:
    tag_pc: int
    last_addr: int
    stride: int
    state: str = "INIT"                   # "INIT", "TRANSIENT", "STEADY"


@dataclass
class StreamEntry:
    stream_id: int
    start_line: int
    end_line: int
    direction: int = 1                    # +1 or -1
    active: bool = False
    last_cycle: int = 0


@dataclass
class PrefetchRequest:
    line_addr: int
    issued_cycle: int
    pc: int
    is_useful: bool = False
    is_late: bool = False


class Prefetcher:
    """
    Pluggable hardware prefetcher engine.
    Never crosses page boundaries (4KB) unless configured.
    """

    def __init__(self, config: PrefetcherConfig, line_size_bytes: int = 64, page_size_bytes: int = 4096):
        self.cfg = config
        self.line_size = line_size_bytes
        self.page_size = page_size_bytes
        self.pftype = config.prefetcher_type.lower()
        self.degree = config.degree
        self.distance = config.distance
        self.cross_page = config.cross_page_boundary

        # Stride Reference Prediction Table (RPT)
        self.rpt: Dict[int, StrideEntry] = {}
        self.max_rpt_entries = config.stride_table_entries

        # Stream table
        self.streams: List[StreamEntry] = []
        self.max_stream_entries = config.stream_table_entries

        # History and Tracking
        self.prefetched_lines: Dict[int, PrefetchRequest] = {} # line_addr -> PrefetchRequest
        self.useful_prefetches = 0
        self.late_prefetches = 0
        self.total_prefetches_issued = 0
        self.pollution_evictions = 0
        self.demand_misses_saved = 0

    def _crosses_page(self, base_addr: int, target_addr: int) -> bool:
        """Checks if target prefetch address crosses the 4KB page boundary of base_addr."""
        if self.cross_page:
            return False
        return (base_addr // self.page_size) != (target_addr // self.page_size)

    def on_access(self, addr: int, pc: int = 0, is_write: bool = False, current_cycle: int = 0) -> List[int]:
        """
        Observes a memory reference. Returns list of block-aligned addresses to prefetch.
        """
        if not self.cfg.enabled or self.pftype == "none":
            return []

        line_aligned = (addr // self.line_size) * self.line_size
        candidates: List[int] = []

        # 1. Check if this access consumed a previously prefetched line (Accuracy / Timeliness)
        if line_aligned in self.prefetched_lines:
            req = self.prefetched_lines.pop(line_aligned)
            self.useful_prefetches += 1
            if current_cycle - req.issued_cycle < 10: # Sample threshold for late
                self.late_prefetches += 1
                req.is_late = True
            req.is_useful = True
            self.demand_misses_saved += 1

        # 2. Next-Line Prefetcher
        if self.pftype == "next_line":
            for d in range(1, self.degree + 1):
                target = line_aligned + (d * self.line_size)
                if not self._crosses_page(line_aligned, target):
                    candidates.append(target)

        # 3. Stride Prefetcher (Reference Prediction Table)
        elif self.pftype == "stride":
            if pc in self.rpt:
                entry = self.rpt[pc]
                stride = addr - entry.last_addr
                if entry.state == "TRANSIENT" and stride == entry.stride:
                    entry.state = "STEADY"
                elif entry.state != "STEADY":
                    entry.stride = stride
                    entry.state = "TRANSIENT"

                if entry.state == "STEADY" and stride == entry.stride:
                    for d in range(1, self.degree + 1):
                        target = line_aligned + (d * entry.stride)
                        target = (target // self.line_size) * self.line_size
                        if not self._crosses_page(line_aligned, target):
                            candidates.append(target)
                entry.last_addr = addr
            else:
                if len(self.rpt) >= self.max_rpt_entries:
                    # Evict first
                    self.rpt.pop(next(iter(self.rpt)))
                self.rpt[pc] = StrideEntry(tag_pc=pc, last_addr=addr, stride=0, state="INIT")

        # 4. Stream Prefetcher
        elif self.pftype == "stream":
            matched = False
            for s in self.streams:
                if abs(line_aligned - s.end_line) <= (self.distance * self.line_size):
                    matched = True
                    s.direction = 1 if line_aligned >= s.end_line else -1
                    s.end_line = line_aligned
                    for d in range(1, self.degree + 1):
                        target = line_aligned + (s.direction * (self.distance + d) * self.line_size)
                        if not self._crosses_page(line_aligned, target):
                            candidates.append(target)
                    break
            if not matched:
                if len(self.streams) >= self.max_stream_entries:
                    self.streams.pop(0)
                self.streams.append(StreamEntry(
                    stream_id=len(self.streams),
                    start_line=line_aligned,
                    end_line=line_aligned,
                    direction=1,
                    active=True,
                    last_cycle=current_cycle
                ))

        # Record issued prefetches
        for c in candidates:
            if c not in self.prefetched_lines:
                self.prefetched_lines[c] = PrefetchRequest(line_addr=c, issued_cycle=current_cycle, pc=pc)
                self.total_prefetches_issued += 1

        return candidates

    def on_evict_unaccessed(self, line_addr: int) -> None:
        """Called when a prefetched line was evicted without ever being read."""
        if line_addr in self.prefetched_lines:
            self.prefetched_lines.pop(line_addr)
            self.pollution_evictions += 1

    def get_accuracy(self) -> float:
        if self.total_prefetches_issued == 0:
            return 0.0
        return (self.useful_prefetches / self.total_prefetches_issued) * 100.0

    def print_stats(self) -> str:
        acc = self.get_accuracy()
        out = [
            f"=== Hardware Prefetcher Statistics ({self.pftype.upper()}) ===",
            f"  Prefetches Issued   : {self.total_prefetches_issued}",
            f"  Useful Prefetches   : {self.useful_prefetches} ({acc:.2f}% accuracy)",
            f"  Late Prefetches     : {self.late_prefetches}",
            f"  Pollution Evictions : {self.pollution_evictions}",
            f"  Extra BW Overhead   : {max(0, self.total_prefetches_issued - self.useful_prefetches) * self.line_size} Bytes",
        ]
        return "\n".join(out)
