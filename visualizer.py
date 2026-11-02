"""
visualizer.py: ASCII visualizer suite for the RISC201 memory hierarchy:
  - Cache set x way visualizer
  - DRAM bank grid with open row buffers
  - Memory hierarchy pyramid with live hit counts and AMAT/EAT
  - Per-cycle pipeline diagram with translation/memory stalls
  - MSHR outstanding miss occupancy timeline
  - TLB table view
  - Multi-level Page Table ASCII tree
  - Virtual Address Space segment map
  - Physical Frame allocation map
"""

from __future__ import annotations
from typing import List, Dict, Tuple, Optional, Any
from memory.cache import Cache
from memory.dram import DRAMController
from memory.hierarchy import MemoryHierarchy
from memory.vm import VirtualMemorySubsystem, PTE


class MemoryVisualizer:
    @staticmethod
    def render_cache_grid(cache: Cache, max_sets: int = 16) -> str:
        """Renders sets x ways grid for cache."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"| CACHE SET x WAY GRID: {cache.cfg.name:10s} ({cache.num_sets} sets, {cache.associativity}-way)              |",
            f"+--------------------------------------------------------------------------+",
            "  Set | " + " | ".join([f"Way {w:2d} (V/D/Tag)    " for w in range(cache.associativity)]),
            "  ----+-" + "-+-".join(["--------------------" for _ in range(cache.associativity)])
        ]
        num_to_show = min(max_sets, cache.num_sets)
        for s in range(num_to_show):
            cset = cache.sets[s]
            way_strs = []
            for w in range(cache.associativity):
                line = cset.lines[w]
                if line.valid:
                    d = "D" if line.dirty else "C"
                    way_strs.append(f"[V|{d}|0x{line.tag:08X}]")
                else:
                    way_strs.append("[ -- EMPTY -- ]    ")
            out.append(f"  {s:3d} | " + " | ".join(way_strs))
        if cache.num_sets > max_sets:
            out.append(f"  ... ({cache.num_sets - max_sets} more sets omitted) ...")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_dram_grid(dram: DRAMController) -> str:
        """Renders 2D bank grid with open rows and state."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"| DRAM BANK GRID: {dram.cfg.dram_type:15s} (Policy: {dram.page_policy.upper()})                     |",
            f"+--------------------------------------------------------------------------+",
            "  Chan | Rank | BankGroup | Bank | Row Buffer State / Open Row",
            "  -----+------+-----------+------+-----------------------------------------"
        ]
        for (c, r, bg, b), bank in sorted(dram.banks.items()):
            if bank.open_row is not None:
                state_str = f"OPEN [Row: 0x{bank.open_row:04X}] (Hits: {bank.row_hits}, Conf: {bank.row_conflicts})"
            else:
                state_str = f"PRECHARGED / CLOSED (Empties: {bank.row_empties})"
            out.append(f"   {c:2d}  |  {r:2d}  |    {bg:2d}     |  {b:2d}  | {state_str}")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_hierarchy_pyramid(hier: MemoryHierarchy) -> str:
        """ASCII Memory Hierarchy Pyramid with live hit counts, AMAT, and EAT."""
        l1i_hits = hier.l1i.hits
        l1d_hits = hier.l1d.hits
        l2_hits = hier.l2.hits if hier.l2 else 0
        l3_hits = hier.l3.hits if hier.l3 else 0
        dram_reqs = hier.dram.total_reads + hier.dram.total_writes
        amat = hier.calculate_amat()
        eat = hier.calculate_eat()

        pyramid = f"""
                       /\\
                      /  \\          [ CPU Registers ]
                     / RF \\         0.3 ns / 1 cycle
                    /------\\
                   /  TLB   \\       ITLB: {hier.vm.itlb.hits} hits | DTLB: {hier.vm.dtlb.hits} hits
                  /----------\\
                 /    L1      \\     L1I: {l1i_hits} hits | L1D: {l1d_hits} hits
                /--------------\\    4 cycles hit latency (VIPT)
               /       L2       \\   L2: {l2_hits} hits
              /------------------\\  12 cycles hit latency
             /         L3         \\ L3: {l3_hits} hits
            /----------------------\\40 cycles hit latency
           /          DRAM          \\DRAM: {dram_reqs} accesses
          /--------------------------\\60-90 ns (tCL={hier.dram.timings.tCL}, tRCD={hier.dram.timings.tRCD}, tRP={hier.dram.timings.tRP})
         /       SSD / Swap Disk      \\Minor: {hier.vm.minor_faults} | Major: {hier.vm.major_faults} faults
        /------------------------------\\0.1 ms - 3.0 ms

    >>> Live Average Memory Access Time (AMAT)       : {amat:.2f} cycles
    >>> Live Effective Access Time (EAT with TLB/VM) : {eat:.2f} cycles
        """
        return pyramid

    @staticmethod
    def render_tlb_table(vm: VirtualMemorySubsystem) -> str:
        """Renders ITLB, DTLB, and STLB contents."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|                     TRANSLATION LOOKASIDE BUFFERS                        |",
            f"+--------------------------------------------------------------------------+",
            f"--- ITLB ({vm.itlb.entries_count} entries, {vm.itlb.associativity}-way) ---"
        ]
        for s_idx, tset in enumerate(vm.itlb.sets):
            for w_idx, e in enumerate(tset):
                if e.valid:
                    perm = ("R" if e.read else "-") + ("W" if e.write else "-") + ("X" if e.execute else "-")
                    d = "D" if e.dirty else "C"
                    out.append(f"  [Set {s_idx:2d}, Way {w_idx}] VPN=0x{e.vpn:07X} -> PFN=0x{e.pfn:05X} | ASID={e.asid} | Perm={perm} | {d}")
        out.append(f"--- DTLB ({vm.dtlb.entries_count} entries, {vm.dtlb.associativity}-way) ---")
        for s_idx, tset in enumerate(vm.dtlb.sets):
            for w_idx, e in enumerate(tset):
                if e.valid:
                    perm = ("R" if e.read else "-") + ("W" if e.write else "-") + ("X" if e.execute else "-")
                    d = "D" if e.dirty else "C"
                    out.append(f"  [Set {s_idx:2d}, Way {w_idx}] VPN=0x{e.vpn:07X} -> PFN=0x{e.pfn:05X} | ASID={e.asid} | Perm={perm} | {d}")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_page_table_tree(vm: VirtualMemorySubsystem, asid: int = 0) -> str:
        """Renders ASCII Radix Page Table Tree."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|                MULTI-LEVEL PAGE TABLE TREE (ASID: {asid})                     |",
            f"+--------------------------------------------------------------------------+",
        ]
        if asid not in vm.page_tables:
            out.append("  (No page table allocated for ASID)")
            return "\n".join(out)

        root = vm.page_tables[asid]
        if vm.pt_levels == 4:
            out.append("  CR3 Root (PML4)")
            for pml4_idx, pdp in list(root.items())[:4]:
                out.append(f"    |-- [PML4 idx 0x{pml4_idx:03X}] -> PDP Table")
                if isinstance(pdp, dict):
                    for pdp_idx, pd in list(pdp.items())[:2]:
                        out.append(f"    |    |-- [PDP idx 0x{pdp_idx:03X}] -> PD Table")
                        if isinstance(pd, dict):
                            for pd_idx, pt in list(pd.items())[:2]:
                                out.append(f"    |    |    |-- [PD idx 0x{pd_idx:03X}] -> PT Table")
                                if isinstance(pt, dict):
                                    for pt_idx, pte in list(pt.items())[:3]:
                                        if isinstance(pte, PTE):
                                            p = "P" if pte.present else "NP"
                                            perm = ("R" if pte.read else "-") + ("W" if pte.write else "-") + ("X" if pte.execute else "-")
                                            out.append(f"    |    |    |    |-- [PT idx 0x{pt_idx:03X}] -> PTE: PFN=0x{pte.pfn:05X} ({p}|{perm}|D={int(pte.dirty)})")
        elif vm.pt_levels == 2:
            out.append("  Page Directory Root")
            for dir_idx, pt in list(root.items())[:6]:
                out.append(f"    |-- [Dir idx 0x{dir_idx:03X}] -> Page Table")
                if isinstance(pt, dict):
                    for pt_idx, pte in list(pt.items())[:4]:
                        if isinstance(pte, PTE):
                            p = "P" if pte.present else "NP"
                            perm = ("R" if pte.read else "-") + ("W" if pte.write else "-") + ("X" if pte.execute else "-")
                            out.append(f"    |    |-- [PT idx 0x{pt_idx:03X}] -> PTE: PFN=0x{pte.pfn:05X} ({p}|{perm}|D={int(pte.dirty)})")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_virtual_address_map(vm: VirtualMemorySubsystem, asid: int = 0) -> str:
        """ASCII Virtual Address Space memory layout."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|             VIRTUAL ADDRESS SPACE SEGMENT MAP (ASID: {asid})                 |",
            f"+--------------------------------------------------------------------------+",
            f"  [0x000FFFFC]  +-------------------------------------+",
            f"                | Stack Origin / Base (RW-)           |",
            f"                | (Grows downwards toward limit)      |",
            f"  [0x00080000]  +-------------------------------------+",
            f"  [0x0007FFC0]  | GUARD PAGE BAND (NO ACCESS, 64 B)   | <-- Overflow Barrier",
            f"  [0x00070000]  +-------------------------------------+",
            f"                | Heap Segment (RW-)                  |",
            f"  [0x00020000]  +-------------------------------------+",
            f"                | Data Segment (.data / .bss, RW-)    |",
            f"  [0x00000000]  +-------------------------------------+",
            f"                | Code Segment (.text, R-X)           |",
            f"                +-------------------------------------+",
            f"+--------------------------------------------------------------------------+",
        ]
        return "\n".join(out)

    @staticmethod
    def render_physical_frame_map(vm: VirtualMemorySubsystem) -> str:
        """Renders physical memory frame allocation grid with owners."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"| PHYSICAL FRAME ALLOCATION MAP ({vm.total_frames} frames of {vm.page_size // 1024} KB each)             |",
            f"+--------------------------------------------------------------------------+",
            "  Frame | State       | Owner ASID | Mapped VPN  | Physical Base Address",
            "  ------+-------------+------------+-------------+----------------------"
        ]
        for f in range(min(32, vm.total_frames)):
            if f in vm.frame_to_vpn:
                asid, vpn = vm.frame_to_vpn[f]
                state = "ALLOCATED"
                pbase = f * vm.page_size
                out.append(f"  {f:4d}  | {state:11s} |    {asid:2d}      | 0x{vpn:07X}  | 0x{pbase:08X}")
            else:
                out.append(f"  {f:4d}  | FREE        |    --      |    --       | 0x{(f * vm.page_size):08X}")
        if vm.total_frames > 32:
            out.append(f"  ... ({vm.total_frames - 32} additional physical frames omitted) ...")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_pipeline_stalls(stage_names: List[str], instructions: List[str], stalls: List[str], cycle: int) -> str:
        """Renders ASCII pipeline diagram marking translation and memory stalls."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|        CYCLE-BY-CYCLE PIPELINE DIAGRAM (Clock Cycle: {cycle:6d})              |",
            f"+--------------------------------------------------------------------------+",
        ]
        for name, inst, stall in zip(stage_names, instructions, stalls):
            if stall:
                status = f"[STALL: {stall:15s}]"
            elif inst:
                status = f"{inst:25s}"
            else:
                status = "-- BUBBLE / NOP --"
            out.append(f"  [{name:6s}] : {status}")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_ascii_timing_diagram(trace: List[Dict[str, Any]], max_cycles: int = 25) -> str:
        """
        Renders a textbook-style ASCII Pipeline Timing Diagram:
        Cycle along the top, stages per cycle, with forwarding event annotations.
        """
        if not trace:
            return "No pipeline trace history available."
        recent = trace[-max_cycles:]
        start_cyc = recent[0]["cycle"]
        end_cyc = recent[-1]["cycle"]

        out = [
            f"+--------------------------------------------------------------------------+",
            f"| ASCII PIPELINE TIMING DIAGRAM (Cycles {start_cyc} .. {end_cyc})                     |",
            f"+--------------------------------------------------------------------------+",
        ]

        # Header with cycle numbers
        cyc_header = " Stage  |" + "".join([f" {c % 100:2d} " for c in range(start_cyc, end_cyc + 1)])
        out.append(cyc_header)
        out.append(" -------+" + "----" * (end_cyc - start_cyc + 1))

        # Distinct stages in trace
        sample_stages = list(recent[0].get("stages", {}).keys())
        for stage in sample_stages:
            row = f" {stage:<6s} |"
            for t in recent:
                st_info = t.get("stages", {}).get(stage, {})
                inst_text = st_info.get("inst", " ")
                is_stall = st_info.get("stall", False)
                is_bubble = st_info.get("bubble", False)

                if is_stall:
                    cell = " *  "
                elif is_bubble:
                    cell = " -  "
                elif inst_text and inst_text != "nop":
                    short_op = inst_text.split()[0][:3]
                    cell = f"{short_op:3s} "
                else:
                    cell = " .  "
                row += cell
            out.append(row)

        out.append(" -------+" + "----" * (end_cyc - start_cyc + 1))
        # Forwarding & Stall Events in recent cycles
        fwd_events_found = []
        for t in recent:
            cyc = t["cycle"]
            fwd = t.get("forwarding", [])
            stall_code = t.get("stall_reason")
            if fwd:
                fwd_events_found.append(f"  Cyc {cyc:3d}: Bypass -> {', '.join(fwd)}")
            if stall_code:
                fwd_events_found.append(f"  Cyc {cyc:3d}: Stall  -> [{stall_code}]")

        if fwd_events_found:
            out.append(" Live Pipeline Telemetry Events:")
            out.extend(fwd_events_found[-10:])
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_hazard_summary(perf: Any) -> str:
        """Renders comprehensive hazard and stall breakdown table."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|                     HAZARD & STALL ACCOUNTING SUMMARY                    |",
            f"+--------------------------------------------------------------------------+",
            f"  RAW Hazards Detected   : {perf.raw_detected_count:5d} | Hazards Bypassed (FWD) : {perf.hazards_avoided_by_forwarding:5d}",
            f"  WAR Hazards Detected   : {perf.war_detected_count:5d} | WAW Hazards Detected  : {perf.waw_detected_count:5d}",
            f"+--------------------------------------------------------------------------+",
            "  Stall Reason Code         | Cycles Stalled | % of Total Stalls",
            "  --------------------------+----------------+------------------"
        ]
        total = perf.total_stalls()
        for reason, count in sorted(perf.stall_cycles.items(), key=lambda x: x[1], reverse=True):
            if count > 0:
                pct = (count / total * 100.0) if total > 0 else 0.0
                out.append(f"  {reason:<25s} | {count:14d} | {pct:>6.1f}%")
        out.append("  --------------------------+----------------+------------------")
        out.append(f"  TOTAL STALL CYCLES        | {total:14d} | 100.0%")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_forwarding_network_state(fwd_unit: Any, perf: Any) -> str:
        """Renders the bypass multiplexer topology and active usage."""
        out = [
            f"+--------------------------------------------------------------------------+",
            f"|          DATA FORWARDING NETWORK CONFIGURATION & UTILIZATION             |",
            f"+--------------------------------------------------------------------------+",
            f"  Active Forwarding Mode   : {fwd_unit.mode.upper()}",
            "  -------------------------------------------------------------------------",
            "  Bypass Path Identifier   | Status | Usage Count | Source -> Destination",
            "  -------------------------+--------+-------------+------------------------"
        ]
        descriptions = {
            "EX_TO_EX_RS1": "EX stage ALU result -> Next inst EX operand A (rs1)",
            "EX_TO_EX_RS2": "EX stage ALU result -> Next inst EX operand B (rs2)",
            "MEM_TO_EX_RS1": "MEM/WB stage result -> EX operand A (rs1)",
            "MEM_TO_EX_RS2": "MEM/WB stage result -> EX operand B (rs2)",
            "MEM_TO_MEM": "WB stage result -> MEM stage store-data",
            "STORE_DATA_FWD": "EX/MEM result -> Store data register input",
            "STORE_TO_LOAD": "StoreBuffer (Full+) -> Load data without DRAM/cache access"
        }
        for path, enabled in fwd_unit.path_enables.items():
            status = "ON " if enabled else "OFF"
            count = perf.forwarding_path_usage.get(path, 0)
            desc = descriptions.get(path, "Custom bypass path")
            out.append(f"  {path:<24s} |  {status:3s}   | {count:11d} | {desc}")
        out.append(f"+--------------------------------------------------------------------------+")
        return "\n".join(out)

    @staticmethod
    def render_instruction_binary_view(raw_word: int, pc: int = 0) -> str:
        """
        Displays 32-bit instruction with exact binary string opcode and sliced bitfields.
        """
        from common.isa import word_to_bits, slice_field, decode, INSTRUCTION_TABLE, OPCODE_BITS
        word_bits = word_to_bits(raw_word, 32)
        op_bits  = slice_field(word_bits, 31, 27)
        i_bit    = slice_field(word_bits, 26, 26)
        rd_bits  = slice_field(word_bits, 25, 22)
        rs1_bits = slice_field(word_bits, 21, 18)
        rs2_bits = slice_field(word_bits, 17, 14)
        mod_bits = slice_field(word_bits, 17, 16)
        imm_bits = slice_field(word_bits, 15, 0)

        info = INSTRUCTION_TABLE.get(op_bits)
        mn = info.mnemonic if info else "UNKNOWN"
        try:
            inst = decode(raw_word, pc)
            disasm = inst.disassembly
        except Exception:
            disasm = f"<illegal: {op_bits}>"

        out = [
            f"+--------------------------------------------------------------------------+",
            f"| INSTRUCTION BINARY ENCODING VIEW (PC: 0x{pc:08X} | Word: 0x{raw_word:08X})       |",
            f"+--------------------------------------------------------------------------+",
            f"  Full 32-bit Binary : {word_bits[0:8]} {word_bits[8:16]} {word_bits[16:24]} {word_bits[24:32]}",
            f"  Sliced Bitfields   :",
            f"    - [31:27] Opcode   (5b) : '{op_bits}' -> Mnemonic: {mn.upper()}",
            f"    - [26:26] Imm Flag (1b) : '{i_bit}'   -> {'Immediate Operand' if i_bit=='1' else 'Register Operand'}",
            f"    - [25:22] Rd Reg   (4b) : '{rd_bits}' -> r{int(rd_bits, 2)}",
            f"    - [21:18] Rs1 Reg  (4b) : '{rs1_bits}' -> r{int(rs1_bits, 2)}",
            f"    - [17:14] Rs2 Reg  (4b) : '{rs2_bits}' -> r{int(rs2_bits, 2)} (when I=0)",
            f"    - [17:16] Mod Flag (2b) : '{mod_bits}' -> Modifier {int(mod_bits, 2)}",
            f"    - [15:00] Imm16    (16b): '{imm_bits}' -> #{int(imm_bits, 2)} (when I=1)",
            f"  Disassembly Output : {disasm}",
            f"+--------------------------------------------------------------------------+"
        ]
        return "\n".join(out)
