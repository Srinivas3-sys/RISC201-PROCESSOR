# RISC201-PROCESSOR: Module-by-Module Technical Reading Notes

This document provides deep technical reading notes on every module in the repository, analyzing data structures, function contracts, design mechanisms, and invariants directly from the C++ source code.

---

## 1. Module: `common/`

### 1.1 `common/ISA.hpp` (Lines 1–130)
- **Role**: Single source of truth for all instruction set architecture definitions.
- **Opcode Representation**: 5-bit binary strings (`std::string`). No integers, no enum types.
- **Instruction Definitions**:
  - Arithmetic/Logic: `OP_ADD = "00000"`, `OP_SUB = "00001"`, `OP_MUL = "00010"`, `OP_DIV = "00011"`, `OP_MOD = "00100"`
  - Bitwise/Shift: `OP_CMP = "00101"`, `OP_AND = "00110"`, `OP_OR = "00111"`, `OP_NOT = "01000"`, `OP_MOV = "01001"`, `OP_LSL = "01010"`, `OP_LSR = "01011"`, `OP_ASR = "01100"`
  - Control Flow: `OP_NOP = "01101"`, `OP_LD = "01110"`, `OP_ST = "01111"`, `OP_BEQ = "10000"`, `OP_BGT = "10001"`, `OP_B = "10010"`, `OP_CALL = "10011"`, `OP_RET = "10100"`
  - Extended Operations: `OP_MIN = "10101"`, `OP_MAX = "10110"`, `OP_ROTS = "10111"`, `OP_XOR = "11000"`, `OP_ABS = "11001"`
  - Privileged/System: `OP_MOVZ = "11010"`, `OP_RETZ = "11011"`
  - Special: `OP_INVALID = "11111"`, `OP_BUBBLE = OP_NOP`
- **Metadata Table (`isa::getMeta(opcode)`)**:
  Returns `InstructionMeta` struct containing: `mnemonic`, `format` (R, I, B, J), `is_load`, `is_store`, `is_branch`, `is_call`, `is_ret`, `writes_rd`, `reads_rs1`, `reads_rs2`, `is_alu`, `is_privileged`.

### 1.2 `common/Types.hpp` (Lines 1–110)
- **Role**: Architectural basic types and non-opcode enumerations.
- **Types**:
  - `Word = uint32_t` (32-bit CPU word, address, and register value).
  - `HalfWord = uint16_t`, `Byte = uint8_t`.
  - `Address = uint64_t` (supports up to 48-bit virtual address and 52-bit physical address).
- **Structures**:
  - `DecodedInst`: Carries `opcode` (5-bit string), `rd`, `rs1`, `rs2`, `imm` (signed 32-bit integer), `isImmediate`, `valid`, `cpl` (Current Privilege Level: 0=Kernel, 1=User).
  - `BranchPolicy`: `ALWAYS_NOT_TAKEN`, `ONE_BIT`, `TWO_BIT_BIMODAL`, `GSHARE`.
  - `ForwardingMode`: `NONE`, `BASIC`, `FULL`, `FULL_PLUS`.
  - `DivAlgo`: `RESTORING`, `NON_RESTORING`, `NEWTON_RAPHSON`.
  - `AdderType`: `RCA`, `CSLA`, `CLA`.
  - `MultiplierType`: `ITERATIVE`, `BOOTH_RADIX4`, `WALLACE_TREE`.

### 1.3 `common/RegisterFile.hpp` (Lines 1–95)
- **Role**: Models the general-purpose registers and CPU architectural status flags.
- **Registers**:
  - 16 General Purpose Registers: `r0` through `r15`.
  - Aliases: `r14` is `sp` (Stack Pointer), `r15` is `ra` (Return Address).
  - Privileged Registers: `cpl` (Current Privilege Level: 0=Kernel, 1=User), `oldPC` (saved PC on interrupt/trap), `oldSP` (saved SP on privilege transition).
- **Flags**:
  - `E` (Equal flag): Set when ALU comparison operands are equal.
  - `GT` (Greater-Than flag): Set when first operand is strictly greater than second operand.
