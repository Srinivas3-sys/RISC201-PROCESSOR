# RISC201-PROCESSOR: Complete Project Architecture Map & Inventory

**Course**: CSC-201 Computer Organization & Architecture  
**Assignment**: Processor Design  
**Branch**: `study-notes`  
**Latest Main Commit**: `52da53f` (Thu Nov 5 2026)  
**Contributors**: 
- `bandlapalliyss-max` (Srinivas B.): 26 commits — Architecture lead, C++17 conversion, 4-stage and 6-stage pipelines, hazard/forwarding units, memory hierarchy (caches, DRAM, VM/TLBs), microprogrammed controllers, two-pass assembler, verification test suites, CPI accounting.
- `Varshith Teja`: 1 commit (`bbd9ea1`) — Visual ASCII pipeline trace visualizer, startup assembly prompt, Newton-Raphson division, CLI branch predictor flags, XOR and ABS opcodes.

---

## 1. The Project in One Picture

Here is how an assembly program moves through every layer of this repository:

```
                          [ Assembly Source Code (.s) ]
                           (e.g., benchmarks/hazard_test.s)
                                        |
                                        v
                    +---------------------------------------+
                    |        TWO-PASS ASSEMBLER             |
                    |      (assembler/Assembler.hpp)        |
                    |  Pass 1: Symbol Table & Labels        |
                    |  Pass 2: Binary 32-bit Machine Code   |
                    +-------------------+-------------------+
                                        |
                 +----------------------+----------------------+
                 | 32-bit Binary Words (.bin / .hex)           |
                 v                                             v
+------------------------------------+   +------------------------------------+
|       GOLDEN REFERENCE ISS         |   |     INTERACTIVE CLI / DEBUGGER     |
|         (common/ISS.hpp)           |   |       (debugger/Debugger.hpp)      |
| Single-cycle instruction-by-       |   | Step cycles, set breakpoints,      |
| instruction verification baseline  |   | inspect memory/registers/stack     |
+-----------------+------------------+   +-----------------+------------------+
                  |                                        |
                  +--------------------+-------------------+
                                       |
                                       v
                     +-----------------------------------+
                     |      SELECT EXECUTION CORE        |
                     +-----------------+-----------------+
                                       |
            +--------------------------+--------------------------+
            |                                                     |
            v                                                     v
+-------------------------------+             +-------------------------------+
|     4-STAGE PIPELINE CORE     |             |     6-STAGE PIPELINE CORE     |
| (processor_4stage/            |             | (processor_6stage/            |
|  Pipeline4Stage.hpp)          |             |  Pipeline6Stage.hpp)          |
| IF -> OF -> EX -> MA_RW       |             | IF -> ID -> RF -> EX -> MA -> WB |
+---------------+---------------+             +---------------+---------------+
                |                                             |
                +----------------------+----------------------+
                                       |
       +-------------------------------+-------------------------------+
       |                               |                               |
       v                               v                               v
+--------------+               +---------------+               +---------------+
| HAZARD UNIT  |               | FORWARDING    |               | BRANCH        |
| Detects RAW, |               | UNIT          |               | PREDICTORS    |
| load-use, and|               | Bypasses data |               | Static, 1-bit,|
| injects      |               | EX->EX, MEM->EX|              | 2-bit bimodal,|
| bubbles/stalls               | without stalls|               | gshare, BTB   |
+--------------+               +---------------+               +---------------+
       |                               |                               |
       +-------------------------------+-------------------------------+
                                       |
                                       v
                    +-------------------------------------+
                    |       EXECUTION DATAPATH            |
                    |  - Register File (r0-r15, sp, ra)   |
                    |  - Full ALU (alu/ALU.hpp):          |
                    |    Adders (CLA, CSLA, RCA)          |
                    |    Multipliers (Booth Radix-4,      |
                    |                 Wallace tree)       |
                    |    Dividers (Non-restoring,         |
                    |              Newton-Raphson)        |
                    |  - Microcoded Controller (Optional) |
                    |    Horizontal & Vertical Stores     |
                    +------------------+------------------+
                                       |
                                       v
                    +-------------------------------------+
                    |     MODERN MEMORY HIERARCHY         |
                    |   (memory/MemoryHierarchy.hpp)      |
                    +------------------+------------------+
                                       |
                 +---------------------+---------------------+
                 | Virtual PC                                | Virtual Address (Data)
                 v                                           v
       +-------------------+                       +-------------------+
       |      L1 ITLB      |                       |      L1 DTLB      |
       | (64-entry, 4-way) |                       | (64-entry, 4-way) |
       +---------+---------+                       +---------+---------+
                 | Miss                                      | Miss
                 +---------------------+---------------------+
                                       |
                                       v
                             +-------------------+
                             |      L2 STLB      |
                             | (1024-entry, 8-way|
                             +---------+---------+
                                       | Miss
                                       v
                             +-------------------+
                             |  Page-Walk Cache  |
                             |      (PWC)        |
                             +---------+---------+
                                       | Miss / Page Walk
                                       v
                             +-------------------+
                             |  4-Level Radix    |
                             |  Page Table / SSD |
                             +---------+---------+
                 +---------------------+
                 | Physical Address (PA)
                 v
       +-------------------+                       +-------------------+
       |     L1I Cache     |                       |     L1D Cache     |
       |  (32 KB, 8-way)   |                       |  (32 KB, 8-way)   |
       +---------+---------+                       +---------+---------+
                 | Line Miss                                 | Line Miss
                 +---------------------+---------------------+
                                       |
                                       v
                             +-------------------+
                             |     L2 Cache      |
                             |  (512 KB, 8-way)  |
                             +---------+---------+
                                       | Line Miss
                                       v
                             +-------------------+
                             |     L3 Cache      |
                             |  (8 MB, 16-way)   |
                             +---------+---------+
                                       | Line Miss (64 Bytes)
                                       v
                             +-------------------+
                             | DRAM Controller   |
                             | (FR-FCFS Schedu-  |
                             |  ler, DDR4/DDR5)  |
                             +---------+---------+
                                       |
                                       v
                             +-------------------+
                             |  Physical DRAM    |
                             |  Channels & Banks |
                             +-------------------+
                                       |
                                       v
                    +-------------------------------------+
                    |        TELEMETRY & VISUALIZERS      |
                    |  - ASCII Pipeline Trace & Latches   |
                    |  - Memory Visualizer (Cache Grids)  |
                    |  - ASCII Stack Visualizer (SP, FP)  |
                    |  - CPI Stack Decomposition          |
                    |  - Empirical Performance Reports    |
                    +-------------------------------------+
```

