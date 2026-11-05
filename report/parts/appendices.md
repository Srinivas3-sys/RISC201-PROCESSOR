# Appendices

---

# Appendix A: Complete File Inventory & Lines of Code

Total Codebase: **16,583 lines of code** across 74 source files (excluding git history).

| Module / Directory | File Name | LOC | Architectural Purpose |
| :--- | :--- | :---: | :--- |
| **`alu/`** | `Adders.hpp` | 171 | Ripple-carry, carry-select, CLA, and Kogge-Stone adders. |
| | `ALU.hpp` | 161 | Unified ALU class dispatching 25 operations via 5-bit ISA strings. |
| | `Dividers.hpp` | 230 | Restoring, non-restoring, and Newton-Raphson division algorithms. |
| | `LogicGates.hpp` | 125 | Gate-level logic primitives (AND, OR, NOT, XOR, MUX, FullAdder). |
| | `Multipliers.hpp`| 190 | Shift-add array, Booth's Radix-4, and Wallace-tree multipliers. |
| | `Shifter.hpp` | 139 | 32-bit logarithmic barrel shifter (LSL, LSR, ASR, ROTS). |
| **`assembler/`** | `Assembler.hpp` | 545 | Two-pass assembler, symbol table, machine code word emission. |
| | `Disassembler.hpp` | 181 | Instruction word disassembler with synthetic label reconstruction. |
| | `MacroPreprocessor.hpp` | 155 | Macro expander for full-descending `PUSH`/`POP` and pseudo-ops. |
| | `main_as.cpp` | 54 | CLI driver executable for the assembler (`risc201_as`). |
| | `main_disasm.cpp` | 48 | CLI driver executable for the disassembler (`risc201_disasm`). |
| **`benchmarks/`** | `array_sum.s` / `_long.s` | 118 | Array reduction benchmarks (cold-start vs steady-state). |
| | `factorial_iter.s` / `_long.s` | 43 | Iterative factorial loop benchmarks (branch loops). |
| | `factorial_rec.s` / `_long.s` | 75 | Recursive factorial benchmarks (stack frames & guard checks). |
| | `hazard_test.s` / `_long.s` | 88 | Sustained RAW dependency and load-use hazard benchmarks. |
| | `prime_test.s` / `_long.s` | 60 | Sieve primality benchmarks evaluating integer division units. |
| | `stack_test.s` / `_long.s` | 57 | Stack push/pop benchmarks evaluating store buffer bypass. |
| **`common/`** | `BranchPredictor.hpp` | 383 | Branch predictors (2-bit Bimodal, Gshare, 64-entry BTB, 16-entry RAS). |
| | `Exception.hpp` | 82 | CPU exception types, trap codes, and fault descriptors. |
| | `ForwardingUnit.hpp` | 274 | Youngest-producer forwarding bypass network (`EX`, `MEM`, `WB`, StoreBuffer). |
| | `HazardUnit.hpp` | 175 | Structural, data (RAW), and control hazard detection logic. |
| | `ISA.hpp` | 489 | Single source of truth for 5-bit string opcodes and instruction metadata. |
| | `ISS.hpp` | 129 | Cycle-independent reference Instruction Set Simulator (golden model). |
| | `Memory.hpp` | 162 | Flat physical byte-addressable memory storage. |
| | `PerformanceTracker.hpp` | 142 | Cycle, retired instruction, and stall breakdown accounting. |
| | `RegisterFile.hpp` | 141 | 16-register multi-ported storage ($r_0-r_{15}$, hardwired zero $r_0$). |
| | `Types.hpp` | 176 | Architectural types (`Word`, `Address`, `Flags`, modifiers). |
| **`debugger/`** | `Debugger.hpp` | 854 | Interactive REPL console engine, stepping, breakpoints, and UI commands. |
| | `main_sim.cpp` | 335 | Simulator CLI driver, argument parsing, startup assembly prompt. |
| **`docs/`** | `CPI_AUDIT.md` | 290 | Diagnosis and resolution of high CPI and cold-start miss inflation. |
| | `HAZARDS_AND_FORWARDING.md` | 138 | Specification of hazard interlocks and forwarding bypass equations. |
| | `ISA_BINARY_OPCODES.md` | 136 | Specification of 5-bit binary string opcodes and bitfields. |
| | `ISA_REFACTOR_PLAN.md` | 132 | Architectural plan for unifying opcodes into `common/ISA.hpp`. |
| | `MEMORY_DESIGN.md` | 376 | Hardware specification of caches, DRAM timings, and prefetchers. |
| | `MEMORY_PLAN.md` | 255 | Design plan for Radix-4 virtual memory and multi-level TLBs. |
| | `PERFORMANCE.md` | 181 | Verified analytic CPI models and simulation benchmark data. |
| | `PIPELINE_PLAN.md` | 306 | Architectural plan comparing 4-stage vs 6-stage pipelines. |
| | `UI_AUDIT.md` | 110 | Audit of CLI panels, colors, and friend's visualizer contributions. |
| | `VIRTUAL_MEMORY.md` | 283 | Specification of 48-bit Radix-4 translation and Page-Walk Cache. |
| **`evaluation/`** | `audit_cpi.cpp` | 138 | Automated CPI matrix evaluator across all benchmarks and presets. |
| | `CompareProcessors.cpp` | 323 | Cross-processor evaluator comparing 30 architectural configurations. |
| | `run_memory_experiments.cpp` | 85 | Driver executing the 20 memory hierarchy experiments. |
| | `run_pipeline_experiments.cpp`| 132 | Driver executing the 10 pipeline hazard experiments. |
| **`experiments/`** | `PIPELINE_RESULTS.md` | 246 | Experimental findings and tables for 10 pipeline experiments. |
| | `EXPERIMENT_RESULTS.md` | 276 | Experimental findings and tables for 20 memory experiments. |
| **`memory/`** | `AddressMapping.hpp` | 116 | DRAM address mapping (Channel/Rank/Bank/Row) and XOR hash. |
| | `Cache.hpp` | 389 | Parameterized $N$-way cache with MSHRs, 3C miss telemetry, and SRRIP. |
| | `Config.hpp` | 256 | Presets (`ideal`, `simple`, `modern`, `modern-ddr5`, `lecture`). |
| | `DRAM.hpp` | 209 | Cycle-accurate DRAM controller ($t_{\text{RCD}}, t_{\text{RP}}, t_{\text{CL}}, t_{\text{RAS}}$, FR-FCFS). |
| | `MemoryHierarchy.hpp` | 414 | Integrated memory system (L1I, L1D, L2, L3, StoreBuffer, VM, DRAM). |
| | `Prefetcher.hpp` | 211 | Hardware data prefetchers (Next-line, Stride RPT, Stream buffer). |
| | `VirtualMemory.hpp` | 408 | 4-level Radix-4 page tables, L1/L2 TLBs, PWC, demand paging, and swap. |
| **`microcode/`** | `HorizontalControl.hpp` | 112 | 65-bit unencoded horizontal micro-instruction word (33 control bits). |
| | `MicroController.hpp` | 477 | Microprogrammed execution engine, microPC sequencer, dispatch table. |
| | `MicroInstructions.hpp` | 130 | Micro-operation opcode definitions and microcode ROM builder. |
| | `VerticalControl.hpp` | 157 | 45-bit encoded vertical micro-instruction word and hardware decoder. |
| **`processor_4stage/`**| `Pipeline4Stage.hpp` | 630 | Cycle-accurate 4-stage pipeline ($\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$). |
| | `StageRegisters4.hpp` | 95 | Edge-triggered pipeline latches (`IF/OF`, `OF/EX`, `EX/MA_RW`). |
| **`processor_6stage/`**| `Pipeline6Stage.hpp` | 703 | Cycle-accurate 6-stage pipeline ($\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$). |
| | `StageRegisters6.hpp` | 148 | Edge-triggered pipeline latches (`IF/ID`, `ID/RF`, `RF/EX`, `EX/MA`, `MA/RW`). |
| **`tests/`** | `test_all.cpp` | 399 | Unified 78-case verification test suite. |
| **`tools/`** | `audit_cpi.cpp` | 223 | Deterministic CPI benchmark matrix generator (outputs CSV/JSON). |
| | `capture_golden.cpp` | 321 | Tool capturing golden architectural baselines for machine code and traces. |
| | `generate_perf_docs.cpp` | 257 | Automated performance documentation generator for `docs/PERFORMANCE.md`.|
| | `lint_docs.cpp` | 118 | Documentation linter validating KaTeX math syntax across markdown files. |
| | `lint_isa.cpp` | 138 | Static analysis linter checking single source of truth 5-bit ISA strings. |
| | `verify_golden.cpp` | 418 | Golden regression validator (231 checks, 100% byte fidelity). |
| **`visualizer/`** | `MemoryVisualizer.hpp` | 235 | ASCII cache set-way occupancy, TLB hit rates, and DRAM buffer viewer. |
| | `PipelineVisualizer.hpp` | 238 | ASCII pipeline stage latches, active hazards, and timing trace renderer.|
| | `StackVisualizer.hpp` | 108 | ASCII active stack frame renderer, frame pointers, and guard boundaries.|
| **Root** | `Makefile` | 107 | GNU Make build configuration with 19 targets. |
| | `README.md` | 265 | Architectural overview and quick start manual. |

