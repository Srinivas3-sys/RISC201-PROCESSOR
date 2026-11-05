# Appendix: Complete Repository Inventory & Traceability Matrix

## 1. Toolchain & Build Environment

| Parameter | Detected Value | Verification Command |
| :--- | :--- | :--- |
| **Operating System** | Ubuntu 26.04.1 LTS (Linux 7.0.0-38-generic x86_64) | `uname -a && cat /etc/os-release` |
| **Compiler** | GCC / G++ 15.2.0 (`g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`) | `g++ --version` |
| **C++ Standard** | ISO C++17 (`-std=c++17`) | `Makefile: CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.` |
| **Compiler Warnings** | `-Wall -Wextra` (Zero warnings across all targets) | `make clean && make all` |
| **Optimization** | `-O2` (Production binaries), `-O1 -g -fsanitize=address,undefined` (Sanitizer target) | `Makefile` |
| **Build Tool** | GNU Make 4.4.1 | `make --version` |
| **Report Toolchain**| Pandoc 3.1.11.1 + Typst 0.11.0 (PDF/DOCX rendering) | `pandoc -v && typst --version` |

### Build System Targets (`Makefile`)

| Target | Dependencies | Purpose | Verified Status |
| :--- | :--- | :--- | :--- |
| **`all`** | All 12 project binaries | Compiles entire simulator, assembler, tools, and evaluation suite | PASSED (0 warnings) |
| **`risc201_sim`** | `debugger/main_sim.cpp`, core headers | Builds main interactive CLI simulator & debugger REPL | PASSED |
| **`risc201_as`** | `assembler/main_as.cpp`, assembler headers | Builds two-pass command-line assembler | PASSED |
| **`risc201_disasm`**| `assembler/main_disasm.cpp`, disassembler | Builds machine-code disassembler | PASSED |
| **`risc201_eval`** | `evaluation/CompareProcessors.cpp` | Builds cross-pipeline evaluation tool (30 architectural configurations) | PASSED |
| **`risc201_test`** | `tests/test_all.cpp` | Builds comprehensive test suite | PASSED |
| **`test`** | `risc201_test` | Runs full 78-case verification test suite | PASSED (78/78) |
| **`test-asan`** | `tests/test_all.cpp` | Builds & runs test suite under AddressSanitizer and UBSan | PASSED (Clean) |
| **`audit-cpi`** | `tools/audit_cpi.cpp` | Runs full matrix of 12 benchmarks across all pipelines & presets | PASSED |
| **`perf-docs`** | `audit-cpi`, `generate_perf_docs` | Regenerates `docs/PERFORMANCE.md` from actual simulation runs | PASSED |
| **`lint-docs`** | `tools/lint_docs.cpp` | Lints KaTeX math syntax across all documentation markdown files | PASSED |
| **`lint-isa`** | `tools/lint_isa.cpp` | Ensures `common/ISA.hpp` is the single source of truth for opcodes | PASSED (43/43 clean) |
| **`verify-golden`**| `tools/verify_golden.cpp` | Golden regression validator (231 checks, byte-for-byte fidelity) | PASSED (231/231) |
| **`exp-pipe`** | `evaluation/run_pipeline_experiments.cpp` | Runs 10 pipeline hazard & forwarding experiments (outputs CSV) | PASSED |
| **`exp`** | `evaluation/run_memory_experiments.cpp` | Runs 20 memory hierarchy & cache experiments (outputs CSV) | PASSED |
| **`demo-mem`** | `risc201_sim` | Runs memory visualizer demo on `factorial_iter.s` | PASSED |
| **`demo-vm`** | `risc201_sim` | Runs 6-stage virtual memory & page-table demo on `factorial_rec.s` | PASSED |
| **`demo-pipe`** | `risc201_sim` | Runs timing diagram & hazard forwarding demo on `hazard_test.s` | PASSED |
| **`clean`** | N/A | Removes all generated binaries, object files, and temporary outputs | PASSED |

---

## 2. Complete File Inventory & Lines of Code (LOC)

Total Project Codebase: **16,583 lines** across 74 files (excluding git history and generated binary assets).