---

## 2. Complete File Inventory & Module Dependencies

The project consists of **74 files** totaling **16,583 lines** (including documentation, benchmarks, and data), with **12,929 lines of pure modern C++17**.

### Directory Inventory

| Directory | Purpose in Plain Words | Files | C++ Lines |
| :--- | :--- | :---: | :---: |
| **`common/`** | Core definitions shared by all modules: canonical 5-bit string ISA, Register File, Types, Hazard/Forwarding Units, Branch Predictors, Golden ISS, Exception definitions. | 10 | 1,749 |
| **`alu/`** | Arithmetic Logic Unit: Adders (RCA, CSLA, CLA), Multipliers (Booth, Wallace), Dividers (Restoring, Non-restoring, Newton-Raphson), Shifter, and Logic Gates. | 6 | 986 |
| **`processor_4stage/`** | 4-Stage pipelined CPU core (`IF -> OF -> EX -> MA_RW`) with inter-stage latch registers and hazard squashing. | 3 | 754 |
| **`processor_6stage/`** | 6-Stage pipelined CPU core (`IF -> ID -> RF -> EX -> MA -> WB`) splitting decode and register read to allow 800 MHz clocking. | 3 | 895 |
| **`microcode/`** | Microprogrammed Control Unit: Horizontal and Vertical control stores, microinstructions, dispatch ROM, and step-by-step microPC stepping. | 4 | 938 |
| **`assembler/`** | Two-pass assembler, macro preprocessor (PUSH, POP, CALL, RET), disassembler, CLI utilities `risc201_as` and `risc201_disasm`. | 5 | 1,482 |
| **`memory/`** | Modern memory hierarchy: L1I, L1D, L2, L3 caches, MSHRs, stream prefetcher, Radix-4 Virtual Memory, L1/L2 TLBs, PWC, DDR4/DDR5 DRAM controller. | 7 | 2,246 |
| **`debugger/`** | Interactive CLI REPL simulator `risc201_sim`, breakpoint manager, step commands, pipeline mode switcher. | 2 | 1,228 |
| **`visualizer/`** | ASCII visualizers: Pipeline timing diagrams, live latches with forwarding arrows, cache set-way ASCII grids, DRAM bank buffers, full-descending stack display. | 3 | 794 |
| **`evaluation/`** | Empirical evaluation drivers: `CompareProcessors.cpp`, pipeline experiments, memory experiments, and automated benchmarks. | 4 | 1,024 |
| **`tools/`** | Verification and audit tools: `audit_cpi.cpp`, `verify_golden.cpp`, `lint_isa.cpp`, `lint_docs.cpp`, `generate_perf_docs.cpp`. | 6 | 833 |
| **`tests/`** | Unified test suite `test_all.cpp` (78 unit/differential tests) and golden reference binaries/traces. | 1+ | 1,048 |
| **`benchmarks/`** | 6 baseline assembly benchmarks + 6 extended steady-state benchmarks (`*_long.s`) with assembled `.hex` and `.bin` artifacts. | 18 | ~1,200 |
| **`experiments/`** | 30 CSV output datasets across pipeline and memory sweeps + markdown summaries. | 40+ | ~2,500 |
| **`docs/`** | Technical specifications and audit reports. | 10 | ~3,500 |

