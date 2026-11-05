# Part E: Pipeline Hazards, Forwarding, & Branch Prediction

This section explains pipeline hazards from zero, how data forwarding eliminates RAW hazards, why load-use delays still need a bubble, how branch predictors work, and how precise exceptions are handled.

---

# Chapter E1: The Three Types of Pipeline Hazards

### 1. In One Sentence
A pipeline hazard is any condition that prevents the next instruction in the instruction stream from executing in its designated clock cycle.

### 2. Start from Zero: Everyday Analogy
Think of traffic hazards on a highway:
1. **Structural Hazard (Lane Closure)**: Two cars try to occupy the exact same physical bridge lane at the same moment.
2. **Data Hazard (Waiting for Cargo)**: Truck B cannot leave the warehouse until Truck A delivers the cargo that Truck B needs to transport.
3. **Control Hazard (Fork in the Road)**: The driver reaches a highway fork and doesn't know whether to turn Left or Right until the navigator finishes reading the map.

---

### 3. The Concept in Simple Steps

#### 1. Data Hazards
Data hazards occur when an instruction depends on the result of an earlier instruction that is still in flight in the pipeline.
- **RAW (Read-After-Write) — True Dependency**:
  ```assembly
  add r1, r2, r3   ; Writes to r1
  sub r4, r1, r5   ; Reads from r1 (RAW hazard!)
  ```
  If `sub` reads `r1` from the Register File before `add` writes back its result, `sub` will read an old, incorrect number!
- **WAR (Write-After-Read) — Anti-Dependency**: Occurs only in out-of-order CPUs. In our in-order pipeline, instructions read operands before younger instructions can write them.
- **WAW (Write-After-Write) — Output Dependency**: Occurs when instructions finish out of order. In our in-order pipeline, instructions commit strictly in program order.

#### 2. Control Hazards (Branch Hazards)
Occur when a conditional branch (`beq`, `bgt`) depends on a flag calculated in `EX`. The CPU cannot know whether to fetch instruction $PC+4$ or the branch target until the condition is evaluated.

#### 3. Structural Hazards (Hardware Resource Conflicts)
Occurs when hardware lacks enough resources to support all concurrent stage requests (e.g., a single-port memory attempting to serve both `IF` and `MA` simultaneously).

---

# Chapter E2: Data Forwarding (Bypassing)

### 1. In One Sentence
Data forwarding is a hardware optimization that routes the newly calculated result directly from an execution stage's output latch to an earlier stage's ALU input multiplexer, eliminating RAW stalls without waiting for the register writeback cycle.

### 2. Start from Zero: Everyday Analogy
Imagine a chef at Station 1 finishing a freshly chopped bowl of onions.
- **Without Forwarding**: The chef carries the bowl all the way back to the main walk-in pantry shelf (Register Writeback). Then, the cook at Station 2 walks all the way to the pantry to retrieve it. Total delay = 2 wasted trips!
- **With Forwarding**: The chef simply hands the bowl across the counter directly to Station 2's cutting board (Hardware Bypass). Delay = 0!

---

### 3. Forwarding Paths in RISC201 (`common/ForwardingUnit.hpp`)

```
+-----------------------------------------------------------------------------------------+
|                                    FORWARDING NETWORK                                   |
+-----------------------------------------------------------------------------------------+

           +------------+        +------------+        +------------+
           |     EX     | =====> |     MA     | =====> |     RW     |
           |   Stage    |        |   Stage    |        |   Stage    |
           +-----+------+        +-----+------+        +-----+------+
                 |                     |                     |
                 | ALU Result          | Memory/ALU Data     | Writeback Value
                 |                     |                     |
                 +--------+------------+----------+----------+
                          |                       |
                          | Path A: EX -> EX      | Path B: MEM -> EX
                          v                       v
               +--------------------------------------+
               |          ALU Input Muxes             |
               | (Routes fresh operand to EX inputs)  |
               +--------------------------------------+
```

1. **Path A: `EX -> EX` Forwarding**:
   - Instruction in `EX` consumes the result calculated by the instruction *immediately preceding* it (currently in `EX/MA` latch).
   - Latency = **0 cycles**!
2. **Path B: `MEM -> EX` Forwarding**:
   - Instruction in `EX` consumes the result produced two cycles ago (currently in `MA/RW` latch).
   - Latency = **0 cycles**!
3. **Priority Rule**:
   - If both `EX/MA` and `MA/RW` write to the same register, **always forward from `EX/MA`** because it holds the youngest, most up-to-date value!

