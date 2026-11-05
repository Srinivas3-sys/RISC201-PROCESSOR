# Part K: Quality Assurance, Verification, & Project History

This section details the automated verification suite (78/78 tests, 231/231 golden checks, AddressSanitizer/UBSan), explains every Makefile target in plain words, recounts the engineering refactoring history from git logs, and provides an honest analysis of project limitations so you never get trapped during your presentation.

---

# Chapter K1: Automated Verification & Testing Strategy

### 1. In One Sentence
The repository maintains six layers of automated quality assurance: unit tests, differential testing against the Golden ISS, golden baseline regression validation, AddressSanitizer memory safety checks, an ISA single-source linter, and documentation syntax linting.

---

### 2. The Six QA Layers

```
Layer 6: Documentation & LaTeX Syntax Linter (tools/lint_docs.cpp)
Layer 5: Single Source of Truth ISA Linter (tools/lint_isa.cpp)
Layer 4: AddressSanitizer & UndefinedBehaviorSanitizer (make test-asan)
Layer 3: Golden Output Regression Validator (tools/verify_golden.cpp - 231 checks)
Layer 2: Differential Testing (Pipelines vs Golden ISS - 192 configs)
Layer 1: Unit Test Suite (tests/test_all.cpp - 78 comprehensive tests)
```

1. **Unit Tests (`tests/test_all.cpp`)**:
   - 78 tests covering ALU circuits, cache associativity, TLB replacement, hazard detection, and microcode dispatch.
   - Run via: `make test` (Pass rate: **78 / 78 = 100%**).
2. **Differential Testing**:
   - Compares 4-stage and 6-stage pipeline register outputs against the single-cycle Golden ISS across hundreds of random and corner-case instruction sequences.
3. **Golden Regression Validator (`tools/verify_golden.cpp`)**:
   - 231 checks verifying assembler `.hex` output, disassembler round-tripping, microcode control stores, and cycle count baselines.
   - Run via: `make verify-golden` (Pass rate: **231 / 231 = 100%**).
4. **Memory Safety & Sanitizers**:
   - Compiles under `-fsanitize=address,undefined` to verify zero memory leaks, zero buffer overruns, and zero unaligned memory pointer dereferences.
   - Run via: `make test-asan` (**Zero leaks, zero UB**).
5. **ISA Single Source of Truth Linter (`tools/lint_isa.cpp`)**:
   - Scans 43 files ensuring zero integer opcodes, zero raw string literals, and zero duplicate enums.
   - Run via: `make lint-isa`.
6. **Documentation KaTeX Linter (`tools/lint_docs.cpp`)**:
   - Scans all markdown documentation ensuring mathematical syntax renders without formatting errors.
   - Run via: `make lint-docs`.

---

# Chapter K2: The Makefile Target Reference

Every target explained in simple words:

| Target | Command | What It Does in Plain Words |
| :--- | :--- | :--- |
| **`make all`** | Builds all 12 binaries | Compiles simulator, assembler, disassembler, test suite, and evaluation tools. |
| **`make test`** | `./risc201_test` | Runs the 78 automated unit and differential tests. |
| **`make test-asan`** | Clang/GCC Sanitizer | Compiles and runs test suite under AddressSanitizer and UndefinedBehaviorSanitizer. |
| **`make verify-golden`**| `./verify_golden` | Runs 231 regression checks against locked baseline binaries and traces. |
| **`make audit-cpi`** | `./audit_cpi` | Runs all 12 benchmarks across all pipeline and memory presets, printing real CPIs. |
| **`make perf-docs`** | `./generate_perf_docs` | Regenerates `docs/PERFORMANCE.md` exclusively from fresh simulation runs. |
| **`make lint-isa`** | `./lint_isa` | Scans all 43 C++ files verifying adherence to `common/ISA.hpp`. |
| **`make lint-docs`** | `./lint_docs` | Verifies clean LaTeX math formatting across all markdown documents. |
| **`make demo-pipe`** | Run `hazard_test.s` | Runs 6-stage pipeline with live ASCII timing diagrams and stall explanations. |
| **`make demo-mem`** | Run `factorial_iter.s` | Runs 4-stage pipeline with live cache set-way grid and DRAM bank displays. |
| **`make demo-vm`** | Run `factorial_rec.s` | Runs 6-stage pipeline with active Radix-4 Virtual Memory and TLB translations. |
| **`make clean`** | `rm -f ...` | Deletes all compiled binaries and temporary `.hex`/`.bin` files. |