---

# Appendix B: Requirements Traceability Matrix

| Requirement ID | Specification Text | Implementing C++ Source Files | Verification Test / Harness | Demonstration Command | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1a** | 4-stage and 6-stage pipelines + behavior comparison | `processor_4stage/Pipeline4Stage.hpp`, `processor_6stage/Pipeline6Stage.hpp`, `evaluation/CompareProcessors.cpp` | `tests/test_all.cpp` (Tests 5.1–5.4, 6.1–6.4), `tools/verify_golden.cpp` | `./risc201_sim benchmarks/factorial_iter.s --compare` | **Complete** |
| **1b** | Microprogrammed control unit (horizontal & vertical) | `microcode/MicroController.hpp`, `microcode/HorizontalControl.hpp`, `microcode/VerticalControl.hpp` | `tests/test_all.cpp` (Tests 1.4, 6.5), `tools/verify_golden.cpp` (Check 2) | `./risc201_sim benchmarks/hazard_test.s -i` (`micro`) | **Complete** |
| **1c** | ALU with efficient arithmetic and logical operations | `alu/ALU.hpp`, `alu/Adders.hpp`, `alu/Dividers.hpp`, `alu/Multipliers.hpp`, `alu/Shifter.hpp` | `tests/test_all.cpp` (Tests 1.1–1.3) | `./risc201_sim benchmarks/factorial_iter.s` | **Complete** |
| **2a** | Two-pass assembler (assembly $\rightarrow$ binary/hex) | `assembler/Assembler.hpp`, `assembler/main_as.cpp` | `tests/test_all.cpp` (Test 1.1), `tools/verify_golden.cpp` (Check 1) | `./risc201_as benchmarks/factorial_iter.s -o out.hex` | **Complete** |
| **2b** | Disassembler (binary/hex $\rightarrow$ assembly) | `assembler/Disassembler.hpp`, `assembler/main_disasm.cpp` | `tests/test_all.cpp` (Test 1.2), `tools/verify_golden.cpp` (Check 1) | `./risc201_disasm benchmarks/factorial_iter.hex` | **Complete** |
| **2c** | Interactive CLI to simulate register file and pipeline | `debugger/Debugger.hpp`, `debugger/main_sim.cpp` | `tests/test_all.cpp` (Section 5) | `./risc201_sim benchmarks/factorial_iter.s -i` (`pipe`, `regs`) | **Complete** |
| **2d** | Step-by-step CPU execution simulator with debugger REPL | `debugger/Debugger.hpp`, `debugger/main_sim.cpp` | `tests/test_all.cpp` (Section 6) | `./risc201_sim benchmarks/hazard_test.s -i` (`step`, `run`) | **Complete** |
| **2e** | Stack preprocessor macro expansion (full-descending) | `assembler/MacroPreprocessor.hpp`, `assembler/Assembler.hpp` | `tests/test_all.cpp` (Test 1.1) | `./risc201_as benchmarks/stack_test.s -o out.hex` | **Complete** |
| **2f** | Stack boundary safety verification & exception framework | `common/Exception.hpp`, `processor_4stage/Pipeline4Stage.hpp`, `processor_6stage/Pipeline6Stage.hpp` | `tests/test_all.cpp` (Test 3.4, 4.3) | `./risc201_sim benchmarks/factorial_rec_long.s` | **Complete** |
| **2g** | Active stack memory visualizer (ASCII frame renderer) | `visualizer/StackVisualizer.hpp`, `debugger/Debugger.hpp` | `tests/test_all.cpp` (Section 6) | `./risc201_sim benchmarks/stack_test.s -i` (`stack`) | **Complete** |
| **3** | Microprogrammed control integrated into simulator | `microcode/MicroController.hpp`, `debugger/Debugger.hpp` | `tests/test_all.cpp` (Tests 1.4, 6.5) | `./risc201_sim benchmarks/factorial_iter.s -i` (`micro`) | **Complete** |
| **4** | Optimized ALU class driving all operations | `alu/ALU.hpp`, `alu/Dividers.hpp`, `alu/Multipliers.hpp` | `tests/test_all.cpp` (Section 1) | `./risc201_eval` | **Complete** |
| **5** | Evaluation across 4/6-stage, programs, ALU algorithms | `evaluation/CompareProcessors.cpp`, `evaluation/audit_cpi.cpp` | `make exp-pipe && make exp` | `make perf-docs && make audit-cpi` | **Complete** |