- **Invariants**:
  - In hardware, reading from a register being written in the same cycle exhibits write-before-read transparency (handled by Forwarding Unit or internal bypass).

### 1.4 `common/Memory.hpp` (Lines 1–115)
- **Role**: Simple flat physical memory buffer used by the ISS and standalone tests.
- **Capacity**: Configurable (default 1 MB to 64 MB).
- **Addressing**: Little-endian byte-addressed.
- **Operations**:
  - `readWord(addr)`: Reads 4 bytes and packs into little-endian uint32_t.
  - `writeWord(addr, val)`: Unpacks uint32_t into 4 bytes in memory.
  - Checks alignment and bounds, throwing exceptions on access violations.

### 1.5 `common/ForwardingUnit.hpp` (Lines 1–150)
- **Role**: Eliminates RAW (Read-After-Write) data hazard stalls by detecting when an operand needed by the ALU in `EX` stage is currently resident in a downstream pipeline latch (`EX/MEM`, `MEM/WB`).
- **Forwarding Sources**:
  - `FORWARD_NONE`: Operand read directly from Register File.
  - `FORWARD_FROM_EX_MEM` (`EX -> EX` bypass): Zero-latency bypass from the instruction just finished in ALU.
  - `FORWARD_FROM_MEM_WB` (`MEM -> EX` bypass): Bypasses load data or earlier ALU result from memory/writeback latch.
- **Hazard Priority**: Always forward from `EX/MEM` first if both stages write to the same register, ensuring the youngest value is consumed.

### 1.6 `common/HazardUnit.hpp` (Lines 1–145)
- **Role**: Interlocks the pipeline when forwarding is physically impossible.
- **The Load-Use Hazard**:
  - When instruction $i$ is a `LOAD` (`OP_LD`) and instruction $i+1$ uses the loaded register as `rs1` or `rs2`.
  - Because load data is only returned at the end of the `MA` stage, it cannot be bypassed to `EX` in time.
  - Action: Injects **1 bubble** into `ID/EX` and stalls `IF` and `OF/ID` latches for 1 cycle.
- **Branch Mispredict Flush**:
  - Injects bubbles into young pipeline stages when a branch condition evaluation disagrees with the branch predictor.

### 1.7 `common/BranchPredictor.hpp` (Lines 1–160)
- **Implemented Predictors**:
  - `StaticPredictor`: Predicts branch not taken (ideal for backward-compatible baseline).
  - `OneBitPredictor`: 1-bit state table indexed by lower PC bits (switches on single mispredict).
  - `TwoBitBimodalPredictor`: 2-bit saturating up-down counter (`00` Strongly Not Taken, `01` Weakly Not Taken, `10` Weakly Taken, `11` Strongly Taken). Tolerates loop exit anomalies.
  - `GSharePredictor`: XORs global branch history register (BHR) with PC bits to capture correlation between neighboring branches.
  - `BranchTargetBuffer (BTB)`: Caches target jump addresses, eliminating the branch target calculation penalty.
  - `ReturnAddressStack (RAS)`: 8-entry LIFO stack predicting function returns (`ret`) with 100% accuracy.

### 1.8 `common/ISS.hpp` (Lines 1–210)
- **Role**: Golden Reference Instruction Set Simulator.
- **Execution Model**: Single-cycle, non-pipelined, architectural reference model.
- **Purpose**: Used by unit tests and `verify_golden.cpp` to perform differential verification against both 4-stage and 6-stage pipelines.

---

## 2. Module: `alu/`

### 2.1 `alu/ALU.hpp` (Lines 1–165)
- **Single Entry Point**: `execute(const std::string& opcodeBits, Word op1, Word op2, Flags& flags)`
- **Dispatch**: Clean `if-else` chain comparing against canonical string constants `isa::OP_ADD`, `isa::OP_SUB`, etc.
- **Integrated Arithmetic Cores**:
  - Adders: Uses `Adders.hpp` for CLA, CSLA, or RCA.
  - Multipliers: Uses `Multipliers.hpp` for Booth Radix-4 or Wallace Tree.
  - Dividers: Uses `Dividers.hpp` for Non-restoring or Newton-Raphson.
  - Barrel Shifter: Uses `Shifter.hpp` for logarithmic 1-cycle shifts.

