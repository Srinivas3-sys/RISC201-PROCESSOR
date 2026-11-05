# Master Presentation Guide & Live Demo Scripts

This document provides everything needed for a flawless project presentation: slide outlines, spoken scripts (2-min, 5-min, 15-min), a step-by-step live demo script with exact console commands and outputs, and teammate role splitting.

---

## 1. Slide-by-Slide Outline (22 Slides)

| Slide | Title | Key Talking Points | Visual to Display |
| :---: | :--- | :--- | :--- |
| **1** | **Title Slide** | RISC201-PROCESSOR: Cycle-Accurate Pipeline Simulation, Microcode Control, and Empirical Memory Hierarchy Evaluation. | Project logo, course name, team members (`bandlapalliyss-max` and `Varshith Teja`). |
| **2** | **Project Overview** | 100% modern C++17 simulator (12,929 LOC) with zero external dependencies, modeling textbook SimpleRisc ISA. | High-level system architecture flowchart (`00_project_map.md`). |
| **3** | **The Canonical ISA** | 25 instructions; single source of truth in `common/ISA.hpp`; 5-bit string opcodes; zero integer opcode enums. | Table of 25 opcodes + 4 regular formats (R, I, M, B). |
| **4** | **Software Toolchain** | Two-pass assembler (`risc201_as`) and disassembler (`risc201_disasm`) with 100% round-trip fidelity. | Diagram of Pass 1 (symbols) $\rightarrow$ Pass 2 (machine code). |
| **5** | **The Processors** | 4-Stage (`IF->OF->EX->MA_RW`) vs 6-Stage (`IF->ID->RF->EX->MA->WB`). | Dual pipeline datapath ASCII diagrams. |
| **6** | **Why 6-Stage Wins** | Splitting `OF` into `ID` and `RF` cuts logic delay, raising clock from 500 MHz to 800 MHz (**1.59x speedup**!). | Frequency scaling graph & wall-clock execution time formula. |
| **7** | **The Golden ISS** | Single-cycle reference model in `common/ISS.hpp` verifying 100% behavioral equivalence across pipelines. | Differential testing block diagram (P4 vs P6 vs ISS). |
| **8** | **Hazard Mitigation** | RAW data hazards, load-use delays, and control hazards. | RAW dependency timeline diagram. |
| **9** | **Data Forwarding** | Full forwarding unit (`EX->EX`, `MEM->EX`) eliminating RAW stalls in 0 cycles. | Forwarding bypass multiplexer diagram. |
| **10**| **Branch Prediction** | Static, 1-bit, 2-bit bimodal (93.4% accuracy), and gshare predictors; wrong-path squashing. | 2-bit saturating counter state machine (`00` to `11`). |
| **11**| **Control Unit: Hardwired vs Microcode**| Fixed combinational gates vs control store ROM; trade-offs in speed and flexibility. | Comparison table: Hardwired vs Microprogrammed. |
| **12**| **Microcode: Horizontal vs Vertical**| 32-bit unencoded horizontal words vs 16-bit encoded vertical words (50% ROM reduction). | Word geometry bit diagrams. |
| **13**| **Optimized ALU** | 25 operations; configurable Adders (CLA, CSLA, RCA), Multipliers (Booth, Wallace), Dividers (Newton-Raphson). | ALU block diagram showing operational sub-units. |
| **14**| **Memory Hierarchy** | Split L1 (Harvard) eliminates structural hazards; unified L2/L3 (Von Neumann) shares capacity. | 5-level memory hierarchy pyramid (L1 $\rightarrow$ L2 $\rightarrow$ L3 $\rightarrow$ DRAM). |
| **15**| **Cycle-Accurate DRAM** | DDR4-3200 & DDR5-4800; channels, ranks, bank groups, row buffer hits vs conflicts, FR-FCFS scheduler. | DRAM channel and bank organization diagram. |
| **16**| **Radix-4 Virtual Memory & TLBs** | 48-bit translation (PML4 $\rightarrow$ PDPT $\rightarrow$ PD $\rightarrow$ PT); multi-tier TLBs; Page-Walk Cache (74% latency cut). | 4-level Radix page-walk traversal flowchart. |
| **17**| **Stack Safety & Guard Pages** | Full-descending stack (`0x000FFFFC`); Guard Page detects overflow before memory corruption. | Stack memory diagram showing Guard Page boundary. |
| **18**| **Interactive Terminal Visualizers**| Live ASCII pipeline trace with forwarding arrows, cache set-way occupancy, and stack frames (Varshith Teja). | Terminal screenshots of `pipe`, `cache`, `stack`. |
| **19**| **Empirical CPI Decomposition** | Resolution of the 2.560 CPI discrepancy: cold-start compulsory misses vs 1.12 steady-state CPI. | CPI Stack stacked bar chart. |
| **20**| **Benchmark Evaluation** | 12 assembly workloads; performance scaling on arrays, recursion, primes, and matrix multiplication. | Performance comparison table across all 12 benchmarks. |
| **21**| **Verification & Quality** | 78/78 unit tests pass, 231/231 golden checks pass, zero leaks under AddressSanitizer. | Test summary terminal banner. |
| **22**| **Conclusion & Summary** | Completed all 13 course requirements; delivered cycle-accurate C++ simulation matching physical architecture. | Final recap bullet points. |

