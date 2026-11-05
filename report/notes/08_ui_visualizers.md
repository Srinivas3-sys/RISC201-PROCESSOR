# Module Notes: Debugger REPL, Visualizers, and UI Interface

## 1. Interactive Debugger REPL (`debugger/Debugger.hpp`, `debugger/main_sim.cpp`)
* **Architecture**: Standalone command-line debugger engine operating over all three CPU models (4-stage pipeline, 6-stage pipeline, and microcoded controller).
* **Launch Options**:
  ```bash
  ./risc201_sim [assembly.s] [-i|--interactive] [--pipeline 4|6] [--mem-preset ideal|simple|modern|modern-ddr5] [--forwarding none|basic|full|full+] [--branch-policy stall|flush|1bit|2bit|gshare]
  ```
* **Startup Assembly Prompt (Contributed by Varshith Teja)**:
  * When launched without an input file or in interactive mode, prompts the user to type custom SimpleRisc assembly code directly into the terminal.
  * Dynamically assembles the input in memory, loads it into address 0x00000000, runs the pipeline, and prints the execution trace automatically!

---

## 2. Complete Interactive Debugger Command Reference

| Command | Aliases | Description |
| :--- | :--- | :--- |
| `run` | `r` | Runs program to completion or re-runs from cycle 0. |
| `continue` | `c` | Continues execution from current cycle until next breakpoint or halt. |
| `step [N]` | `s [N]` | Advances processor execution by $N$ clock cycles (or 1 cycle). |
| `step_inst` | `si` | Advances processor execution by 1 retired instruction. |
| `trace` | - | Displays full multi-cycle execution matrix & timing diagram (Varshith's contribution). |
| `asm` | `code` | Opens interactive prompt to enter and execute new assembly code. |
| `regs` | - | Dumps general-purpose registers ($r_0 - r_{15}$), condition flags ($E, GT$), and PC. |
| `pipeline` | `p` | Displays current pipeline stage latches, instructions, active hazards, and forwarding paths. |
| `hazards` | - | Displays cumulative data, structural, and control hazard telemetry. |
| `fwd` | - | Displays data forwarding bypass multiplexer activations. |
| `cpi` | - | Displays numeric CPI breakdown and ASCII bar chart. |
| `cpistack [steady\|cold]`| - | Renders steady-state vs cold-start CPI stack breakdown. |
| `compare` | - | Performs side-by-side execution comparison between 4-stage and 6-stage pipelines. |
| `explain [cycle]`| - | Explains root cause, stall latency, and hazard category for a specific cycle. |
| `mem` | `memory` | Summarizes cache/TLB hit rates, MSHR depths, and memory access records. |
| `cache` | - | Renders graphical ASCII set-way grid and valid/dirty line status for L1I, L1D, L2, L3. |
| `dram` | - | Renders DRAM channel, rank, bank group, and bank buffer states. |
| `tlb` | - | Displays L1 ITLB, L1 DTLB, and L2 STLB cached virtual-to-physical translations. |
| `amat` | - | Displays Effective Access Time (EAT) and Average Memory Access Time (AMAT). |
| `stack` | - | Renders ASCII active stack frame diagram with frame pointers and boundaries. |
| `micro` | - | Displays microPC, micro-operation mnemonic, and active control signals. |
| `mode <4\|6\|micro>`| - | Switches active simulation model between 4-stage, 6-stage, and microcode. |
| `preset <name>` | - | Changes memory preset (`ideal`, `simple`, `modern`, `modern-ddr5`). |
| `bp_policy <p>` | - | Switches branch predictor policy (`stall`, `flush`, `1bit`, `2bit`, `gshare`). |
| `div <algo>` | - | Switches division hardware algorithm (`rest`, `nonrest`, `newton`). |
| `stats` | - | Prints complete architectural performance counter report. |
| `help` | `h` | Displays complete command help menu. |
| `quit` | `q`, `exit` | Exits simulator. |

---

## 3. Dedicated ASCII Visualizers (`visualizer/`)

### 3.1 Pipeline Visualizer (`visualizer/PipelineVisualizer.hpp`)
* **Stage Latch Renderer**: Prints box diagrams for active instructions in each latch:
  ```
  [ IF: 0x0010 add r1, r2, r3 ] ---> [ OF: 0x000C ld r2, 0[r4] ] ---> [ EX: 0x0008 sub r5, r6, 1 ] ---> [ MA_RW: 0x0004 mov r4, 10 ]
  ```
* **Timing Trace Renderer**: Renders horizontal cycle-by-cycle execution matrix showing instructions moving through stages across consecutive clock cycles.
* **Hazard Overlay**: Labels bubbles with exact stall reasons (`[STALL: RAW r2 from EX]`, `[BUBBLE: LOAD-USE]`, `[FLUSH: BRANCH MISPREDICT]`).

### 3.2 Memory Visualizer (`visualizer/MemoryVisualizer.hpp`)
* **Set-Way Cache Grid**: Renders ASCII occupancy matrices showing tag values, dirty/clean status (`[D]`/`[C]`), and replacement age across all ways.
* **Telemetry Bars**: Hit rate percentages, MSHR queue occupancy, and memory controller request buffer depth.

### 3.3 Stack Visualizer (`visualizer/StackVisualizer.hpp`)
* **Stack Frame Display**: Visualizes the memory layout of the full-descending stack:
  * Frame base ($r_{14} + \text{frameSize}$)
  * Local variables inside active activation record
  * Saved return address ($r_{15}$ / RA)
  * Current Stack Pointer ($r_{14}$ / SP)
  * Guard-page boundary line (raises visual warning on stack overflow or underflow).