### 2.2 `alu/Adders.hpp` (Lines 1–120)
- **Ripple Carry Adder (RCA)**: $O(N)$ delay. Chained full adders.
- **Carry Select Adder (CSLA)**: Divides 32 bits into 4-bit blocks, speculatively computing sums for carry-in=0 and carry-in=1, multiplexing at the end.
- **Carry Lookahead Adder (CLA)**: $O(\log N)$ delay. Computes Generate ($G_i = A_i \cdot B_i$) and Propagate ($P_i = A_i \oplus B_i$) signals across 4-bit hierarchical groups.

### 2.3 `alu/Multipliers.hpp` (Lines 1–190)
- **Iterative Shift-Add Multiplier**: 32 cycles.
- **Booth's Radix-4 Multiplier**: 16 cycles. Encodes 3 overlapping bits ($y_{2i+1}, y_{2i}, y_{2i-1}$) into operation set $\{0, +M, -M, +2M, -2M\}$, halving partial product count.
- **Wallace Tree Multiplier**: Carry-save adder (CSA) tree reducing 32 partial products to 2 vectors in $O(\log_{1.5} N)$ levels, followed by a fast final adder.

### 2.4 `alu/Dividers.hpp` (Lines 1–185)
- **Restoring Divider**: 32 cycles. Subtracts divisor, and if remainder is negative, adds divisor back.
- **Non-Restoring Divider**: 32 cycles. Eliminates the restore addition by conditionally adding or subtracting in the subsequent cycle based on sign of remainder.
- **Newton-Raphson Divider**: High-speed iterative approximation calculating reciprocal $X_{k+1} = X_k(2 - D \cdot X_k)$ with quadratic convergence, followed by single multiply.

---

## 3. Module: `processor_4stage/`

### 3.1 `processor_4stage/StageRegisters4.hpp` (Lines 1–85)
- Defines inter-stage latches:
  - `IF_OF_Latch`: `pc`, `instructionWord`, `valid`.
  - `OF_EX_Latch`: `pc`, `opcode`, `rd`, `rs1Val`, `rs2Val`, `imm`, `isImmediate`, `rs1`, `rs2`.
  - `EX_MA_Latch`: `pc`, `opcode`, `rd`, `aluResult`, `storeVal`, `flags`.
  - `MA_RW_Latch`: `pc`, `opcode`, `rd`, `finalVal`, `regWrite`.

### 3.2 `processor_4stage/Pipeline4Stage.hpp` (Lines 1–480)
- **Datapath**:
  1. **IF (Instruction Fetch)**: Fetches instruction word at current PC from memory or L1I cache; increments PC by 4; queries Branch Predictor.
  2. **OF (Operand Fetch / Decode)**: Decodes instruction word into 5-bit opcode string, destination register, and source registers; reads register file.
  3. **EX (Execute)**: Evaluates ALU operation, resolves conditional branches, evaluates branch target.
  4. **MA_RW (Memory Access & Register Write)**: Combined memory access and register writeback stage. Reads/writes memory; updates register file.

---

## 4. Module: `processor_6stage/`

### 4.1 `processor_6stage/Pipeline6Stage.hpp` (Lines 1–520)
- **Motivation**: In a 4-stage pipeline, `OF` stage performs instruction decoding AND register file reading sequentially, limiting clock frequency to 500 MHz ($t_{\text{crit}} = 2.0\text{ ns}$).
- **The Split**: 6-stage splits `OF` into two distinct clock cycles:
  1. **IF**: Instruction Fetch
  2. **ID (Instruction Decode)**: Decodes binary fields, generates control signals.
  3. **RF (Register File Read)**: Reads source registers from SRAM register file.
  4. **EX**: ALU execution.
  5. **MA**: Memory access.
  6. **WB**: Register writeback.
- **Frequency Scaling**: Critical path drops to $1.25\text{ ns}$, enabling clock frequency of **800 MHz** (a 1.6× clock rate increase!).

---

## 5. Module: `microcode/`

