# RISC201 Architecture: Single Source of Truth ISA Opcode Refactor Plan

## 1. Executive Summary & Architectural Objective

The goal of this refactor is to make [`common/ISA.hpp`](../common/ISA.hpp) the **single source of truth** for instruction opcodes across the entire RISC201 processor simulator.
The duplicate integer opcode enum `enum Opcode : uint8_t` in [`common/Types.hpp`](../common/Types.hpp) is eliminated. All subsystems—including both 4-stage and 6-stage pipelines, the golden ISS, the ALU, assembler, disassembler, branch predictor, hazard units, microcode control store, debugger, visualizer, evaluation suite, and tests—will carry and compare opcodes as canonical 5-bit binary strings (`isa::OP_ADD`, `isa::OP_SUB`, etc.).

Machine code generation, cycle counts, CPI numbers, exception semantics, and UI formatting are guaranteed byte-for-byte identical to the golden baselines captured in `tests/golden_refactor/`.

---

## 2. Comprehensive Inventory of Old Opcode Usages

### 2.1 `common/Types.hpp`
- **Lines 36–70**: `enum Opcode : uint8_t` definition containing:
  - Arithmetic/Logical: `OP_ADD = 0b00000`, `OP_SUB = 0b00001`, `OP_MUL = 0b00010`, `OP_DIV = 0b00011`, `OP_MOD = 0b00100`, `OP_CMP = 0b00101`, `OP_AND = 0b00110`, `OP_OR = 0b00111`, `OP_NOT = 0b01000`, `OP_MOV = 0b01001`, `OP_LSL = 0b01010`, `OP_LSR = 0b01011`, `OP_ASR = 0b01100`, `OP_NOP = 0b01101`
  - Memory: `OP_LD = 0b01110`, `OP_ST = 0b01111`
  - Control Flow: `OP_BEQ = 0b10000`, `OP_BGT = 0b10001`, `OP_B = 0b10010`, `OP_CALL = 0b10011`, `OP_RET = 0b10100`
  - Privileged / Extensions: `OP_MOVZ = 0b10101`, `OP_RETZ = 0b10110`, `OP_MIN = 0b10111`, `OP_MAX = 0b11000`, `OP_ROTS = 0b11001`, `OP_XOR = 0b11010`, `OP_ABS = 0b11011`, `OP_INVALID = 0b11111`
- **Line 93**: `struct DecodedInst`: `Opcode opcode{OP_NOP};`
- **Lines 106–140**: Member functions checking integer opcodes (`opcode == OP_B || ...`)

### 2.2 `common/ISA.hpp`
- **Lines 34–65**: `namespace isa { static const std::string OP_ADD = "00000"; ... }`
  - Must be upgraded from `static const std::string` to `inline const std::string` to guarantee single-instance storage and avoid static initialization order issues.
- **Lines 67–95**: Redundant `BIN_OP_* = isa::OP_*;` aliases to be deleted.
- **Lines 231, 256, 263, 269**: `Instruction` struct using `isa::OP_NOP`, `isa::OP_BEQ`, etc.

### 2.3 `alu/ALU.hpp`
- **Lines 57–63**: Overload `execute(const std::string& opcodeBits, ...)` parsing strings into integers and casting via `static_cast<Opcode>(val)`.
- **Lines 69–195**: `AluOutput execute(Opcode op, ...)` with integer `switch (op)` and `case OP_ADD:`, `case OP_SUB:`, etc.
  - Must be refactored into a single string-keyed `execute(const std::string& opcode, ...)` utilizing metadata dispatch / function pointer dispatch table built once.

### 2.4 `assembler/Disassembler.hpp`
- **Line 23**: `d.opcode = static_cast<Opcode>((word >> 27) & 0x1F);` (integer bit extraction).
  - Must use `isa::wordToOpcode(word)` as the single conversion helper.
- **Lines 86–155**: `disassemble(...)` using integer `switch (d.opcode)` and `case OP_NOP:`, `case OP_ADD:`, etc.
  - Must format disassemblies using the `InstructionInfo` metadata table in `ISA.hpp`.

### 2.5 `assembler/Assembler.hpp`
- **Lines 208, 220, 233**: Lambdas `encodeReg`, `encodeImm`, `encodeBranch` accepting `Opcode op`.
- **Lines 210, 222, 235**: `(static_cast<Word>(op) & 0x1F) << 27` integer bit shifts.
  - Must use `isa::opcodeToWordBits(opcode)` at the machine-word boundary.
- **Lines 265, 291, 364**: Variables of type `Opcode`.
- **Lines 302, 308, 319, 325, 336, 342, 353, 359**: Usages of `OP_MOV`, `OP_NOT`, `OP_ABS`, `OP_CMP`.
- **Line 466**: Instruction scheduler check: `if (dj.isBranch() || dj.opcode == OP_NOP || dj.opcode == OP_CMP) continue;`.

