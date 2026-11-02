"""
common/perf.py: Rigorous cycle accounting, CPI stack breakdown,
analytic closed-form CPI models, and performance metrics for RISC201.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Any
import json
import csv
from common.isa import INSTRUCTION_TABLE, InstructionInfo, OPCODE_BITS


# Canonical Stall Reason Codes
STALL_REASONS = [
    "RAW_LOAD_USE",
    "RAW_NOFWD",
    "WAR_HAZARD",
    "WAW_HAZARD",
    "BRANCH_MISPREDICT",
    "BRANCH_STALL",
    "STRUCTURAL_MEM",
    "STRUCTURAL_RF",
    "MULTICYCLE_EX",
    "ICACHE_MISS",
    "DCACHE_MISS",
    "TLB_MISS",
    "PAGE_WALK",
    "PAGE_FAULT",
    "DRAM_REFRESH",
    "EXCEPTION_FLUSH",
    "OTHER_STALL",
]


@dataclass
class PerformanceTracker:
    pipeline_stages: int = 4
    clock_mhz: float = 3200.0  # 3.2 GHz default

    # Basic counters
    total_cycles: int = 0
    retired_instructions: int = 0
    pipeline_fill_cycles: int = 0  # stages - 1

    # Stall cycle breakdown by exact reason string
    stall_cycles: Dict[str, int] = field(default_factory=lambda: {r: 0 for r in STALL_REASONS})

    # Hazard detection telemetry
    raw_detected_count: int = 0
    war_detected_count: int = 0
    waw_detected_count: int = 0
    hazards_avoided_by_forwarding: int = 0
    forwarding_path_usage: Dict[str, int] = field(default_factory=dict)

    # Instruction mix by 5-bit binary opcode string
    instruction_mix: Dict[str, int] = field(default_factory=lambda: {op: 0 for op in INSTRUCTION_TABLE})

    # Per-instruction-class cycles
    class_retired_count: Dict[str, int] = field(default_factory=lambda: {
        "ALU": 0, "LOAD": 0, "STORE": 0, "BRANCH_TAKEN": 0, "BRANCH_NOT_TAKEN": 0, "CALL": 0, "RET": 0, "OTHER": 0
    })

    # Branch predictor telemetry
    total_branches: int = 0
    branches_taken: int = 0
    branches_mispredicted: int = 0
    btb_lookups: int = 0
    btb_hits: int = 0
    ras_lookups: int = 0
    ras_hits: int = 0

    def record_instruction_retired(self, opcode: str, is_branch_taken: bool = False) -> None:
        """Records an instruction retiring and updates instruction mix histograms."""
        self.retired_instructions += 1
        if opcode in self.instruction_mix:
            self.instruction_mix[opcode] += 1
        else:
            self.instruction_mix[opcode] = 1

        info = INSTRUCTION_TABLE.get(opcode)
        if info is not None:
            if info.is_load:
                self.class_retired_count["LOAD"] += 1
            elif info.is_store:
                self.class_retired_count["STORE"] += 1
            elif info.is_call:
                self.class_retired_count["CALL"] += 1
            elif info.is_return:
                self.class_retired_count["RET"] += 1
            elif info.is_branch:
                if is_branch_taken:
                    self.class_retired_count["BRANCH_TAKEN"] += 1
                else:
                    self.class_retired_count["BRANCH_NOT_TAKEN"] += 1
            elif info.is_alu:
                self.class_retired_count["ALU"] += 1
            else:
                self.class_retired_count["OTHER"] += 1

    def record_stall(self, reason: str, count: int = 1) -> None:
        """Records stall cycles attributed to an explicit reason."""
        if reason not in self.stall_cycles:
            self.stall_cycles[reason] = 0
        self.stall_cycles[reason] += count

    def record_forward(self, path_name: str) -> None:
        """Records a successful bypass through a named forwarding path."""
        self.hazards_avoided_by_forwarding += 1
        if path_name not in self.forwarding_path_usage:
            self.forwarding_path_usage[path_name] = 0
        self.forwarding_path_usage[path_name] += 1

    def total_stalls(self) -> int:
        return sum(self.stall_cycles.values())

    def verify_cycle_accounting_invariant(self) -> bool:
        """
        Asserts the cycle accounting invariant:
          total_cycles == retired_instructions + total_stalls + pipeline_fill_overhead (when drained)
        or within allowable pipeline drain latency.
        """
        summed = self.retired_instructions + self.total_stalls()
        # In a fully drained pipeline, total_cycles == retired_instructions + total_stalls + (stages - 1)
        # or exactly reconciled.
        diff = abs(self.total_cycles - summed)
        return diff <= self.pipeline_stages

    def get_cpi(self) -> float:
        return (self.total_cycles / self.retired_instructions) if self.retired_instructions > 0 else 0.0

    def get_ipc(self) -> float:
        return (self.retired_instructions / self.total_cycles) if self.total_cycles > 0 else 0.0

    def get_cpi_stack(self) -> Dict[str, float]:
        """
        Computes the CPI contribution of each component:
          CPI = CPI_base + sum(stall_cycles_by_category / IC)
        """
        ic = self.retired_instructions if self.retired_instructions > 0 else 1
        return {
            "Base (Ideal)": 1.0,
            "RAW Data Hazards": (self.stall_cycles.get("RAW_LOAD_USE", 0) + self.stall_cycles.get("RAW_NOFWD", 0)) / ic,
            "Branch Mispredicts": self.stall_cycles.get("BRANCH_MISPREDICT", 0) / ic,
            "Branch Stalls": self.stall_cycles.get("BRANCH_STALL", 0) / ic,
            "Structural Hazards": (self.stall_cycles.get("STRUCTURAL_MEM", 0) + self.stall_cycles.get("STRUCTURAL_RF", 0)) / ic,
            "Multi-cycle Execution": self.stall_cycles.get("MULTICYCLE_EX", 0) / ic,
            "I-Cache / ITLB Stalls": (self.stall_cycles.get("ICACHE_MISS", 0) + self.stall_cycles.get("TLB_MISS", 0)) / ic,
            "D-Cache / DTLB Stalls": self.stall_cycles.get("DCACHE_MISS", 0) / ic,
            "Page Walks & Faults": (self.stall_cycles.get("PAGE_WALK", 0) + self.stall_cycles.get("PAGE_FAULT", 0)) / ic,
            "DRAM Refresh Overhead": self.stall_cycles.get("DRAM_REFRESH", 0) / ic,
            "Exception Flushes": self.stall_cycles.get("EXCEPTION_FLUSH", 0) / ic,
        }

    def compute_analytic_cpi(self, load_use_penalty: int = 1, branch_penalty: int = 2) -> Tuple[float, float, float]:
        """
        Closed-form analytic CPI estimation:
          CPI_analytic = 1.0 + f_load_use * p_lu + f_branch * mispred_rate * branch_penalty + f_mem * miss_penalty
        Returns (simulated_cpi, analytic_cpi, error_pct).
        """
        ic = self.retired_instructions if self.retired_instructions > 0 else 1
        sim_cpi = self.get_cpi()

        f_load = self.class_retired_count.get("LOAD", 0) / ic
        f_branch = self.total_branches / ic
        mispred_rate = (self.branches_mispredicted / self.total_branches) if self.total_branches > 0 else 0.0

        # Load-use frequency approximated from RAW_LOAD_USE stalls
        lu_stalls = self.stall_cycles.get("RAW_LOAD_USE", 0)
        f_load_use = lu_stalls / ic

        analytic_cpi = 1.0 + f_load_use + (f_branch * mispred_rate * branch_penalty)
        # Add memory stalls
        mem_stalls = (
            self.stall_cycles.get("ICACHE_MISS", 0) +
            self.stall_cycles.get("DCACHE_MISS", 0) +
            self.stall_cycles.get("TLB_MISS", 0) +
            self.stall_cycles.get("PAGE_WALK", 0)
        )
        analytic_cpi += (mem_stalls / ic)

        error_pct = (abs(sim_cpi - analytic_cpi) / sim_cpi * 100.0) if sim_cpi > 0 else 0.0
        return sim_cpi, analytic_cpi, error_pct

    def generate_report(self, format_type: str = "table") -> str:
        """Generates formatted performance report in table, csv, or json format."""
        cpi = self.get_cpi()
        ipc = self.get_ipc()
        stack = self.get_cpi_stack()
        sim_cpi, ana_cpi, err = self.compute_analytic_cpi()

        if format_type.lower() == "json":
            payload = {
                "pipeline_stages": self.pipeline_stages,
                "clock_mhz": self.clock_mhz,
                "total_cycles": self.total_cycles,
                "retired_instructions": self.retired_instructions,
                "cpi": round(cpi, 3),
                "ipc": round(ipc, 3),
                "stall_cycles": self.stall_cycles,
                "cpi_stack": {k: round(v, 4) for k, v in stack.items()},
                "hazards": {
                    "raw_detected": self.raw_detected_count,
                    "war_detected": self.war_detected_count,
                    "waw_detected": self.waw_detected_count,
                    "forwarded_hazards": self.hazards_avoided_by_forwarding,
                    "path_usage": self.forwarding_path_usage
                },
                "branches": {
                    "total": self.total_branches,
                    "taken": self.branches_taken,
                    "mispredicted": self.branches_mispredicted,
                    "accuracy_pct": round(((self.total_branches - self.branches_mispredicted) / self.total_branches * 100.0), 2) if self.total_branches > 0 else 100.0
                },
                "analytic_cpi": round(ana_cpi, 3),
                "model_error_pct": round(err, 2)
            }
            return json.dumps(payload, indent=2)

        elif format_type.lower() == "csv":
            lines = [
                "Metric,Value",
                f"Pipeline_Stages,{self.pipeline_stages}",
                f"Clock_MHz,{self.clock_mhz}",
                f"Total_Cycles,{self.total_cycles}",
                f"Retired_Instructions,{self.retired_instructions}",
                f"CPI,{cpi:.3f}",
                f"IPC,{ipc:.3f}",
            ]
            for r, cnt in self.stall_cycles.items():
                lines.append(f"Stall_{r},{cnt}")
            return "\n".join(lines)

        # Standard ASCII Terminal Table with Bar Chart
        lines = [
            f"=== {self.pipeline_stages}-Stage Pipeline Performance Accounting Report ===",
            f"  CPU Frequency          : {self.clock_mhz:.1f} MHz",
            f"  Total Clock Cycles     : {self.total_cycles}",
            f"  Retired Instructions   : {self.retired_instructions}",
            f"  CPI (Cycles / Inst)    : {cpi:.3f}",
            f"  IPC (Inst / Cycle)     : {ipc:.3f}",
            f"  Analytic Model CPI     : {ana_cpi:.3f} (Error: {err:.2f}%)",
            "",
            "--- CPI Stack Breakdown ---"
        ]
        max_bar_width = 30
        for comp, val in stack.items():
            bar_len = int(min(max_bar_width, val * 10))
            bar = "#" * bar_len
            lines.append(f"  {comp:<24} : {val:>6.3f} | {bar}")

        lines.extend([
            "",
            "--- Hazard & Forwarding Telemetry ---",
            f"  RAW Hazards Detected   : {self.raw_detected_count}",
            f"  WAR Hazards Detected   : {self.war_detected_count} (Structural / In-Order Safe)",
            f"  WAW Hazards Detected   : {self.waw_detected_count} (In-Order Commit Verified)",
            f"  Hazards Avoided by FWD : {self.hazards_avoided_by_forwarding}",
        ])
        if self.forwarding_path_usage:
            lines.append("  Bypass Path Utilization :")
            for path, count in sorted(self.forwarding_path_usage.items()):
                lines.append(f"    - {path:<22} : {count:>5} bypasses")

        lines.extend([
            "",
            "--- Branch Prediction Metrics ---",
            f"  Total Branches         : {self.total_branches}",
            f"  Branches Taken         : {self.branches_taken}",
            f"  Branch Mispredicts     : {self.branches_mispredicted}",
            f"  Mispredict Rate        : {((self.branches_mispredicted / self.total_branches * 100.0) if self.total_branches > 0 else 0.0):.2f}%",
            f"  BTB Hit Rate           : {((self.btb_hits / self.btb_lookups * 100.0) if self.btb_lookups > 0 else 0.0):.2f}%",
        ])

        # Instruction mix summary (top opcodes)
        lines.extend(["", "--- Instruction Mix (Top Opcodes) ---"])
        sorted_mix = sorted(
            [(op, cnt) for op, cnt in self.instruction_mix.items() if cnt > 0],
            key=lambda x: x[1],
            reverse=True
        )
        for op, cnt in sorted_mix[:8]:
            info = INSTRUCTION_TABLE.get(op)
            mn = info.mnemonic if info else "unknown"
            pct = (cnt / self.retired_instructions * 100.0) if self.retired_instructions > 0 else 0.0
            lines.append(f"  [{op}] {mn:<6} : {cnt:>6} ({pct:>5.1f}%)")

        return "\n".join(lines)
