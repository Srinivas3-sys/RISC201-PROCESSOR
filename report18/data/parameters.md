# RISC201-PROCESSOR Architectural Parameters & Specifications

Extracted directly from modern C++17 implementation source files.

## 1. Instruction Set Architecture (ISA) & Encoding
- **Source File:** `common/ISA.hpp:20-185`, `common/Types.hpp:10-85`
- **Word Width:** 32 bits (fixed length, little-endian byte ordering).
- **Opcode Encoding:** 28 canonical 5-bit binary strings (`std::string`). Numbers appear strictly at the 32-bit instruction-word boundary (`decode()` / `encode()`).
  - Arithmetic: `OP_ADD = "00000"`, `OP_SUB = "00001"`, `OP_MUL = "00010"`, `OP_DIV = "00011"`, `OP_MOD = "00100"`
  - Logical: `OP_AND = "00101"`, `OP_OR = "00110"`, `OP_NOT = "00111"`, `OP_XOR = "01000"`
  - Shifts: `OP_LSL = "01001"`, `OP_LSR = "01010"`, `OP_ASR = "01011"`, `OP_ROR = "01100"`
  - Data Transfer: `OP_MOV = "01101"`, `OP_LD = "01110"`, `OP_ST = "01111"`
  - Control Flow: `OP_BEQ = "10000"`, `OP_BGT = "10001"`, `OP_B = "10010"`, `OP_CALL = "10011"`, `OP_RET = "10100"`
  - Stack Operations: Expanded by preprocessor into `sub sp, sp, 4; st rx, 0[sp]` (Push) and `ld rx, 0[sp]; add sp, sp, 4` (Pop).
  - Privileged & System: `OP_HALT = "10101"`, `OP_NOP = "10110"`, `OP_IN = "10111"`, `OP_OUT = "11000"`, `OP_SYS = "11001"`, `OP_RETZ = "11010"`, `OP_INVALID = "11111"`
  - Unassigned opcodes: `11011` to `11110` (reserved, decode raises `ILLEGAL_INSTRUCTION`).
- **Instruction Formats:**
  - Register Format: `[31:27] opcode (5b) | [26:23] rd (4b) | [22:19] rs1 (4b) | [18:15] rs2 (4b) | [14:0] unused/flags (15b)`
  - Immediate Format: `[31:27] opcode (5b) | [26:23] rd (4b) | [22:19] rs1 (4b) | [18] I-flag (1b) | [17:0] imm18 (18b signed)`
  - Branch Format: `[31:27] opcode (5b) | [26:0] offset27 (27b signed)`
- **Register File:** `common/RegisterFile.hpp:15-60`
  - 16 general-purpose 32-bit registers (`r0` to `r15`).
  - `r0`: Hardwired to constant zero (writes are silently discarded).
  - `r1` - `r13`: General scratchpad, argument, and temporary registers.
  - `r14`: Link Register (`lr` / `ra`), receives return address on `call`.
  - `r15`: Stack Pointer (`sp`), enforces full-descending stack convention.
- **CPU Flags:** `common/Types.hpp:75-92`
  - Zero Flag (`Z`): Bit 0 of EFLAGS, set when ALU result equals zero.
  - Negative Flag (`N`): Bit 1 of EFLAGS, set when bit 31 of ALU result equals 1.
  - Overflow Flag (`V`): Bit 2 of EFLAGS, set on signed 2s-complement arithmetic overflow.
  - Carry Flag (`C`): Bit 3 of EFLAGS, set on unsigned carry-out from adder.

## 2. Microarchitecture & Pipeline Stages
- **4-Stage Core:** `processor_4stage/Pipeline4Stage.hpp:25-95`
  - Stages: `IF` (Fetch) -> `OF` (Decode & Reg Read) -> `EX` (Execute & Branch Resolve) -> `MA_RW` (Memory Access & Writeback).
  - Clock Frequency: 500 MHz (Clock period $t_{clk} = 2.00$ ns).
  - Pipeline Depth: $N = 4$ stages. Cold fill latency $= N - 1 = 3$ cycles.
  - Branch Resolution: Resolved in `EX` (Stage 3). Branch penalty $= 3 - 2 = 1$ cycle bubble.