---

## 2. Spoken Scripts

### A. The 2-Minute Elevator Pitch
> "Good morning, professors. Our project is **RISC201-PROCESSOR**, a cycle-accurate modern computer systems simulator built entirely in 100% modern C++17.
> 
> We modeled the 32-bit SimpleRisc architecture from Smruti Sarangi's textbook, implementing both a **4-stage pipeline** and a high-frequency **6-stage pipeline**. We solved RAW data hazards using a comprehensive forwarding unit that routes results with zero stall cycles, and we implemented 2-bit bimodal and gshare branch predictors to eliminate control stalls.
> 
> Beyond the processor core, we implemented a full **modern memory hierarchy**: split L1 Harvard caches to prevent structural hazards, non-blocking MSHRs, a cycle-accurate DDR4/DDR5 DRAM controller with FR-FCFS scheduling, and a 4-level Radix-4 Virtual Memory subsystem with TLBs and a Page-Walk Cache that slashes walk overhead by 74%.
> 
> We also verified every claim empirically: all 78 unit tests and 231 golden regression checks pass with 100% success, verified completely leak-free under AddressSanitizer. We are ready to demonstrate the live simulator!"

---

### B. The 5-Minute Standard Presentation
> "Good morning. Today we present our processor design project: **RISC201-PROCESSOR**.
> 
> **Architecture & ISA**:
> We established a strict architectural foundation by making `common/ISA.hpp` the single source of truth for all 25 instruction opcodes. Every opcode is represented as a canonical 5-bit binary string, completely eliminating duplicate integer enums and matching physical hardware bit lines.
> 
> **The Two Pipeline Implementations**:
> We designed and compared two distinct pipeline cores:
> 1. A balanced **4-stage pipeline** (`IF -> OF -> EX -> MA_RW`) running at 500 MHz.
> 2. An optimized **6-stage pipeline** (`IF -> ID -> RF -> EX -> MA -> WB`) running at 800 MHz.
> 
> By splitting the operand fetch into separate Instruction Decode and Register Read cycles, we halved the critical path logic delay. Although the deeper pipeline has a slightly higher branch penalty, its higher clock frequency delivers a **1.59x wall-clock speedup** on real workloads.
> 
> **Hazard Mitigation & Control**:
> ALU-to-ALU dependencies are bypassed in zero cycles via our `EX->EX` and `MEM->EX` forwarding paths. Only load-use dependencies require a single bubble stall. For branches, our 2-bit saturating counter achieves 93.4% accuracy. We also implemented both Horizontal (32-bit) and Vertical (16-bit) microcode control stores, showing that vertical microcode reduces control ROM size by 50% with minimal decoder overhead.
> 
> **Memory & Safety**:
> Our memory subsystem features a Split L1 Harvard cache to allow simultaneous instruction fetch and data memory access without structural stalls. Backing it is a cycle-accurate DDR4 DRAM controller and a 48-bit Radix-4 Virtual Memory subsystem. We enforce stack safety via a memory-mapped Guard Page beneath our full-descending stack, trapping recursive stack overflow before memory corruption can occur.
> 
> All 78 automated tests and 231 golden regression checks pass with 100% fidelity. We would now like to run the live demonstration."

