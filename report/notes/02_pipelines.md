# Module Notes: Pipelined Processors (4-Stage vs 6-Stage)

## 1. Architectural Stage Breakdown

### 1.1 4-Stage Pipeline (`processor_4stage/Pipeline4Stage.hpp`)
* **Datapath Flow**: $\text{IF} \rightarrow \text{OF} \rightarrow \text{EX} \rightarrow \text{MA\_RW}$
* **Stage Merging Decision**:
  * **Memory Access (MA)** and **Register Writeback (RW)** are merged into a single stage `MA_RW`.
  * **Hardware Rationale**: In SimpleRisc, ALU operations never access memory; they only perform register writeback. Load operations access data memory and latch their result into the destination register during the trailing edge of the same clock phase. Merging eliminates a pipeline latch stage, shortens pipeline depth to 4, reduces the branch misprediction penalty to **2 clock cycles**, and yields lower CPI.
* **Stage Definitions**:
  1. **`IF` (Instruction Fetch)**: Sends `pc` to memory hierarchy (L1 ITLB $\rightarrow$ L1I Cache), fetches 32-bit word, predicts branch direction/target via BranchPredictor, updates PC ($\text{PC} + 4$ or predicted target), latches into `next_IF_OF`.
  2. **`OF` (Operand Fetch)**: Decodes 32-bit instruction bits into `DecodedInst`, reads register operands $r_{s1}, r_{s2}$ from `RegisterFile`, signs/shifts immediate, resolves RAW data hazards and control hazards via `HazardUnit` and `ForwardingUnit`, latches into `next_OF_EX`.
  3. **`EX` (Execute)**: Evaluates arithmetic/logic/branch comparison in `ALU`, computes load/store effective address ($r_{s1} + imm$), verifies branch condition against prediction, flushes earlier stages on misprediction, latches into `next_EX_MARW`.
  4. **`MA_RW` (Memory Access & Writeback)**: Performs load/store to `MemoryHierarchy` (L1 DTLB $\rightarrow$ L1D Cache). Writes ALU result or loaded data back to `RegisterFile` at destination $r_d$ (skipping $r_0$).

### 1.2 6-Stage Pipeline (`processor_6stage/Pipeline6Stage.hpp`)
* **Datapath Flow**: $\text{IF} \rightarrow \text{ID} \rightarrow \text{RF} \rightarrow \text{EX} \rightarrow \text{MA} \rightarrow \text{RW}$
* **Stage Splitting Decision**:
  * **Operand Fetch (OF)** is split into two distinct stages:
    1. **`ID` (Instruction Decode & Immediate Generation)**
    2. **`RF` (Register Fetch & Operand Routing)**
  * **Hardware Rationale**: In high-frequency designs, decoding 32-bit instruction fields, calculating immediate bit shifts, and accessing multi-ported SRAM register cells in a single clock cycle creates an excessively long critical path. Splitting `OF` into `ID` and `RF` cuts the cycle time substantially (e.g. allowing the clock to scale from 500 MHz / 2.0 ns to 800 MHz / 1.25 ns, or 3.2 GHz to 3.6 GHz), delivering superior wall-clock execution time despite a slightly deeper branch penalty.
* **Stage Definitions**:
  1. **`IF` (Instruction Fetch)**: Fetches instruction word, consults BTB/BranchPredictor, advances PC.
  2. **`ID` (Instruction Decode)**: Slices 5-bit opcode string, extracts register indices ($r_d, r_{s1}, r_{s2}$), unpacks 16-bit/27-bit immediate with modifier decoding.
  3. **`RF` (Register Fetch)**: Reads physical register file ports $A$ and $B$, checks data hazards in `HazardUnit`, evaluates forward bypass operands.
  4. **`EX` (Execute)**: ALU computation, branch condition resolution, branch target address validation, misprediction detection.
  5. **`MA` (Memory Access)**: Dedicated data cache/DRAM memory access stage for `ld` and `st`.
  6. **`RW` (Register Writeback)**: Dedicated register file write stage.

---

## 2. Pipeline Latches & Edge-Triggered Synchronization

To avoid race conditions and model true flip-flop edge-triggered transfers:
* Each stage reads from the current cycle latch (`l_XX_YY`).
* Each stage writes to the next cycle buffer (`next_XX_YY`).
* At cycle end, `tick()` commits `l_XX_YY = next_XX_YY`.

### 2.1 4-Stage Latches (`processor_4stage/StageRegisters4.hpp`)
* `Latch_IF_OF`: `pc`, `instructionWord`, `instructionBits`, `valid`, `bubble`, `predictedTaken`, `predictedTarget`.
* `Latch_OF_EX`: `pc`, `decoded` (`opcode`, `rd`, `rs1`, `rs2`, `imm`, `format`), `valRs1`, `valRs2`, `valid`, `bubble`.
* `Latch_EX_MARW`: `pc`, `decoded`, `aluResult`, `valRs2` (store data), `flags`, `branchTaken`, `branchTarget`, `valid`, `bubble`.

### 2.2 6-Stage Latches (`processor_6stage/StageRegisters6.hpp`)
* `Latch_IF_ID`: `pc`, `instructionWord`, `instructionBits`, `valid`, `bubble`.
* `Latch_ID_RF`: `pc`, `decoded`, `valid`, `bubble`.
* `Latch_RF_EX`: `pc`, `decoded`, `valRs1`, `valRs2`, `valid`, `bubble`.
* `Latch_EX_MA`: `pc`, `decoded`, `aluResult`, `valRs2`, `flags`, `valid`, `bubble`.
* `Latch_MA_RW`: `pc`, `decoded`, `aluResult`, `memData`, `valid`, `bubble`.

---

## 3. Comparative Architecture Trade-Offs

| Architectural Property | 4-Stage Core | 6-Stage Core | Architectural Analysis |
| :--- | :---: | :---: | :--- |
| **Stage Count** | 4 | 6 | 4-stage merges MA and RW; 6-stage splits OF into ID and RF. |
| **Branch Penalty** | 2 cycles | 3 cycles | Branch resolves in EX; 4-stage squashes IF and OF; 6-stage squashes IF, ID, and RF. |
| **Load-Use Stall Penalty (Full Fwd)** | 0 cycles | 1 cycle | In 4-stage, `ld` finishes memory access in MA_RW, bypassing directly into EX. In 6-stage, `ld` produces data at MA end, requiring 1 bubble for immediately dependent EX consumers. |
| **Critical Path Delay** | Higher ($\sim 2.0\text{ ns}$) | Lower ($\sim 1.25\text{ ns}$) | Decode + Register SRAM access in one cycle limits 4-stage clock speed. |
| **Max Clock Frequency** | $\sim 500\text{ MHz} - 3.2\text{ GHz}$ | $\sim 800\text{ MHz} - 3.6\text{ GHz}$ | 6-stage achieves $\sim 60\%$ higher clock frequency. |
| **Typical Benchmark CPI** | $1.02 - 1.15$ | $1.10 - 1.25$ | 4-stage achieves lower CPI due to shorter branch and load penalties. |
| **Wall-Clock Execution Time** | Slower | **$\approx 30\% - 45\%$ Faster** | High clock frequency overcomes slight CPI increase, winning in wall-clock execution time. |

