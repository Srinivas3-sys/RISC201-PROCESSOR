# Part D: Pipelined Processors (4-Stage vs 6-Stage)

This section explains pipelined execution from absolute zero using analogies, details both the 4-stage and 6-stage implementations in this repository, presents cycle-by-cycle execution tables, and explains why a deeper pipeline can finish programs faster even when its CPI is slightly higher.

---

# Chapter D1: What a Pipeline Is & Why It Helps

### 1. In One Sentence
Pipelining is an implementation technique where multiple instructions overlap in execution across different hardware stages, completing one instruction per clock cycle under ideal conditions.

### 2. Start from Zero: Everyday Analogy
Imagine doing laundry for 4 roommates (Loads A, B, C, D):
- Doing laundry involves 4 steps:
  1. **Wash** (30 mins in Washer)
  2. **Dry** (30 mins in Dryer)
  3. **Fold** (30 mins on Table)
  4. **Put Away** (30 mins into Closet)
- **Sequential Laundry (Non-Pipelined)**:
  - You wash Load A, wait for dryer, fold it, put it away. Total = 2 hours.
  - Only then do you start Load B.
  - Doing 4 loads sequentially takes **8 hours**!
- **Pipelined Laundry**:
  - Load A goes into Washer.
  - When Load A moves to Dryer, **you put Load B into the Washer immediately**!
  - When Load A moves to Fold, Load B goes to Dryer, and Load C enters Washer.
  - After an initial startup delay, **one complete clean load of laundry finishes every 30 minutes**!
  - 4 loads finish in **2.5 hours** instead of 8 hours!

```
Time:     0:00    0:30    1:00    1:30    2:00    2:30
Washer:  [Load A][Load B][Load C][Load D]
Dryer:           [Load A][Load B][Load C][Load D]
Table:                   [Load A][Load B][Load C][Load D]
Closet:                          [Load A][Load B][Load C][Load D] -> Finish!
```

---

### 3. The Concept in Simple Steps
In a CPU, instead of waiting for one instruction to finish all stages (Fetch, Decode, Execute, Memory, Writeback) before starting the next, we put independent hardware units in a sequence:

```
Cycle:    1     2     3     4     5     6     7
Inst 1:  [IF]  [OF]  [EX]  [MA]  [RW]
Inst 2:        [IF]  [OF]  [EX]  [MA]  [RW]
Inst 3:              [IF]  [OF]  [EX]  [MA]  [RW]
Inst 4:                    [IF]  [OF]  [EX]  [MA]  [RW]
```
1. **Clock Cycle**: The fundamental heartbeat of the processor. On each clock tick, instructions advance to the next stage.
2. **Throughput**: Under ideal conditions (no stalls or hazard bubbles), **1 instruction completes every single cycle**.
3. **Latency vs Throughput**:
   - The latency of a single instruction does not decrease (it still takes 4 or 5 cycles from start to finish).
   - But the **throughput (instructions completed per second)** increases dramatically (up to $4\times$ or $6\times$ faster!).
4. **Ideal CPI (Cycles Per Instruction)**:
   $$\text{Ideal CPI} = 1.000$$

---

### 4. How THIS Project Implements It
In this project, pipeline stages are separated by **Stage Registers (Latches)**. On every call to `stepCycle()`, data flows from current latches through stage logic, and updates the next latches for the subsequent cycle.

---

### 5. Worked Example (Ideal 4-Instruction Execution)
Consider 4 independent instructions:
```assembly
mov r1, 10
mov r2, 20
mov r3, 30
mov r4, 40
```
- **Cycle 1**: Inst 1 fetched in `IF`.
- **Cycle 2**: Inst 1 moves to `OF`; Inst 2 fetched in `IF`.
- **Cycle 3**: Inst 1 in `EX`; Inst 2 in `OF`; Inst 3 in `IF`.
- **Cycle 4**: Inst 1 in `MA_RW` (retires!); Inst 2 in `EX`; Inst 3 in `OF`; Inst 4 in `IF`.
- **Cycles 5, 6, 7**: Inst 2, 3, 4 retire one by one.
- Total cycles = 7 cycles for 4 instructions (due to 3 cycles of initial pipeline filling).
- For $N = 100,000$ instructions, total cycles $\approx 100,003 \rightarrow \text{CPI} \approx 1.00003$.

