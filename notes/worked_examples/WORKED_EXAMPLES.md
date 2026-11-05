# 10 Complete Hand-Traceable Whiteboard Examples

Practice these 10 examples step-by-step so you can walk up to a whiteboard during your presentation or viva and confidently solve any problem with zero hesitation.

---

## Example 1: Assembling a Branching Program by Hand

**Problem**: Assemble the following 4-line program into 32-bit hexadecimal machine words:
```assembly
0x00:   mov  r1, 0
0x04: loop:
0x04:   add  r1, r1, 1
0x08:   cmp  r1, 5
0x0C:   bgt  loop
```

**Step-by-Step Solution**:
1. **Line 1 (`0x00: mov r1, 0`)**:
   - Mnemonic `mov` $\rightarrow$ Opcode `isa::OP_MOV = "01001"` ($9_{10}$)
   - Immediate mode $\rightarrow$ $I = 1$
   - Destination $rd = 1$ (`0001`)
   - Immediate value $0$ $\rightarrow$ $0$
   - Binary pack: `01001 1 0001 0000 000000000000000000`
   - **Hex Result: `0x4C400000`**

2. **Line 2 (`0x04: add r1, r1, 1`)**:
   - Mnemonic `add` $\rightarrow$ Opcode `isa::OP_ADD = "00000"` ($0_{10}$)
   - Immediate mode $\rightarrow$ $I = 1$
   - Destination $rd = 1$ (`0001`), Source $rs1 = 1$ (`0001`), Immediate $= 1$
   - Binary pack: `00000 1 0001 0001 000000000000000001`
   - **Hex Result: `0x04440001`**

3. **Line 3 (`0x08: cmp r1, 5`)**:
   - Mnemonic `cmp` $\rightarrow$ Opcode `isa::OP_CMP = "00101"` ($5_{10}$)
   - Immediate mode $\rightarrow$ $I = 1$
   - Source $rs1 = 1$ (`0001`), Immediate $= 5$
   - Binary pack: `00101 1 0000 0001 000000000000000101`
   - **Hex Result: `0x2C040005`**

4. **Line 4 (`0x0C: bgt loop`)**:
   - Mnemonic `bgt` $\rightarrow$ Opcode `isa::OP_BGT = "10001"` ($17_{10} = 0x11$)
   - Current PC = `0x0C`, Target `loop` = `0x04`
   - Relative byte distance = `0x04 - 0x0C = -8 bytes`
   - Word offset = `-8 / 4 = -2`
   - In 27-bit two's complement: $-2$ is `0x07FFFFFE`
   - Binary pack: `10001 000011111111111111111111110`
   - **Hex Result: `0x8FFFFFFE`**

---

## Example 2: Disassembling a Machine Word by Hand

**Problem**: Disassemble the 32-bit machine word `0x00888000`.

**Step-by-Step Solution**:
1. Convert hex to binary:
   - `0x0` = `0000`
   - `0x0` = `0000`
   - `0x8` = `1000`
   - `0x8` = `1000`
   - `0x8` = `1000`
   - Full 32 bits: `0000 0000 1000 1000 1000 0000 0000 0000`
2. Extract fields:
   - Bits `[31:27]` = `00000` $\rightarrow$ `isa::OP_ADD` (`add`)
   - Bit `[26]` = `0` $\rightarrow$ Register format ($I = 0$)
   - Bits `[25:22]` = `0010` $\rightarrow$ Destination $rd = 2$ (`r2`)
   - Bits `[21:18]` = `0010` $\rightarrow$ Source $rs1 = 2$ (`r2`)
   - Bits `[17:14]` = `0010` $\rightarrow$ Source $rs2 = 2$ (`r2`)
3. **Reconstructed Assembly**: **`add r2, r2, r2`** (doubles the value of `r2`).

---

## Example 3: RAW Data Hazard With and Without Forwarding

**Program**:
```assembly
add r1, r2, r3   ; Inst 1: Writes to r1
sub r4, r1, r5   ; Inst 2: Reads r1 (RAW!)
```

### Case A: Without Forwarding (Requires Stalls)
```
Cycle:          1     2     3     4     5     6     7
Inst 1 (add):  [IF]  [OF]  [EX]  [MA]  [RW*]
                                        ^ r1 written at END of Cycle 5
Inst 2 (sub):        [IF]  [OF]  [STALL][STALL][OF*] [EX]
                                                ^ r1 read at START of Cycle 6
```
- **Stall Penalty**: **2 wasted cycles**! CPI drops to 2.0.