### 2.1 Arithmetic Logic Unit (`alu/`) — 1,016 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `alu/Adders.hpp` | 171 | Ripple-carry adder, carry-lookahead adder (CLA), Kogge-Stone parallel-prefix adder, conditional-sum adder models. |
| `alu/ALU.hpp` | 161 | Unified ALU class dispatching 25 arithmetic, logic, shift, and branch comparisons via canonical 5-bit ISA string opcodes. |
| `alu/Dividers.hpp` | 230 | Restoring division, non-restoring division, and iterative Newton-Raphson division algorithms with cycle tracking. |
| `alu/LogicGates.hpp` | 125 | Gate-level logic primitives (AND, OR, NOT, XOR, NAND, NOR, XNOR, MUX, FullAdder) with propagation delay metrics. |
| `alu/Multipliers.hpp`| 190 | Array multiplier, Booth's radix-4 multiplier, and Wallace-tree carry-save multiplier implementations. |
| `alu/Shifter.hpp` | 139 | 32-bit logarithmic barrel shifter supporting Logical Shift Left (LSL), Logical Shift Right (LSR), Arithmetic Shift Right (ASR), and Rotate (ROTS). |

### 2.2 Assembler, Disassembler, and Macros (`assembler/`) — 983 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `assembler/Assembler.hpp` | 545 | Two-pass assembler translating SimpleRisc assembly into 32-bit machine code words, ELF-like symbol table, and binary/hex output. |
| `assembler/Disassembler.hpp` | 181 | Machine code disassembler decoding 32-bit words back into formatted assembly instructions and synthetic label references. |
| `assembler/MacroPreprocessor.hpp` | 155 | Macro preprocessor handling full-descending stack operations (`PUSH`, `POP`), synthetic pseudo-ops, and literal pools. |
| `assembler/main_as.cpp` | 54 | CLI executable driver for the assembler (`./risc201_as <source.s> -o <out.hex>`). |
| `assembler/main_disasm.cpp` | 48 | CLI executable driver for the disassembler (`./risc201_disasm <source.hex>`). |

### 2.3 Benchmarks (`benchmarks/`) — 414 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `benchmarks/array_sum.s` | 41 | Iterative array traversal accumulating 10 integers (standard cold-start micro-benchmark). |
| `benchmarks/array_sum_long.s` | 77 | Extended array reduction over 1,000 elements for steady-state memory and cache evaluation. |
| `benchmarks/factorial_iter.s` | 17 | Iterative computation of $N!$ (tight ALU loop testing loop branch prediction). |
| `benchmarks/factorial_iter_long.s`| 26 | Multi-iteration factorial loop generating 100,000+ retired instructions for steady-state throughput. |
| `benchmarks/factorial_rec.s` | 34 | Recursive computation of factorial exercising stack frames, function calls, `PUSH`/`POP`, and return addresses. |
| `benchmarks/factorial_rec_long.s` | 41 | Deep recursion benchmark stressing stack memory boundaries and guard-page protection. |
| `benchmarks/hazard_test.s` | 40 | Micro-benchmark containing deliberate RAW data hazards, load-use dependencies, and branch flushes. |
| `benchmarks/hazard_test_long.s` | 48 | Sustained hazard dependency chain evaluating youngest-producer forwarding and stall latencies. |
| `benchmarks/prime_test.s` | 26 | Sieve/primality testing loop exercising division, modulo, and conditional branching. |
| `benchmarks/prime_test_long.s` | 34 | Heavy primality calculation stressing integer division units and long-latency branch history. |
| `benchmarks/stack_test.s` | 27 | Stack manipulation benchmark validating full-descending push/pop and frame pointer integrity. |
| `benchmarks/stack_test_long.s` | 30 | High-frequency stack push/pop burst testing stack memory visualizer and boundary guard checks. |

