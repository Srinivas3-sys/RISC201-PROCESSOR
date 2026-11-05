# Module Notes: Arithmetic Logic Unit (ALU)

## 1. Unified ALU Architecture (`alu/ALU.hpp`)
* **Interface**: Dispatches all operations through a single entry point:
  ```cpp
  AluOutput execute(const std::string& opcode, Word a, Word b, const Flags& currentFlags);
  ```
* **Single Source of Truth**: Keyed strictly by the 5-bit binary string opcodes defined in [`common/ISA.hpp`](file:///home/srinivas/RISC201-PROCESSOR/common/ISA.hpp).
* **Flag Invariant**: Flags ($E, GT$) are **only modified when `opcode == isa::OP_CMP`**. For all arithmetic and logic operations, existing flags are preserved, adhering strictly to Sarangi Section 3.3.4.

---

## 2. Gate-Level Primitives (`alu/LogicGates.hpp`)
* Direct gate-level modeling for academic delay and complexity evaluation:
  * Primitives: `and_gate`, `or_gate`, `not_gate`, `xor_gate`, `nand_gate`, `nor_gate`, `xnor_gate`, `mux2to1`.
  * `FullAdder`: Models sum $S = A \oplus B \oplus C_{in}$ and carry $C_{out} = (A \cdot B) + (C_{in} \cdot (A \oplus B))$ with 2 gate delays for carry and 2 gate delays for sum.

---

## 3. Adder Hardware Algorithms (`alu/Adders.hpp`)

1. **Ripple-Carry Adder (RCA)**:
   * 32 cascaded Full Adders.
   * Worst-case propagation delay: $T = 32 \times 2\Delta = 64\Delta$ ($O(N)$).
   * Hardware cost: 32 Full Adders (minimal silicon area).

2. **Carry-Select Adder (CSLA)**:
   * Splits 32 bits into 4-bit and 8-bit blocks.
   * For each block, computes two candidate sums speculatively: one assuming $C_{in} = 0$ and one assuming $C_{in} = 1$.
   * A multiplexer selects the valid sum once the block carry-in arrives.
   * Delay: $O(\sqrt{N}) \approx 16\Delta$.

3. **Carry-Lookahead Adder (CLA)**:
   * Generates bit-level Generate $G_i = A_i \cdot B_i$ and Propagate $P_i = A_i \oplus B_i$ in parallel.
   * 4-bit group carry generators expand carries without serial ripple:
     $$C_1 = G_0 + P_0 C_0$$
     $$C_2 = G_1 + P_1 G_0 + P_1 P_0 C_0$$
     $$C_3 = G_2 + P_2 G_1 + P_2 P_1 G_0 + P_2 P_1 P_0 C_0$$
     $$C_4 = G_3 + P_3 G_2 + P_3 P_2 G_1 + P_3 P_2 P_1 G_0 + P_3 P_2 P_1 P_0 C_0$$
   * Delay: $O(\log N) \approx 8\Delta$.

4. **Kogge-Stone Parallel-Prefix Adder**:
   * Radix-2 tree generating group generate/propagate pairs with minimal fan-out ($O(\log_2 N) = 5$ prefix stages).

---

## 4. Multiplier Hardware Algorithms (`alu/Multipliers.hpp`)

1. **Iterative Shift-Add Multiplier**:
   * Evaluates multiplier bit by bit over 32 cycles.
   * Worst-case latency: 32 cycles.

2. **Booth's Radix-4 Multiplier**:
   * Groups multiplier bits into overlapping 3-bit triplets: $(y_{2i+1}, y_{2i}, y_{2i-1})$.
   * Operations per cycle: $+0, +M, -M, +2M, -2M$.
   * Reduces the number of partial products from 32 down to 16 cycles!

3. **Wallace-Tree Multiplier**:
   * Carry-save adder (CSA) tree that reduces partial products from $N \rightarrow 2$ in $O(\log_{1.5} N)$ tree levels.
   * Final addition performed by a fast Carry-Lookahead Adder.

---

## 5. Divider Hardware Algorithms (`alu/Dividers.hpp`)

1. **Restoring Division**:
   * 65-bit register $UV$ shifted left each cycle.
   * Trial subtraction $U \leftarrow U - D$.
   * If $U < 0$, restore $U \leftarrow U + D$ and set quotient bit $q_i = 0$. Else set $q_i = 1$.
   * Latency: 32 cycles.

2. **Non-Restoring Division**:
   * Eliminates restoring addition. If $U < 0$, next cycle performs addition $U \leftarrow U + D$ instead of subtraction.
   * Latency: 32 cycles with lower gate activity.

3. **Newton-Raphson Division (Contributed by Varshith Teja)**:
   * Computes reciprocal $X \approx 1/D$ iteratively using Newton's quadratic formula:
     $$X_{k+1} = X_k (2 - D_{\text{norm}} X_k)$$
   * Initial linear seed: $X_0 = \frac{48}{17} - \frac{32}{17} D_{\text{norm}}$.
   * Full 32-bit precision achieved in **5 iterations** ($O(\log(\text{bits}))$ quadratic convergence!).
   * Followed by hardware post-processing for exact integer truncation and remainder calculation.

---

## 6. Logarithmic Barrel Shifter (`alu/Shifter.hpp`)
* 5 cascaded 2-to-1 MUX stages controlling shift distances of 16, 8, 4, 2, and 1 bit.
* Operates in a single cycle for all 32-bit shift distances.
* Implements `LSL` (zero fill right), `LSR` (zero fill left), `ASR` (sign bit replication left), and `ROTS` (circular end-around rotation).

