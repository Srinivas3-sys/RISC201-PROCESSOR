# Guided Source Code Tour & Reading Walkthroughs

If you open the codebase in VS Code, follow this exact reading order. Each entry tells you what the file does, its core classes and functions, and the exact lines to look at first.

---

### 1. `common/ISA.hpp` (130 lines)
* **What it is for**: The single source of truth for all instruction definitions in the simulator.
* **Key Structures & Functions**:
  * `namespace isa`: Defines 5-bit string constants `OP_ADD = "00000"` through `OP_INVALID = "11111"`.
  * `struct InstructionMeta`: Holds mnemonic, format, and boolean flags (`is_load`, `writes_rd`, etc.).
  * `getMeta(opcode)`: Returns metadata struct for any 5-bit string.
* **What to look at first**: Lines 20–55 (the opcode string constants) and lines 65–85 (`getMeta()` table).

---

### 2. `common/Types.hpp` (110 lines)
* **What it is for**: Fundamental architectural types, bit widths, and non-opcode enumerations.
* **Key Types**:
  * `Word = uint32_t` (32-bit registers and addresses).
  * `struct DecodedInst`: The decoded instruction packet carried through pipeline stages.
  * `enum class BranchPolicy`, `enum class ForwardingMode`, `enum class AdderType`.
* **What to look at first**: Lines 25–45 (`struct DecodedInst`). Notice there is NO `enum Opcode` here!

---

### 3. `common/RegisterFile.hpp` (95 lines)
* **What it is for**: Models the 16 general-purpose registers and condition flags.
* **Key Classes**:
  * `class RegisterFile`: Holds `std::array<Word, 16> regs`, flags `flagE` and `flagGT`, and privileged registers `cpl`, `oldPC`, `oldSP`.
* **What to look at first**: Lines 20–35 (`read()` and `write()` methods).

---

### 4. `common/ISS.hpp` (210 lines)
* **What it is for**: The unpipelined, single-cycle Golden Reference Instruction Set Simulator.
* **Key Functions**:
  * `step()`: Executes one instruction completely (Fetch $\rightarrow$ Decode $\rightarrow$ Execute $\rightarrow$ Commit).
  * `decode()`: Slices 32-bit word into fields.
* **What to look at first**: Lines 45–65 (`step()`). It shows the simplest possible implementation of the computer architecture loop.

---

### 5. `alu/ALU.hpp` (165 lines)
* **What it is for**: The central arithmetic and logic execution engine.
* **Key Methods**:
  * `execute(opcodeBits, op1, op2, flags)`: Dispatches the operation using `if-else` on `isa::OP_*` string constants.
* **What to look at first**: Lines 30–80 (the `if-else` chain comparing `opcodeBits == isa::OP_ADD`). Notice how clean it is.

---

### 6. `common/ForwardingUnit.hpp` (150 lines)
* **What it is for**: Detects RAW data dependencies and routes bypass operands to the ALU.
* **Key Methods**:
  * `getForwardA()`, `getForwardB()`: Checks `EX/MA` then `MA/RW` to select operand source.
* **What to look at first**: Lines 40–75 (the bypass priority checks).

---

### 7. `common/HazardUnit.hpp` (145 lines)
* **What it is for**: Detects hazards that cannot be forwarded (load-use and branch mispredicts).
* **Key Methods**:
  * `detectLoadUse()`: Returns `true` when an instruction immediately uses loaded data.
  * `shouldFlushBranch()`: Signals pipeline squashing when branch predictor was wrong.
* **What to look at first**: Lines 35–60 (the load-use check comparing `OF_EX.rd` with `IF_OF.rs1/rs2`).

---

### 8. `common/BranchPredictor.hpp` (160 lines)
* **What it is for**: Dynamic branch prediction algorithms.
* **Key Classes**:
  * `TwoBitBimodalPredictor`: Implements 2-bit saturating counters (`00` to `11`).
  * `GSharePredictor`: Global history XOR hash predictor.
  * `ReturnAddressStack`: 8-entry LIFO stack for function returns.
* **What to look at first**: Lines 50–75 (the 2-bit bimodal state transition logic).

---

### 9. `processor_4stage/Pipeline4Stage.hpp` (480 lines)
* **What it is for**: The 4-stage pipelined CPU core (`IF -> OF -> EX -> MA_RW`).
* **Key Methods**:
  * `stepCycle()`: Calls `stageMA_RW()`, `stageEX()`, `stageOF()`, `stageIF()` in reverse order.
* **What to look at first**: Lines 120–160 (`stepCycle()`), and lines 180–230 (`stageEX()`).

---

### 10. `processor_6stage/Pipeline6Stage.hpp` (520 lines)
* **What it is for**: The 6-stage pipelined CPU core (`IF -> ID -> RF -> EX -> MA -> WB`).
* **Key Methods**:
  * `stageID()`: Pure instruction decode.
  * `stageRF()`: Pure Register File read.
* **What to look at first**: Compare `stageID()` and `stageRF()` with `stageOF()` in 4-stage to see the split.

---

### 11. `microcode/MicroController.hpp` (280 lines)
* **What it is for**: Microprogrammed control unit executing horizontal and vertical control stores.
* **Key Methods**:
  * `stepMicroCycle()`: Advances `microPC` by 1 micro-instruction.
* **What to look at first**: Lines 60–90 (the dispatch table mapping opcodes to starting microPC addresses).

---

### 12. `memory/VirtualMemory.hpp` (409 lines)
* **What it is for**: 4-level Radix-4 Virtual Memory, multi-tier TLBs, and Page-Walk Cache.
* **Key Classes**:
  * `GenericTLB`: Set-associative TLB with LRU replacement.
  * `VirtualMemorySubsystem`: Implements 48-bit address translation, PWC lookup, and page fault handling.
* **What to look at first**: Lines 300–360 (`translate()` method).

---

### 13. `assembler/Assembler.hpp` (320 lines)
* **What it is for**: The two-pass assembler translating `.s` assembly files to `.bin` and `.hex`.
* **Key Methods**:
  * `pass1()`: Builds the symbol table.
  * `pass2()`: Emits machine code words and resolves relative offsets.
* **What to look at first**: Lines 60–100 (`pass1()`) and lines 120–180 (`pass2()`).