---

### 4. Why Load-Use Hazards Still Need 1 Bubble
Consider a load followed immediately by its user:
```assembly
ld  r1, [r2]      ; Instruction 1: Load word from memory into r1
add r3, r1, r4    ; Instruction 2: Uses r1 immediately!
```

```
Cycle:          1      2      3      4      5
Inst 1 (ld):   [IF]   [OF]   [EX]   [MA]   [RW]
                                     |
                                     v Load data ready at END of Cycle 4
Inst 2 (add):         [IF]   [OF]   [EX]
                                     ^
                                     | ALU needs operand at START of Cycle 3!
```

- In Cycle 3, `add` enters `EX` and needs `r1`.
- But `ld` only reads data from memory in Cycle 4 (`MA` stage)!
- **Hardware cannot forward backwards in time!**
- **The Solution**: The Hazard Unit detects this condition, **stalls `IF` and `OF` for 1 cycle**, and injects **1 bubble (`nop`)** into `OF/EX`.
- In Cycle 4, `ld` finishes `MA`, and forwards load data directly to `EX` for Cycle 5!
- **Load-Use Penalty = Exactly 1 Cycle**.

---

# Chapter E3: Branch Prediction & Speculative Execution

### 1. In One Sentence
Branch prediction speculatively guesses whether a conditional branch will be taken or not taken before the condition flags are evaluated, keeping the pipeline full and squashing instructions only if the guess is wrong.

### 2. Start from Zero: Everyday Analogy
Think of a highway commuter navigating an intersection:
- Every morning, the driver takes Exit 4 to go to work.
- Rather than stopping completely at the intersection to check the map every single day, the driver assumes "I will take Exit 4" and drives through smoothly.
- 99 days out of 100, the guess is right (0 lost time).
- On the 1 day they need to visit the dentist instead, they realize the mistake, take the U-turn, and lose 2 minutes (Branch Flush Penalty).

---

### 3. The Predictors Implemented in RISC201 (`common/BranchPredictor.hpp`)

| Predictor Type | Algorithm / Mechanism | Hardware Cost | Typical Accuracy |
| :--- | :--- | :---: | :---: |
| **Static Not-Taken** | Always predicts branch will not jump ($PC = PC + 4$). | 0 bits | ~60% |
| **1-Bit Dynamic** | 1-bit memory per branch (`0` = Not Taken, `1` = Taken). Inverts state on every mispredict. | Low | ~85% |
| **2-Bit Bimodal** | 2-bit saturating counter (`00`, `01`, `10`, `11`). Requires two consecutive mispredicts to change direction. | Medium | **~93%** |
| **GShare** | XORs global branch history register (BHR) with PC bits to predict correlated branches. | High | **~96%** |
| **BTB (Branch Target Buffer)** | Caches calculated target address, eliminating target calculation delay. | Medium | 100% on Hit |
| **RAS (Return Address Stack)**| 8-entry LIFO stack predicting function returns (`ret`). | Low | **100% on Calls** |

---

### 4. What Happens on a Branch Mispredict? (Pipeline Flushing)

```
Cycle 1:  [IF] Branch fetched
Cycle 2:  [OF] Branch decoded; Predictor predicts "NOT TAKEN"; fetches Next Inst 1
Cycle 3:  [EX] Branch evaluates flags: Condition is TRUE! Branch is TAKEN!
          Target = 0x00000040. Predictor was WRONG!
```

When a mispredict is detected in `EX`:
1. **Squash Wrong-Path Instructions**:
   - In 4-stage: Injects 1 bubble into `IF/OF` (1 cycle penalty).
   - In 6-stage: Injects 2 bubbles into `IF/ID` and `ID/RF` (2 cycle penalty).
2. **Redirect Program Counter**:
   - Updates `PC = targetAddress`.
3. **Train Predictor**:
   - Updates the 2-bit counter: if taken, counter increments toward `11` (Strongly Taken).

---

### 5. Summary: If You Only Remember 5 Things
1. **RAW hazards** are solved in **0 cycles** by the **Forwarding Unit**.
2. **Load-Use hazards** cannot forward backwards in time and require **1 bubble stall**.
3. The **2-Bit Bimodal Predictor** uses saturating counters to avoid tripping on loop exits.
4. Branch penalty is **1 cycle in 4-stage** and **2 cycles in 6-stage**.
5. When a branch mispredicts, wrong-path instructions are **squashed** into `nop` bubbles.