- **6-Stage Core:** `processor_6stage/Pipeline6Stage.hpp:30-110`
  - Stages: `IF` (Fetch) -> `ID` (Instruction Decode) -> `RF` (Register Read) -> `EX` (Execute & Branch Resolve) -> `MA` (Memory Access) -> `WB` (Writeback).
  - Clock Frequency: 800 MHz (Clock period $t_{clk} = 1.25$ ns).
  - Pipeline Depth: $N = 6$ stages. Cold fill latency $= N - 1 = 5$ cycles.
  - Branch Resolution: Resolved in `EX` (Stage 4). Branch penalty $= 4 - 2 = 2$ cycle bubbles.

## 3. Pipeline Hazards & Resolution
- **Source Files:** `common/HazardUnit.hpp:18-120`, `common/PerformanceTracker.hpp:30-65`
- **Hazard Categories:**
  - Structural Hazard: Eliminated via separate Harvard L1 Instruction and Data caches.
  - RAW Data Hazard: True data dependency resolved via Forwarding or Stalls.
  - WAR / WAW Hazards: Mathematically proved impossible in an in-order pipeline with in-order operand reads (`OF`/`RF`) and in-order register writes (`MA_RW`/`WB`).
  - Control Hazard: Branch mispredictions flush younger stages (`IF_OF` in 4S; `IF_ID` + `ID_RF` in 6S).
- **Stall Reason Codes:**
  - `STALL_NONE`: No stall, instruction proceeds normally.
  - `STALL_RAW_DATA`: Interlock waiting for register writeback (when forwarding is disabled).
  - `STALL_LOAD_USE`: Mandatory 1-cycle bubble between memory load and dependent ALU consumer.
  - `STALL_BRANCH_FLUSH`: Pipeline squash on branch misprediction.
  - `STALL_MEMORY_L1D`: L1 data cache miss stall.
  - `STALL_MEMORY_L1I`: L1 instruction cache miss stall.
  - `STALL_TLB_PAGEWALK`: Hardware radix page table walk penalty.
  - `STALL_DRAM_BURST`: DRAM memory controller latency.
  - `STALL_STRUCTURAL`: Hardware structural contention.

## 4. Branch Prediction Suite
- **Source File:** `common/BranchPredictor.hpp:15-180`
- **Policies:**
  1. `PRED_STATIC_NOT_TAKEN`: Always predicts sequential fall-through ($PC + 4$).
  2. `PRED_STATIC_TAKEN`: Always predicts branch taken.
  3. `PRED_1BIT_BIMODAL`: 256-entry table of 1-bit flags.
  4. `PRED_2BIT_BIMODAL`: 256-entry table of 2-bit saturating counters (Strongly Not Taken `00`, Weakly Not Taken `01`, Weakly Taken `10`, Strongly Taken `11`).
  5. `PRED_GSHARE`: 256-entry table indexed by $(PC[9:2] \oplus GHR[9:0]) \pmod{256}$, 10-bit Global History Register.
  6. `PRED_BTB`: 64-entry direct-mapped Branch Target Buffer caching resolved branch targets.
  7. `PRED_RAS`: 16-entry Return Address Stack intercepting `call` (push) and `ret` (pop) instructions.

## 5. Data Forwarding Network
- **Source File:** `common/ForwardingUnit.hpp:40-165`
- **Forwarding Modes:**
  - `NONE`: No bypass paths; interlocks stall pipeline until producing instruction retires.
  - `BASIC`: Forwarding between immediately adjacent ALU instructions (`EX -> EX`).
  - `FULL`: Complete active forwarding network (`EX -> EX`, `MEM -> EX`, `WB -> EX`, and `MEM -> MEM` store-data).
  - `FULL_PLUS`: Includes all `FULL` bypass paths plus a 16-entry `StoreBuffer` enabling store-to-load forwarding before cache line commitment.
