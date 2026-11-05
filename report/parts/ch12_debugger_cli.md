# Chapter 12: Simulator, Interactive CLI, and Debugger Environment

## 12.1 Command-Line Executable Reference

The project compiles into 12 standalone executables (`Makefile` target `all`):

### 12.1.1 Main Processor Simulator (`risc201_sim`)
* **Source**: [`debugger/main_sim.cpp`](file:///home/srinivas/RISC201-PROCESSOR/debugger/main_sim.cpp).
* **CLI Syntax**:
  ```bash
  ./risc201_sim [assembly.s] [options]
  ```
* **Supported Command-Line Flags**:
  * `-i`, `--interactive`: Launch directly into interactive debugger REPL mode.
  * `--pipeline <4|6>`: Selects active pipeline core (default: 4-stage).
  * `--mem-preset <ideal|simple|modern|modern-ddr5|lecture>`: Configures memory hierarchy model (default: `modern`).
  * `--forwarding <none|basic|full|full+>`: Configures data forwarding network (default: `full`).
  * `--branch-policy <stall|flush|1bit|2bit|gshare|perfect>`: Selects branch predictor algorithm (default: `2bit`).
  * `--diagram`: Renders horizontal pipeline execution timing diagram upon program completion.
  * `--report`: Emits complete architectural telemetry report upon program completion.
  * `--help`: Displays simulator help menu.

### 12.1.2 Assembler & Disassembler (`risc201_as`, `risc201_disasm`)
* `risc201_as <source.s> -o <out.hex>`: Assembles SimpleRisc assembly into ASCII hex and raw binary (`.bin`).
* `risc201_disasm <source.hex>`: Disassembles machine code back into readable assembly.

### 12.1.3 Evaluation & Verification Tools
* `risc201_eval` (`evaluation/CompareProcessors.cpp`): Evaluates 30 cross-pipeline configurations.
* `risc201_test` (`tests/test_all.cpp`): Unified 78-case verification test suite.
* `audit_cpi` (`tools/audit_cpi.cpp`): Runs full benchmark matrix (192 configs) and generates CSVs.
* `generate_perf_docs` (`tools/generate_perf_docs.cpp`): Updates `docs/PERFORMANCE.md` from actual simulation data.
* `lint_isa` (`tools/lint_isa.cpp`): Verifies single source of truth 5-bit string opcodes across all 43 files.
* `lint_docs` (`tools/lint_docs.cpp`): Validates KaTeX math rendering across all markdown documents.
* `verify_golden` (`tools/verify_golden.cpp`): Validates 231 golden baselines with 100% byte fidelity.

---

## 12.2 Interactive REPL Commands & Output Demonstrations

When launched with `-i`, the simulator enters the interactive debugger console.

### 12.2.1 Cycle Stepping & Latch Inspection (`step`, `pipeline`)
```
RISC201[4-Stage]> step
Cycle 1 completed (4-Stage).
[PIPELINE STAGE LATCHES]
  IF    : 0x00000000 | mov r1, 5
  OF    : [EMPTY / BUBBLE]
  EX    : [EMPTY / BUBBLE]
  MA_RW : [EMPTY / BUBBLE]

RISC201[4-Stage]> step 3
Cycle 4 completed (4-Stage).
[PIPELINE STAGE LATCHES]
  IF    : 0x0000000C | beq label_0024
  OF    : 0x00000008 | cmp r1, 0
  EX    : 0x00000004 | mov r2, 1
  MA_RW : 0x00000000 | mov r1, 5  (Retiring!)
```

### 12.2.2 Register File Dumps (`regs`)
```
RISC201[4-Stage]> regs
============================= REGISTER FILE =============================
  r0  : 0x00000000 (      0) | r8  : 0x00000000 (      0)
  r1  : 0x00000005 (      5) | r9  : 0x00000000 (      0)
  r2  : 0x00000001 (      1) | r10 : 0x00000000 (      0)
  r3  : 0x00000000 (      0) | r11 : 0x00000000 (      0)
  r4  : 0x00000000 (      0) | r12 : 0x00000000 (      0)
  r5  : 0x00000000 (      0) | r13 : 0x00000000 (      0)
  r6  : 0x00000000 (      0) | r14 : 0x7FFFFFFF (SP, Stack Pointer)
  r7  : 0x00000000 (      0) | r15 : 0x00000000 (RA, Return Address)
-------------------------------------------------------------------------
  PC  : 0x00000010           | Flags: [ E: 0, GT: 1 ]
=========================================================================
```

### 12.2.3 CPI Breakdown Display (`cpi`, `cpistack`)
```
RISC201[4-Stage]> cpi
========================= CPI STACK BREAKDOWN =========================
  Retired Instructions : 168044
  Total Cycles         : 189050
  Total CPI            : 1.125 (Steady-State CPI: 1.125)

  Stall Component                 Cycles        CPI Penalty   Fraction
  ---------------------------------------------------------------------
  Base Pipeline Execution       : 168044        1.0000        88.89%
  RAW Data Dependency Stalls    :  20000        0.1190        10.58%
  Branch Misprediction Bubbles  :   1003        0.0060         0.53%
  Memory Structural Stalls      :      3        0.0000         0.00%
  Cache Line Refill Stalls      :      0        0.0000         0.00%
=======================================================================
```

---

## 12.3 Visualizer Modules & UI Components

### 12.3.1 Pipeline Visualizer (`visualizer/PipelineVisualizer.hpp`)
* Formats cycle history arrays into horizontal timing diagrams.
* Annotates active forwarding paths (`[FWD: EX-to-EX r1]`) and hazard bubbles (`[BUBBLE: LOAD-USE]`).

### 12.3.2 Memory Visualizer (`visualizer/MemoryVisualizer.hpp`)
* **Set-Way ASCII Matrix**:
```
RISC201[4-Stage]> cache
========================== L1D CACHE OCCUPANCY ==========================
Set 00: [Way 0: Valid, Clean, Tag=0x000010] [Way 1: Valid, Dirty, Tag=0x000020]
Set 01: [Way 0: Valid, Clean, Tag=0x000010] [Way 1: Free]
...
L1D Hit Rate: 99.85% (167800 Hits / 168044 Accesses) | MSHR Depth: 0/16
=========================================================================
```

### 12.3.3 Varshith Teja's UI Contributions
In commit `bbd9ea1`, Varshith Teja contributed key UI enhancements:
1. **Interactive Startup Assembly Prompt (`promptStartupAssembly()`)**:
   * Prompts the user on simulator launch to enter custom assembly code line by line with automatic assembly and immediate execution.
2. **Cycle Trace Visualizer (`PipelineVisualizer::renderTrace()`, `showTrace()`)**:
   * Produces the multi-cycle execution matrix showing instructions moving through stages across time.
3. **Branch Predictor CLI Flag Selection**:
   * Added CLI arguments and REPL command `bp_policy` allowing dynamic switching between predictors.
4. **Interactive Division Switch**:
   * Added REPL command `div` enabling real-time toggling between Restoring, Non-Restoring, and Newton-Raphson division algorithms.