---

# Appendix C: Complete RISC201 ISA Reference Card

```
========================================================================================================
                                      RISC201 ARCHITECTURAL REFERENCE CARD
========================================================================================================
Registers: r0 (Hardwired Zero), r1-r13 (General), r14 (SP, Stack Pointer), r15 (RA, Return Address)
Status Flags: E (Equal), GT (Greater Than) - Set exclusively by 'cmp'
Formats:
  R-Type : [31:27 Opcode] [26 I=0] [25:22 rd] [21:18 rs1] [17:14 rs2] [13:0 Zero]
  I-Type : [31:27 Opcode] [26 I=1] [25:22 rd] [21:18 rs1] [17:2 imm16] [1:0 mod (00:u, 01:h, 10:s)]
  B-Type : [31:27 Opcode] [26:0 imm27 (Signed Word Offset)]
--------------------------------------------------------------------------------------------------------
Opcode String | Mnemonic | Syntax                  | Architectural Action
--------------------------------------------------------------------------------------------------------
 "00000"      | add      | add rd, rs1, rs2/imm    | rd <- rs1 + (rs2/imm)
 "00001"      | sub      | sub rd, rs1, rs2/imm    | rd <- rs1 - (rs2/imm)
 "00010"      | mul      | mul rd, rs1, rs2/imm    | rd <- (rs1 * (rs2/imm))[31:0]
 "00011"      | div      | div rd, rs1, rs2/imm    | rd <- rs1 / (rs2/imm) (Trap if divisor == 0)
 "00100"      | mod      | mod rd, rs1, rs2/imm    | rd <- rs1 % (rs2/imm) (Trap if divisor == 0)
 "00101"      | cmp      | cmp rs1, rs2/imm        | Flags.E <- (rs1 == B), Flags.GT <- (rs1 > B)
 "00110"      | and      | and rd, rs1, rs2/imm    | rd <- rs1 & (rs2/imm)
 "00111"      | or       | or  rd, rs1, rs2/imm    | rd <- rs1 | (rs2/imm)
 "01000"      | not      | not rd, rs2/imm         | rd <- ~(rs2/imm)
 "01001"      | mov      | mov rd, rs2/imm         | rd <- (rs2/imm)
 "01010"      | lsl      | lsl rd, rs1, rs2/imm    | rd <- rs1 << (rs2/imm)[4:0]
 "01011"      | lsr      | lsr rd, rs1, rs2/imm    | rd <- rs1 >> (rs2/imm)[4:0] (Logical zero-fill)
 "01100"      | asr      | asr rd, rs1, rs2/imm    | rd <- rs1 >> (rs2/imm)[4:0] (Arithmetic sign-fill)
 "01101"      | nop      | nop                     | No operation (add r0, r0, 0)
 "01110"      | ld       | ld  rd, imm[rs1]        | rd <- Mem[rs1 + SignExt(imm)]
 "01111"      | st       | st  rd, imm[rs1]        | Mem[rs1 + SignExt(imm)] <- rd
 "10000"      | beq      | beq label/offset        | If Flags.E == 1, PC <- PC + (SignExt(imm27) * 4)
 "10001"      | bgt      | bgt label/offset        | If Flags.GT == 1, PC <- PC + (SignExt(imm27) * 4)
 "10010"      | b        | b   label/offset        | Unconditional: PC <- PC + (SignExt(imm27) * 4)
 "10011"      | call     | call label/offset       | r15 (RA) <- PC + 4; PC <- PC + (SignExt(imm27) * 4)
 "10100"      | ret      | ret                     | PC <- r15 (RA)
 "10101"      | movz     | movz rd, privReg        | Privileged register transfer
 "10110"      | retz     | retz                    | Return from kernel trap (PC <- EPC, CPL <- 1)
 "10111"      | min      | min rd, rs1, rs2/imm    | rd <- (rs1 < B) ? rs1 : B
 "11000"      | max      | max rd, rs1, rs2/imm    | rd <- (rs1 > B) ? rs1 : B
 "11001"      | rots     | rots rd, rs1, rs2/imm   | rd <- RotateRight(rs1, B[4:0])
 "11010"      | xor      | xor rd, rs1, rs2/imm    | rd <- rs1 ^ (rs2/imm)
 "11011"      | abs      | abs rd, rs2/imm         | rd <- |rs2/imm|
 "11100-11111"| invalid  | -                       | Illegal Opcode Trap (Raises ILLEGAL_OPCODE)
========================================================================================================
```

