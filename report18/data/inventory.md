# RISC201-PROCESSOR Repository Source Code Inventory

Generated on: 2026-11-05 | Branch: `report-18page` | Commit: `b82d1f0`

## 1. Module-by-Module File Inventory

| Directory / File | LOC | Architectural Purpose & Scope |
|:---|---:|:---|
| `alu/ALU.hpp` | 161 | Top-level ALU wrapper dispatching arithmetic, logical, and shift operations |
| `alu/Adders.hpp` | 171 | Configurable binary adders: Ripple-Carry (RCA), Carry-Lookahead (CLA), Carry-Select (CSLA) |
| `alu/Dividers.hpp` | 230 | Hardware division units: Restoring, Non-Restoring, and Newton-Raphson iterative reciprocal |
| `alu/LogicGates.hpp` | 125 | Bitwise logic primitives (AND, OR, NOT, XOR, NAND, NOR, XNOR) with cycle tracking |
| `alu/Multipliers.hpp` | 190 | High-speed multipliers: Sequential Shift-and-Add, Booth Radix-4, Wallace Tree |
| `alu/Shifter.hpp` | 139 | Logarithmic barrel shifter supporting LSL, LSR, ASR, and ROR operations |
| `assembler/Assembler.hpp` | 545 | Two-pass assembler resolving labels, pseudos, and encoding canonical 32-bit instructions |
| `assembler/Disassembler.hpp` | 181 | Disassembler reconstructing assembly mnemonics and operands from binary/hex words |
| `assembler/MacroPreprocessor.hpp` | 155 | Macro preprocessor expanding PUSH/POP stack macros into full-descending memory operations |
| `assembler/main_as.cpp` | 54 | CLI entry point for the standalone assembler binary (`risc201_as`) |
| `assembler/main_disasm.cpp` | 48 | CLI entry point for the standalone disassembler binary (`risc201_disasm`) |
| `common/BranchPredictor.hpp` | 384 | Branch prediction suite: Static, 1-bit, 2-bit Bimodal, GShare, 64-entry BTB, 16-entry RAS |
| `common/Exception.hpp` | 82 | Precise CPU exception handling (Arithmetic Overflow, Illegal Opcode, Stack Overflow, Page Fault) |
| `common/ForwardingUnit.hpp` | 275 | Active bypass network supporting NONE, BASIC, FULL, and FULL_PLUS (+StoreBuffer) modes |
| `common/HazardUnit.hpp` | 176 | Pipeline hazard detection unit managing interlocks, load-use stalls, and branch squashes |
| `common/ISA.hpp` | 489 | Single source of truth for SimpleRisc ISA: 28 canonical 5-bit string opcodes and bitfields |
| `common/ISS.hpp` | 130 | Differential golden reference Instruction Set Simulator (single-cycle functional model) |
| `common/Memory.hpp` | 162 | Physical memory abstraction managing flat RAM address space and word/byte operations |
| `common/PerformanceTracker.hpp` | 143 | Cycle accounting engine strictly enforcing the performance invariant and CPI stack |
| `common/RegisterFile.hpp` | 141 | 16-register dual-read single-write register file with r0 hardwired to zero |
| `common/Types.hpp` | 176 | Fundamental data types, word definitions, bitfield unions, and status flag masks |
| `debugger/Debugger.hpp` | 854 | Interactive CLI debugger supporting breakpoints, watchpoints, step-by-step trace, memory dump |
| `debugger/main_sim.cpp` | 336 | Main simulator CLI executable (`risc201_sim`) integrating pipelines, memory, and UI |
| `evaluation/CompareProcessors.cpp` | 323 | Comprehensive benchmark runner comparing 4-stage and 6-stage core execution metrics |
| `evaluation/audit_cpi.cpp` | 138 | Automated verification audit script asserting exact cycle-accounting invariants |
| `evaluation/run_memory_experiments.cpp` | 86 | Sweep driver evaluating cache sizes, replacement policies, DRAM, and TLB configurations |
| `evaluation/run_pipeline_experiments.cpp` | 133 | Driver measuring pipeline throughput across forwarding modes and branch predictors |
| `memory/AddressMapping.hpp` | 117 | DRAM physical address slicing (Channel, Rank, Bank Group, Bank, Row, Column, Offset) |
| `memory/Cache.hpp` | 390 | Set-associative L1/L2 caches with LRU/SRRIP/BIP, 8 MSHRs, and 3C miss telemetry |
| `memory/Config.hpp` | 257 | Configuration presets (IDEAL, SIMPLE, REALISTIC, MODERN) and hardware parameters |
| `memory/DRAM.hpp` | 210 | Cycle-accurate DDR4/DDR5 DRAM controller with bank groups and FR-FCFS out-of-order scheduling |
| `memory/MemoryHierarchy.hpp` | 415 | Unified memory subsystem connecting L1I, L1D, L2 caches, TLB, Page Walker, and DRAM |
| `memory/Prefetcher.hpp` | 212 | Hardware prefetching engine featuring Stream Buffers and Stride Prefetchers |
| `memory/VirtualMemory.hpp` | 409 | 48-bit 4-level radix page table (PML4->PDPT->PD->PT) with TLB and Page-Walk Cache (PWC) |
| `microcode/HorizontalControl.hpp` | 112 | 65-bit horizontal control store containing 33 discrete unencoded datapath control signals |
| `microcode/MicroController.hpp` | 477 | Microprogrammed control unit orchestrating microPC sequencing, dispatch, and exceptions |
| `microcode/MicroInstructions.hpp` | 130 | Micro-assembly syntax definitions, field encodings, and control store ROM image |
| `microcode/VerticalControl.hpp` | 157 | 45-bit vertically encoded control store with decoder logic for reduced bit storage |
| `processor_4stage/Pipeline4Stage.hpp` | 630 | 4-stage in-order pipeline (IF, OF, EX, MA_RW) with 500 MHz clock and 1-cycle branch penalty |
| `processor_4stage/StageRegisters4.hpp` | 95 | Inter-stage latches (IF_OF, OF_EX, EX_MARW) for the 4-stage processor core |
| `processor_6stage/Pipeline6Stage.hpp` | 703 | 6-stage in-order pipeline (IF, ID, RF, EX, MA, WB) with 800 MHz clock and 2-cycle branch penalty |
| `processor_6stage/StageRegisters6.hpp` | 148 | Inter-stage latches (IF_ID, ID_RF, RF_EX, EX_MA, MA_WB) for the 6-stage processor core |
| `tests/test_all.cpp` | 400 | Master verification suite executing 78 unit and integration tests across all modules |
| `tools/audit_cpi.cpp` | 223 | Standalone CPI audit utility validating cycle invariants against simulation CSV logs |
| `tools/capture_golden.cpp` | 321 | Reference baseline generator capturing golden outputs for regression checks |
| `tools/generate_perf_docs.cpp` | 257 | Automated documentation generator synthesizing performance tables into markdown |
| `tools/lint_docs.cpp` | 118 | Documentation linter asserting numerical consistency across markdown reports |
| `tools/lint_isa.cpp` | 138 | ISA consistency linter validating 5-bit string opcodes across all headers |
| `tools/verify_golden.cpp` | 418 | Regression validator asserting 100% equivalence against 231 golden test baselines |
| `visualizer/MemoryVisualizer.hpp` | 236 | Terminal visualizer displaying formatted hex/ASCII memory views and cache sets |
| `visualizer/PipelineVisualizer.hpp` | 238 | ASCII pipeline stage visualizer rendering instruction bubbles, squashes, and stalls |
| `visualizer/StackVisualizer.hpp` | 108 | ASCII stack frame visualizer rendering frames, stack pointer (r15), and frame pointer |
| **Total Core Sources (52 files)** | **12946** | **Complete cycle-accurate processor simulator** |