---

### Detailed File-by-File Breakdown & `#include` Graph

```
                                 [ common/ISA.hpp ]
                               (Single Source of Truth)
                                         |
     +-------------------+---------------+-------------------+
     |                   |                                   |
     v                   v                                   v
[ common/Types.hpp ] [ common/BranchPredictor.hpp ] [ common/ForwardingUnit.hpp ]
     |                                                       |
     +-------------------+                                   v
     |                   |                          [ common/HazardUnit.hpp ]
     v                   v
[ common/RegisterFile.hpp ] [ common/Memory.hpp ]
     |                             |
     +--------------+--------------+
                    |
                    v
            [ alu/LogicGates.hpp ]
                    |
            [ alu/Adders.hpp ]
                    |
     +--------------+--------------+
     |              |              |
     v              v              v
[ alu/Multipliers.hpp ] [ alu/Dividers.hpp ] [ alu/Shifter.hpp ]
     |              |              |
     +--------------+--------------+
                    |
                    v
             [ alu/ALU.hpp ]
                    |
    +---------------+---------------+
    |                               |
    v                               v
[ microcode/MicroInstructions.hpp ] [ memory/Config.hpp ]
    |                               |
    v                               v
[ microcode/HorizontalControl.hpp ] [ memory/Cache.hpp ] [ memory/VirtualMemory.hpp ]
    |                               |
    v                               v
[ microcode/VerticalControl.hpp ]   [ memory/DRAM.hpp ]  [ memory/Prefetcher.hpp ]
    |                               |
    v                               v
[ microcode/MicroController.hpp ]   [ memory/MemoryHierarchy.hpp ]
    |                               |
    +---------------+---------------+
                    |
                    v
    +---------------+---------------+
    |                               |
    v                               v
[ processor_4stage/Pipeline4Stage.hpp ] [ processor_6stage/Pipeline6Stage.hpp ]
    |                               |
    +---------------+---------------+
                    |
                    v
        [ debugger/Debugger.hpp ]
                    |
        [ debugger/main_sim.cpp ] ===> risc201_sim binary
```

---

## 3. Build & Verification Status (Clean Run)

Every binary compiles from scratch with **zero warnings** using GCC 15.2.0 on Linux x86_64 (`-std=c++17 -Wall -Wextra -O2 -I.`):