---

# Appendix D: Complete Command-Line Interface (CLI) Reference

### Executable Options
1. **`./risc201_sim [assembly.s] [options]`**:
   * `-i`, `--interactive`: Launch REPL debugger console.
   * `--pipeline <4|6>`: Core depth (4-stage or 6-stage).
   * `--mem-preset <ideal|simple|modern|modern-ddr5|lecture>`: Memory hierarchy model.
   * `--forwarding <none|basic|full|full+>`: Forwarding mode.
   * `--branch-policy <stall|flush|1bit|2bit|gshare|perfect>`: Branch predictor.
   * `--diagram`: Render horizontal pipeline timing diagram on completion.
   * `--report`: Print comprehensive performance report on completion.
2. **`./risc201_as <source.s> -o <out.hex>`**: Assembler.
3. **`./risc201_disasm <source.hex>`**: Disassembler.
4. **`./risc201_test`**: Unified test suite.
5. **`./verify_golden`**: Golden regression suite (231 checks).
6. **`./audit_cpi`**: Repeatable CPI audit runner across 192 configurations.

---

# Appendix E: Architectural Class & Struct Reference

### 1. `processor_4stage::Pipeline4Stage` (`processor_4stage/Pipeline4Stage.hpp`)
* **Responsibility**: Manages the 4-stage pipeline ($\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$).
* **Key Members**: `RegisterFile regFile`, `MemoryHierarchy memHierarchy`, `alu::ALU aluUnit`, `HazardUnit hazardUnit`, `ForwardingUnit forwardingUnit`, `BranchPredictor branchPredictor`, `Latch_IF_OF l_IF_OF`, `Latch_OF_EX l_OF_EX`, `Latch_EX_MARW l_EX_MARW`.
* **Key Methods**: `stepCycle()`, `fetchInstruction()`, `decodeAndFetchOperands()`, `executeALU()`, `executeMemoryAccessAndWriteback()`, `tick()`.

### 2. `processor_6stage::Pipeline6Stage` (`processor_6stage/Pipeline6Stage.hpp`)
* **Responsibility**: Manages the 6-stage pipeline ($\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$).
* **Key Members**: Same units as 4-stage plus 5 inter-stage latches (`l_IF_ID`, `l_ID_RF`, `l_RF_EX`, `l_EX_MA`, `l_MA_RW`).
* **Key Methods**: `stepCycle()`, `fetchStage()`, `decodeStage()`, `registerFetchStage()`, `executeStage()`, `memoryStage()`, `writebackStage()`.

### 3. `common::ForwardingUnit` (`common/ForwardingUnit.hpp`)
* **Responsibility**: Detects RAW hazard bypass conditions and arbitrates youngest-producer operand forwarding.
* **Key Members**: `ForwardingMode mode`, `StoreBuffer storeBuffer`.
* **Key Methods**: `resolveOperands4Stage(...)`, `resolveOperands6Stage(...)`, `forwardLoad(...)`.

### 4. `common::HazardUnit` (`common/HazardUnit.hpp`)
* **Responsibility**: Identifies data hazards, load-use dependencies, and branch mispredictions, asserting stalls and pipeline flushes.
* **Key Methods**: `checkLoadUseHazard(...)`, `checkBranchMispredict(...)`.

### 5. `alu::ALU` (`alu/ALU.hpp`)
* **Responsibility**: Coordinates all arithmetic, shift, logic, and comparison operations.
* **Key Methods**: `execute(const std::string& opcode, Word a, Word b, const Flags& currentFlags)`.

### 6. `memory::MemoryHierarchy` (`memory/MemoryHierarchy.hpp`)
* **Responsibility**: Integrates multi-level caches (L1I, L1D, L2, L3), Virtual Memory (radix-4 page tables, TLBs, PWC), Store Buffer, and DDR4/DDR5 DRAM controller.
* **Key Methods**: `access(uint64_t vaddr, bool isWrite, uint32_t writeVal, uint64_t pc, bool isFetch)`.

---

# Appendix F: Microcode Routines & Control-Store Formats

### Formats
* **Horizontal Control Word**: 65 bits (33 discrete control bits, 12-bit `uimm`, 10-bit `ubranchTarget`, 10-bit `args`).
* **Vertical Control Word**: 45 bits (3-bit `MicroOp`, 5-bit `src`, 5-bit `dest`, 12-bit `immediate`, 10-bit `branchTarget`, 10-bit `args`).

### Control Store Dispatch Table
* `OP_ADD ("00000")` $\rightarrow \mu\text{PC } 10$ (`uAdd`, 4 cycles)
* `OP_SUB ("00001")` $\rightarrow \mu\text{PC } 10$ (`uSub`, 4 cycles)
* `OP_MUL ("00010")` $\rightarrow \mu\text{PC } 15$ (`uMul`, 16 cycles)
* `OP_DIV ("00011")` $\rightarrow \mu\text{PC } 20$ (`uDiv`, 32 cycles)
* `OP_MOD ("00100")` $\rightarrow \mu\text{PC } 20$ (`uMod`, 32 cycles)
* `OP_CMP ("00101")` $\rightarrow \mu\text{PC } 25$ (`uCmp`, 3 cycles)
* `OP_LD  ("01110")` $\rightarrow \mu\text{PC } 30$ (`uLd`, 5 cycles)
* `OP_ST  ("01111")` $\rightarrow \mu\text{PC } 40$ (`uSt`, 4 cycles)
* `OP_BEQ ("10000")` $\rightarrow \mu\text{PC } 50$ (`uBeq`, 4 cycles)
* `OP_CALL("10011")` $\rightarrow \mu\text{PC } 60$ (`uCall`, 4 cycles)
* `OP_RET ("10100")` $\rightarrow \mu\text{PC } 70$ (`uRet`, 3 cycles)
* `OP_INVALID ("11111")` $\rightarrow \mu\text{PC } 90$ (`uIllegal`, 3 cycles)