## 2. Makefile Build Targets

| Target | Command / Flags | Description |
|:---|:---|:---|
| `all` | `g++ -std=c++17 -Wall -Wextra -O2 -I.` | Compiles all simulator binaries, tools, and test suites |
| `risc201_sim` | `debugger/main_sim.cpp` | Main cycle-accurate processor simulator CLI |
| `risc201_as` | `assembler/main_as.cpp` | Standalone two-pass SimpleRisc assembler |
| `risc201_disasm` | `assembler/main_disasm.cpp` | Standalone machine code disassembler |
| `risc201_eval` | `evaluation/CompareProcessors.cpp` | Processor evaluation and 4S vs 6S comparison utility |
| `risc201_test` | `tests/test_all.cpp` | Automated test suite (78 test cases, 100% pass) |
| `audit-cpi` | `tools/audit_cpi.cpp` | Cycle-accounting invariant audit tool |
| `verify-golden`| `tools/verify_golden.cpp` | 231-check golden baseline regression validator |
| `lint-isa` | `tools/lint_isa.cpp` | Static linter verifying 5-bit canonical string opcodes |
| `lint-docs` | `tools/lint_docs.cpp` | Cross-verification linter checking docs vs code |
| `test-asan` | `g++ -fsanitize=address,undefined` | AddressSanitizer and UndefinedBehaviorSanitizer run |
| `report18` | `typst compile report18/source/report.typ` | Compiles exactly 18-page project report PDF |