### Case B: With Forwarding (`EX -> EX` Bypass)
```
Cycle:          1     2     3     4     5
Inst 1 (add):  [IF]  [OF]  [EX]  [MA]  [RW]
                            |
                            +-----> [EX -> EX Bypass]
                                  |
Inst 2 (sub):        [IF]  [OF]  [EX]  [MA]  [RW]
```
- At the end of Cycle 3, `add` produces its result in `EX`.
- At the start of Cycle 4, the **Forwarding Unit** routes that result directly into the ALU input multiplexer for `sub`.
- **Stall Penalty: 0 cycles!** Smooth 1-cycle execution.

---

## Example 4: Load-Use Hazard Hand Trace

**Program**:
```assembly
ld  r1, [r2]      ; Inst 1: Loads r1 from memory
add r3, r1, r4    ; Inst 2: Uses r1 immediately!
```

```
Cycle:          1     2     3     4        5     6
Inst 1 (ld):   [IF]  [OF]  [EX]  [MA*]    [RW]
                                   | Data arrives at END of Cycle 4
                                   v
Inst 2 (add):        [IF]  [OF]  [BUBBLE] [EX*] [MA] [RW]
```
- In Cycle 3, `add` enters `OF`. The Hazard Unit checks:
  $$\text{IF\_OF.rs1} == \text{OF\_EX.rd} \quad (r1 == r1) \quad \land \quad \text{OF\_EX.is\_load} == \text{true}$$
- The Hazard Unit freezes `PC` and `IF_OF` latch, and inserts a `nop` **bubble** into `OF_EX`.
- In Cycle 4, `ld` completes memory read in `MA`.
- In Cycle 5, `ld` forwards data via `MEM -> EX` bypass into `add`'s ALU input.
- **Result: Exactly 1 unavoidable stall cycle.**

---

## Example 5: Cache Address Slicing Hand Calculation

**Problem**: A processor has a **32 KB, 8-way set-associative L1 Data Cache** with **64-byte cache lines**. The CPU accesses 32-bit physical address `0x0001_8F54`.  
Find the Byte Offset, Set Index, and Tag.

**Step-by-Step Solution**:
1. **Find Bit Widths**:
   - Block Size $B = 64\text{ bytes} \rightarrow 2^b = 64 \implies b = \mathbf{6\text{ offset bits}}$.
   - Number of cache lines = $\text{Total Size} / \text{Block Size} = 32,768 / 64 = 512\text{ lines}$.
   - Number of Sets $S = 512 / 8 = 64\text{ sets} \rightarrow 2^s = 64 \implies s = \mathbf{6\text{ index bits}}$.
   - Tag bits $t = 32 - (s + b) = 32 - (6 + 6) = \mathbf{20\text{ tag bits}}$.

2. **Decompose Address `0x0001_8F54`**:
   - In binary: `0000 0000 0000 0001 1000 1111 0101 0100`
   - Split:
     $$\underbrace{0000\,0000\,0000\,0001\,1000}_{\text{Tag (20 bits)}} \quad \underbrace{1111\,01}_{\text{Index (6 bits)}} \quad \underbrace{01\,0100}_{\text{Offset (6 bits)}}$$
3. **Convert to Hexadecimal**:
   - **Byte Offset**: `010100` binary = **`0x14` ($20_{10}$)** (Byte 20 inside line).
   - **Set Index**: `111101` binary = **`0x3D` ($61_{10}$)** (Look in Set 61).
   - **Tag**: `0x00018` = **`0x00018`** (Tag stored in directory).

---

## Example 6: AMAT Calculation Hand Problem

**Problem**: A memory hierarchy has:
- L1 Hit Time = 1 cycle, Miss Rate = $5\%$
- L2 Hit Time = 10 cycles, Miss Rate = $20\%$
- DRAM Latency = 100 cycles  
Calculate the Average Memory Access Time (AMAT).

**Solution**:
1. Compute L2 Effective Miss Penalty:
   $$\text{Penalty}_{L1} = \text{Hit}_{L2} + (\text{MissRate}_{L2} \times \text{Latency}_{\text{DRAM}})$$
   $$\text{Penalty}_{L1} = 10 + (0.20 \times 100) = 10 + 20 = 30\text{ cycles}$$
2. Compute AMAT:
   $$\text{AMAT} = \text{Hit}_{L1} + (\text{MissRate}_{L1} \times \text{Penalty}_{L1})$$
   $$\text{AMAT} = 1 + (0.05 \times 30) = 1 + 1.5 = \mathbf{2.5\text{ cycles}}$$