- **Priority Resolution:**
  - Youngest producer strictly overrides older instructions:
  - $	ext{Priority: } 	ext{Stage}_{k-1} (	ext{EX\_MA}) > 	ext{Stage}_{k-2} (	ext{MA\_RW}) > 	ext{Stage}_{k-3} (	ext{RW})$.

## 6. Microprogrammed Control Units
- **Source Files:** `microcode/HorizontalControl.hpp`, `microcode/VerticalControl.hpp`, `microcode/MicroController.hpp`
- **Horizontal Control Store:**
  - Micro-instruction width: **65 bits**.
  - Control bits: 33 discrete unencoded datapath control signals (ALU opcodes, register read/write enables, multiplexer selectors, bus enables).
  - ROM size: 28 routines $	imes$ 5 micro-instructions $= 140$ words.
  - Total bit storage: $140 	imes 65 = 9,100$ bits.
- **Vertical Control Store:**
  - Micro-instruction width: **45 bits** (encoded function fields with hardware decoders).
  - Fields: ALU function code (4b), Source Reg A (4b), Source Reg B (4b), Dest Reg (4b), Memory Op (3b), Branch Op (3b), Next microPC address (8b), Condition (3b), Immediate (12b).
  - Total bit storage: $140 	imes 45 = 6,300$ bits.
  - Bit Storage Savings: $rac{9,100 - 6,300}{9,100} = \mathbf{30.77\%}$ reduction in control ROM footprint.

## 7. Arithmetic Logic Unit (ALU) Algorithms
- **Source Files:** `alu/ALU.hpp`, `alu/Adders.hpp`, `alu/Dividers.hpp`, `alu/Multipliers.hpp`, `alu/Shifter.hpp`, `alu/LogicGates.hpp`
- **Adder Architectures:**
  - Ripple-Carry Adder (RCA): Latency $= O(N) = 32$ gate delays.
  - Carry-Lookahead Adder (CLA): 4-bit blocks with generate/propagate tree: Latency $= O(\log N) = 4$ gate delays.
  - Carry-Select Adder (CSLA): Dual ripple chains evaluated for $C_{in}=0$ and $C_{in}=1$: Latency $= 8$ gate delays.
- **Multiplier Architectures:**
  - Shift-and-Add: 32 cycles.
  - Booth Radix-4: 2 bits recoded per cycle with overlapping triplets: 16 cycles.
  - Wallace Tree: 3:2 carry-save reduction tree: 4 cycles.
- **Divider Architectures:**
  - Restoring Divider: 32 cycles (subtract, test sign, restore remainder if negative).
  - Non-Restoring Divider: 32 cycles (subtract or add based on sign, eliminates restorative cycle).
  - Newton-Raphson Divider: Iterative reciprocal refinement $X_{k+1} = X_k (2 - D \cdot X_k)$ followed by multiplication $Q = N \cdot X_m$. Converges quadratically in 3 iterations.
- **Logarithmic Barrel Shifter:**
  - 5 cascading multiplexer stages ($2^0, 2^1, 2^2, 2^3, 2^4$ bit shifts). Latency $= 1$ clock cycle.

## 8. Memory Hierarchy & Caches
- **Source Files:** `memory/Cache.hpp:45-125`, `memory/Prefetcher.hpp`, `memory/DRAM.hpp`
- **L1 Data / Instruction Cache:**
  - Capacity: $C = 32$ KB.
  - Cache Line Size: $B = 64$ bytes ($b = 6$ offset bits).
  - Associativity: $W = 8$-way set-associative.
  - Sets: $S = rac{32768}{8 	imes 64} = 64$ sets ($s = 6$ index bits).
  - Tag Width: $t = 32 - (6 + 6) = 20$ bits.
  - Hit Latency: 1 cycle.
  - Write Policy: Write-Back with Write-Allocate (`wb_wa`).
  - Replacement Policies: True LRU, SRRIP (2-bit Static Re-Reference Interval Prediction), BIP (Bimodal Insertion Policy).
  - Non-Blocking MSHRs: 8 Miss Status Holding Registers supporting hit-under-miss and secondary miss merging.
