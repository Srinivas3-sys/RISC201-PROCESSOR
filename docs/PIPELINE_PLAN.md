# RISC201-PROCESSOR: Comprehensive Pipeline Hazards, Forwarding, and Binary-String Opcodes Plan

**Course**: CSC-201 Computer Organization & Architecture  
**Document**: Architectural Audit, Hazard/Forwarding Strategy, and Implementation Roadmap  
**Branch**: `pipeline-hazards`  

---

## 1. Baseline Architectural Audit

### 1.1 Existing SimpleRisc ISA Specification
The RISC201 processor implements the 32-bit SimpleRisc architecture (Sarangi, Chapters 3 & 9):
- **Word Width**: 32 bits (`[31:0]`).
- **Registers**: 16 General Purpose Registers (`r0` to `r15`), where:
  - `r0`: Scratch / general register (often used as 0 in idioms)
  - `r14` (`sp`): Stack Pointer
  - `r15` (`ra`): Return Address (link register for `call`)
- **Flags**: Condition flags `E` (Equal) and `GT` (Greater Than), updated by `cmp`, `min`, `max`, `rots`.
- **Privileged Registers**: `oldPC`, `oldSP`, `flags`, `oldFlags`, `CPL` (Current Privilege Level).

#### Instruction Formats and Field Widths
1. **Register-Register Format ($I = 0$)**:
   ```
   31       27 26 25    22 21    18 17    14 13                       0
   +----------+--+--------+--------+--------+--------------------------+
   |  opcode  |0 |   rd   |  rs1   |  rs2   |          unused          |
   |  (5b)    |(1|(4b)    |  (4b)  |  (4b)  |          (14b)           |
   +----------+--+--------+--------+--------+--------------------------+
   ```
2. **Immediate Format ($I = 1$)**:
   ```
   31       27 26 25    22 21    18 17  16 15                         0
   +----------+--+--------+--------+------+----------------------------+
   |  opcode  |1 |   rd   |  rs1   | mod  |           imm16            |
   |  (5b)    |(1|(4b)    |  (4b)  | (2b) |           (16b)            |
   +----------+--+--------+--------+------+----------------------------+
   ```
   - Immediate Modifier (`mod`, bits `[17:16]`):
     - `00`: default sign-extended 16-bit
     - `01`: `u` unsigned 16-bit (zero-extended)
     - `10`: `h` high 16-bit (shifted left 16 bits)
3. **Branch Format**:
   ```
   31       27 26                                                     0
   +----------+--------------------------------------------------------+
   |  opcode  |                        offset27                        |
   |  (5b)    |                 (27-bit signed word)                   |
   +----------+--------------------------------------------------------+
   ```
   - Target Address: $\text{Target} = \text{PC} + (\text{offset27} \ll 2)$.

#### Complete Opcode Mapping Table
Opcode width is strictly **5 bits** (`OPCODE_BITS = 5`), spanning `0b00000` to `0b11001` and `0b11111`:

| Opcode Name | Numeric Value | Binary String | Mnemonic | Format | Description |
| :--- | :---: | :---: | :---: | :---: | :--- |
| `OP_ADD` | 0 | `"00000"` | `add` | R / I | Add: $rd \leftarrow rs1 + (rs2 \mid immx)$ |
| `OP_SUB` | 1 | `"00001"` | `sub` | R / I | Subtract: $rd \leftarrow rs1 - (rs2 \mid immx)$ |
| `OP_MUL` | 2 | `"00010"` | `mul` | R / I | Multiply: $rd \leftarrow rs1 \times (rs2 \mid immx)$ |
| `OP_DIV` | 3 | `"00011"` | `div` | R / I | Divide: $rd \leftarrow rs1 / (rs2 \mid immx)$ |
| `OP_MOD` | 4 | `"00100"` | `mod` | R / I | Modulo: $rd \leftarrow rs1 \pmod{rs2 \mid immx}$ |
| `OP_CMP` | 5 | `"00101"` | `cmp` | R / I | Compare: updates `flags.E` and `flags.GT` |
| `OP_AND` | 6 | `"00110"` | `and` | R / I | Bitwise AND: $rd \leftarrow rs1 \ \& \ (rs2 \mid immx)$ |
| `OP_OR`  | 7 | `"00111"` | `or`  | R / I | Bitwise OR: $rd \leftarrow rs1 \mid (rs2 \mid immx)$ |
| `OP_NOT` | 8 | `"01000"` | `not` | R / I | Bitwise NOT: $rd \leftarrow \sim(rs2 \mid immx)$ |
| `OP_MOV` | 9 | `"01001"` | `mov` | R / I | Move: $rd \leftarrow (rs2 \mid immx)$ |
| `OP_LSL` | 10 | `"01010"` | `lsl` | R / I | Logical Shift Left: $rd \leftarrow rs1 \ll (rs2 \mid immx)$ |
| `OP_LSR` | 11 | `"01011"` | `lsr` | R / I | Logical Shift Right: $rd \leftarrow rs1 \gg (rs2 \mid immx)$ |
| `OP_ASR` | 12 | `"01100"` | `asr` | R / I | Arithmetic Shift Right: $rd \leftarrow rs1 \gg_{\text{arith}} (rs2 \mid immx)$ |
| `OP_NOP` | 13 | `"01101"` | `nop` | Branch | No-operation |
| `OP_LD`  | 14 | `"01110"` | `ld`  | I | Load Word: $rd \leftarrow \text{Mem}[rs1 + imm16]$ |
| `OP_ST`  | 15 | `"01111"` | `st`  | I | Store Word: $\text{Mem}[rs1 + imm16] \leftarrow rd$ |
| `OP_BEQ` | 16 | `"10000"` | `beq` | Branch | Branch if Equal: if `flags.E == 1`, $\text{PC} \leftarrow \text{target}$ |
| `OP_BGT` | 17 | `"10001"` | `bgt` | Branch | Branch if Greater: if `flags.GT == 1`, $\text{PC} \leftarrow \text{target}$ |
| `OP_B`   | 18 | `"10010"` | `b`   | Branch | Unconditional Branch: $\text{PC} \leftarrow \text{target}$ |
| `OP_CALL`| 19 | `"10011"` | `call`| Branch | Call Subroutine: $ra \leftarrow \text{PC}+4$, $\text{PC} \leftarrow \text{target}$ |
| `OP_RET` | 20 | `"10100"` | `ret` | Register | Return from Subroutine: $\text{PC} \leftarrow ra$ |
| `OP_MOVZ`| 21 | `"10101"` | `movz`| Privileged| Move to/from Privileged Registers |
| `OP_RETZ`| 22 | `"10110"` | `retz`| Privileged| Return from Exception: $\text{PC} \leftarrow oldPC, \text{CPL} \leftarrow 1$ |
| `OP_MIN` | 23 | `"10111"` | `min` | R / I | Minimum: $rd \leftarrow \min(rs1, rs2 \mid immx)$ |
| `OP_MAX` | 24 | `"11000"` | `max` | R / I | Maximum: $rd \leftarrow \max(rs1, rs2 \mid immx)$ |
| `OP_ROTS`| 25 | `"11001"` | `rots`| R / I | Rotate Shift: $rd \leftarrow \text{rot}(rs1, rs2 \mid immx)$ |
| `OP_INVALID`| 31 | `"11111"` | `invalid` | - | Undefined / Illegal instruction |

---

### 1.2 Pipeline Stage Definitions and Latches

#### 4-Stage Pipeline (`cpu/pipeline4.py` / `processor_4stage`)
- **Stage 1: IF (Instruction Fetch)**: Fetches the 32-bit machine word from virtual memory using `PC`. Handles multi-cycle I-cache and ITLB misses.
- **Stage 2: OF (Operand Fetch / Decode)**: Decodes instruction fields, reads source registers from `RegisterFile`, checks for RAW hazards (load-use), and injects bubbles into EX.
- **Stage 3: EX (Execute)**: Computes ALU result, updates flags (for `cmp`, `min`, `max`, `rots`), evaluates branch condition (`b`, `beq`, `bgt`, `call`, `ret`), updates branch counters, and receives bypass data.
- **Stage 4: MA_RW (Memory Access & Register Writeback)**: Performs D-cache read (`ld`) or write (`st`), checks stack guard boundaries, and writes the committed result into the destination register in `RegisterFile`.
- **Latches**:
  - `l_if_of` / `next_if_of`: `Latch_IF_OF(pc, instruction, is_bubble, disassembly)`
  - `l_of_ex` / `next_of_ex`: `Latch_OF_EX(pc, instruction, decoded, op1, op2, immx, branch_target, is_immediate, is_bubble, disassembly)`
  - `l_ex_marw` / `next_ex_marw`: `Latch_EX_MARW(pc, instruction, decoded, alu_result, op2, is_bubble, disassembly)`