---

### 6. Design Choices & Trade-Offs
- **Why not make a 20-stage pipeline?**
  - Splitting stages adds latch overhead (registers consume chip area and power).
  - Branch mispredicts and data hazards become much more expensive in deeper pipelines (as explained in Chapter D4).

---

# Chapter D2: The 4-Stage Pipeline Core

### 1. In One Sentence
The 4-stage core (`processor_4stage/Pipeline4Stage.hpp`) combines memory access and register writeback into a single stage (`MA_RW`), creating a simple, balanced pipeline (`IF -> OF -> EX -> MA_RW`).

### 2. Start from Zero: Everyday Analogy
Think of a 4-person assembly line in an automotive workshop:
1. **Station 1 (Fetch)**: Worker pulls the parts box from the warehouse shelf.
2. **Station 2 (Decode/Operand Fetch)**: Worker opens the box, reads the instruction sheet, and pulls the matching wrench and bolts from the toolbox.
3. **Station 3 (Execute)**: Mechanic uses the wrench to tighten the bolts.
4. **Station 4 (Memory/Write)**: Inspector writes down the result on the clipboard and files the parts box.

### 3. The Concept in Simple Steps

```
+------------+       +------------+       +------------+       +---------------+
|     IF     | ====> |     OF     | ====> |     EX     | ====> |     MA_RW     |
| Fetch Inst |       | Decode &   |       | Execute in |       | Access Memory |
|  from PC   |       | Read Regs  |       |    ALU     |       | & Write Reg   |
+------------+       +------------+       +------------+       +---------------+
```

1. **Stage 1: IF (Instruction Fetch)**:
   - Reads instruction from `PC`.
   - Increments `PC = PC + 4`.
   - Sends instruction word to `IF/OF` latch.
2. **Stage 2: OF (Operand Fetch / Decode)**:
   - Slices bits: extracts 5-bit opcode string, destination `rd`, source registers `rs1`, `rs2`.
   - Reads operand values from the Register File.
   - Forwards data if an older instruction just wrote that register.
3. **Stage 3: EX (Execute)**:
   - ALU evaluates the arithmetic/logic operation.
   - For memory instructions, computes effective address: `vaddr = rs1Val + imm`.
   - For branches, evaluates condition (`E` and `GT` flags) and computes target PC.
4. **Stage 4: MA_RW (Memory Access & Register Write)**:
   - If `ld`: reads data from memory or L1D cache.
   - If `st`: writes data to memory or L1D cache.
   - Updates target register in Register File: `regFile.write(rd, finalVal)`.

---

### 4. How THIS Project Implements It

#### Latch Registers (`processor_4stage/StageRegisters4.hpp`)
```cpp
// File: processor_4stage/StageRegisters4.hpp (lines 20-45)
struct IF_OF_Latch {
    Word pc{0};
    Word instructionWord{0};
    bool valid{false};
};

struct OF_EX_Latch {
    Word pc{0};
    std::string opcode{isa::OP_NOP};
    uint32_t rd{0};
    uint32_t rs1{0};
    uint32_t rs2{0};
    Word rs1Val{0};
    Word rs2Val{0};
    int32_t imm{0};
    bool isImmediate{false};
    bool valid{false};
};

struct EX_MA_Latch {
    Word pc{0};
    std::string opcode{isa::OP_NOP};
    uint32_t rd{0};
    Word aluResult{0};
    Word storeVal{0};
    bool branchTaken{false};
    Word branchTarget{0};
    bool valid{false};
};
```

---

### 5. Worked Example (Tracing `benchmarks/hazard_test.s` on 4-Stage)
```assembly
0x00: mov r1, 30        ; r1 = 30
0x04: mov r2, 10        ; r2 = 10
0x08: add r3, r1, r2    ; r3 = 30 + 10 = 40 (RAW on r1 and r2!)
```
- **Cycle 1**: `mov r1, 30` enters `IF`.
- **Cycle 2**: `mov r1, 30` in `OF`; `mov r2, 10` in `IF`.
- **Cycle 3**: `mov r1, 30` in `EX`; `mov r2, 10` in `OF`; `add r3, r1, r2` in `IF`.
- **Cycle 4**:
  - `mov r1, 30` in `MA_RW`. ALU result ($30$) forwarded via `ForwardingUnit` to `EX`!
  - `mov r2, 10` in `EX`. Result ($10$) forwarded via `ForwardingUnit` to `EX`!
  - `add r3, r1, r2` in `EX`: ALU receives forwarded operands ($30$ and $10$), computes $40$ with **0 stall cycles**!
