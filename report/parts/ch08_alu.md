# Chapter 8: Optimized Arithmetic Logic Unit (ALU)

## 8.1 Unified ALU Architecture (`alu/ALU.hpp`)

The RISC201 Arithmetic Logic Unit coordinates all computational, logical, shift, and address calculation operations. It provides a single canonical entry point:
```cpp
AluOutput execute(const std::string& opcode, Word a, Word b, const Flags& currentFlags = {});
```

### 8.1.1 Single Source of Truth Dispatch
The ALU matches `opcode` directly against the 5-bit binary strings in `common/ISA.hpp`:
```cpp
if (opcode == isa::OP_ADD || opcode == isa::OP_LD || opcode == isa::OP_ST) {
    // Adder execution
} else if (opcode == isa::OP_SUB) {
    // Subtractor execution
} else if (opcode == isa::OP_CMP) {
    // Comparison flag generation
} else if (opcode == isa::OP_MUL) {
    // Multiplier execution
} else if (opcode == isa::OP_DIV) {
    // Divider execution
}
```

### 8.1.2 Flag Generation Invariant
In strict adherence to Sarangi Section 3.3.4, **architectural flags ($E, GT$) are modified exclusively by `cmp` (`isa::OP_CMP`)**:
* For `cmp rs1, rs2/imm`:
  * Computes difference: $\text{diff} = A - B$.
  * Sets Equal flag: $\text{flags}.E = (\text{diff} == 0)$.
  * Sets Greater-Than flag: $\text{flags}.GT = (\text{int32\_t}(A) > \text{int32\_t}(B))$ (signed 2's complement comparison).
* For all other 24 ALU opcodes, `out.flags = currentFlags` (existing condition flags are preserved across arithmetic and logical instructions).

---

## 8.2 Gate-Level Primitives (`alu/LogicGates.hpp`)

To provide accurate hardware complexity and propagation delay analysis, the simulator models underlying gate-level logic:
* **Gates**: `and_gate`, `or_gate`, `not_gate`, `xor_gate`, `nand_gate`, `nor_gate`, `xnor_gate`, `mux2to1`.
* **Full Adder Modeling**:
  * Sum: $S = A \oplus B \oplus C_{in}$ (2 XOR gate delays $= 2\Delta$).
  * Carry Out: $C_{out} = (A \cdot B) + (C_{in} \cdot (A \oplus B))$ (2 gate delays $= 2\Delta$).
* **Propagation Delay Metric ($\Delta$)**: The propagation delay of a minimum-sized CMOS inverter or 2-input NAND gate ($\approx 10\text{ ps}$ in 7nm technology).

---

## 8.3 Adder Topologies & Comparative Analysis (`alu/Adders.hpp`)

The ALU allows dynamic runtime selection between four adder topologies (`AdderAlgorithm`):

```
+------------------------------------------------------------------------------------------------------+
| ADDER COMPLEXITY & PROPAGATION DELAY COMPARISON                                                      |
+--------------------------+---------------------+-------------------+---------------------------------+
| Topology                 | Area Complexity     | Gate Delay        | Delay for 32-bit Addition       |
+--------------------------+---------------------+-------------------+---------------------------------+
| Ripple-Carry (RCA)       | O(N) = 32 FAs       | O(N) = 2N * Delta | 64 Delta (Slowest, smallest)    |
| Carry-Select (CSLA)      | O(N * 1.5)          | O(sqrt(N))        | 16 Delta (Intermediate)         |
| Carry-Lookahead (CLA)    | O(N log N)          | O(log N)          | 8 Delta (Standard high-speed)   |
| Kogge-Stone Parallel-Pre | O(N log2 N) prefix  | O(log2 N)         | 5 Delta (Ultra-fast, high area) |
+--------------------------+---------------------+-------------------+---------------------------------+
```

### 8.3.1 Ripple-Carry Adder (RCA)
* Cascades 32 1-bit Full Adders in series.
* Carry ripples from bit 0 to bit 31: $T_{\text{RCA}} = 32 \times 2\Delta = 64\Delta$.
* Area: Minimal (32 Full Adders).

### 8.3.2 Carry-Lookahead Adder (CLA)
* Eliminates the serial ripple delay by computing carries in parallel across 4-bit blocks.
* Bit Generate ($G_i = A_i \cdot B_i$) and Bit Propagate ($P_i = A_i \oplus B_i$) generated in $1\Delta$.
* Group carry equations expanded up to 4 bits:
  $$C_4 = G_3 + P_3 G_2 + P_3 P_2 G_1 + P_3 P_2 P_1 G_0 + P_3 P_2 P_1 P_0 C_0$$
* A 2-level CLA tree performs 32-bit addition in only **$8\Delta$** (an **$8\times$ speedup** over RCA!).

### 8.3.3 Carry-Select Adder (CSLA)
* Partitions 32 bits into 4-bit and 8-bit groups.
* Computes two speculative sums for each block ($C_{in} = 0$ and $C_{in} = 1$). A 2-to-1 MUX selects the correct sum once the actual carry arrives. Delay: $16\Delta$.

---

## 8.4 Multiplier Topologies (`alu/Multipliers.hpp`)

1. **Iterative Shift-Add Multiplier**:
   * Evaluates multiplier bit by bit over 32 clock cycles.
   * Cycles: 32.
2. **Booth's Radix-4 Multiplier**:
   * Inspects overlapping 3-bit triplets of the multiplier: $(y_{2i+1}, y_{2i}, y_{2i-1})$.
   * Encodes operations: $+0, +M, -M, +2M, -2M$.
   * Cuts the number of partial product additions from 32 down to **16 cycles**!
3. **Wallace-Tree Multiplier**:
   * Uses Carry-Save Adders (CSA) to reduce 32 partial products to 2 words in $O(\log_{1.5} N)$ tree levels without carry propagation.
   * Final addition executed by a Carry-Lookahead Adder in 1 cycle.

---

## 8.5 Divider Topologies & Algorithms (`alu/Dividers.hpp`)

1. **Restoring Division Algorithm**:
   * Uses a 65-bit register $UV$ ($U$ 33-bit signed, $V$ 32-bit unsigned).
   * In each cycle: shift $UV$ left, subtract divisor $D$ from $U$. If $U < 0$, add $D$ back (restore) and set $q_i = 0$; otherwise set $q_i = 1$.
   * Latency: 32 cycles.
2. **Non-Restoring Division Algorithm**:
   * Eliminates the restoring addition. If $U < 0$, the subsequent cycle performs an addition $U \leftarrow U + D$ instead of subtraction.
   * Latency: 32 cycles with reduced switching activity.
3. **Newton-Raphson Iterative Division (Contributed by Varshith Teja)**:
   * Computes reciprocal $X \approx 1/D$ using Newton's root-finding iteration:
     $$X_{k+1} = X_k (2 - D_{\text{norm}} X_k)$$
   * Normalizes divisor to $[0.5, 1.0)$ and seeds $X_0 = \frac{48}{17} - \frac{32}{17} D_{\text{norm}}$.
   * Achieves full 32-bit precision in only **5 iterations** ($O(\log(\text{bits}))$ quadratic convergence!).
   * Followed by hardware integer truncation and exact remainder calculation.
4. **Division By Zero Protection**:
   * When $B = 0$, both `div` and `mod` raise a precise `DIVISION_BY_ZERO` CPU exception.

---

## 8.6 Logarithmic Barrel Shifter (`alu/Shifter.hpp`)
* Implements 32-bit shifts and rotations in a **single clock cycle**.
* 5 cascaded 2-to-1 MUX stages controlled by shift distance bits $[4:0]$:
  * Stage 0: Shift by 16 bits if distance bit 4 is set.
  * Stage 1: Shift by 8 bits if distance bit 3 is set.
  * Stage 2: Shift by 4 bits if distance bit 2 is set.
  * Stage 3: Shift by 2 bits if distance bit 1 is set.
  * Stage 4: Shift by 1 bit if distance bit 0 is set.
* Supports:
  * `LSL`: Logical Shift Left (zero fill).
  * `LSR`: Logical Shift Right (zero fill).
  * `ASR`: Arithmetic Shift Right (sign-bit replication).
  * `ROTS`: Circular end-around bit rotation.

---

## 8.7 Empirical Evaluation of ALU Algorithm Variants
Table 8.1 presents empirical measurements captured by `evaluation/CompareProcessors.cpp` (`report/data/compare_processors_results.txt`):

| Algorithm Category | Implemented Variant | Simulated Delay / Latency | Benchmark Execution Time (100k ops) |
| :--- | :--- | :---: | :---: |
| **Adders** | Ripple-Carry (RCA) | $64\Delta$ (Worst-case) | 12.4 ms |
| **Adders** | Carry-Select (CSLA) | $16\Delta$ (Intermediate) | 9.8 ms |
| **Adders** | **Carry-Lookahead (CLA)** | **$8\Delta$ (Fastest)** | **7.6 ms** |
| **Multipliers** | Shift-Add Iterative | 32 cycles | 38.2 ms |
| **Multipliers** | **Radix-4 Booth** | **16 cycles** | **19.5 ms** |
| **Multipliers** | Wallace Tree | $O(\log N)$ tree | 16.2 ms |
| **Dividers** | Restoring | 32 cycles | 45.1 ms |
| **Dividers** | Non-Restoring | 32 cycles | 41.3 ms |
| **Dividers** | **Newton-Raphson** | **5 iterations** | **14.8 ms** |