#### 6-Stage Pipeline (`cpu/pipeline6.py` / `processor_6stage`)
- **Stage 1: IF (Instruction Fetch)**: Fetches 32-bit word from memory at `PC`.
- **Stage 2: ID (Instruction Decode)**: Decodes opcode, fields, immediate expansion, and branch target calculation.
- **Stage 3: RF (Register Fetch)**: Reads `rs1`, `rs2`, or `rd` from `RegisterFile`, applies early writeback bypass, and checks for hazards.
- **Stage 4: EX (Execute)**: Executes ALU operations, evaluates condition flags and branch outcomes.
- **Stage 5: MA (Memory Access)**: Reads from or writes to the memory hierarchy; tracks D-cache and DTLB stalls.
- **Stage 6: RW (Register Writeback)**: Writes load result or ALU result to destination register in `RegisterFile`.
- **Latches**:
  - `l_if_id` / `next_if_id`: `Latch_IF_ID`
  - `l_id_rf` / `next_id_rf`: `Latch_ID_RF`
  - `l_rf_ex` / `next_rf_ex`: `Latch_RF_EX`
  - `l_ex_ma` / `next_ex_ma`: `Latch_EX_MA`
  - `l_ma_rw` / `next_ma_rw`: `Latch_MA_RW`

---

### 1.3 How Stalls, Flushes, Branches, and Memory Stalls Operate Today
1. **Load-Use Hazards**:
   - In 4-stage: In OF, if the instruction in EX is a load (`d.is_load()`) and writes to `rs1` or `rs2`, the pipeline asserts `stall_if = True, stall_of = True, bubble_ex = True`, stalling for 1 cycle.
   - In 6-stage: In RF, if the instruction in EX is a load and writes to `rs1` or `rs2`, the pipeline asserts `stall_if = True, stall_id = True, stall_rf = True, bubble_ex = True`.
2. **Branch Resolution**:
   - Both pipelines currently resolve branches in the **EX stage**.
   - In 4-stage: On branch taken, `pc = branch_pc`, younger latches (`next_if_of` and `next_of_ex`) are reset to bubbles $\implies$ 2-cycle branch penalty.
   - In 6-stage: On branch taken, `pc = branch_pc`, younger latches (`next_if_id`, `next_id_rf`, `next_rf_ex`) are reset to bubbles $\implies$ 3-cycle branch penalty.
3. **Memory Subsystem Stalls**:
   - When `mem.read_word()` or `mem.write_word()` returns `lat > 1`, `mem_pending_stall = lat - 1` freezes upstream pipeline stages while letting downstream stages drain.
   - When IF encounters a cache/TLB miss, `if_pending_stall = fetch_lat - 1` holds `PC` and injects bubbles into IF/OF (or IF/ID).
4. **Microcode Controller**:
   - `cpu/microcode.py` uses a state machine (`upc = 0` fetch, `upc = 1` decode, `upc = 2` dispatch).
   - Stalls waiting on memory are modeled via `mem_wait_cycles`.

---

### 1.4 Comprehensive Opcode Audit: Locations Where Opcodes Are Numbers
Here is the exact catalog of every file and line where opcodes are currently used as integers, shifts, masks, or enum numbers:

1. **`cpu/isa.py`**:
   - Lines 12-40: `Opcode(IntEnum)` definition with integer bit-values `0b00000` to `0b11111`.
   - Line 52: `opcode: Opcode = Opcode.OP_NOP`
   - Lines 64-67: `self.opcode in (Opcode.OP_B, Opcode.OP_BEQ, Opcode.OP_BGT, Opcode.OP_CALL, Opcode.OP_RET, Opcode.OP_RETZ)`
   - Line 70: `self.opcode in (Opcode.OP_BEQ, Opcode.OP_BGT)`
   - Line 73: `self.opcode == Opcode.OP_CALL`
   - Line 76: `self.opcode in (Opcode.OP_RET, Opcode.OP_RETZ)`
   - Line 79: `self.opcode == Opcode.OP_LD`
   - Line 82: `self.opcode == Opcode.OP_ST`
   - Line 85: `self.opcode == Opcode.OP_NOP`
   - Lines 88-93: `self.opcode in (Opcode.OP_NOP, Opcode.OP_ST, Opcode.OP_CMP, ...)`
   - Line 97: `self.opcode == Opcode.OP_CALL`
   - Lines 102-106: `self.opcode in (Opcode.OP_NOP, Opcode.OP_B, ...)`
   - Lines 112-117: `self.opcode in (Opcode.OP_NOP, Opcode.OP_B, ...)`
   - Line 121: `self.opcode == Opcode.OP_ST`
   - Line 126: `op_val = (word >> 27) & 0x1F` (integer mask and shift)
   - Line 128: `opcode = Opcode(op_val)`
   - Line 130: `opcode = Opcode.OP_INVALID`
   - Line 157: `if opcode == Opcode.OP_RET:`
   - Lines 180-184: `if op == Opcode.OP_NOP: ... if op == Opcode.OP_RET: ...`
   - Lines 187-189: `if op in (Opcode.OP_B, ...)`
   - Lines 192-198: `op_names = {Opcode.OP_ADD: "add", ...}`
   - Lines 201-216: `if op == Opcode.OP_MOV: ...`