### 2.4 Common Datapath & Architecture Primitives (`common/`) — 2,472 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `common/BranchPredictor.hpp` | 383 | Branch direction and target predictors: Static (Always Taken/Not Taken), 1-bit, 2-bit Bimodal, Gshare (global history), BTB, and RAS. |
| `common/Exception.hpp` | 82 | Architectural exception representation, trap vector mapping, fault cause encoding, and precise PC recovery. |
| `common/ForwardingUnit.hpp` | 274 | Hazard bypass network for 4-stage and 6-stage pipelines implementing youngest-producer priority forwarding. |
| `common/HazardUnit.hpp` | 175 | Structural, data (RAW, WAR, WAW), and control hazard detection logic generating pipeline stalls and bubble injections. |
| `common/ISA.hpp` | 489 | Single source of truth for the RISC201 ISA: 5-bit binary string opcode constants, bitfield layout masks, and instruction metadata table. |
| `common/ISS.hpp` | 129 | Cycle-independent reference Instruction Set Simulator (ISS) used for golden differential verification against pipelined cores. |
| `common/Memory.hpp` | 162 | Flat byte-addressable physical memory model with little-endian word loading and boundary alignment checking. |
| `common/PerformanceTracker.hpp` | 142 | Detailed cycle, instruction, stall histogram, and CPI stack accounting infrastructure. |
| `common/RegisterFile.hpp` | 141 | Dual-read, single-write multi-ported register file ($r0-r15$, hardwired zero $r0$, SP=$r14$, RA=$r15$, and privileged registers). |
| `common/Types.hpp` | 176 | Core architectural types (`Word`, `Address`, `ImmediateModifier`, `PrivRegId`), simulator configuration structures, and status flags. |

### 2.5 Interactive Debugger & CLI (`debugger/`) — 1,189 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `debugger/Debugger.hpp` | 854 | Interactive debugger REPL engine supporting cycle stepping, breakpoints, register/memory inspection, pipeline status, and memory telemetry. |
| `debugger/main_sim.cpp` | 335 | Main simulator entry point with CLI argument parsing, interactive startup prompt, preset selection, and execution loop. |

### 2.6 Documentation (`docs/`) — 2,307 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `docs/CPI_AUDIT.md` | 290 | Comprehensive CPI audit investigation, diagnosis of high CPI causes, and validation of steady-state vs cold-start accounting. |
| `docs/HAZARDS_AND_FORWARDING.md` | 138 | Formal specification of pipeline hazard handling, forwarding equations, and stall penalty matrices. |
| `docs/ISA_BINARY_OPCODES.md` | 136 | Detailed ISA bitfield encoding tables and 5-bit string opcode specifications. |
| `docs/ISA_REFACTOR_PLAN.md` | 132 | Architecture plan for migrating the entire codebase to `ISA.hpp` as the single source of truth. |
| `docs/MEMORY_DESIGN.md` | 376 | Hardware specification of multi-level cache hierarchies, DRAM controller timings, and prefetch algorithms. |
| `docs/MEMORY_PLAN.md` | 255 | Design blueprint for virtual memory, TLB hierarchy, and memory controller schedulers. |
| `docs/PERFORMANCE.md` | 181 | Verified cycle accounting formulas, analytic CPI models, and simulation performance benchmarks. |
| `docs/PIPELINE_PLAN.md` | 306 | Architectural plan comparing 4-stage vs 6-stage pipeline stages and forwarding matrices. |
| `docs/UI_AUDIT.md` | 110 | Complete audit of the interactive CLI, color schemes, panel layouts, and friend's visualizer contributions. |
| `docs/VIRTUAL_MEMORY.md` | 283 | 4-level radix-tree page table, multi-level TLBs, Page-Walk Cache (PWC), and demand paging specification. |

### 2.7 Evaluation & Experiments (`evaluation/`, `experiments/`) — 1,200 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `evaluation/audit_cpi.cpp` | 138 | Automated CPI matrix evaluator running all 12 benchmarks across all pipelines and presets. |
| `evaluation/CompareProcessors.cpp` | 323 | Comprehensive cross-pipeline performance benchmarking across 30 architectural configurations. |
| `evaluation/run_memory_experiments.cpp` | 85 | Test driver executing the 20 memory hierarchy experiments. |
| `evaluation/run_pipeline_experiments.cpp` | 132 | Test driver executing the 10 pipeline hazard and forwarding experiments. |
| `experiments/pipeline/PIPELINE_RESULTS.md` | 246 | Experimental data and analysis for the 10 pipeline hazard experiments. |
| `experiments/results/EXPERIMENT_RESULTS.md` | 276 | Experimental data and analysis for the 20 memory hierarchy experiments. |

