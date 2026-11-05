# Chapter 1: Introduction

## 1.1 Problem Statement & Course Context
This project report documents the design, hardware modeling, software toolchain, verification, and empirical evaluation of **RISC201-PROCESSOR**, developed for the course **CSC-201: Computer Organization and Architecture (Autumn 2026)** at the Department of Computer Science & Engineering.

The assignment, titled *"Processor Design"*, specifies the comprehensive realization of an instruction set architecture and execution environment:
> *"Design (a) a 4-stage and 6-stage RISC201 ISA and compare the behavior of both processors, (b) a corresponding microprogrammed control unit (horizontal or vertical), (c) a corresponding ALU with efficient arithmetic and logical operations. Implement a software environment: two-pass assembler (assembly $\rightarrow$ binary/hex), disassembler, interactive command-line interface to simulate the register file and instruction execution pipeline, step-by-step CPU execution simulator with debugger capabilities and interactive CLI debugger, stack preprocessor macro expansion for full-descending stack operations, stack boundary safety verification and a runtime exception framework for memory protection against stack corruption, and an ASCII stack memory frame visualizer. Add microprogrammed control logic into the CPU simulator using control memory microcode rather than hardwired logic... Add an optimized ALU class to drive all arithmetic, shift and logical operations. Evaluate the processor, control unit and ALU for different test cases considering 4-stage and 6-stage ISAs, different assembly programs, horizontal vs vertical microprogrammed controllers (optional) and different ALU algorithms."*

## 1.2 Objectives & Scope
The project's objective is to construct an industrial-grade, cycle-accurate computer systems simulation framework in modern C++ that faithfully reflects textbook microarchitectural theory while providing deep, cycle-by-cycle visibility into pipeline hazards, memory hierarchy dynamics, and control store execution.

### What Was Delivered
1. **Core Processors**: Two complete, cycle-accurate, parameterizable pipelined processors:
   * **4-Stage Pipeline** ($\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$) featuring merged memory/writeback and a 2-cycle branch penalty.
   * **6-Stage Pipeline** ($\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$) featuring split decode/register-read stages for high-frequency scaling.
2. **Microprogrammed Control Unit**: Dual microprogrammed control stores:
   * **Horizontal Microcode** (65-bit unencoded word with 33 discrete datapath control bits).
   * **Vertical Microcode** (45-bit encoded word with hardware decoding logic achieving a 30.8% ROM size reduction).
   * MicroPC sequencing, dispatch table keyed by 5-bit canonical string opcodes, and micro-exception handling.
3. **Optimized Arithmetic Logic Unit (ALU)**:
   * Configurable adder algorithms (Ripple-Carry, Carry-Select, Carry-Lookahead, Kogge-Stone).
   * Configurable multiplier algorithms (Iterative Shift-Add, Radix-4 Booth, Wallace-Tree).
   * Configurable divider algorithms (Restoring, Non-Restoring, and Newton-Raphson iterative division).
   * 32-bit Logarithmic Barrel Shifter (LSL, LSR, ASR, ROTS).
   * Gate-level logic primitives (`LogicGates.hpp`) with propagation delay accounting.
4. **Software Toolchain**:
   * Two-pass assembler (`risc201_as`) with symbol table resolution, macro preprocessor, and binary/hex outputs.
   * Full-descending stack preprocessor macro expansion (`PUSH` / `POP`).
   * Machine code disassembler (`risc201_disasm`) with synthetic label recovery.
5. **Interactive Debugger & Visualizers**:
   * Command-line REPL debugger with cycle-stepping, instruction-stepping, register inspection, and memory dumps.
   * Interactive startup assembly prompt for immediate on-the-fly execution.
   * Three ASCII visualizers: `PipelineVisualizer` (stage latches & timing trace), `MemoryVisualizer` (cache set-way grid & DRAM buffer status), and `StackVisualizer` (stack frames & guard page boundary).
