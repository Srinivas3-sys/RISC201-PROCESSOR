# RISC201 Textbook Tiny Programs (E1 - E10) Evaluation & Verification

This document catalogs the 10 purpose-written micro-programs (3–12 instructions each) designed to isolate and verify each fundamental concept from Smruti R. Sarangi's *Computer Organisation and Architecture* (Chapters 8 & 9).

Every single metric, cycle count, and stall attribution was obtained directly by running the actual C++ implementation and cross-checking with analytical hand calculations.

---

## E1: Cache Address Split & 3C Miss Categorization
- **Purpose**: Verify 32-bit address bitfield extraction and 3C miss classification (Compulsory, Capacity, Conflict) on a 32 KB, 8-way set-associative cache with 64-byte lines.
- **Formulas**:
  - Offset bits: $b = \log_2(64) = 6$ bits (bits [5:0])
  - Number of sets: $S = \frac{32768}{8 \times 64} = 64 \implies s = \log_2(64) = 6$ bits (bits [11:6])
  - Tag bits: $t = 32 - (s + b) = 32 - 12 = 20$ bits (bits [31:12])
- **Simulator Run**:
  1. Access `0x00001000` (Tag `0x1`, Set `0`, Offset `0`): First access to block -> **Compulsory Miss**.
  2. Access `0x00001004` (Tag `0x1`, Set `0`, Offset `4`): Same line already present in Set 0 -> **Hit** (1 cycle latency).
  3. Fill remaining 7 ways of Set 0 with distinct tags `0x2000` to `0x8000`: 7 Compulsory misses filling ways 1 through 7.
  4. Access `0x00009000` (Tag `0x9`, Set `0`, Offset `0`): 9th distinct block mapping to 8-way Set 0 -> **Conflict Miss** (LRU line evicted).

---

## E2: AMAT (Average Memory Access Time) Calculation
- **Purpose**: Demonstrate analytical and empirical memory hierarchy access time.
- **Formulas**:
  $$\text{AMAT} = t_{\mathrm{hit}} + P_{\mathrm{miss}} \times t_{\mathrm{miss}}$$
  With Write-Back / Write-Allocate dirty eviction probability $P_{\mathrm{dirty}}$:
  $$t_{\mathrm{miss}} = t_{\mathrm{read}} + P_{\mathrm{dirty}} \times t_{\mathrm{writeback}}$$
- **Parameters**: $t_{\mathrm{hit}} = 1\text{ cycle}$, $P_{\mathrm{miss}} = 5\% = 0.05$, DRAM Miss Penalty $t_{\mathrm{miss}} = 50\text{ ns}$.
- **Simulator Results**:
  - **4-Stage Core (500 MHz, $t_{\mathrm{clk}} = 2.0\text{ ns}$)**:
    $t_{\mathrm{miss}} = 50 / 2.0 = 25\text{ cycles}$.
    $$\text{AMAT} = 1 + 0.05 \times 25 = 2.25\text{ cycles} \quad (4.50\text{ ns})$$
  - **6-Stage Core (800 MHz, $t_{\mathrm{clk}} = 1.25\text{ ns}$)**:
    $t_{\mathrm{miss}} = 50 / 1.25 = 40\text{ cycles}$.
    $$\text{AMAT} = 1 + 0.05 \times 40 = 3.00\text{ cycles} \quad (3.75\text{ ns})$$

---

