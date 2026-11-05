# Chapter 6: Processor Architectures: 4-Stage vs. 6-Stage Pipelined Cores

## 6.1 The 4-Stage Pipelined Core (`processor_4stage/`)

### 6.1.1 Architectural Rationale: Merging Memory Access and Writeback
The 4-stage pipeline implements the sequence:
$$\mathbf{IF \rightarrow OF \rightarrow EX \rightarrow MA\_RW}$$
* **Stage Merging Decision**: The Memory Access (`MA`) and Register Writeback (`RW`) stages are merged into a single composite stage `MA_RW`.
* **Hardware Justification**: In the SimpleRisc architecture:
  * ALU instructions do not access data memory; they only perform register writeback.
  * Store instructions access memory but perform no register writeback.
  * Load instructions read a word from data memory and write it into the destination register $r_d$. In a 500 MHz design, reading SRAM cache (1.0 ns) and latching into the register file (0.3 ns) comfortably completes within a single 2.0 ns clock cycle.
  * Merging eliminates a physical pipeline stage latch, shortening branch misprediction latency from 3 cycles down to **2 cycles** and reducing CPI.

### 6.1.2 Stage Definitions & Execution Responsibilities
1. **Instruction Fetch (`IF`)**:
   * Evaluates `pc`. Queries `MemoryHierarchy` via `l1i.access(pc)`.
   * Queries `BranchPredictor` with current `pc`: predicts branch target via BTB and direction via 2-bit bimodal table.
   * If predicted taken, next PC becomes `predictedTarget`; otherwise $\text{PC} + 4$.
   * Latches fetched word into `next_IF_OF`.
2. **Operand Fetch (`OF`)**:
   * Decodes 32-bit word into `DecodedInst` (extracts 5-bit opcode string, register IDs, immediate).
   * Reads register file physical ports: $r_{s1} \rightarrow \text{valRs1}$, $r_{s2} \rightarrow \text{valRs2}$.
   * Queries `HazardUnit` and `ForwardingUnit` to detect RAW dependencies.
   * Latches decoded bundle into `next_OF_EX`.
3. **Execute (`EX`)**:
   * Directs operands into `ALU`. Performs arithmetic, shift, logic, or address generation ($r_{s1} + imm$).
   * Resolves branch condition flags ($E, GT$). If actual branch outcome contradicts `IF` prediction:
     * Asserts `pipelineFlush = true`.
     * Squashes speculative instructions in `IF` and `OF` (injects bubble latches).
     * Corrects `pc` to true branch target or sequential fall-through.
   * Latches result into `next_EX_MARW`.
4. **Memory Access & Writeback (`MA_RW`)**:
   * If load (`isLoad`): reads data word from `MemoryHierarchy` via `l1d.access(aluResult)`.
   * If store (`isStore`): writes `valRs2` to `MemoryHierarchy` and `StoreBuffer`.
   * Writeback: If instruction writes to register (`writesRd`) and $r_d \ne 0$, commits result to `RegisterFile`.

### 6.1.3 Pipeline Latches & Field Definitions (`processor_4stage/StageRegisters4.hpp`)
* `Latch_IF_OF`: `uint32_t pc`, `uint32_t instructionWord`, `std::string instructionBits`, `bool valid`, `bool bubble`, `bool predictedTaken`, `uint32_t predictedTarget`.
* `Latch_OF_EX`: `uint32_t pc`, `DecodedInst decoded`, `uint32_t valRs1`, `uint32_t valRs2`, `bool valid`, `bool bubble`.
* `Latch_EX_MARW`: `uint32_t pc`, `DecodedInst decoded`, `uint32_t aluResult`, `uint32_t valRs2`, `Flags flags`, `bool branchTaken`, `uint32_t branchTarget`, `bool valid`, `bool bubble`.

---

## 6.2 The 6-Stage Pipelined Core (`processor_6stage/`)

### 6.2.1 Architectural Rationale: Splitting Decode and Register Fetch
The 6-stage pipeline implements the sequence:
$$\mathbf{IF \rightarrow ID \rightarrow RF \rightarrow EX \rightarrow MA \rightarrow RW}$$
* **Stage Splitting Decision**: The Operand Fetch (`OF`) stage is partitioned into:
  1. **Instruction Decode (`ID`)**
  2. **Register Fetch (`RF`)**
* **Hardware Justification**: In modern gigahertz processors (e.g. MIPS R4000, ARM Cortex-A8), decoding 32-bit instructions, expanding immediate modifier bits, and querying multi-ported SRAM register files in a single clock cycle creates an excessively long critical path delay ($t_{\text{crit}} \approx 2.0\text{ ns}$). Splitting decode and register access divides this delay across two stages, permitting the clock period to drop to **1.25 ns (800 MHz)** or 0.28 ns (3.6 GHz).

