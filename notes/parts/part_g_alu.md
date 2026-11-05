# Part G: The Arithmetic Logic Unit (ALU) & Sub-Circuits

This section covers the ALU design in `alu/ALU.hpp`, every arithmetic and logical operation supported, the underlying hardware circuit algorithms (adders, multipliers, dividers, barrel shifters), and empirical latency comparisons.

---

# Chapter G1: ALU Architecture & Operations

### 1. In One Sentence
The Arithmetic Logic Unit (ALU) is the combinational core that executes all 25 computational, bitwise, and comparison operations, keyed strictly by canonical 5-bit string opcodes.

### 2. Start from Zero: Everyday Analogy
Think of the ALU as a **Swiss Army Knife**:
- One master handle connects to the user's hand.
- Inside the handle are specialized blades: a knife (adder), a saw (multiplier), scissors (divider), and a magnifying glass (comparator).
- Depending on which tool is folded out (the 5-bit opcode), the knife performs that specific job on the workpiece.

---

### 3. Complete List of ALU Operations in `alu/ALU.hpp`

| Category | Instruction | Opcode String | Implementation Circuit | Latency |
| :--- | :--- | :---: | :--- | :---: |
| **Addition** | `add` | `"00000"` | CLA / CSLA / RCA Adders | 1 cycle |
| **Subtraction**| `sub` | `"00001"` | 2's Complement Invert & Add | 1 cycle |
| **Multiplication**| `mul` | `"00010"` | Booth Radix-4 / Wallace Tree | 1 cycle |
| **Division** | `div` | `"00011"` | Non-Restoring / Newton-Raphson | 1 cycle |
| **Modulo** | `mod` | `"00100"` | Hardware Remainder Unit | 1 cycle |
| **Comparison** | `cmp` | `"00101"` | Subtraction magnitude & zero detector | 1 cycle |
| **Bitwise AND**| `and` | `"00110"` | Parallel 32-bit AND gates | 1 cycle |
| **Bitwise OR** | `or` | `"00111"` | Parallel 32-bit OR gates | 1 cycle |
| **Bitwise NOT**| `not` | `"01000"` | Parallel 32-bit Inverters | 1 cycle |
| **Register Move**| `mov` | `"01001"` | Bus Pass-Through | 1 cycle |
| **Shift Left** | `lsl` | `"01010"` | 5-stage Logarithmic Barrel Shifter | 1 cycle |
| **Shift Right (Logic)**| `lsr` | `"01011"` | Barrel Shifter (Zero Fill) | 1 cycle |
| **Shift Right (Arith)**| `asr` | `"01100"` | Barrel Shifter (Sign Bit Fill) | 1 cycle |
| **Minimum** | `min` | `"10101"` | Comparator + Multiplexer | 1 cycle |
| **Maximum** | `max` | `"10110"` | Comparator + Multiplexer | 1 cycle |
| **Rotate Shift**| `rots`| `"10111"` | Circular Barrel Rotator | 1 cycle |
| **Bitwise XOR**| `xor` | `"11000"` | Parallel 32-bit XOR gates | 1 cycle |
| **Absolute Value**| `abs` | `"11001"` | Conditional 2's complement negation | 1 cycle |

---

# Chapter G2: Hardware Arithmetic Algorithms

### 1. Adders (`alu/Adders.hpp`)

#### A. Ripple Carry Adder (RCA)
- **Concept**: 32 1-bit full adders chained sequentially.
- **Delay**: $O(N) = 32\text{ gate delays}$. Carry ripples slowly from bit 0 to bit 31.
- **Hardware Cost**: Minimal (lowest transistor count).

#### B. Carry Select Adder (CSLA)
- **Concept**: Splits the 32 bits into 4-bit blocks. Each block speculatively calculates two sums simultaneously: one assuming carry-in = 0, and one assuming carry-in = 1. When the actual carry arrives, a multiplexer selects the correct answer in $O(\sqrt{N})$ time.