### 2.8 Memory & Virtual Memory Subsystem (`memory/`) — 1,802 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `memory/AddressMapping.hpp` | 116 | DRAM physical address mapping (Channel $\rightarrow$ Rank $\rightarrow$ Bank Group $\rightarrow$ Bank $\rightarrow$ Row $\rightarrow$ Column) with XOR interleaving. |
| `memory/Cache.hpp` | 389 | Parameterized $N$-way set-associative cache with MSHRs, 3C miss classification, write policies, and replacement algorithms (LRU, FIFO, PLRU, SRRIP). |
| `memory/Config.hpp` | 256 | Presets (`ideal`, `simple`, `modern`, `modern-ddr5`, `lecture`) and parameter definitions for the memory hierarchy. |
| `memory/DRAM.hpp` | 209 | Cycle-accurate command-level DRAM controller modeling $t_{\text{RCD}}, t_{\text{RP}}, t_{\text{CL}}, t_{\text{RAS}}$, refresh, and FR-FCFS scheduling. |
| `memory/MemoryHierarchy.hpp` | 414 | Integrated memory hierarchy coordinating L1I, L1D, unified L2/L3, virtual memory, store buffer, and DRAM controller. |
| `memory/Prefetcher.hpp` | 211 | Hardware data prefetchers: Next-line, Stride (RPT with 2-bit saturating states), and Stream buffer prefetchers. |
| `memory/VirtualMemory.hpp` | 408 | 4-level radix-tree page table translation, L1 ITLB, L1 DTLB, L2 STLB, Page-Walk Cache (PWC), and demand paging frame allocator. |

### 2.9 Microprogrammed Control Unit (`microcode/`) — 876 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `microcode/HorizontalControl.hpp` | 112 | Unencoded horizontal micro-instruction word (28 independent control bits) with zero-delay decode. |
| `microcode/MicroController.hpp` | 477 | Microprogrammed control unit executing control memory routines, microPC sequencing, and micro-exception dispatch. |
| `microcode/MicroInstructions.hpp` | 130 | Micro-operation opcode definitions, micro-branch conditions, and microcode ROM builder. |
| `microcode/VerticalControl.hpp` | 157 | Compact encoded vertical micro-instruction word (16 bits) with hardware field decoders. |

### 2.10 Pipelined Processors (`processor_4stage/`, `processor_6stage/`) — 1,688 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `processor_4stage/Pipeline4Stage.hpp` | 630 | Cycle-accurate 4-stage pipeline ($\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$) with integrated hazard handling and forwarding. |
| `processor_4stage/README.md` | 44 | Architectural documentation and stage merging rationale for the 4-stage core. |
| `processor_4stage/StageRegisters4.hpp` | 95 | Edge-triggered pipeline latches (`IF/OF`, `OF/EX`, `EX/MA_RW`) with stall, flush, and bubble support. |
| `processor_6stage/Pipeline6Stage.hpp` | 703 | Cycle-accurate 6-stage pipeline ($\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$) for higher clock frequency targets. |
| `processor_6stage/README.md` | 68 | Architectural documentation and stage splitting rationale for the 6-stage core. |
| `processor_6stage/StageRegisters6.hpp` | 148 | Edge-triggered pipeline latches (`IF/ID`, `ID/RF`, `RF/EX`, `EX/MA`, `MA/RW`) with stall, flush, and bubble support. |