- **Cycle 5**: `add r3, r1, r2` writes $40$ to `r3`. Done!

---

### 6. Summary: If You Only Remember 5 Things
1. The 4-stage pipeline is **`IF -> OF -> EX -> MA_RW`**.
2. **`OF`** both decodes instructions and reads the Register File.
3. **`MA_RW`** both reads/writes memory and commits the final result to registers.
4. Latch registers isolate stages so they can execute concurrently on separate clock ticks.
5. With full data forwarding, ALU-to-ALU dependencies execute with **zero stalls**.

---

# Chapter D3: The 6-Stage Pipeline Core

### 1. In One Sentence
The 6-stage core (`processor_6stage/Pipeline6Stage.hpp`) splits the instruction fetch/decode front-end into separate **`ID`** (Instruction Decode) and **`RF`** (Register File Read) stages, shortening the critical path to allow higher clock frequencies.

### 2. Start from Zero: Everyday Analogy
In the 4-stage kitchen, Worker 2 had to BOTH translate the order AND find the ingredients in the pantry in a single 30-minute shift. This was the bottleneck slowing down the entire kitchen.
By hiring an assistant:
- Worker 2A strictly translates and verifies the order tickets (`ID`).
- Worker 2B strictly fetches the ingredients from the shelves (`RF`).
- Now, each station takes only **18 minutes** instead of 30 minutes!
- The whole kitchen runs much faster!

---

### 3. The Concept in Simple Steps

```
+-----+       +-----+       +-----+       +-----+       +-----+       +----+
| IF  | ====> | ID  | ====> | RF  | ====> | EX  | ====> | MA  | ====> | WB |
|Fetch|       |Decod|       |RegRd|       |Exec |       |Mem  |       |Writ|
+-----+       +-----+       +-----+       +-----+       +-----+       +----+
```

1. **IF (Instruction Fetch)**: Fetches instruction word from instruction port.
2. **ID (Instruction Decode)**: Decodes 5-bit opcode string, extracts register numbers and immediate fields.
3. **RF (Register File Read)**: Reads source operands from the Register File.
4. **EX (Execute)**: Evaluates ALU operation and resolves branches.
5. **MA (Memory Access)**: Accesses data memory (load/store).
6. **WB (Writeback)**: Writes final result back into destination register `rd`.

---

### 4. How THIS Project Implements It

```cpp
// File: processor_6stage/Pipeline6Stage.hpp (lines 120-155)
void stepCycle() {
    cycles++;
    
    // Execute stages in reverse order to model hardware latch updates
    stageWB();
    stageMA();
    stageEX();
    stageRF();
    stageID();
    stageIF();
    
    // Update performance counters
    perf.recordCycle(cycles);
}
```
**Why reverse order (`WB -> MA -> EX -> RF -> ID -> IF`)?**
In C++ software simulation, executing stages in reverse order ensures that an instruction in `EX` consumes the latched inputs from the *previous* cycle, rather than consuming data just produced in the same software loop iteration!

---

# Chapter D4: 4-Stage vs 6-Stage Trade-Offs (Why Deeper Can Be Faster)

### 1. In One Sentence
Although a 6-stage pipeline has a slightly higher CPI due to deeper branch penalties, it finishes programs **59% faster in wall-clock time** because its shorter critical path allows a much higher clock frequency (800 MHz vs 500 MHz).

### 2. Start from Zero: Everyday Analogy
Compare two runners:
- **Runner A (4-Stage)**: Takes longer strides (lower frequency, 500 MHz), but rarely trips (CPI = 1.125).
- **Runner B (6-Stage)**: Takes rapid, shorter strides (higher frequency, 800 MHz), but trips occasionally on turns (CPI = 1.131).
- Even though Runner B has to correct slightly more steps, their rapid cadence makes them reach the finish line **far ahead of Runner A**!