#### C. Carry Lookahead Adder (CLA)
- **Concept**: Computes Generate ($G_i = A_i \cdot B_i$) and Propagate ($P_i = A_i \oplus B_i$) signals across a tree hierarchy:
  $$C_{i+1} = G_i + P_i \cdot C_i$$
- **Delay**: $O(\log N) = 5\text{ gate delays}$! Fastest addition circuit.

---

### 2. Multipliers (`alu/Multipliers.hpp`)

#### A. Booth's Radix-4 Multiplier
- **Concept**: Scans 3 overlapping multiplier bits ($y_{2i+1}, y_{2i}, y_{2i-1}$) in each cycle.
- **Recoding Table**:
  - `000` or `111`: Add $0$
  - `001` or `010`: Add $+1 \times M$
  - `011`: Add $+2 \times M$
  - `100`: Add $-2 \times M$
  - `101` or `110`: Add $-1 \times M$
- **Speedup**: Halves the number of partial products from 32 down to **16**!

#### B. Wallace Tree Multiplier
- **Concept**: Uses a tree of Carry-Save Adders (CSAs) where 3 inputs are reduced to 2 outputs (Sum and Carry) without propagating carry bits.
- **Delay**: Reduces 32 partial products to 2 in only $O(\log_{1.5} N) = 8$ levels, followed by a fast final Carry Lookahead Adder.

---

### 3. Dividers (`alu/Dividers.hpp`)

#### A. Restoring Divider
- Subtracts divisor from remainder. If result is negative, it **restores** the old value by adding the divisor back (32 cycles).

#### B. Non-Restoring Divider
- If remainder is positive, it shifts left and subtracts divisor in next cycle.
- If remainder is negative, it shifts left and **adds** divisor in next cycle.
- **Advantage**: Eliminates the restorative addition cycle!

#### C. Newton-Raphson Divider (Contributed by Varshith Teja)
- Instead of bit-by-bit division, computes reciprocal $X \approx 1/D$ using Newton's quadratic iterative approximation:
  $$X_{k+1} = X_k \cdot (2 - D \cdot X_k)$$
- Doubles precision on every iteration! After 4 iterations, performs single multiplication: $Q = N \times X$.

---

# Chapter G3: Empirical ALU Benchmark Comparison

Data captured by running `./risc201_eval`:

| Algorithm Variant | Measured Execution Time (100k ops) | Theoretical Complexity | Architectural Trade-Off |
| :--- | :---: | :---: | :--- |
| **Ripple Carry Adder (RCA)** | $12.4\text{ ms}$ | $O(N)$ | Simple silicon, slow carry propagation |
| **Carry Select Adder (CSLA)**| $8.1\text{ ms}$ | $O(\sqrt{N})$ | Moderate area, balanced latency |
| **Carry Lookahead (CLA)** | **$4.2\text{ ms}$** | **$O(\log N)$** | Highest performance, larger gate count |
| **Iterative Multiplier** | $38.2\text{ ms}$ | $O(N)$ | 32 shift-adds, highest cycle count |
| **Booth Radix-4 Multiplier**| **$19.5\text{ ms}$** | $O(N/2)$ | Cuts partial products in half |
| **Wallace Tree Multiplier** | **$9.8\text{ ms}$** | $O(\log N)$ | Parallel tree reduction |
| **Restoring Divider** | $45.1\text{ ms}$ | 32 cycles | Requires restore cycles on negative |
| **Non-Restoring Divider** | $31.0\text{ ms}$ | 32 cycles | Zero restoration cycles |
| **Newton-Raphson Divider** | **$14.2\text{ ms}$** | Quadratic | Fast quadratic convergence reciprocal |

---

### 4. Summary: If You Only Remember 5 Things
1. The ALU executes **25 operations** keyed by 5-bit string opcodes (`isa::OP_*`).
2. **Carry Lookahead Adder (CLA)** is the fastest adder ($O(\log N)$ tree).
3. **Booth's Radix-4** multiplier scans 2 bits per cycle, **halving partial products**.
4. **Non-Restoring division** eliminates the cycle spent restoring negative remainders.
5. **Newton-Raphson division** uses quadratic reciprocal approximation for maximum throughput.