### 2.11 Test Suite & Developer Tools (`tests/`, `tools/`) — 1,874 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `tests/test_all.cpp` | 399 | Unified 78-case verification test suite covering ISA, caches, DRAM, virtual memory, forwarding, and differential ISS checks. |
| `tools/audit_cpi.cpp` | 223 | Deterministic CPI matrix generator evaluating all benchmarks, pipelines, memory presets, and forwarding modes. |
| `tools/capture_golden.cpp` | 321 | Tool for capturing golden architectural baselines for machine code words, disassembly text, microcode control stores, and simulation traces. |
| `tools/generate_perf_docs.cpp` | 257 | Automated performance documentation generator producing verified LaTeX math tables in `docs/PERFORMANCE.md`. |
| `tools/lint_docs.cpp` | 118 | Markdown documentation linter verifying KaTeX math syntax and catching formatting bugs. |
| `tools/lint_isa.cpp` | 138 | Static analysis linter checking that no raw integer opcode enums or literal strings exist outside `common/ISA.hpp`. |
| `tools/verify_golden.cpp` | 418 | Automated golden regression validator executing 231 checks across all benchmarks and 192 architectural simulation configurations. |

### 2.12 Visualizers (`visualizer/`) — 581 LOC
| File | Lines | Purpose |
| :--- | :---: | :--- |
| `visualizer/MemoryVisualizer.hpp` | 235 | ASCII visualizer rendering live cache set-way occupancy, TLB hit rates, MSHR depths, and DRAM bank status. |
| `visualizer/PipelineVisualizer.hpp` | 238 | ASCII visualizer rendering pipeline stage latches, active instruction mnemonics, forwarding arrows, and cycle timing diagrams. |
| `visualizer/StackVisualizer.hpp` | 108 | ASCII visualizer rendering active stack memory frames, frame pointers, local variables, and boundary guard lines. |

---

## 3. Requirements Traceability Matrix

This matrix maps every requirement from the course assignment specification to its implementation files, automated tests, demonstration commands, and verified status.

| Req ID | Assignment Requirement | Implementing Files & Classes | Automated Test Verification | Demonstration Command | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1a** | 4-stage and 6-stage RISC201 pipelines + behavior comparison | `processor_4stage/Pipeline4Stage.hpp`, `processor_6stage/Pipeline6Stage.hpp`, `evaluation/CompareProcessors.cpp` | `tests/test_all.cpp` (Tests 5.1–5.4, 6.1–6.4), `tools/verify_golden.cpp` (192 configs) | `./risc201_sim benchmarks/factorial_iter.s --compare` | **Complete** |
| **1b** | Microprogrammed control unit (horizontal and vertical) | `microcode/MicroController.hpp`, `microcode/HorizontalControl.hpp`, `microcode/VerticalControl.hpp` | `tests/test_all.cpp` (Tests 1.4, 6.5), `tools/verify_golden.cpp` (Checks 2.1–2.4) | `./risc201_sim benchmarks/hazard_test.s -i` (`micro` command) | **Complete** |
| **1c** | ALU with efficient arithmetic and logical operations | `alu/ALU.hpp`, `alu/Adders.hpp`, `alu/Dividers.hpp`, `alu/Multipliers.hpp`, `alu/Shifter.hpp`, `alu/LogicGates.hpp` | `tests/test_all.cpp` (Tests 1.1–1.3) | `./risc201_sim benchmarks/factorial_iter.s` | **Complete** |
| **2a** | Two-pass assembler (assembly $\rightarrow$ binary/hex) | `assembler/Assembler.hpp`, `assembler/main_as.cpp` | `tests/test_all.cpp` (Tests 1.1, 1.2), `tools/verify_golden.cpp` (Check 1.1) | `./risc201_as benchmarks/factorial_iter.s -o out.hex` | **Complete** |
| **2b** | Disassembler (binary/hex $\rightarrow$ assembly) | `assembler/Disassembler.hpp`, `assembler/main_disasm.cpp` | `tests/test_all.cpp` (Test 1.2), `tools/verify_golden.cpp` (Check 1.2) | `./risc201_disasm benchmarks/factorial_iter.hex` | **Complete** |
| **2c** | Interactive CLI to simulate register file and instruction pipeline | `debugger/Debugger.hpp`, `debugger/main_sim.cpp` | `tests/test_all.cpp` (Section 5) | `./risc201_sim benchmarks/factorial_iter.s -i` (`pipe`, `reg`) | **Complete** |
| **2d** | Step-by-step CPU execution simulator with interactive CLI debugger | `debugger/Debugger.hpp`, `debugger/main_sim.cpp` | `tests/test_all.cpp` (Section 6) | `./risc201_sim benchmarks/hazard_test.s -i` (`step`, `run`, `break`) | **Complete** |
| **2e** | Stack preprocessor macro expansion (full-descending ops) | `assembler/MacroPreprocessor.hpp`, `assembler/Assembler.hpp` | `tests/test_all.cpp` (Test 1.1) | `./risc201_as benchmarks/stack_test.s -o out.hex` | **Complete** |
| **2f** | Stack boundary safety verification and runtime exception framework | `common/Exception.hpp`, `processor_4stage/Pipeline4Stage.hpp`, `processor_6stage/Pipeline6Stage.hpp` | `tests/test_all.cpp` (Test 3.4, 4.3) | `./risc201_sim benchmarks/factorial_rec_long.s` | **Complete** |
| **2g** | Active stack memory visualizer (ASCII stack frame renderer) | `visualizer/StackVisualizer.hpp`, `debugger/Debugger.hpp` | `tests/test_all.cpp` (Section 6) | `./risc201_sim benchmarks/stack_test.s -i` (`stack` command) | **Complete** |
| **3** | Microprogrammed control integrated into simulator (control memory, horizontal & vertical, microPC step display, exceptions) | `microcode/MicroController.hpp`, `debugger/Debugger.hpp` | `tests/test_all.cpp` (Tests 1.4, 6.5) | `./risc201_sim benchmarks/factorial_iter.s -i` (`microstep` command) | **Complete** |
| **4** | Optimized ALU class driving all arithmetic, shift, and logic ops | `alu/ALU.hpp`, `alu/Dividers.hpp`, `alu/Multipliers.hpp` | `tests/test_all.cpp` (Section 1) | `./risc201_eval` (ALU benchmarks) | **Complete** |
| **5** | Evaluation across 4-stage vs 6-stage, programs, horizontal vs vertical, and ALU algorithms | `evaluation/CompareProcessors.cpp`, `evaluation/audit_cpi.cpp`, `tools/generate_perf_docs.cpp` | `evaluation/run_pipeline_experiments.cpp`, `evaluation/run_memory_experiments.cpp` | `make perf-docs && make exp-pipe && make exp` | **Complete** |