2. **`cpu/alu.py`**:
   - Line 23: `def execute(op: Opcode, a: int, b: int, current_flags: Flags)`
   - Line 28: `if op == Opcode.OP_ADD or op in (Opcode.OP_LD, Opcode.OP_ST):`
   - Lines 31-93: `elif op == Opcode.OP_SUB: ... OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR, OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_MIN, OP_MAX, OP_ROTS:`

3. **`cpu/assembler.py`**:
   - Lines 173-177: `word = (Opcode.OP_NOP << 27)`, `(Opcode.OP_RET << 27)`, `(Opcode.OP_RETZ << 27)`
   - Line 186: `op_map = {"b": Opcode.OP_B, "beq": Opcode.OP_BEQ, "bgt": Opcode.OP_BGT, "call": Opcode.OP_CALL}`
   - Line 187: `word = (op_map[m] << 27) | ...`
   - Line 199: `op = Opcode.OP_LD if m == "ld" else Opcode.OP_ST`
   - Line 200: `word = (op << 27) | ...`
   - Lines 207, 210: `(Opcode.OP_MOV << 27)`
   - Lines 217, 220: `(Opcode.OP_NOT << 27)`
   - Lines 227, 230: `(Opcode.OP_CMP << 27)`
   - Lines 235-239: `"add": Opcode.OP_ADD, ...`
   - Lines 250, 253: `(op << 27) | ...`

4. **`cpu/pipeline4.py`**:
   - Line 268: `if d.opcode == Opcode.OP_CMP:`
   - Lines 280-295: `if d.opcode == Opcode.OP_B: ... elif d.opcode == Opcode.OP_BEQ: ...`

5. **`cpu/pipeline6.py`**:
   - Line 340: `if d.opcode == Opcode.OP_CMP:`
   - Lines 351-366: `if d.opcode == Opcode.OP_B: ... elif d.opcode == Opcode.OP_BEQ: ...`

6. **`cpu/microcode.py`**:
   - Lines 91-171: `if op == Opcode.OP_ADD: ... elif op == Opcode.OP_SUB: ...`

---

## 2. Refactoring & Implementation Plan

### Phase 1: Planning, Audit & Golden Verification (Current)
- Completed exhaustive audit of existing ISA, formats, field bit-slices, and pipeline behaviors.
- Established `tests/golden/` containing byte-exact `.hex` and `.bin` reference outputs for all 6 benchmark programs (`array_sum`, `factorial_iter`, `factorial_rec`, `hazard_test`, `prime_test`, `stack_test`).
- Authored this architectural plan document.

### Phase 2: Part A — Binary-String Opcodes as Single Source of Truth
- Implement `common/isa.py`:
  - Fixed-width binary string constants: `OP_ADD = "00000"`, `OP_SUB = "00001"`, ..., `OP_NOP = "01101"`, ..., `OP_INVALID = "11111"`.
  - Bit-string utility library: `int_to_bits(val, width)`, `bits_to_int(bitstr)`, `word_to_bits(word, width)`, `bits_to_word(bitstr)`, `slice_field(bitstr, hi, lo)`.
  - Rich `InstructionInfo` dataclass and global `INSTRUCTION_TABLE` keyed by binary string opcodes.
  - Slicing and bit-concatenation `encode()` and `decode()` producing `Instruction` objects with cached binary string opcodes.
  - Strict validation: raises `IllegalInstructionError` carrying binary string, hex word, and bit representations.