---

## 3. The Live Demo Script (Step-by-Step)

Follow these exact terminal commands during your live demonstration:

### Step 1: Demonstrate Assembly & Disassembly Round-Trip
**What to say**: *"First, we will assemble an assembly source file into 32-bit machine code, and disassemble it back to text to verify 100% round-trip fidelity."*
```bash
./risc201_as benchmarks/hazard_test.s -o demo.bin --hex demo.hex
./risc201_disasm demo.bin
```
**Expected Output**: The terminal prints the disassembled assembly matching `benchmarks/hazard_test.s` line for line.

---

### Step 2: Demonstrate 6-Stage Pipeline & Data Forwarding
**What to say**: *"Next, we run the 6-stage pipeline on `hazard_test.s`. You will see our visual ASCII pipeline execution state and our forwarding unit bypassing data across stages."*
```bash
make demo-pipe
```
**Expected Output**:
```
+--------------------------------------------------------------------------+
|               PIPELINE EXECUTION STATE (Clock Cycle:     25)             |
+--------------------------------------------------------------------------+
  [IF    ] : -- BUBBLE / NOP --
  [ID    ] : -- BUBBLE / NOP --
  [RF    ] : -- BUBBLE / NOP --
  [EX    ] : -- BUBBLE / NOP --
  [MA    ] : -- BUBBLE / NOP --
  [RW    ] : nop (PC: 0x00000048)
+--------------------------------------------------------------------------+
========================================================================
                  6-STAGE PROCESSOR REPORT
========================================================================
Total Cycles:               25
Retired Instructions:       17
CPI:                        1.471
Branch Bubbles / Flushes:   2
------------------------------------------------------------------------
CPI STACK BREAKDOWN BY CANONICAL STALL REASON:
RAW_LOAD_USE            1           0.059       33.3%
BRANCH_MISPREDICT       2           0.118       66.7%
========================================================================
```

---

### Step 3: Demonstrate Cache Hierarchy & DRAM Controller
**What to say**: *"Now we run our 4-stage core with the `modern` memory preset, displaying live L1 cache set-way occupancy and DDR4 DRAM bank buffer states."*
```bash
make demo-mem
```
**Expected Output**: Displays 8-way L1D cache hit rate (100.00%), 3C miss counts, and the DDR4 bank state table showing open row buffers.

---

### Step 4: Demonstrate Radix-4 Virtual Memory & TLBs
**What to say**: *"Finally, we run a recursive factorial program on the 6-stage pipeline with 48-bit Radix-4 Virtual Memory and TLBs active."*
```bash
make demo-vm
```
**Expected Output**: Shows 164 execution cycles, recursive function calls, L1D hit rate of 94.12%, and Effective Access Time (EAT) calculation.

---

### Backup Plan (If Anything Goes Wrong During Live Demo)
- **Problem**: Terminal display garbled or window resized too small.
  - **Action**: Type `reset` and press Enter, then rerun the command.
- **Problem**: Binary missing or permission error.
  - **Action**: Run `make clean && make all` (takes only 4 seconds).
- **Problem**: Examiner asks to see source code proof of single source of truth.
  - **Action**: Run `./lint_isa` directly in the terminal to show all 43 files passing inspection.

---

## 4. Teammate Role Division Suggestion

Based on commit history:
- **`bandlapalliyss-max` (Srinivas B.)**:
  - Present: Architecture overview, 4-stage vs 6-stage datapath, Forwarding and Hazard Units, Memory Hierarchy (Caches, DRAM, Radix-4 VM), and CPI performance analysis.
- **`Varshith Teja`**:
  - Present: Software toolchain, UI/Visualizers (ASCII pipeline trace, startup prompt), Newton-Raphson division algorithms, CLI branch predictor selection, and execute the live terminal demo.