```bash
$ make clean && make all
rm -f risc201_sim risc201_as risc201_disasm risc201_eval risc201_test exp_pipeline exp_memory audit_cpi generate_perf_docs lint_docs lint_isa verify_golden *.hex *.bin
g++ -std=c++17 -Wall -Wextra -O2 -I. debugger/main_sim.cpp -o risc201_sim
g++ -std=c++17 -Wall -Wextra -O2 -I. assembler/main_as.cpp -o risc201_as
g++ -std=c++17 -Wall -Wextra -O2 -I. assembler/main_disasm.cpp -o risc201_disasm
g++ -std=c++17 -Wall -Wextra -O2 -I. evaluation/CompareProcessors.cpp -o risc201_eval
g++ -std=c++17 -Wall -Wextra -O2 -I. tests/test_all.cpp -o risc201_test
g++ -std=c++17 -Wall -Wextra -O2 -I. evaluation/run_pipeline_experiments.cpp -o exp_pipeline
g++ -std=c++17 -Wall -Wextra -O2 -I. evaluation/run_memory_experiments.cpp -o exp_memory
g++ -std=c++17 -Wall -Wextra -O2 -I. tools/audit_cpi.cpp -o audit_cpi
g++ -std=c++17 -Wall -Wextra -O2 -I. tools/generate_perf_docs.cpp -o generate_perf_docs
g++ -std=c++17 -Wall -Wextra -O2 -I. tools/lint_docs.cpp -o lint_docs
g++ -std=c++17 -Wall -Wextra -O2 -I. tools/lint_isa.cpp -o lint_isa
g++ -std=c++17 -Wall -Wextra -O2 -I. tools/verify_golden.cpp -o verify_golden
# Build Result: SUCCESS (0 errors, 0 warnings)
```

### Real Test Execution Results

```bash
$ ./risc201_test
========================================================================
           RISC201 C++ EXHAUSTIVE VERIFICATION TEST SUITE               
========================================================================
[TEST] 1. ISA Binary Strings & Encodings...
[TEST] 2. Cache Hierarchy & DRAM...
[TEST] 3. Virtual Memory & TLBs...
[TEST] 4. Hazards, Forwarding, & Branch Prediction...
[TEST] 5. Golden ISS vs 4-Stage & 6-Stage Processors...
[TEST] 6. CPI Invariants, Scheduling, & Micro-kernels...
========================================================================
TEST RESULTS: 78 / 78 PASSED
ALL TESTS PASSED WITH 100% SUCCESS!
========================================================================

$ ./lint_isa
========================================================================
                 RISC201 ISA SINGLE SOURCE OF TRUTH LINTER             
========================================================================
Files checked: 43
[LINT SUCCESS] Zero integer opcodes, zero case OP_, zero BIN_OP_, zero quoted 5-bit literals outside ISA.hpp!
All modules strictly conform to canonical 5-bit string ISA source of truth.

$ ./verify_golden
========================================================================
         RISC201 GOLDEN VERIFICATION & REGRESSION VALIDATOR             
========================================================================
[CHECK 1/4] Verifying Assembler & Disassembler against Golden Baselines...
[CHECK 2/4] Verifying Microcode Control Store against Golden Baselines...
[CHECK 3/4] Verifying Pipeline Simulation Matrix (192 configurations)...
[CHECK 4/4] Verifying Simulator Throughput (Speed Regression Check)...
  Throughput measured: 749013 inst/sec
========================================================================
Verification Checks: 231 / 231 passed.
ALL GOLDEN BASELINES MATCH 100%! ZERO DEVIATIONS IN ARCHITECTURAL BEHAVIOR.
```

---

## 4. Assignment Requirements Traceability Matrix

Every single requirement from the course assignment is fully implemented, verified with automated tests, and interactive in the CLI:

| Req # | Assignment Requirement Description | Primary Implementation File(s) | Verification & Demo Method | Status |
| :---: | :--- | :--- | :--- | :---: |
| **1a** | **Two-pass Assembler & Disassembler** | `assembler/Assembler.hpp`, `assembler/Disassembler.hpp` | `make test`, `tools/verify_golden.cpp` | **Done** |
| **1b** | **Interactive CLI Simulator & Debugger** | `debugger/Debugger.hpp`, `debugger/main_sim.cpp` | `./risc201_sim -i` | **Done** |
| **1c** | **Optimized Arithmetic Logic Unit (ALU)** | `alu/ALU.hpp`, `Adders.hpp`, `Multipliers.hpp`, `Dividers.hpp` | `evaluation/CompareProcessors.cpp` | **Done** |
| **2a** | **Hardwired vs Microprogrammed Control** | `microcode/MicroController.hpp`, `processor_4stage/` | `evaluation/CompareProcessors.cpp` | **Done** |
| **2b** | **Horizontal Microcode Control Store** | `microcode/HorizontalControl.hpp` (32-bit control words) | `tools/verify_golden.cpp` | **Done** |
| **2c** | **Vertical Microcode Control Store** | `microcode/VerticalControl.hpp` (16-bit encoded words) | `tools/verify_golden.cpp` | **Done** |
| **2d** | **Horizontal vs Vertical Microcode Tradeoffs** | `microcode/MicroController.hpp`, `evaluation/` | `evaluation/CompareProcessors.cpp` | **Done** |
| **2e** | **Stack Handling Macros (Full-Descending)** | `assembler/MacroPreprocessor.hpp` (`push`, `pop`, `call`, `ret`) | `benchmarks/stack_test.s` | **Done** |
| **2f** | **Stack Safety & Runtime Exceptions** | `common/Exception.hpp`, `common/Memory.hpp`, `VirtualMemory.hpp` | `tests/test_all.cpp` (Guard page tests) | **Done** |
| **2g** | **MicroPC Step Display & ASCII Stack UI** | `visualizer/StackVisualizer.hpp`, `debugger/Debugger.hpp` | `./risc201_sim benchmarks/stack_test.s -i` | **Done** |
| **3** | **4-Stage Pipelined Processor Core** | `processor_4stage/Pipeline4Stage.hpp` (`IF->OF->EX->MA_RW`) | `make demo-mem`, `tools/audit_cpi.cpp` | **Done** |
| **4** | **6-Stage Pipelined Processor Core** | `processor_6stage/Pipeline6Stage.hpp` (`IF->ID->RF->EX->MA->WB`) | `make demo-pipe`, `tools/audit_cpi.cpp` | **Done** |
| **5** | **4-Stage vs 6-Stage Comparative Evaluation**| `evaluation/CompareProcessors.cpp`, `tools/audit_cpi.cpp` | `./risc201_eval`, `docs/PERFORMANCE.md` | **Done** |

---

## 5. Extra Architectural Capabilities (Beyond Baseline Course Requirements)

This project contains substantial state-of-the-art microarchitecture far exceeding basic classroom pipelines:

1. **Canonical 5-bit String ISA Single Source of Truth**: All 25 instructions are keyed by canonical 5-bit strings in `common/ISA.hpp`. Zero raw strings, zero integer enum duplicates, enforced by automated linter (`./lint_isa`).
2. **Modern Multi-Level Cache Hierarchy**: L1I/L1D (32 KB, 8-way, 1-cycle hit), L2 (512 KB, 8-way, 12 cycles), L3 (8 MB, 16-way, 38 cycles) with non-blocking MSHRs (Miss Status Holding Registers) and Stream Prefetchers.
3. **Cycle-Accurate DDR4/DDR5 DRAM Controller**: Models channels, ranks, bank groups, row buffer hits/conflicts, $t_{RCD}, t_{RP}, t_{CL}$, and FR-FCFS (First-Ready First-Come-First-Served) memory scheduling.
4. **4-Level Radix-4 Virtual Memory & Multi-Tier TLBs**: 48-bit address translation via PML4 $\rightarrow$ PDPT $\rightarrow$ PD $\rightarrow$ PT, L1 ITLB, L1 DTLB, L2 STLB, and Page-Walk Cache (PWC) cutting walk latency by 74%.
5. **Comprehensive Hazard Mitigation & Operand Forwarding**: Full forwarding matrix (`EX->EX`, `MEM->EX`, `WB->EX`) cutting data stalls to near zero, load-use bubble detection, and instruction scheduling pass (`--schedule`).
6. **Advanced Dynamic Branch Prediction**: Static (always not taken), 1-bit dynamic, 2-bit bimodal saturating counter, gshare global branch history, Branch Target Buffer (BTB), and Return Address Stack (RAS).
7. **Empirical CPI Decomposition & Telemetry**: Cycle-accurate accounting decomposing CPI into base, RAW load-use, branch mispredict, and memory stall categories.
8. **Visual ASCII Debugger Console**: Live pipeline ASCII latch views with forwarding arrows, cache set-way occupancy tables, DRAM bank state monitors, and stack frame renderers.