---

# Chapter K3: Project History & Major Refactoring Milestones

From git logs (`git log --oneline`):

1. **Milestone 1 — Baseline Microcoded Architecture**:
   - Original C++ datapath, basic instruction decoding, and horizontal/vertical control memory stores implemented.
2. **Milestone 2 — Pipelining & Hazard Mitigation**:
   - Implementation of 4-stage and 6-stage pipeline cores, inter-stage latch registers, Hazard Unit, and Forwarding Unit.
3. **Milestone 3 — Memory Hierarchy & Virtual Memory**:
   - Addition of multi-level caches (L1-L3), non-blocking MSHRs, DDR4/DDR5 DRAM controller, 4-level Radix-4 page tables, and TLBs.
4. **Milestone 4 — Teammate UI & Visualizer Enhancements (Varshith Teja, `bbd9ea1`)**:
   - Addition of terminal ASCII pipeline timing trace visualizer, startup assembly prompt, Newton-Raphson divider, and dynamic CLI branch predictor selection.
5. **Milestone 5 — The CPI Audit & Bug Fixes (`e491088`)**:
   - Diagnosis of the 2.560 CPI discrepancy; discovered cold-start vs steady-state phenomenon; added 6 extended benchmarks (`*_long.s`); fixed branch flush penalty logic.
6. **Milestone 6 — ISA Single Source of Truth Refactor (`fa33baa`, `4dceaf7`)**:
   - Complete elimination of duplicate integer opcode enum from `Types.hpp`; all 43 files unified strictly around canonical 5-bit binary strings in `common/ISA.hpp`.

---

# Chapter K4: Honest Limitations & What NOT to Claim

In a university project defense, examiners respect honesty far more than exaggerated claims. Here is what you should and should NOT claim:

### What You CAN Confidently Claim:
- 100% pure modern C++17 simulator (12,929 lines, zero external dependencies).
- Cycle-accurate in-order execution matching textbook SimpleRisc ISA.
- Complete memory hierarchy (L1-L3 non-blocking caches, DDR4/DDR5 DRAM, 48-bit Radix-4 VM).
- Zero integer opcode enum duplicates (single source of truth).
- 78/78 tests and 231/231 golden checks pass with 100% verification.

### What You Should NOT Claim (Known Limitations):
1. **Out-of-Order Execution**: The core is strictly **In-Order** (Scoreboarding / Tomasulo / Reorder Buffer are NOT implemented).
2. **Branch Mispredict Delay Slots**: Branch delay slots are not used; branches are handled via hardware squashing and dynamic 2-bit bimodal/gshare prediction.
3. **Multicore / Cache Coherence**: The processor is single-core; multiprocessor snooping protocols (MESI/MOESI) are NOT implemented.
4. **Physical Synthesis**: The project is a cycle-accurate software simulator in C++, not synthesized Verilog on an FPGA. Clock frequencies (500 MHz, 800 MHz) are based on standard 28nm standard-cell gate delay modeling, not physical place-and-route.

---

### 5. Summary: If You Only Remember 5 Things
1. **78/78 tests** and **231/231 golden checks** pass with 100% success.
2. Verified with **AddressSanitizer and UndefinedBehaviorSanitizer** (zero memory leaks).
3. The project underwent two major refactors: the **CPI Audit** and the **String ISA Refactor**.
4. Use **`make demo-pipe`**, **`make demo-mem`**, and **`make demo-vm`** for live presentation demos.
5. Honestly acknowledge that the CPU is **in-order single-core**; examiners love mature technical honesty!