- Refactor all Python modules:
  - `cpu/assembler.py`: Assemble by concatenating binary strings.
  - `cpu/isa.py`: Re-export from `common/isa.py` for full backward compatibility.
  - `cpu/alu.py`: ALU operation select via binary string dispatch.
  - `cpu/microcode.py`: Microcode fields and dispatch ROM keyed by binary string opcodes.
  - Pipeline latches and decoder: Single decode in ID/OF stage into `Instruction` object.
- Verification: Golden regression against `tests/golden/`, round-trip tests, exhaustive table tests, and lint test forbidding numeric opcodes.

### Phase 3: Reference Interpreter (ISS) & Differential Test Harness
- Implement `cpu/iss.py`: Non-pipelined architectural Instruction Set Simulator driven by binary-string opcodes.
- Build differential testing harness comparing final register states, memory bytes, and exception outcomes between the ISS and both pipelined processors.

### Phase 4: Cycle Accounting & Exact Performance Framework (Part D)
- Implement `common/perf.py` (or `perf/` package):
  - Cycle accounting invariant:
    $$\text{Total Cycles} = \text{Retired Instructions} \times \text{CPI}_{\text{base}} + \sum \text{Stall Cycles by Reason}$$
  - Dedicated stall reason codes: `RAW_LOAD_USE`, `RAW_NOFWD`, `BRANCH_MISPREDICT`, `BRANCH_STALL`, `STRUCTURAL_MEM`, `STRUCTURAL_RF`, `MULTICYCLE_EX`, `ICACHE_MISS`, `DCACHE_MISS`, `TLB_MISS`, `PAGE_WALK`, `PAGE_FAULT`, `DRAM_REFRESH`, `EXCEPTION_FLUSH`.
  - CPI Stack breakdown, instruction-mix histogram keyed by binary opcode string, MIPS, speedup vs unpipelined CPU, and analytic closed-form CPI model.

### Phase 5: Hazard Detection Unit (Part B)
- Implement `common/hazard.py` (and re-export in `processor_4stage/hazard.py`, `processor_6stage/hazard.py`):
  - Driven by instruction-table metadata (flags `reads_rs1`, `reads_rs2`, `writes_rd`, `is_load`, `is_store`).
  - RAW, WAR, and WAW hazard detection.
  - Load-use hazard detection inserting minimum necessary bubbles per pipeline depth.
  - Structural hazard detection: unified memory port conflicts, register file write port limits.

### Phase 6: Best-in-Class Data Forwarding Bypass Network (Part C)
- Implement `common/forwarding.py` (and re-export in `processor_4stage/forwarding.py`, `processor_6stage/forwarding.py`):
  - Forwarding modes: `none`, `basic`, `full`, `full+`.
  - Configurable per-path enable flags.
  - Generated bypass network based on exact stage layouts:
    - 4-Stage: `EX/MARW -> EX (rs1)`, `EX/MARW -> EX (rs2)`, `EX/MARW -> OF`, Store data forwarding (`EX/MARW -> EX/MARW`), Branch forwarding.
    - 6-Stage: `EX/MA -> EX`, `MA/RW -> EX`, `EX/MA -> RF`, `MA/RW -> RF`, late load data forwarding from MA arrival, store data forwarding into MA.
  - Youngest-older-producer priority resolution; never forward from bubbles or faulted instructions.
  - Producer-selection mux select signals as binary strings (e.g., `FWD_SEL_REG = "00"`, `FWD_SEL_EXMA = "01"`, `FWD_SEL_MARW = "10"`).

### Phase 7: Advanced Branch Handling & Prediction (Part B.2)
- Implement `common/branch.py`:
  - Policies: `stall`, `flush` (predict not taken), `predict-taken`, `static-btfn`, `1bit`, `2bit`, `gshare`, `perfect`.
  - Branch Target Buffer (BTB) with configurable sets and associativity.
  - Return Address Stack (RAS) for `call`/`ret`.
  - Configurable branch delay slots (0, 1, 2) with assembler filling capabilities.
  - Squashing logic ensuring zero architectural side effects on mispredictions.

### Phase 8: Pipeline Integration, Microcode, VM & Exceptions
- Integrate HazardUnit, ForwardingUnit, BranchPredictor, and PerformanceTracker into `cpu/pipeline4.py` and `cpu/pipeline6.py`.
- Preserve cycle-accurate integration with `MemoryHierarchy`, TLB page walks, and precise exceptions.

