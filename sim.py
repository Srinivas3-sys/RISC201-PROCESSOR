#!/usr/bin/env python3
"""
sim.py: RISC201 Memory Hierarchy Simulator and Interactive CLI Debugger.
Supports full command-line parameterization and interactive debugging commands.
"""

from __future__ import annotations
import sys
import os
import argparse
from typing import Optional, List, Dict

# Ensure local packages can be imported
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from memory.config import MemorySystemConfig, get_preset
from memory.hierarchy import MemoryHierarchy
from memory.vm import PageFaultException
from cpu.assembler import Assembler
from cpu.pipeline4 import Pipeline4Stage
from cpu.pipeline6 import Pipeline6Stage
from cpu.microcode import MicroController
from cpu.isa import decode_instruction, disassemble_instruction
from visualizer import MemoryVisualizer


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="RISC201 Memory Hierarchy & Processor Simulator")
    p.add_argument("source", nargs="?", default="benchmarks/factorial_iter.s", help="Assembly file to run")
    p.add_argument("--pipeline", choices=["4", "6", "micro"], default="4", help="Pipeline engine (4, 6, micro)")
    p.add_argument("--interactive", action="store_true", help="Launch interactive CLI debugger REPL")

    # Memory Presets & Config File
    p.add_argument("--mem-preset", choices=["ideal", "simple", "modern", "modern-ddr5", "lecture"], default="modern", help="Memory preset")
    p.add_argument("--mem-config", type=str, default=None, help="Path to JSON or YAML config file")

    # Cache Geometry & Policies
    p.add_argument("--l1-size", type=int, default=None, help="L1 cache size in bytes")
    p.add_argument("--line-size", type=int, default=None, help="Cache line size in bytes (32/64/128)")
    p.add_argument("--assoc", type=int, default=None, help="L1 cache associativity")
    p.add_argument("--replacement", choices=["lru", "fifo", "random", "plru", "srrip"], default=None, help="Cache replacement policy")
    p.add_argument("--write-policy", choices=["wb_wa", "wt_nwa"], default=None, help="Write policy")
    p.add_argument("--prefetcher", choices=["none", "next_line", "stride", "stream"], default=None, help="Hardware prefetcher")
    p.add_argument("--mshrs", type=int, default=None, help="Number of MSHR entries")
    p.add_argument("--cache-index", choices=["pipt", "vipt", "vivt"], default=None, help="L1 cache indexing mode")

    # DRAM Parameters
    p.add_argument("--dram-type", type=str, default=None, help="DRAM technology string (e.g. DDR4-3200, DDR5-4800)")
    p.add_argument("--tRCD", type=int, default=None, help="DRAM tRCD latency")
    p.add_argument("--tRP", type=int, default=None, help="DRAM tRP latency")
    p.add_argument("--tCL", type=int, default=None, help="DRAM tCL latency")
    p.add_argument("--channels", type=int, default=None, help="Number of DRAM channels")
    p.add_argument("--banks", type=int, default=None, help="Banks per group")
    p.add_argument("--bank-groups", type=int, default=None, help="Bank groups per rank")
    p.add_argument("--addr-map", type=str, default=None, help="Address mapping scheme")
    p.add_argument("--page-policy", choices=["open", "closed"], default=None, help="DRAM row-buffer policy")
    p.add_argument("--sched", choices=["fcfs", "frfcfs"], default=None, help="DRAM scheduler")

    # Virtual Memory & TLBs
    p.add_argument("--vm", choices=["on", "off"], default=None, help="Enable or disable virtual memory")
    p.add_argument("--page-size", type=int, default=None, help="Virtual memory page size in bytes (e.g. 4096, 2097152)")
    p.add_argument("--pt-levels", choices=[1, 2, 4], type=int, default=None, help="Number of page table levels")
    p.add_argument("--va-bits", type=int, default=None, help="Virtual address bits (32 or 48)")
    p.add_argument("--pa-bits", type=int, default=None, help="Physical address bits")
    p.add_argument("--phys-mem-size", type=int, default=None, help="Physical memory capacity in bytes")
    p.add_argument("--itlb-entries", type=int, default=None, help="ITLB entry count")
    p.add_argument("--dtlb-entries", type=int, default=None, help="DTLB entry count")
    p.add_argument("--stlb-entries", type=int, default=None, help="L2 unified STLB entry count")
    p.add_argument("--tlb-assoc", type=int, default=None, help="TLB associativity")
    p.add_argument("--tlb-replacement", choices=["lru", "fifo", "random", "plru"], default=None, help="TLB replacement policy")
    p.add_argument("--pwc", choices=["on", "off"], default=None, help="Enable/disable Page-Walk Cache")
    p.add_argument("--asid", choices=["on", "off"], default=None, help="Enable/disable ASID tagging")
    p.add_argument("--page-replacement", choices=["fifo", "lru", "clock", "opt"], default=None, help="Page replacement policy")
    p.add_argument("--demand-paging", choices=["on", "off"], default=None, help="Enable/disable demand paging")

    return p