## E3: Cycle-Accurate DRAM Timing (Row Hit, Empty, Conflict)
- **Purpose**: Calculate and verify DRAM access latency across row buffer states for SimpleDRAM ($t_{\mathrm{CK}} = 1.0\text{ ns}$).
- **Parameters**: $t_{\mathrm{RCD}} = 15$, $t_{\mathrm{RP}} = 15$, $t_{\mathrm{CAS}} = 15$, $t_{\mathrm{BURST}} = 4$ cycles.
- **Analytical & Simulator Verification**:
  1. **Row Empty (Idle Bank)**:
     $$t_{\mathrm{empty}} = t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 15 + 4 = \mathbf{34\text{ ns}}$$
     - 4-Stage Core ($2.0\text{ ns}$): $\lceil 34 / 2.0 \rceil = \mathbf{17\text{ CPU cycles}}$.
     - 6-Stage Core ($1.25\text{ ns}$): $\lceil 34 / 1.25 \rceil = \mathbf{28\text{ CPU cycles}}$.
  2. **Row Hit (Row Buffer Open)**:
     $$t_{\mathrm{hit}} = t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 4 = \mathbf{19\text{ ns}}$$
     - 4-Stage Core ($2.0\text{ ns}$): $\lceil 19 / 2.0 \rceil = \mathbf{10\text{ CPU cycles}}$.
     - 6-Stage Core ($1.25\text{ ns}$): $\lceil 19 / 1.25 \rceil = \mathbf{16\text{ CPU cycles}}$.
  3. **Row Conflict (Different Row in Open Bank)**:
     $$t_{\mathrm{conflict}} = t_{\mathrm{RP}} + t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 15 + 15 + 4 = \mathbf{49\text{ ns}}$$
     - 4-Stage Core ($2.0\text{ ns}$): $\lceil 49 / 2.0 \rceil = \mathbf{25\text{ CPU cycles}}$.
     - 6-Stage Core ($1.25\text{ ns}$): $\lceil 49 / 1.25 \rceil = \mathbf{40\text{ CPU cycles}}$.

---

## E4: Virtual Memory 2-Level Translation & TLB Operation
- **Purpose**: Verify 32-bit virtual address decomposition into 2-level page directory/table and TLB caching.
- **Split**: 32-bit VA, 4 KB page size ($12\text{ offset bits}$):
  - Page Directory Index: bits [31:22] (10 bits, 1024 entries)
  - Page Table Index: bits [21:12] (10 bits, 1024 entries)
  - Page Offset: bits [11:0] (12 bits, 4096 bytes)
- **Access Trace**:
  - Virtual Address: `0x00401004`
    - PD Index: `(0x00401004 >> 22) & 0x3FF = 1`
    - PT Index: `(0x00401004 >> 12) & 0x3FF = 1`
    - Offset: `0x00401004 & 0xFFF = 0x004`
  - Access 1: DTLB Miss -> 2-level walk -> Page Fault (demand page allocated) -> Frame `0x1000` -> Translated PA `0x1004`. TLB updated.
  - Access 2 (same page): DTLB **Hit** in 1 clock cycle -> Translated PA `0x1004`.

---

## E5: RAW Data Hazard & Forwarding (ALU to ALU)
- **Assembly Program**:
  ```asm
  mov r1, 10
  add r2, r1, 5
  sub r3, r2, 2
  ```
- **Results**:
  - **4-Stage Pipeline**:
    - With `FULL` forwarding: 3 insts + 3 fill = **6 cycles**, 0 stalls ($\mathrm{CPI} = 2.000$).
    - With `NONE` forwarding: 3 insts + 3 fill + 2 stalls = **8 cycles**, 2 RAW stalls ($\mathrm{CPI} = 2.667$).
  - **6-Stage Pipeline**:
    - With `FULL` forwarding: 3 insts + 5 fill = **8 cycles**, 0 stalls ($\mathrm{CPI} = 2.667$).
    - With `NONE` forwarding: 3 insts + 5 fill + 4 stalls = **12 cycles**, 4 RAW stalls ($\mathrm{CPI} = 4.000$).

---

## E6: Load-Use Hazard & Instruction Scheduling
- **Assembly Program A (Adjacent Load-Use)**:
  ```asm
  load r1, 100[r0]
  add r2, r1, 5
  sub r3, r2, 1
  ```
  - Data available at end of `MA_RW`, needed at start of `EX`.
  - Incurred **1 load-use stall bubble**.
  - Total Cycles (4-Stage): $3 + 3 + 1 = \mathbf{7\text{ cycles}}$.