### Phase 9: CLI, Visualizer & Interactive REPL Debugger
- Extend `sim.py` with flags: `--pipeline`, `--forwarding`, `--fwd-path`, `--branch-policy`, `--predictor-size`, `--history-bits`, `--btb-entries`, `--ras-entries`, `--delay-slots`, `--unified-mem`, `--regfile-write-ports`, `--clock-mhz-4stage`, `--clock-mhz-6stage`, `--check-forwarding`, `--report`, `--isa-table`, `--opcode-style`, `--trace-hazards`, `--diagram`.
- Update debugger REPL with commands: `hazards`, `fwd`, `stalls`, `pipe`, `bp`, `cpi`, `opcode`.
- Render ASCII pipeline timing diagrams with forwarding arrows and labeled bubbles.

### Phase 10: Fuzzer & Experiments
- Random hazard fuzzer generating dense dependence sequences and illegal opcode bit strings.
- Implement 10 experiments in `experiments/pipeline/run_pipeline_experiments.py` producing CSV and Markdown reports.

### Phase 11: Comprehensive Course Documentation
- `docs/ISA_BINARY_OPCODES.md`
- `docs/HAZARDS_AND_FORWARDING.md`
- `docs/PERFORMANCE.md`
- Update `README.md`

---

## 3. List of Every File Touched or Created

| File | Action | Purpose |
| :--- | :---: | :--- |
| `tests/golden/*.hex`, `*.bin` | Created | Byte-exact golden outputs for all 6 benchmark programs |
| `common/__init__.py` | Created | Package initialization for common modules |
| `common/isa.py` | Created | Single source of truth for binary-string opcodes, bit utilities, and instruction table |
| `cpu/isa.py` | Modified | Re-export and wrap `common/isa.py` |
| `cpu/assembler.py` | Modified | Concatenates binary strings for machine word generation |
| `cpu/alu.py` | Modified | ALU operation select via binary string dispatch |
| `cpu/microcode.py` | Modified | Binary-string control fields and opcode dispatch ROM |
| `common/hazard.py` | Created | Central Hazard Detection Unit |
| `common/forwarding.py` | Created | Configurable multi-path forwarding bypass network |
| `common/branch.py` | Created | Branch prediction (1-bit, 2-bit, gshare, BTB, RAS, delay slots) |
| `common/perf.py` | Created | Exact cycle accounting, CPI stack, and closed-form analytic models |
| `cpu/iss.py` | Created | Non-pipelined Instruction Set Simulator for differential testing |
| `processor_4stage/__init__.py` | Created | Package export for 4-stage pipeline |
| `processor_4stage/forwarding.py`| Created | 4-stage specific forwarding unit |
| `processor_4stage/hazard.py` | Created | 4-stage specific hazard unit |
| `processor_6stage/__init__.py` | Created | Package export for 6-stage pipeline |
| `processor_6stage/forwarding.py`| Created | 6-stage specific forwarding unit |
| `processor_6stage/hazard.py` | Created | 6-stage specific hazard unit |
| `cpu/pipeline4.py` | Modified | Integrated 4-stage processor with hazard, forwarding, and perf accounting |
| `cpu/pipeline6.py` | Modified | Integrated 6-stage processor with hazard, forwarding, and perf accounting |
| `sim.py` | Modified | Added CLI arguments and new REPL debugger commands |
| `visualizer.py` | Modified | Added pipeline diagram with forwarding arrows and bubble labels |
| `Makefile` | Modified | Added targets: `test-pipe`, `test-isa`, `fuzz`, `exp-pipe`, `demo-pipe` |
| `tests/test_isa.py` | Created | Unit tests for binary string opcodes, round-trip, exhaustive table, lint |
| `tests/test_pipeline_hazards.py`| Created | Unit tests for exact cycle counts on all hazard patterns |
| `tests/test_differential_fuzz.py`| Created | Differential tests against ISS and random hazard fuzzer |
| `experiments/pipeline/run_pipeline_experiments.py` | Created | Implementation of 10 pipeline experiments |
| `docs/PIPELINE_PLAN.md` | Created | Architectural roadmap and baseline audit |
| `docs/ISA_BINARY_OPCODES.md` | Created | Complete specification of binary-string opcodes |
| `docs/HAZARDS_AND_FORWARDING.md`| Created | Bypass network diagrams and hazard handling |
| `docs/PERFORMANCE.md` | Created | CPI stacks, formulas, and 4- vs 6-stage trade-offs |
| `README.md` | Modified | Documentation of new CLI flags and capabilities |

---
**Attestation**: Existing C++ simulators and test targets remain completely unmodified; all 30 benchmarks (`make all && ./risc201_eval`) continue to build and pass cleanly.