def apply_args_to_config(cfg: MemorySystemConfig, args: argparse.Namespace) -> None:
    if args.mem_config:
        if args.mem_config.endswith(".yaml") or args.mem_config.endswith(".yml"):
            with open(args.mem_config, "r") as f:
                cfg = MemorySystemConfig.from_yaml(f.read())
        else:
            with open(args.mem_config, "r") as f:
                cfg = MemorySystemConfig.from_json(f.read())

    # Cache overrides
    if args.l1_size is not None:
        cfg.l1i.size_bytes = args.l1_size
        cfg.l1d.size_bytes = args.l1_size
    if args.line_size is not None:
        cfg.l1i.line_size_bytes = args.line_size
        cfg.l1d.line_size_bytes = args.line_size
        cfg.l2.line_size_bytes = args.line_size
        cfg.l3.line_size_bytes = args.line_size
    if args.assoc is not None:
        cfg.l1i.associativity = args.assoc
        cfg.l1d.associativity = args.assoc
    if args.replacement is not None:
        cfg.l1i.replacement_policy = args.replacement
        cfg.l1d.replacement_policy = args.replacement
    if args.write_policy is not None:
        cfg.l1i.write_policy = args.write_policy
        cfg.l1d.write_policy = args.write_policy
    if args.prefetcher is not None:
        cfg.prefetcher.prefetcher_type = args.prefetcher
        cfg.prefetcher.enabled = (args.prefetcher != "none")
    if args.mshrs is not None:
        cfg.l1d.mshr_entries = args.mshrs
    if args.cache_index is not None:
        cfg.l1i.indexing_mode = args.cache_index
        cfg.l1d.indexing_mode = args.cache_index

    # DRAM overrides
    if args.dram_type is not None: cfg.dram.dram_type = args.dram_type
    if args.tRCD is not None: cfg.dram.timings.tRCD = args.tRCD
    if args.tRP is not None: cfg.dram.timings.tRP = args.tRP
    if args.tCL is not None: cfg.dram.timings.tCL = args.tCL
    if args.channels is not None: cfg.dram.channels = args.channels
    if args.banks is not None: cfg.dram.banks_per_group = args.banks
    if args.bank_groups is not None: cfg.dram.bank_groups_per_rank = args.bank_groups
    if args.addr_map is not None: cfg.dram.address_mapping = args.addr_map
    if args.page_policy is not None: cfg.dram.page_policy = args.page_policy
    if args.sched is not None: cfg.dram.scheduler = args.sched

    # VM overrides
    if args.vm is not None: cfg.vm.enabled = (args.vm == "on")
    if args.page_size is not None: cfg.vm.page_size_bytes = args.page_size
    if args.pt_levels is not None: cfg.vm.pt_levels = args.pt_levels
    if args.va_bits is not None: cfg.vm.va_bits = args.va_bits
    if args.pa_bits is not None: cfg.vm.pa_bits = args.pa_bits
    if args.phys_mem_size is not None: cfg.vm.phys_mem_size_bytes = args.phys_mem_size
    if args.itlb_entries is not None: cfg.vm.itlb.entries = args.itlb_entries
    if args.dtlb_entries is not None: cfg.vm.dtlb.entries = args.dtlb_entries
    if args.stlb_entries is not None: cfg.vm.stlb.entries = args.stlb_entries
    if args.tlb_assoc is not None:
        cfg.vm.itlb.associativity = args.tlb_assoc
        cfg.vm.dtlb.associativity = args.tlb_assoc
    if args.tlb_replacement is not None:
        cfg.vm.itlb.replacement_policy = args.tlb_replacement
        cfg.vm.dtlb.replacement_policy = args.tlb_replacement
    if args.pwc is not None: cfg.vm.pwc_enabled = (args.pwc == "on")
    if args.asid is not None: cfg.vm.asid_enabled = (args.asid == "on")
    if args.page_replacement is not None: cfg.vm.page_replacement = args.page_replacement
    if args.demand_paging is not None: cfg.vm.demand_paging = (args.demand_paging == "on")