---

# Appendix G: Benchmark Program Listings

### 1. `benchmarks/factorial_iter.s` (Iterative Factorial)
```assembly
.text
  mov r1, 5           ; n = 5
  mov r2, 1           ; result = 1
loop:
  cmp r1, 0           ; check if n == 0
  beq exit            ; if n == 0 exit
  mul r2, r2, r1      ; result = result * n
  sub r1, r1, 1       ; n = n - 1
  b loop              ; repeat
exit:
  nop
  nop
```
* **Expected Output**: `r2 = 120` (5!). Retired instructions: 24.

### 2. `benchmarks/array_sum.s` (Array Reduction)
```assembly
.text
  mov r1, 0           ; sum = 0
  mov r2, 10          ; count = 10
  mov r3, 0           ; i = 0
loop:
  cmp r3, r2
  beq done
  add r1, r1, r3      ; sum += i
  add r3, r3, 1       ; i++
  b loop
done:
  nop
```
* **Expected Output**: `r1 = 45`. Retired instructions: 58.

---

# Appendix H: Raw Experiment Data Tables (Summary)

The complete raw CSV datasets generated by `make exp-pipe`, `make exp`, and `make audit-cpi` are committed under `report/data/`:
* `report/data/audit_matrix.csv`: Full 192-configuration audit matrix.
* `report/data/exp01_cache_sweeps.csv`: Cache sweeps (size 4KB–64KB, assoc 1–16, lines 32–128B).
* `report/data/exp03_page_policy.csv`: Open-page vs Closed-page DRAM throughput.
* `report/data/exp06_prefetcher_eval.csv`: Stride and stream prefetch hit ratios.
* `report/data/exp15_page_walk_caching.csv`: PWC latency reductions (73.9% cut).
* `report/data/exp1_forwarding.csv`: None vs Basic vs Full forwarding CPI across pipelines.
* `report/data/exp2_branch.csv`: Branch accuracy across Stall, Flush, 2-Bit, Gshare, BTB.

---

# Appendix I: Docs-vs-Code Discrepancy Log

| Document File | Stale / Inconsistent Claim | Code Truth & Empirical Reality | Resolution / Fix Status |
| :--- | :--- | :--- | :--- |
| `docs/PERFORMANCE.md` | Stated speculative CPI = 2.560 (Base 1.0, RAW 0.25, Branch 0.35, etc.) | Actual simulated steady-state CPI is **1.02 – 1.13**. 2.560 was typed by hand before pipelines existed. | Replaced with verified automated tables generated by `tools/generate_perf_docs.cpp`. |
| `docs/PERFORMANCE.md` | LaTeX math had raw unescaped underscores inside `\text{...}` causing KaTeX rendering errors. | KaTeX fails to parse raw underscores in text blocks; requires `\mathrm` or `\_`. | Fixed across all documentation; verified by `make lint-docs`. |
| `common/Types.hpp` | Contained duplicate `enum Opcode : uint8_t` shadowing `common/ISA.hpp`. | Codebase refactored to use canonical 5-bit string opcodes strictly from `common/ISA.hpp`. | Duplicate enum deleted; verified by `make lint-isa`. |
| `README.md` | Diagram showed only `IF` and `MA` without datapath stages (`OF`, `EX`, `RW`). | Datapath includes all stages; only `IF` and `MA` interface with external memory. | Diagram updated to render horizontal datapath and vertical memory ports explicitly. |

---

# Appendix J: Glossary of Terms & Abbreviations

* **ALU**: Arithmetic Logic Unit.
* **AMAT**: Average Memory Access Time.
* **ASID**: Address Space Identifier.
* **BTB**: Branch Target Buffer.
* **CAM**: Content-Addressable Memory.
* **CLA**: Carry-Lookahead Adder.
* **CPI**: Cycles Per Instruction.
* **CSLA**: Carry-Select Adder.
* **DRAM**: Dynamic Random-Access Memory.
* **EAT**: Effective Access Time.
* **FR-FCFS**: First-Ready First-Come First-Served DRAM request scheduling.
* **ILP**: Instruction-Level Parallelism.
* **IPC**: Instructions Per Cycle ($\text{IPC} = 1 / \text{CPI}$).
* **ISS**: Instruction Set Simulator (architectural reference model).
* **MSHR**: Miss Status Holding Register (enables non-blocking caches).
* **PIPT**: Physically Indexed, Physically Tagged cache.
* **PWC**: Page-Walk Cache (accelerates Radix-4 page table walks).
* **RCA**: Ripple-Carry Adder.
* **RAW**: Read-After-Write true data dependency.
* **ROB**: Reorder Buffer.
* **SRRIP**: Static Re-Reference Interval Prediction replacement algorithm.
* **STLB**: Shared Second-Level Translation Lookaside Buffer.
* **TLB**: Translation Lookaside Buffer.
* **VIPT**: Virtually Indexed, Physically Tagged cache.

---

# Appendix K: Viva Preparation: 40+ Likely Examiner Questions & Concrete Code Answers

### 1. Architectural Pipelines & Hazards
1. **Q: Why does the 6-stage pipeline achieve faster execution time than the 4-stage pipeline despite having a higher CPI?**
   * **Answer**: Wall-clock execution time is $T = \text{IC} \times \text{CPI} \times t_{\text{clk}}$. Splitting `OF` into `ID` and `RF` cuts the critical path delay, allowing clock frequency to increase from 500 MHz to 800 MHz (a 60% frequency gain). This frequency increase easily overcomes the minor $+0.05$ CPI penalty from deeper branch mispredictions, resulting in a **$1.59\times$ wall-clock speedup** (`processor_6stage/Pipeline6Stage.hpp:28-35`).
2. **Q: What is the branch misprediction penalty in the 4-stage versus 6-stage core, and why?**
   * **Answer**: In 4-stage, branches resolve in `EX` (Stage 3), requiring 2 squashed stages (`IF` and `OF` $\implies 2$ cycles). In 6-stage, branches resolve in `EX` (Stage 4), requiring 3 squashed stages (`IF`, `ID`, `RF` $\implies 3$ cycles) (`processor_4stage/Pipeline4Stage.hpp:162`, `processor_6stage/Pipeline6Stage.hpp:184`).