---

### 3. The Math: Wall-Clock Execution Time Formula

$$\text{Execution Time } (T) = \frac{\text{Instruction Count } (IC) \times \text{CPI}}{\text{Clock Frequency } (f)}$$

#### Real Simulation Numbers on `matrix_mult_long.s` (189,050 Instructions):

| Metric | 4-Stage Core | 6-Stage Core | Architectural Cause |
| :--- | :---: | :---: | :--- |
| **Pipeline Stages** | 4 | 6 | Split `OF` into `ID` and `RF` |
| **Max Frequency ($f$)**| **500 MHz** | **800 MHz** | Shorter critical path logic delay |
| **Clock Period ($t_{\text{clk}}$)**| $2.00\text{ ns}$ | $1.25\text{ ns}$ | $t_{\text{clk}} = 1 / f$ |
| **Branch Penalty** | **1 cycle** | **2 cycles** | Branch resolved at `EX` (2 stages behind `IF` in 6-stage) |
| **Retired Instructions**| 189,050 | 189,050 | Identical workload |
| **Total Clock Cycles**| 212,681 | 213,815 | $+1,134$ cycles due to 2-cycle branch flushes |
| **Steady-State CPI** | **1.125** | **1.131** | Slightly higher CPI on 6-stage |
| **Execution Time ($T$)**| **$425.4\ \mu\text{s}$** | **$267.3\ \mu\text{s}$** | $213,815 \times 1.25\text{ ns}$ vs $212,681 \times 2.00\text{ ns}$ |
| **Wall-Clock Speedup** | Baseline ($1.00\times$) | **$1.59\times$ (59% Faster!)** | **Higher clock frequency wins!** |

---

### 4. Likely Professor Questions
* **Q**: If 6-stage has a higher CPI than 4-stage, why do we use it?
  * **Answer**: Because performance is measured in **seconds**, not CPI. By halving the stage logic delay, clock frequency increases from 500 MHz to 800 MHz ($1.6\times$). The tiny $0.5\%$ increase in CPI is overwhelmingly compensated by the $60\%$ increase in clock rate, delivering a **$1.59\times$ wall-clock speedup**.

---

# Chapter D5: The Golden Reference ISS

### 1. In One Sentence
The Instruction Set Simulator (`common/ISS.hpp`) is an unpipelined, single-cycle reference model that executes one instruction completely before fetching the next, serving as the ground-truth "gold standard" for architectural verification.

### 2. Start from Zero: Everyday Analogy
When engineers build an experimental jet engine with complex turbofans and bypass valves (the pipelined CPU), they test its thrust against a calibrated solid-fuel test bench (the Golden ISS). If the experimental engine produces a different chemical output than the test bench, the engine has a bug!

### 3. How THIS Project Implements It
In `tests/test_all.cpp` (Section 5) and `tools/verify_golden.cpp`:
1. The test harness loads an assembly benchmark into memory.
2. It runs the program on the **Golden ISS** to completion, capturing the final register file and memory dump.
3. It resets and runs the exact same program on the **4-Stage Pipeline** and **6-Stage Pipeline**.
4. It compares all 16 registers and memory byte-by-byte:
   $$\text{Assert}(\text{RegFile}_{\text{P4}} == \text{RegFile}_{\text{ISS}})$$
   $$\text{Assert}(\text{RegFile}_{\text{P6}} == \text{RegFile}_{\text{ISS}})$$
5. **Result**: 231/231 automated verification checks pass with 100% agreement!

---

### 4. Summary: If You Only Remember 5 Things
1. 4-Stage is **`IF -> OF -> EX -> MA_RW`**; 6-Stage is **`IF -> ID -> RF -> EX -> MA -> WB`**.
2. 6-Stage splits `OF` into `ID` and `RF`, enabling **800 MHz vs 500 MHz**.
3. Branch penalty is **1 cycle in 4-stage** and **2 cycles in 6-stage**.
4. 6-Stage has a slightly higher CPI ($1.131$ vs $1.125$), but finishes **59% faster in wall-clock time**.
5. The **Golden ISS** verifies that pipelined optimizations never alter programmer-visible results.