- **Hardware Prefetchers:**
  - Stream Prefetcher: 4 unit-stride stream buffers (prefetch degree 2).
  - Stride Prefetcher: 16-entry reference prediction table detecting non-unit strides.
- **DRAM Controller:**
  - Standard: JEDEC DDR4-3200 (Clock period $t_{CK} = 0.625$ ns, 1600 MHz clock).
  - Timings: $t_{CL} = 22, t_{RCD} = 22, t_{RP} = 22, t_{RAS} = 52, t_{RC} = 74, t_{WR} = 24, t_{BURST} = 4$.
  - Row Hit: $t_{CL} + t_{BURST} = 22 + 4 = \mathbf{26}$ DRAM cycles ($16.25$ ns).
  - Row Empty: $t_{RCD} + t_{CL} + t_{BURST} = 22 + 22 + 4 = \mathbf{48}$ DRAM cycles ($30.00$ ns).
  - Row Conflict: $t_{RP} + t_{RCD} + t_{CL} + t_{BURST} = 22 + 22 + 22 + 4 = \mathbf{70}$ DRAM cycles ($43.75$ ns).
  - Bank Groups: 16 banks arranged in 4 bank groups. Same bank group penalty: $t_{CCD\_L} = 8$ cycles vs $t_{CCD\_S} = 4$ cycles across distinct bank groups.
  - Memory Scheduler: FR-FCFS (First-Ready First-Come First-Served) reordering requests to maximize row-buffer hits.

## 9. Virtual Memory & Address Translation
- **Source File:** `memory/VirtualMemory.hpp:15-165`
- **Address Space:** 48-bit Virtual Address space, 4 KB page size ($12$ offset bits).
- **4-Level Radix Page Table:**
  - Level 4: PML4 (Page Map Level 4), bits `[47:39]` (9 bits $\implies 512$ entries).
  - Level 3: PDPT (Page Directory Pointer Table), bits `[38:30]` (9 bits $\implies 512$ entries).
  - Level 2: PD (Page Directory), bits `[29:21]` (9 bits $\implies 512$ entries).
  - Level 1: PT (Page Table), bits `[20:12]` (9 bits $\implies 512$ entries).
- **Page-Walk Cache (PWC):**
  - 16-entry cache storing upper directory translation entries (PML4, PDPT, PD).
  - Latency reduction: Bypasses up to 3 serial memory references, reducing walk latency by up to $74\%-85.4\%$.
- **TLB Organization:**
  - L1 ITLB: 64 entries, 4-way set-associative (1-cycle hit).
  - L1 DTLB: 64 entries, 4-way set-associative (1-cycle hit).
  - L2 STLB: 1024 entries, 8-way set-associative (7-cycle hit).

## 10. Stack Safety & Runtime Exceptions
- **Source Files:** `common/Exception.hpp`, `visualizer/StackVisualizer.hpp`
- **Stack Convention:** Full-descending stack. `SP` (`r15`) points directly to the last pushed word. Push decrements `SP` by 4 before storing; Pop loads before incrementing `SP` by 4.
- **Safety Checks:** Memory accesses below the stack boundary or above the allocated frame raise a precise `STACK_OVERFLOW` or `STACK_UNDERFLOW` CPU exception.
- **Interactive Debugger:** `debugger/Debugger.hpp:120-450`
  - Breakpoints, watchpoints, step-by-step cycle execution, register inspection (`reg`), memory dump (`mem`), pipeline visualization (`pipe`), stack visualization (`stack`).