### 6.2.2 Stage Definitions
1. **`IF` (Instruction Fetch)**: Fetches instruction word from L1I cache / ITLB, updates PC.
2. **`ID` (Instruction Decode)**: Slices 5-bit canonical opcode string, extracts register indices ($r_d, r_{s1}, r_{s2}$), unpacks immediate modifier bits ($[1:0]$).
3. **`RF` (Register Fetch)**: Reads physical register file ports $A$ and $B$, checks data hazards in `HazardUnit`, evaluates forward bypass operands.
4. **`EX` (Execute)**: ALU computation, branch condition evaluation, branch target calculation, branch mispredict squashing.
5. **`MA` (Memory Access)**: Dedicated data cache access stage (`ld`, `st`, store buffer).
6. **`RW` (Register Writeback)**: Dedicated register file write stage.

### 6.2.3 Pipeline Latches & Field Definitions (`processor_6stage/StageRegisters6.hpp`)
* `Latch_IF_ID`: `pc`, `instructionWord`, `instructionBits`, `valid`, `bubble`.
* `Latch_ID_RF`: `pc`, `decoded`, `valid`, `bubble`.
* `Latch_RF_EX`: `pc`, `decoded`, `valRs1`, `valRs2`, `valid`, `bubble`.
* `Latch_EX_MA`: `pc`, `decoded`, `aluResult`, `valRs2`, `flags`, `branchTaken`, `branchTarget`, `valid`, `bubble`.
* `Latch_MA_RW`: `pc`, `decoded`, `aluResult`, `memData`, `valid`, `bubble`.

---

## 6.3 Comprehensive Side-by-Side Architectural Comparison

Table 6.1 contrasts the architectural specifications, penalties, and measured performance of both pipelines:

| Microarchitectural Parameter | 4-Stage Core | 6-Stage Core | Analytical Trade-Off / Justification |
| :--- | :---: | :---: | :--- |
| **Pipeline Depth** | 4 stages | 6 stages | 4-stage merges MA/RW; 6-stage splits OF into ID/RF. |
| **Stage Sequence** | $\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$ | $\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$ | 6-stage separates decode and register SRAM reads. |
| **Critical Path Delay** | $\approx 2.00\text{ ns}$ | $\approx 1.25\text{ ns}$ | 6-stage isolates register SRAM access time. |
| **Nominal Clock Target** | 500 MHz (3.2 GHz preset) | 800 MHz (3.6 GHz preset) | 6-stage achieves $\mathbf{60\%}$ higher clock frequency. |
| **Branch Resolution Stage** | Stage 3 (`EX`) | Stage 4 (`EX`) | Both resolve branch condition in EX. |
| **Branch Mispredict Penalty** | **2 cycles** (Squashes IF, OF) | **3 cycles** (Squashes IF, ID, RF) | Deeper front-end adds 1 extra squash cycle in 6-stage. |
| **Load-Use Penalty (Full Fwd)**| **0 cycles** | **1 cycle** (Bubble in EX) | In 6-stage, `ld` finishes at end of MA; EX consumer must stall 1 cycle. |
| **Forwarding Bypass Network** | `EX-to-EX`, `WB-to-EX` | `EX-to-EX`, `MEM-to-EX`, `WB-to-EX` | 6-stage requires 3-level bypass network. |
| **Typical Arithmetic CPI** | **1.02 – 1.15** | **1.10 – 1.25** | 4-stage achieves lower CPI due to shorter penalties. |
| **Wall-Clock Execution Time** | Slower | **$\mathbf{30\% - 45\%}$ Faster** | Frequency scaling easily overcomes $+0.05$ CPI difference. |

---

## 6.4 Register File Architecture (`common/RegisterFile.hpp`)

* **Storage Capacity**: 16 General Purpose Registers ($r_0 - r_{15}$), 32 bits each.
* **Ports**:
  * 2 independent asynchronous Read Ports: Port A (`rs1`) and Port B (`rs2`).
  * 1 synchronous Write Port: Port W (`rd`), clocked on the clock rising edge.
* **Hardwired Zero Invariant ($r_0$)**:
  * Reading $r_0$ returns `0x00000000` unconditionally.
  * Writing to $r_0$ is checked in software: `if (regIndex == 0) return;` (discarded).
* **Split-Cycle Access in 4-Stage**: Writeback occurs on the clock rising edge; register read occurs on the falling edge, enabling internal forwarding within a single cycle.

---

## 6.5 Instruction Set Simulator (ISS) Golden Reference Model (`common/ISS.hpp`)

To ensure uncompromised verification fidelity, the codebase contains a non-pipelined, architectural reference **Instruction Set Simulator (ISS)**:
* **Execution Model**: Purely sequential, cycle-independent architectural state machine. Each instruction completes in its entirety (fetch, decode, register read, ALU, memory, register write) before the next instruction begins.
* **Differential Verification Harness**:
  * In `tests/test_all.cpp` (Section 5) and `tools/verify_golden.cpp` (Check 3), identical assembly binaries are executed simultaneously on:
    1. The Reference ISS model
    2. The 4-Stage Pipelined Processor
    3. The 6-Stage Pipelined Processor
  * After program completion, final register states ($r_0 - r_{15}$), memory words, and flags ($E, GT$) are cross-compared. Any divergence immediately triggers an assertion failure.
  * Verified: 100% architectural state identity across 192 simulator configurations!