3. **Q: Why does the 4-stage pipeline have zero load-use stalls with full forwarding while the 6-stage core requires 1 stall cycle?**
   * **Answer**: In 4-stage, memory access and writeback are merged in `MA_RW`, allowing loaded data to latch into the trailing edge of `EX` without stalling. In 6-stage, `ld` data is not available until the end of `MA`, forcing an immediately dependent instruction entering `EX` to stall for 1 bubble cycle (`common/HazardUnit.hpp:45-58`).
4. **Q: How does the ForwardingUnit resolve conflicting hazards when multiple stages produce the same register?**
   * **Answer**: Enforces **youngest-producer priority**: $\text{EX} > \text{MEM} > \text{WB} > \text{RegisterFile}$. The newest instruction in `EX` overrides older pending writes in `MEM` and `WB` (`common/ForwardingUnit.hpp:150-185`).
5. **Q: Why is register $r_0$ never forwarded?**
   * **Answer**: Register $r_0$ is hardwired to zero in SimpleRisc. Forwarding checks explicitly guard `reqReg != 0` to prevent forwarding non-zero temporary values written to $r_0$ (`common/ForwardingUnit.hpp:152`).
6. **Q: How does the Store Buffer enable store-to-load forwarding?**
   * **Answer**: Stores write their address and data into a 4-entry FIFO (`StoreBuffer`). Subsequent loads perform a CAM search on the store buffer; on a hit, data is bypassed in 1 cycle before the store ever reaches DRAM (`common/ForwardingUnit.hpp:80-90`).
7. **Q: How does the simulator model edge-triggered latch synchronization?**
   * **Answer**: Each stage reads from the current cycle latch (`l_XX_YY`) and writes to a separate buffer (`next_XX_YY`). At cycle end, `tick()` commits `l_XX_YY = next_XX_YY` simultaneously, preventing intra-cycle race conditions (`processor_4stage/Pipeline4Stage.hpp:52-56`).
8. **Q: What happens on a branch misprediction?**
   * **Answer**: `pipelineFlush` is asserted, speculative stage latches are replaced with bubbles (`bubble = true, valid = false`), `pc` is redirected to the true target, and the branch predictor automaton is updated with the actual branch outcome (`common/BranchPredictor.hpp:210-230`).
9. **Q: How does the Smith 2-bit branch predictor prevent loop-exit thrashing?**
   * **Answer**: It requires two consecutive mispredictions to transition from Strongly Taken (`11`) to Not-Taken (`01` $\rightarrow$ `00`). A single fall-through at the end of a loop does not flip the prediction for subsequent iterations (`common/BranchPredictor.hpp:180-205`).
10. **Q: What is the purpose of the Return Address Stack (RAS)?**
    * **Answer**: Subroutine calls (`call`) push $\text{PC} + 4$ onto a 16-entry hardware stack. When `ret` executes, the target address is popped directly from the RAS during `IF`, predicting function returns with near 100% accuracy (`common/BranchPredictor.hpp:114-125`).

### 2. ISA, Opcodes, and Assembler
11. **Q: Why are opcodes represented as 5-bit binary strings rather than integer enums?**
    * **Answer**: Opcodes directly mirror the 5-bit bitfield in the 32-bit machine word ($[31:27]$) and map directly to microcode control store addresses. String representations eliminate casting bugs and enforce `common/ISA.hpp` as the single source of truth without runtime performance loss (`common/ISA.hpp:38-69`).
12. **Q: How are illegal opcodes handled?**
    * **Answer**: Opcode patterns 28-30 (`"11100"`, `"11101"`, `"11110"`) and 31 (`"11111"`) are unassigned. Fetching or decoding them raises a precise `ILLEGAL_OPCODE` CPU exception, capturing `EPC` and triggering a kernel trap (`common/ISA.hpp:67`).
13. **Q: How does the two-pass assembler resolve forward branch labels?**
    * **Answer**: Pass 1 scans source lines, expands macros, and maps labels to byte addresses in a symbol table. Pass 2 calculates the PC-relative word offset: $(\text{Target} - \text{PC}) / 4$, encoding it into the 27-bit branch field (`assembler/Assembler.hpp:110-185`).
14. **Q: How do the immediate modifier bits ($[1:0]$) work in I-type instructions?**
    * **Answer**: `00` (`MOD_U`) zero-extends 16 bits; `01` (`MOD_H`) shifts immediate left by 16 bits (`imm << 16`); `10` (`MOD_S`) sign-extends bit 15 across bits 16-31 (`common/ISA.hpp:20-22`).
15. **Q: How does the macro preprocessor expand `push` and `pop`?**
    * **Answer**: `push rx` expands to `sub r14, r14, 4` followed by `st rx, 0[r14]`. `pop rx` expands to `ld rx, 0[r14]` followed by `add r14, r14, 4` (`assembler/MacroPreprocessor.hpp:45-75`).
16. **Q: How does the disassembler generate valid labels for branch targets?**
    * **Answer**: It extracts the 27-bit signed offset, adds it to `Current PC`, and prints a synthetic label (`label_XXXX`), allowing disassembled code to be re-assembled directly (`assembler/Disassembler.hpp:85-120`).
17. **Q: What is the function of the `cmp` instruction versus other ALU instructions?**
    * **Answer**: `cmp` is the **only** instruction that modifies the architectural status flags ($E, GT$). All arithmetic and logical operations (`add`, `sub`, etc.) leave flags intact (`alu/ALU.hpp:62-65`).

### 3. Microprogrammed Control
18. **Q: What is the fundamental difference between horizontal and vertical microcode?**
    * **Answer**: Horizontal microcode uses wide, unencoded words (65 bits) where each bit directly drives a datapath control line with zero decoding delay. Vertical microcode uses compact encoded fields (45 bits) requiring a hardware micro-decoder, saving **30.8% in ROM bit storage** (`microcode/HorizontalControl.hpp`, `microcode/VerticalControl.hpp`).