6. **Advanced Architectural Extensions Beyond Assignment**:
   * **Canonical 5-Bit String ISA**: Single source of truth in `common/ISA.hpp` with zero redundant integer enums.
   * **Hazard Detection & Youngest-Producer Forwarding**: Exhaustive bypass network (`EX-to-EX`, `MEM-to-EX`, `WB-to-EX`, and 4-entry Store Buffer bypass).
   * **Branch Prediction Suite**: 8 policies including 2-bit Bimodal Smith predictor, Gshare, 64-entry BTB, and 16-entry RAS.
   * **Full Memory Hierarchy**: Multi-level caches (L1I, L1D, L2, L3) with non-blocking MSHRs, 3C miss breakdown, and cycle-accurate DDR4/DDR5 DRAM model (channels, ranks, banks, FR-FCFS scheduling).
   * **Radix-4 Virtual Memory & TLBs**: 48-bit address space, multi-level TLBs (L1 ITLB, L1 DTLB, L2 STLB), Page-Walk Cache (PWC), and demand paging frame allocator.
   * **Automated Verification**: 78-case verification test suite, golden regression validator (231 baselines), ISA string linter, and AddressSanitizer/UBSan validation.

### What Was Out of Scope
* Out-of-order superscalar execution (e.g. Tomasulo reservation stations / reorder buffer).
* Multi-core cache coherence protocols (e.g. MESI / MOESI snooping bus).
* Floating-point IEEE-754 vector SIMD extensions.

---

## 1.3 Team Structure & Contributions
The project was executed by a collaborative engineering team. Contributors are credited based strictly on repository commit records:

```
$ git log --format='%an' | sort | uniq -c
     26 bandlapalliyss-max
      1 Varshith Teja
```

| Contributor | Primary Modules & Responsibilities (from Git Commit History) |
| :--- | :--- |
| **`bandlapalliyss-max`** (Srinivas B.) | Architectural lead; 100% C++17 conversion of datapath; 4-stage and 6-stage pipeline implementations; HazardUnit and ForwardingUnit; full memory hierarchy (caches, DDR4/DDR5 DRAM, radix-4 VM, TLBs, PWC); microprogrammed controller (horizontal and vertical); two-pass assembler and disassembler; verification test suites (78/78 tests, golden regression suite, ISA linter); CPI accounting audit. |
| **`Varshith Teja`** | UI/Visualizer enhancements (commit `bbd9ea1`); cycle-by-cycle execution trace visualizer (`PipelineVisualizer::renderTrace()`); startup interactive assembly prompt (`promptStartupAssembly()`); Newton-Raphson division algorithm (`alu/Dividers.hpp`); runtime branch predictor CLI flag selection; XOR and ABS opcode ALU integration. |

---

## 1.4 Technology Stack & Toolchain
The entire repository is authored in **100% pure modern ISO C++17** with zero external library dependencies, foreign scripting runtimes, or non-native harnesses.

* **Language Standard**: ISO C++17 (`-std=c++17`).
* **Compiler**: GCC / G++ 15.2.0 (`g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`).
* **Build System**: GNU Make 4.4.1 (`Makefile` with modular compilation targets).
* **Compilation Flags**: `-std=c++17 -Wall -Wextra -O2 -I.` (zero compiler warnings across all targets).
* **Sanitizers**: Clang/GCC AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`).
* **Documentation & Report Toolchain**: Pandoc 3.1.11.1 with Typst 0.11.0 rendering engine.

---

## 1.5 Repository Layout Overview
The project is structured into 14 functional directories:

```
RISC201-PROCESSOR/
├── alu/                 # Optimized ALU, adders, dividers, multipliers, barrel shifter, logic gates
├── assembler/           # Two-pass assembler, disassembler, macro preprocessor, CLI drivers
├── benchmarks/          # 12 assembly benchmark programs (.s, .hex, .bin)
├── common/              # Canonical ISA strings, ISS, register file, memory, hazards, forwarding, BP
├── debugger/            # Interactive debugger REPL engine, main simulator driver
├── docs/                # Architecture specifications, CPI audit, memory design, UI audit
├── evaluation/          # Cross-processor comparison, benchmark drivers, experiment harnesses
├── experiments/         # Raw CSV datasets and analytical markdown summaries
├── memory/              # Caches (L1-L3), MSHRs, DRAM controller, Radix-4 VM, TLBs, PWC, prefetcher
├── microcode/           # Horizontal & vertical control stores, microPC sequencer, micro-instructions
├── processor_4stage/    # 4-stage pipelined processor core and stage latches
├── processor_6stage/    # 6-stage pipelined processor core and stage latches
├── tests/               # 78-case verification test suite, golden regression baselines
├── tools/               # CPI audit runner, perf doc generator, golden validator, ISA & doc linters
├── visualizer/          # ASCII pipeline, memory, and stack frame visualizers
├── Makefile             # Unified build system
└── README.md            # Architectural overview and quick start guide
```