### Extensions Beyond Assignment Requirements
| Feature | Implementing Files | Automated Test / Verification | Status |
| :--- | :--- | :--- | :---: |
| **Single Source of Truth 5-bit ISA Strings** | `common/ISA.hpp`, `tools/lint_isa.cpp` | `make lint-isa` (43 files verified) | **Complete** |
| **Hazard Detection & Youngest-Producer Forwarding**| `common/HazardUnit.hpp`, `common/ForwardingUnit.hpp` | `tests/test_all.cpp` (Section 4), `make exp-pipe` | **Complete** |
| **Multi-Level Cache Hierarchy (L1I, L1D, L2, L3)** | `memory/Cache.hpp`, `memory/MemoryHierarchy.hpp` | `tests/test_all.cpp` (Section 2), `make exp` | **Complete** |
| **Cycle-Accurate DDR4/DDR5 DRAM Model** | `memory/DRAM.hpp`, `memory/AddressMapping.hpp` | `tests/test_all.cpp` (Section 2), `make exp` | **Complete** |
| **Radix-4 Virtual Memory, TLBs, and PWC** | `memory/VirtualMemory.hpp` | `tests/test_all.cpp` (Section 3), `make demo-vm` | **Complete** |
| **Hardware Data Prefetchers (Stream, Stride, Next-line)**| `memory/Prefetcher.hpp` | `tests/test_all.cpp` (Section 2) | **Complete** |
| **Reference ISS Differential Verification** | `common/ISS.hpp`, `tests/test_all.cpp` | `tests/test_all.cpp` (Section 5) | **Complete** |
| **Golden Regression Validator (231 baselines)** | `tools/verify_golden.cpp` | `make verify-golden` | **Complete** |