- **Assembly Program B (Scheduled with Independent Instruction)**:
  ```asm
  load r1, 100[r0]
  mov r4, 42
  add r2, r1, 5
  sub r3, r2, 1
  ```
  - Independent instruction `mov r4, 42` fills the gap.
  - Incurred **0 stall bubbles**.
  - Total Cycles (4-Stage): $4 + 3 + 0 = \mathbf{7\text{ cycles}}$ (4 instructions retired in same cycle count!).

---

## E7: Control Hazard Branch Penalty
- **Assembly Program**:
  ```asm
  mov r1, 1
  cmp r1, 1
  beq target
  mov r2, 99
  target:
  add r3, r1, 10
  ```
- **Results**:
  - 4 instructions retired (`mov`, `cmp`, `beq`, `add`).
  - **4-Stage Pipeline**: Branch resolved in `EX` (Stage 3). Squashes 1 younger instruction in `IF_OF`.
    $$\text{Cycles} = 4 + 3 + 1 = \mathbf{8\text{ cycles}}$$
  - **6-Stage Pipeline**: Branch resolved in `EX` (Stage 4). Squashes 2 younger instructions in `IF_ID` and `ID_RF`.
    $$\text{Cycles} = 4 + 5 + 2 = \mathbf{11\text{ cycles}}$$

---

## E8: Forwarding Priority Resolution (Youngest Producer Rule)
- **Assembly Program**:
  ```asm
  mov r1, 10
  mov r1, 20
  add r2, r1, 5
  ```
- **Verification**:
  - `add` needs `r1`.
  - First `mov` is older (in `MA_RW`), holding value 10.
  - Second `mov` is younger (in `EX_MA`), holding value 20.
  - Priority logic enforces `EX_MA` > `MA_RW`.
  - Simulator Output: $r2 = 20 + 5 = \mathbf{25}$ (`0x19`). Correctness verified!

---

## E9: 2-Bit Bimodal Branch Predictor State Machine
- **State Space**: Strongly Not Taken (00), Weakly Not Taken (01), Weakly Taken (10), Strongly Taken (11).
- **Loop Execution (3 iterations)**:
  - Initial State: Weakly Taken (10).
  - Iteration 1 (Taken): Predicted Taken (Hit). State becomes Strongly Taken (11).
  - Iteration 2 (Taken): Predicted Taken (Hit). State remains Strongly Taken (11).
  - Iteration 3 (Not Taken - Loop Exit): Predicted Taken (Mispredict!). Incurs flush bubble (1 bubble in 4-stage, 2 in 6-stage). State transitions to Weakly Taken (10).
- **Verification**: Exact state machine matches Sarangi Ch. 8 Fig 8.16.

---

## E10: 10-Instruction Complete CPI Stack Reconciliation
- **Assembly Program**:
  ```asm
  mov r0, 0
  mov r2, 20
  ld r3, 0[r0]
  add r4, r3, 5
  sub r5, r4, 2
  cmp r5, 45
  beq .target
  mov r6, 999
  .target:
  mov r7, 50
  add r8, r7, r0
  sub r9, r8, r2
  nop
  ```
- **Execution Accounting**:
  - Instructions Retired: 11 instructions.
  - Stalls: 1 Load-Use stall (`ld r3` followed by `add r4, r3, 5`).
  - Branch Flushes: 1 Taken Branch misprediction (skips `mov r6, 999`).
  - **4-Stage Core ($500\text{ MHz}$)**:
    - Pipeline fill: $4 - 1 = 3$ cycles.
    - Branch penalty: 1 bubble.
    - $\text{Total Cycles} = 3 + 11 + 1 + 1 = \mathbf{16\text{ cycles}}$.
    - $\mathrm{CPI} = 16 / 11 = \mathbf{1.455}$.
    - Cycle Accounting Invariant: PASS.
  - **6-Stage Core ($800\text{ MHz}$)**:
    - Pipeline fill: $6 - 1 = 5$ cycles.
    - Branch penalty: 2 bubbles.
    - $\text{Total Cycles} = 5 + 11 + 1 + 2 = \mathbf{19\text{ cycles}}$.
    - $\mathrm{CPI} = 19 / 11 = \mathbf{1.727}$.
    - Cycle Accounting Invariant: PASS.