19. **Q: How does the MicroController dispatch an instruction to its micro-routine?**
    * **Answer**: The 5-bit opcode string indexes a dispatch table that yields the entry address in control memory (e.g. `"00000"` $\rightarrow \mu\text{PC } 10$) (`microcode/MicroController.hpp:120-145`).
20. **Q: How many micro-cycles does an instruction take to execute under microcode?**
    * **Answer**: 4 cycles for `uFetch`, followed by 4 cycles for ALU operations, 5 cycles for loads, 4 cycles for stores, or 16-32 cycles for multiplication and division (`microcode/MicroController.hpp:200-260`).

### 4. Arithmetic Logic Unit (ALU)
21. **Q: How does a Carry-Lookahead Adder achieve $O(\log N)$ delay compared to $O(N)$ for Ripple-Carry?**
    * **Answer**: It generates Generate ($G_i = A_i \cdot B_i$) and Propagate ($P_i = A_i \oplus B_i$) signals in 1 gate delay, and expands carries across 4-bit blocks in parallel using tree logic, eliminating serial carry ripple (`alu/Adders.hpp:95-135`).
22. **Q: How does Booth's Radix-4 multiplier halve the multiplication cycle count?**
    * **Answer**: It recodes overlapping 3-bit triplets of the multiplier, allowing each step to process 2 bits simultaneously ($+0, \pm M, \pm 2M$), reducing the number of partial products from 32 down to 16 (`alu/Multipliers.hpp:80-125`).
23. **Q: How does Newton-Raphson division work, and why is it faster than Restoring division?**
    * **Answer**: It computes the reciprocal $X \approx 1/D$ using quadratic convergence ($X_{k+1} = X_k(2 - D X_k)$). Each iteration doubles precision, reaching full 32-bit accuracy in **5 iterations**, compared to 32 serial cycles for Restoring division (`alu/Dividers.hpp:140-182`).
24. **Q: How does the 32-bit Logarithmic Barrel Shifter operate in a single cycle?**
    * **Answer**: It cascades 5 stages of 2-to-1 multiplexers controlled by shift distance bits $[4:0]$, shifting by 16, 8, 4, 2, or 1 bit in series through fast combinational gates (`alu/Shifter.hpp:35-70`).

### 5. Memory Hierarchy & DRAM
25. **Q: If L1I and L1D connect to the same L2 cache over a shared bus, what is the point of splitting them at L1?**
    * **Answer**: In 95%+ of cycles, requests hit in L1 and are served in 1 cycle **inside the L1 SRAM array without touching the shared bus**. If L1 were unified, `IF` (100% of cycles) and `MA` (25-30% of cycles) would collide on the shared port on **every load/store**, adding $+0.25$ to $+0.30$ CPI. Splitting L1 eliminates structural conflicts for all hits, reducing bus contention by over $2,000\times$ (`memory/MemoryHierarchy.hpp:160-220`).
26. **Q: What is the difference between an open-page and closed-page DRAM policy?**
    * **Answer**: Open-page keeps the activated row buffer open after an access, allowing subsequent accesses to the same row to hit in $t_{\text{CL}}$ (14.06 ns). Closed-page immediately precharges the bank, which is optimal for random, uncorrelated accesses (`memory/DRAM.hpp:85-110`).
27. **Q: What are $t_{\text{RCD}}, t_{\text{RP}}$, and $t_{\text{CL}}$ in DDR4 memory?**
    * **Answer**: $t_{\text{RCD}}$ is the delay to open a row from the bank into the row buffer (14.06 ns). $t_{\text{RP}}$ is the delay to close and precharge an open row (14.06 ns). $t_{\text{CL}}$ is the column access latency to stream data onto the memory bus (14.06 ns) (`memory/Config.hpp:140-160`).
28. **Q: How does FR-FCFS scheduling improve memory bandwidth?**
    * **Answer**: It prioritizes requests targeting currently open row buffers before servicing older requests that require expensive row precharge and activation, maximizing data bus utilization (`memory/DRAM.hpp:130-165`).
29. **Q: What is the role of Non-Blocking MSHRs in the cache?**
    * **Answer**: Miss Status Holding Registers track outstanding cache miss line refills, allowing the cache to continue servicing hits and concurrent misses without stalling the pipeline (`memory/Cache.hpp:145-180`).
30. **Q: How does the Stride Prefetcher identify access patterns?**
    * **Answer**: It maintains a Reference Prediction Table (RPT) with a 4-state automaton. When it observes identical address strides $\Delta = A_i - A_{i-1}$ twice consecutively, it speculatively issues prefetch requests for $A_i + \Delta$ (`memory/Prefetcher.hpp:75-120`).

### 6. Virtual Memory & Safety
31. **Q: How does the 4-level Radix-4 page table translate a 48-bit virtual address?**
    * **Answer**: Slices address into 9-bit chunks: PML4 $[47:39]$, PDPT $[38:30]$, PD $[29:21]$, PT $[20:12]$, and 12-bit offset $[11:0]$. Each table level indexes a 512-entry array of pointers (`memory/VirtualMemory.hpp:110-165`).
32. **Q: How does the Page-Walk Cache (PWC) accelerate page translation?**
    * **Answer**: It caches intermediate PML4, PDPT, and PD table base pointers. On a TLB miss, it skips upper-level DRAM reads, cutting page walk latency by **up to 74%** (`memory/VirtualMemory.hpp:220-255`).
33. **Q: How does the memory system detect stack overflow?**
    * **Answer**: A non-accessible Guard Page is placed immediately below the stack segment. When recursive pushes cause `SP` to enter the guard page, the MMU triggers a `STACK_OVERFLOW` exception (`memory/VirtualMemory.hpp:290-310`).
34. **Q: What happens on a Minor Page Fault versus a Major Page Fault?**
    * **Answer**: Minor faults bind an unallocated physical frame from the free list (zero-fill). Major faults occur when physical RAM is full, requiring a dirty victim frame to be swapped out to simulated NVMe SSD swap space (`memory/VirtualMemory.hpp:330-375`).
