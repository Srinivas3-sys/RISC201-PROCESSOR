# Module Notes: Hazards, Forwarding, and Branch Prediction

## 1. Hazard Classification & Detection (`common/HazardUnit.hpp`)

The pipeline handles three primary classes of architectural hazards:

1. **Data Hazards (RAW, WAR, WAW)**:
   * **RAW (Read-After-Write, True Data Dependency)**: Instruction $J$ reads register $r_{s1}$ or $r_{s2}$ written by an earlier in-flight instruction $I$.
   * **WAR (Write-After-Read, Anti-Dependency)**: Prevented by in-order stage sequencing (reads happen in OF/RF before writeback in MA_RW/RW).
   * **WAW (Write-After-Write, Output Dependency)**: Prevented by in-order writeback retirement.
   * **Load-Use Data Hazard**:
     * Instruction $I$ is `ld rd, imm[rs1]`.
     * Instruction $J$ (next cycle) consumes $r_d$ in ALU execution (`EX`).
     * In the 6-stage pipeline, `ld` data is not available until the end of `MA`. The `HazardUnit` detects this condition (`isLoad && dest == src`), inserts 1 cycle bubble into `EX`, and holds `IF`, `ID`, and `RF` stationary.
     * In the 4-stage pipeline with full forwarding, `ld` finishes memory access in `MA_RW` and forwards into the trailing edge of `EX` without stalling.

2. **Structural Hazards**:
   * **Memory Bus Collision**: Completely eliminated at L1 by the split Harvard architecture (separate `L1I` and `L1D`).
   * **Register File Simultaneous Access**: 2 read ports and 1 write port. In 4-stage, writeback occurs on the clock rising edge, and reads occur on the falling edge (internal forwarding / split-cycle register file).

3. **Control Hazards (Branch & Jump)**:
   * Occur when conditional branches (`beq`, `bgt`), unconditional jumps (`b`), calls (`call`), or returns (`ret`) alter program counter sequencing.
   * If the branch condition is evaluated in `EX`, instructions fetched behind it on the speculative path must be squashed if the branch was mispredicted.

---

## 2. Forwarding Bypass Network (`common/ForwardingUnit.hpp`)

### 2.1 Forwarding Modes
* **`NONE`**: All bypass multiplexers disabled. The pipeline stalls whenever a source register matches any active destination register in later stages until writeback completes.
* **`BASIC`**: Only `EX-to-EX` forwarding is enabled. Bypasses ALU results directly into the next cycle's ALU inputs.
* **`FULL`**: Exhaustive bypass network:
  * `EX-to-EX`: 1 cycle latency (ALU $\rightarrow$ ALU).
  * `MEM-to-EX`: 2 cycle latency (Memory/ALU $\rightarrow$ ALU).
  * `WB-to-EX`: 3 cycle latency (Writeback $\rightarrow$ ALU).
  * `MEM-to-MEM`: Store data forwarding.
* **`FULL_PLUS`**: Full forwarding + **Store-to-Load Buffer Forwarding**.

### 2.2 Youngest-Producer Arbitration Logic
When multiple in-flight instructions write to the same register $r_x$:
$$\text{Priority:} \quad \text{EX} > \text{MEM} > \text{WB} > \text{RegisterFile}$$
* The `ForwardingUnit` checks the newest producing stage first (`EX`). If it matches $r_x$, that value is selected.
* Older stages are ignored to prevent stale value clobbering.
* **Hardwired Zero Invariant**: Register $r_0$ is **never forwarded**. Any forward request targeting $r_0$ is forced to 0.

### 2.3 Store Buffer & Store-to-Load Forwarding
* 4-entry FIFO store buffer (`StoreBuffer`).
* Stores write address and data into the store buffer upon reaching `MA` and complete immediately.
* If a subsequent `ld` instruction targets an address matching an entry in the store buffer, data is bypassed directly in **1 cycle** without querying L1D cache or DRAM!

---

## 3. Branch Prediction Subsystem (`common/BranchPredictor.hpp`)

### 3.1 Implemented Predictor Policies
1. **`STALL`**: Pipeline stalls unconditionally in `IF` until the branch reaches `EX` and resolves.
2. **`FLUSH` (Predict Not-Taken)**: Always fetches $\text{PC} + 4$. If branch is taken, squashes speculative stages and redirects PC.
3. **`PREDICT_TAKEN`**: Always predicts branch taken using target address from BTB.
4. **`STATIC_BTFN`**: Backward Taken, Forward Not-Taken (ideal for loops).
5. **`BIMODAL_1BIT`**: 64-entry table of 1-bit flags (0=Not Taken, 1=Taken).
6. **`BIMODAL_2BIT` (Smith Predictor)**: 64-entry table of 2-bit saturating up/down counters:
   * `00`: Strongly Not-Taken
   * `01`: Weakly Not-Taken
   * `10`: Weakly Taken
   * `11`: Strongly Taken
   * Requires two consecutive mispredictions to change prediction direction (prevents loop exit thrashing).
7. **`GSHARE`**: 64-entry table indexed by $\text{PC}[7:2] \oplus \text{GHR}[5:0]$ (global history register). Captures inter-branch correlation.
8. **`PERFECT`**: Oracle predictor with 100% accuracy for theoretical bounds.

### 3.2 Branch Target Buffer (BTB) & Return Address Stack (RAS)
* **BTB (`BranchTargetBuffer`)**: 64-entry direct-mapped cache storing branch target addresses. Provides predicted target address during `IF` with zero cycle delay.
* **RAS (`ReturnAddressStack`)**: 16-entry hardware LIFO stack. `call` pushes $\text{PC} + 4$; `ret` pops predicted return address with $99.9\%$ accuracy.