### 5.1 `microcode/MicroInstructions.hpp` (Lines 1–110)
- Models micro-operations: register transfers, ALU function select, memory read/write control lines, sequencing (`NEXT`, `BRANCH_MICRO`, `FETCH`).

### 5.2 `microcode/HorizontalControl.hpp` (Lines 1–140)
- Unencoded 32-bit control words. Every control signal has a dedicated wire. Zero decode logic delay; maximum flexibility; larger control store ROM.

### 5.3 `microcode/VerticalControl.hpp` (Lines 1–160)
- Encoded 16-bit control words. Related control signals are encoded into fields (ALU function field, RegDest field, MemControl field), requiring hardware decoders. Cuts control store ROM size in half at the expense of decoder delay.

### 5.4 `microcode/MicroController.hpp` (Lines 1–280)
- Contains dispatch ROM mapping canonical 5-bit opcodes to starting microPC addresses.
- Implements step-by-step micro-instruction stepping for interactive debugging.

---

## 6. Module: `assembler/`

### 6.1 `assembler/Assembler.hpp` (Lines 1–320)
- **Pass 1**: Scans lines, records label names and their byte offsets into `std::unordered_map<std::string, Word> symbolTable`.
- **Pass 2**: Emits 32-bit machine code words, resolving label forward references to relative PC offsets.
- **Instruction Scheduling**: Basic-block instruction scheduling pass reorganizes independent instructions to fill load-use delay slots, eliminating bubbles.

### 6.2 `assembler/MacroPreprocessor.hpp` (Lines 1–145)
- Expands high-level assembly macros into baseline RISC201 instructions:
  - `push reg`: `sub sp, sp, 4` followed by `st reg, [sp]`.
  - `pop reg`: `ld reg, [sp]` followed by `add sp, sp, 4`.
  - `call label`: `st ra, [sp - 4]`, saves PC, jumps to label.
  - `ret`: loads return address, jumps to `ra`.

### 6.3 `assembler/Disassembler.hpp` (Lines 1–180)
- Reads 32-bit machine code words, extracts the 5-bit opcode field, queries `isa::getMeta()`, and formats into human-readable assembly mnemonics and operands.

---

## 7. Module: `memory/`

### 7.1 `memory/Cache.hpp` (Lines 1–280)
- Multi-level cache modeling (L1I, L1D, L2, L3).
- Configurable capacity, associativity (direct-mapped up to 16-way), line size (64 bytes).
- Non-blocking MSHR (Miss Status Holding Register) tracks in-flight misses.
- 3C miss telemetry classifying misses into Compulsory, Capacity, and Conflict.

### 7.2 `memory/DRAM.hpp` (Lines 1–240)
- Cycle-accurate DDR4-3200 and DDR5-4800 memory controller.
- Models channels, ranks, bank groups, row buffer hits, conflicts, and timing constraints ($t_{RCD}, t_{RP}, t_{CL}$).
- FR-FCFS (First-Ready First-Come First-Served) request scheduler prioritizing row buffer hits.

### 7.3 `memory/VirtualMemory.hpp` (Lines 1–409)
- 4-level Radix-4 tree translation (PML4 $\rightarrow$ PDPT $\rightarrow$ PD $\rightarrow$ PT).
- Multi-tier TLB hierarchy (L1 ITLB, L1 DTLB, L2 STLB).
- Page-Walk Cache (PWC) caching intermediate tree pointers.
- Demand paging frame allocator with Clock and FIFO page replacement algorithms.
- Stack Guard Page boundary protection.

---

## 8. Module: `visualizer/` & `debugger/`

### 8.1 `visualizer/PipelineVisualizer.hpp`
- Visual ASCII pipeline diagram showing clock cycle grid, stage occupancy, and forwarding arrows.

### 8.2 `visualizer/MemoryVisualizer.hpp`
- Displays set-way grid of cache line tags, valid/dirty bits, and DRAM bank open row states.

### 8.3 `visualizer/StackVisualizer.hpp`
- ASCII rendering of the full-descending stack frame, highlighting `SP`, `FP`, saved return addresses, and local variables.