### 2.6 `microcode/MicroInstructions.hpp`
- **Lines 89–101**: `enum class FunctionalArg : uint16_t` packing integer `OP_*`:
  `ALU_ADD = (1 << 7) | OP_ADD`, etc.

### 2.7 `microcode/MicroController.hpp`
- **Line 36**: `std::map<Opcode, uint16_t> opcodeDispatchTable;`
- **Lines 138, 166–175**: `emitAlu3Op(Opcode op, ...)` called with `OP_ADD`, `OP_SUB`, etc.
- **Lines 180, 200, 214, 228, 235, 248, 261, 269, 279, 289, 299**: `opcodeDispatchTable[OP_MOV] = start;`, etc.
- **Line 396**: `Opcode op = static_cast<Opcode>((ir >> 27) & 0x1F);` in microcode mswitch dispatch.
- **Lines 462, 465**: `Opcode op = static_cast<Opcode>(static_cast<uint16_t>(arg) & 0x1F); if (op == OP_CMP)`.

### 2.8 `processor_4stage/Pipeline4Stage.hpp`
- **Line 275**: `aluUnit.execute(d.opcode, opA, opB, regFile.getFlags())`
- **Line 276**: `if (d.opcode == OP_CMP)`
- **Lines 289–305**: `if (d.opcode == OP_B) ... else if (d.opcode == OP_BEQ) ... else if (d.opcode == OP_BGT) ... else if (d.opcode == OP_CALL) ... else if (d.opcode == OP_RET) ... else if (d.opcode == OP_RETZ)`

### 2.9 `processor_6stage/Pipeline6Stage.hpp`
- **Line 318**: `aluUnit.execute(d.opcode, opA, opB, regFile.getFlags())`
- **Line 319**: `if (d.opcode == OP_CMP)`
- **Lines 324–337**: `if (d.opcode == OP_B) ... else if (d.opcode == OP_BEQ) ... else if (d.opcode == OP_BGT) ... else if (d.opcode == OP_CALL) ... else if (d.opcode == OP_RET) ... else if (d.opcode == OP_RETZ)`

### 2.10 `common/BranchPredictor.hpp`
- **Line 216**: `if (inst.opcode == isa::OP_B)`

### 2.11 `common/ISS.hpp`
- **Lines 71–108**: Branch, load, store, nop, and cmp checks using `isa::OP_*`.

### 2.12 `tests/test_all.cpp`
- **Lines 53, 62**: Test word encoding with bit-shifts `(0b00000 << 27)` and `(0b01001 << 27)`.

---

## 3. List of Files to be Touched

1. `common/ISA.hpp`: Opcode constants hardening (`inline const std::string`), metadata table expansion, ALU function handle, word boundary conversion helpers (`wordToOpcode`, `opcodeToWordBits`), validation and illegal opcode rejection.
2. `common/Types.hpp`: Delete `enum Opcode`, update `DecodedInst` to hold `std::string opcode` and `const InstructionInfo* info`.
3. `alu/ALU.hpp`: Unify entry points to string-based execution via metadata/lookup table built once.
4. `assembler/Disassembler.hpp`: Use `isa::wordToOpcode()` and `InstructionInfo` metadata table.
5. `assembler/Assembler.hpp`: Use string opcodes `isa::OP_*` and `isa::opcodeToWordBits()`.
6. `microcode/MicroInstructions.hpp`: Separate functional argument identifiers from integer opcodes.
7. `microcode/MicroController.hpp`: Key dispatch table by `std::string` opcode.
8. `processor_4stage/Pipeline4Stage.hpp`: Use `d.opcode == isa::OP_*` and metadata flags.
9. `processor_6stage/Pipeline6Stage.hpp`: Use `d.opcode == isa::OP_*` and metadata flags.
10. `tests/test_all.cpp`: Verify ISA tests use `isa::wordToOpcode` and `isa::OP_*`.
11. `tools/lint_isa.cpp` + `Makefile`: Automated linter verifying zero remaining integer opcode definitions or raw quoted 5-bit comparisons.
12. `docs/ISA_BINARY_OPCODES.md` + `README.md`: Updated architectural documentation.

---

## 4. Migration & Verification Strategy

Each phase will be compiled and verified against the golden baselines captured in `tests/golden_refactor/`:
- **Phase 1**: ISA.hpp hardening & metadata table extension
- **Phase 2**: Decoder + DecodedInst stringification
- **Phase 3**: ALU unified string execution
- **Phase 4**: Common units & ISS verification
- **Phase 5**: Pipelines 4-stage and 6-stage migration
- **Phase 6**: Microcode control store migration
- **Phase 7**: Assembler & Disassembler migration
- **Phase 8**: Types.hpp opcode enum deletion & BIN_OP_* removal
- **Phase 9**: Final verification, linting (`make lint-isa`), and documentation