---

## Example 7: 48-Bit Radix-4 Virtual Address Translation

**Problem**: Given Virtual Address `0x0000_1234_5678_9ABC`, calculate the PML4, PDPT, PD, PT indices and Page Offset.

**Solution**:
1. Slicing bits:
   - **Offset**: `VA[11:0]` = `0xABC` ($2748_{10}$)
   - **PT Index**: `VA[20:12]` = `(0x123456789ABC >> 12) & 0x1FF` = **`0x089` ($137_{10}$)**
   - **PD Index**: `VA[29:21]` = `(0x123456789ABC >> 21) & 0x1FF` = **`0x1B3` ($435_{10}$)**
   - **PDPT Index**: `VA[38:30]` = `(0x123456789ABC >> 30) & 0x1FF` = **`0x0D1` ($209_{10}$)**
   - **PML4 Index**: `VA[47:39]` = `(0x123456789ABC >> 39) & 0x1FF` = **`0x024` ($36_{10}$)**
2. The MMU traverses: $\text{PML4}[36] \rightarrow \text{PDPT}[209] \rightarrow \text{PD}[435] \rightarrow \text{PT}[137] \rightarrow \text{PFN}$.

---

## Example 8: 2-Bit Bimodal Branch Predictor State Machine

**Problem**: A loop executes 4 times and exits. The predictor initializes at Weakly Taken (`10`). Trace the state transitions.

```
State Encoding:
  00: Strongly Not Taken
  01: Weakly Not Taken
  10: Weakly Taken
  11: Strongly Taken
```

| Iteration | Initial State | Prediction | Actual Outcome | Mispredict? | Next State |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | `10` (Weakly Taken) | Taken | Taken | No (Hit) | `11` (Strongly Taken) |
| **2** | `11` (Strongly Taken)| Taken | Taken | No (Hit) | `11` (Strongly Taken) |
| **3** | `11` (Strongly Taken)| Taken | Taken | No (Hit) | `11` (Strongly Taken) |
| **4 (Exit)**| `11` (Strongly Taken)| Taken | **Not Taken** | **Yes (Mispredict)**| `10` (Weakly Taken) |
- **Total Accuracy**: $3 / 4 = \mathbf{75\%}$.
- Notice: On loop exit, state only dropped to `10` (Weakly Taken), so when the loop restarts next time, it will **still predict Taken**!

---

## Example 9: 4-Stage vs 6-Stage Execution Time & Speedup

**Problem**: A program executes $IC = 1,000,000$ instructions:
- 4-Stage Core: $f = 500\text{ MHz}$, $\text{CPI} = 1.12$
- 6-Stage Core: $f = 800\text{ MHz}$, $\text{CPI} = 1.14$  
Calculate execution time for both cores and the speedup.

**Solution**:
1. 4-Stage Time:
   $$T_4 = \frac{1,000,000 \times 1.12}{500 \times 10^6} = \frac{1,120,000}{500,000,000} = \mathbf{2.24\text{ ms}}$$
2. 6-Stage Time:
   $$T_6 = \frac{1,000,000 \times 1.14}{800 \times 10^6} = \frac{1,140,000}{800,000,000} = \mathbf{1.425\text{ ms}}$$
3. Speedup:
   $$\text{Speedup } S = \frac{T_4}{T_6} = \frac{2.24}{1.425} = \mathbf{1.57\times \text{ (57% Faster!)}}$$

---

## Example 10: Full-Descending Stack Push/Pop Sequence

**Problem**: Given `sp = 0x000FFFFC` and registers $r1 = 42, r2 = 99$:
Trace `push r1`, `push r2`, `pop r3`.

**Step-by-Step Solution**:
1. `push r1`:
   - Decrement `sp`: `sp = 0x000FFFFC - 4 = 0x000FFFF8`
   - Store: `Memory[0x000FFFF8] = 42`
2. `push r2`:
   - Decrement `sp`: `sp = 0x000FFFF8 - 4 = 0x000FFFF4`
   - Store: `Memory[0x000FFFF4] = 99`
3. `pop r3`:
   - Read from current top: `r3 = Memory[0x000FFFF4] = 99`
   - Increment `sp`: `sp = 0x000FFFF4 + 4 = 0x000FFFF8`
- **Final State**: $r3 = 99$, `sp = 0x000FFFF8`. Valid data $42$ remains at `0x000FFFF8`.