class InteractiveDebugger:
    def __init__(self, hier: MemoryHierarchy, words: List[int], mode: str = "4"):
        self.hier = hier
        self.words = words
        self.mode = mode
        self.asid = 0

        # Instantiate core
        if mode == "6":
            self.p6 = Pipeline6Stage(hier)
            self.p6.program_size = len(words) * 4
            self.core = self.p6
        elif mode == "micro":
            self.mcu = MicroController(hier)
            self.core = self.mcu
        else:
            self.p4 = Pipeline4Stage(hier)
            self.p4.program_size = len(words) * 4
            self.core = self.p4

        self.breakpoints: set[int] = set()

    def print_help(self) -> None:
        msg = """
============================ RISC201 DEBUGGER COMMANDS ============================
  s, step                 : Step one clock cycle / micro-cycle
  si, step_inst           : Step one complete program instruction
  r, run, c, continue     : Run program to completion or breakpoint
  b, break <addr>         : Set breakpoint at address (e.g. b 0x10)
  db, delbreak <addr>     : Remove breakpoint at address
  regs                    : Display register file (r0-r15, flags, PC)
  p, pipeline             : Display cycle-by-cycle pipeline diagram and latches
  stats                   : Display performance, hazard, and stall counters

--- MEMORY & CACHE COMMANDS ---
  cache [l1i|l1d|l2|l3]   : Dump sets x ways cache grid
  cache stats             : Display cache hits, misses, 3C breakdown, MLP
  mshr                    : Display active MSHR status and outstanding requests
  dram                    : Display DRAM bank state and open row buffers
  dram timing             : Display DRAM timing parameters
  banks                   : Display DRAM per-bank access and conflict bar chart
  addrmap <addr>          : Translate address to Channel/Rank/BG/Bank/Row/Col
  prefetch                : Display hardware prefetcher statistics
  mem trace               : Display recent memory access history trace
  amat                    : Calculate and display recursive AMAT
  refresh                 : Display periodic DRAM refresh statistics

--- VIRTUAL MEMORY & TLB COMMANDS ---
  tlb [itlb|dtlb|stlb]    : Dump TLB contents and valid mappings
  tlb stats               : Display ITLB, DTLB, STLB hit rates and reach
  translate <vaddr>       : Full step-by-step walk with indices, PWC, and PTE
  pagetable               : Display multi-level radix page table tree
  frames                  : Display physical frame allocation map and owners
  fault log               : Display page fault counters (minor/major/protection)
  vmstat                  : Display overall virtual memory statistics
  flush tlb [asid|page]   : Invalidate TLB entries
  switch <pid>            : Context switch to target PID/ASID

  help, h                 : Show this command help
  q, quit, exit           : Exit debugger
===================================================================================
        """
        print(msg)

    def repl(self) -> None:
        self.print_help()
        while True:
            try:
                line = input(f"RISC201[{self.mode}-Stage]> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not line:
                continue

            parts = line.split()
            cmd = parts[0].lower()

            if cmd in ("q", "quit", "exit"):
                break
            elif cmd in ("h", "help"):
                self.print_help()
            elif cmd in ("s", "step"):
                if self.mode == "micro":
                    done = self.mcu.step_micro_cycle()
                    print(f"MicroCycle {self.mcu.total_micro_cycles} | uPC={self.mcu.upc} | Waiting on: {self.mcu.waiting_on}")
                else:
                    self.core.step_cycle()
                    print(f"Cycle {self.core.cycles} completed. (Retired: {self.core.retired_instructions})")
            elif cmd in ("si", "step_inst"):
                if self.mode == "micro":
                    while not self.mcu.step_micro_cycle():
                        pass
                    print(f"Microcode instruction completed at PC=0x{self.mcu.pc:08X}")
                else:
                    prev = self.core.retired_instructions
                    while not self.core.halted and self.core.retired_instructions == prev and not self.core.is_drained():
                        self.core.step_cycle()
                    print(f"Instruction stepped. Retired: {self.core.retired_instructions}")
            elif cmd in ("r", "run", "c", "continue"):
                self.core.run()
                print(f"Execution completed. Total cycles: {self.core.cycles}, Retired: {self.core.retired_instructions}")
            elif cmd == "regs":
                rf = self.core.reg_file
                for i in range(0, 16, 4):
                    line_strs = [f"r{j:2d}=0x{rf.read_port1(j):08X}" for j in range(i, i+4)]
                    print("  " + "  ".join(line_strs))
                print(f"  Flags: E={int(rf.flags.E)} GT={int(rf.flags.GT)} | PC=0x{self.core.pc:08X}")
            elif cmd in ("p", "pipeline"):
                if self.mode == "4":
                    stages = ["IF", "OF", "EX", "MA_RW"]
                    insts = [self.p4.l_if_of.disassembly, self.p4.l_of_ex.disassembly, self.p4.l_ex_marw.disassembly, self.p4.l_ex_marw.disassembly]
                    stalls = ["" for _ in range(4)]
                    print(MemoryVisualizer.render_pipeline_stalls(stages, insts, stalls, self.p4.cycles))
                elif self.mode == "6":
                    stages = ["IF", "ID", "RF", "EX", "MA", "RW"]
                    insts = [self.p6.l_if_id.disassembly, self.p6.l_id_rf.disassembly, self.p6.l_rf_ex.disassembly, self.p6.l_ex_ma.disassembly, self.p6.l_ma_rw.disassembly, self.p6.l_ma_rw.disassembly]
                    stalls = ["" for _ in range(6)]
                    print(MemoryVisualizer.render_pipeline_stalls(stages, insts, stalls, self.p6.cycles))
            elif cmd == "stats":
                print(self.core.print_stats())
            elif cmd == "cache":
                target_cache = self.hier.l1d
                if len(parts) > 1 and parts[1].lower() == "l1i": target_cache = self.hier.l1i
                elif len(parts) > 1 and parts[1].lower() == "l2" and self.hier.l2: target_cache = self.hier.l2
                elif len(parts) > 1 and parts[1].lower() == "l3" and self.hier.l3: target_cache = self.hier.l3
                print(MemoryVisualizer.render_cache_grid(target_cache))
            elif cmd == "cache stats":
                print(self.hier.l1i.print_stats())
                print()
                print(self.hier.l1d.print_stats())
                if self.hier.l2: print("\n" + self.hier.l2.print_stats())
                if self.hier.l3: print("\n" + self.hier.l3.print_stats())
            elif cmd == "dram":
                print(MemoryVisualizer.render_dram_grid(self.hier.dram))
            elif cmd == "banks":
                print(self.hier.mapper.print_bank_utilization_chart())
            elif cmd == "addrmap" and len(parts) > 1:
                addr = int(parts[1], 16) if parts[1].startswith("0x") else int(parts[1])
                coords = self.hier.mapper.map_address(addr)
                print(f"Address 0x{addr:08X} -> {coords}")
            elif cmd == "tlb":
                print(MemoryVisualizer.render_tlb_table(self.hier.vm))
            elif cmd == "tlb stats":
                print(self.hier.vm.print_stats())
            elif cmd == "pagetable":
                print(MemoryVisualizer.render_page_table_tree(self.hier.vm, self.asid))
            elif cmd == "frames":
                print(MemoryVisualizer.render_physical_frame_map(self.hier.vm))
            elif cmd == "amat":
                print(f"Current AMAT : {self.hier.calculate_amat():.2f} cycles")
                print(f"Current EAT  : {self.hier.calculate_eat():.2f} cycles")
            elif cmd == "pyramid":
                print(MemoryVisualizer.render_hierarchy_pyramid(self.hier))
            elif cmd == "translate" and len(parts) > 1:
                vaddr = int(parts[1], 16) if parts[1].startswith("0x") else int(parts[1])
                indices, offset = self.hier.vm.split_virtual_address(vaddr)
                pte = self.hier.vm.get_pte(vaddr, self.asid)
                print(f"VADDR 0x{vaddr:08X}: VPN indices={indices}, Offset=0x{offset:03X}")
                if pte:
                    paddr = (pte.pfn << self.hier.vm.offset_bits) | offset
                    print(f"  PTE Found: PFN=0x{pte.pfn:05X} -> PADDR=0x{paddr:08X} (Present={pte.present}, W={pte.write})")
                else:
                    print(f"  PTE: Unmapped / Not present in page table.")
            elif cmd == "prefetch":
                print(self.hier.prefetcher.print_stats())
            elif cmd == "mem trace":
                print("=== Recent Memory Accesses ===")
                for r in self.hier.access_history[-10:]:
                    print(f"  [Cycle {r.cycle_start:6d}..{r.cycle_end:6d}] {'WRITE' if r.is_write else 'READ'} va=0x{r.vaddr:08X} -> pa=0x{r.paddr:08X} ({r.served_by}) lat={r.total_latency} cyc")
            else:
                print(f"Unknown command: '{cmd}'. Type 'help' for instructions.")


def main():
    parser = build_arg_parser()
    args = parser.parse_args()

    cfg = get_preset(args.mem_preset)
    apply_args_to_config(cfg, args)

    hier = MemoryHierarchy(cfg)

    # Read and assemble source program
    if not os.path.isfile(args.source):
        print(f"Error: Source file '{args.source}' not found!")
        sys.exit(1)

    with open(args.source, "r") as f:
        src = f.read()

    asmb = Assembler()
    if not asmb.assemble(src):
        print("Assembly failed!")
        sys.exit(1)

    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])

    hier.load_binary(base_vaddr=0, data=bytes(data), asid=0)

    if args.interactive:
        dbg = InteractiveDebugger(hier, words, mode=args.pipeline)
        dbg.repl()
    else:
        # Non-interactive batch execution
        if args.pipeline == "6":
            core = Pipeline6Stage(hier)
            core.program_size = len(words) * 4
        elif args.pipeline == "micro":
            core = MicroController(hier)
        else:
            core = Pipeline4Stage(hier)
            core.program_size = len(words) * 4

        print(f"Running '{args.source}' on {args.pipeline}-Stage core (Preset: {cfg.preset_name.upper()})...")
        core.run()
        print(core.print_stats())
        print()
        print(hier.print_summary())


if __name__ == "__main__":
    main()