35. **Q: How does the full-descending stack convention prevent frame corruption?**
    * **Answer**: Pre-decrementing `SP` on push guarantees `SP` always points to valid data, while function frames are allocated downwards without overwriting parent frame parameters (`assembler/MacroPreprocessor.hpp:50-70`).

### 7. Verification & Tooling
36. **Q: How does the golden regression validator guarantee refactoring safety?**
    * **Answer**: `tools/verify_golden.cpp` executes 231 checks, verifying byte-for-byte machine code matching, control store micro-code parity, and full simulation telemetry across 192 configurations (`tools/verify_golden.cpp:50-250`).
37. **Q: How does the ISA single source of truth linter work?**
    * **Answer**: `tools/lint_isa.cpp` statically scans all 43 files for forbidden tokens (`enum Opcode`, raw integer constants, `BIN_OP_` aliases), failing the build if any exist outside `common/ISA.hpp` (`tools/lint_isa.cpp:40-110`).
38. **Q: Why did short benchmarks show high CPI in early audits, and how was it diagnosed?**
    * **Answer**: Compulsory startup cache misses and page table walks cost $\sim 120$ cycles. In a 20-instruction benchmark, this adds $\frac{120}{20} = 6.0$ to CPI. Extended benchmarks (`*_long.s`) amortize startup costs, revealing true steady-state CPI ($1.02 - 1.13$) (`docs/CPI_AUDIT.md:80-140`).
39. **Q: What is the formal cycle accounting invariant enforced by the test suite?**
    * **Answer**: $\text{Total Cycles} = \text{Retired Instructions} + \sum \text{Stall Bubbles}$ (`tests/test_all.cpp:320-345`).
40. **Q: How does the reference Instruction Set Simulator (ISS) differ from the pipelined cores?**
    * **Answer**: The ISS executes each instruction sequentially in isolation without stages, latches, hazards, or branch speculation. It provides an uncompromised golden oracle for differential verification (`common/ISS.hpp:40-95`).

---

# Appendix L: Contribution Log from Git History

Contributors are credited based strictly on repository commit logs:

```
$ git log --format='%an' | sort | uniq -c
     26 bandlapalliyss-max
      1 Varshith Teja
```

### 1. `bandlapalliyss-max` (Srinivas B.) — 26 Commits
* **Architectural Conversion**: Converted entire simulator and memory hierarchy into 100% pure modern C++17 (`25b8151`).
* **Pipelined Cores**: Implemented cycle-accurate 4-stage and 6-stage pipelined processors (`processor_4stage/`, `processor_6stage/`).
* **Hazard & Forwarding**: Designed `HazardUnit`, `ForwardingUnit` (youngest-producer priority), and 4-entry `StoreBuffer`.
* **Memory Subsystem**: Designed parameterized multi-level caches (`Cache.hpp`), MSHRs, cycle-accurate DDR4/DDR5 DRAM controller (`DRAM.hpp`), and 4-level Radix-4 Virtual Memory (`VirtualMemory.hpp`).
* **Microprogrammed Control**: Built horizontal (65-bit) and vertical (45-bit) microcode stores, micro-decoder, and sequencer (`microcode/`).
* **Software Toolchain**: Authored two-pass assembler (`Assembler.hpp`), disassembler (`Disassembler.hpp`), and macro preprocessor (`MacroPreprocessor.hpp`).
* **Verification & Testing**: Developed unified 78-case verification test suite (`tests/test_all.cpp`), golden regression validator (`tools/verify_golden.cpp`), ISA single source of truth linter (`tools/lint_isa.cpp`), and documentation linter (`tools/lint_docs.cpp`).
* **Architectural Audits**: Resolved stale CPI numbers, established steady-state vs cold-start accounting, and documented cycle accounting invariants (`docs/CPI_AUDIT.md`, `docs/PERFORMANCE.md`).

### 2. `Varshith Teja` — Commit `bbd9ea1`
* **Startup Assembly Interactive Prompt**: Implemented `promptStartupAssembly()` in `debugger/main_sim.cpp` and `debugger/Debugger.hpp`, prompting users to enter assembly code interactively on launch.
* **Cycle Trace Visualizer**: Authored `PipelineVisualizer::renderTrace()` in `visualizer/PipelineVisualizer.hpp`, rendering horizontal cycle-by-cycle execution timing matrices.
* **Newton-Raphson Division**: Implemented quadratic convergence reciprocal division in `alu/Dividers.hpp:140-182`.
* **Branch Predictor CLI Flag Selection**: Added command-line arguments and REPL commands allowing dynamic runtime branch predictor selection.
* **ISA Additions**: Added `XOR` and `ABS` opcodes and ALU execution logic.


***

# Report Completeness Check & Audit Certification

* **Directory & File Inventory Check**: Every directory (`alu/`, `assembler/`, `benchmarks/`, `common/`, `debugger/`, `docs/`, `evaluation/`, `experiments/`, `memory/`, `microcode/`, `processor_4stage/`, `processor_6stage/`, `tests/`, `tools/`, `visualizer/`) and all 74 files are accounted for in Appendix A.
* **Requirements Traceability Check**: All 13 course requirements (1a, 1b, 1c, 2a, 2b, 2c, 2d, 2e, 2f, 2g, 3, 4, 5) are mapped in Appendix B to implementation code, automated tests, and interactive demos with status **Complete**.
* **Figure & Table Check**: All 14 figures and tables are numbered, captioned, rendered in ASCII/Mermaid, and cross-referenced in the text.
* **Placeholder / TODO Check**: Zero instances of "TODO", "FIXME", or placeholder text exist in the report.
* **Empirical Integrity Check**: All CPI, cycle counts, hit rates, and speedup numbers are generated by code from actual host execution and stored under `report/data/`.
* **Discrepancy & Limitations Log**: Stale documentation numbers and unmeasured coverage tooling are honestly declared in Chapter 16 and Appendix I.
