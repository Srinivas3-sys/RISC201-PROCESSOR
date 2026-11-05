# One-Page Architectural Cheat Sheets

Quick reference cards for exam revision, whiteboard problem solving, and presentation defense.

---

## Cheat Sheet 1: The Canonical ISA Card (All 25 Opcodes)

All opcodes are 5-bit binary strings in `common/ISA.hpp`:

```
+---------------+---------+--------+-------------------------------------------------+
| Binary String | Hex Int | Mnem.  | Architectural Action                            |
+---------------+---------+--------+-------------------------------------------------+
|     00000     |  0x00   | add    | rd = rs1 + (rs2 or imm)                         |
|     00001     |  0x01   | sub    | rd = rs1 - (rs2 or imm)                         |
|     00010     |  0x02   | mul    | rd = rs1 * (rs2 or imm)                         |
|     00011     |  0x03   | div    | rd = rs1 / (rs2 or imm)                         |
|     00100     |  0x04   | mod    | rd = rs1 % (rs2 or imm)                         |
|     00101     |  0x05   | cmp    | Updates E and GT flags based on rs1 - (rs2/imm) |
|     00110     |  0x06   | and    | rd = rs1 & (rs2 or imm)                         |
|     00111     |  0x07   | or     | rd = rs1 | (rs2 or imm)                         |
|     01000     |  0x08   | not    | rd = ~(rs1 or imm)                              |
|     01001     |  0x09   | mov    | rd = (rs2 or imm)                               |
|     01010     |  0x0A   | lsl    | rd = rs1 << (rs2 or imm) (Logical shift left)   |
|     01011     |  0x0B   | lsr    | rd = rs1 >> (rs2 or imm) (Logical shift right)  |
|     01100     |  0x0C   | asr    | rd = rs1 >> (rs2 or imm) (Arithmetic shift)     |
|     01101     |  0x0D   | nop    | No Operation (Pipeline bubble)                  |
|     01110     |  0x0E   | ld     | rd = Memory[rs1 + imm] (Load word from memory)  |
|     01111     |  0x0F   | st     | Memory[rs1 + imm] = rd (Store word to memory)   |
|     10000     |  0x10   | beq    | If E == 1, then PC = PC + (offset * 4)          |
|     10001     |  0x11   | bgt    | If GT == 1, then PC = PC + (offset * 4)         |
|     10010     |  0x12   | b      | PC = PC + (offset * 4) (Unconditional jump)     |
|     10011     |  0x13   | call   | ra = PC + 4; PC = PC + (offset * 4)             |
|     10100     |  0x14   | ret    | PC = ra (Return from subroutine)                |
|     10101     |  0x15   | min    | rd = min(rs1, rs2 or imm)                       |
|     10110     |  0x16   | max    | rd = max(rs1, rs2 or imm)                       |
|     10111     |  0x17   | rots   | rd = rotate_shift(rs1, rs2 or imm)              |
|     11000     |  0x18   | xor    | rd = rs1 ^ (rs2 or imm)                         |
|     11001     |  0x19   | abs    | rd = abs(rs1)                                   |
|     11010     |  0x1A   | movz   | Privileged system move                          |
|     11011     |  0x1B   | retz   | Privileged trap return (PC = oldPC, CPL = 1)    |
|     11111     |  0x1F   | -      | ILLEGAL_OPCODE (Triggers exception trap)        |
+---------------+---------+--------+-------------------------------------------------+
```

---

## Cheat Sheet 2: Pipeline Stages (4-Stage vs 6-Stage)

```
4-Stage Pipeline (500 MHz, t_clk = 2.00 ns):
[ IF ]  =====>  [ OF ]  =====>  [ EX ]  =====>  [ MA_RW ]
Fetch           Decode &        Execute in      Memory Access &
Inst            Read Regs       ALU             Writeback to Reg

6-Stage Pipeline (800 MHz, t_clk = 1.25 ns):
[ IF ]  =====>  [ ID ]  =====>  [ RF ]  =====>  [ EX ]  =====>  [ MA ]  =====>  [ WB ]
Fetch           Decode          Read            Execute in      Memory          Writeback
Inst            Opcode          RegFile         ALU             Access          to Reg
```

---

## Cheat Sheet 3: Forwarding Paths & Priority

```
When Instruction in EX stage needs an operand:
1. Check EX/MA latch (EX -> EX bypass):
   if (EX_MA.regWrite && EX_MA.rd != 0 && EX_MA.rd == EX.rs1)
       Forward from EX_MA.aluResult (Priority 1: Youngest!)
2. Check MA/RW latch (MEM -> EX bypass):
   else if (MA_RW.regWrite && MA_RW.rd != 0 && MA_RW.rd == EX.rs1)
       Forward from MA_RW.finalVal  (Priority 2)
3. Otherwise:
       Read operand normally from Register File (0 cycles delay)
```

---

## Cheat Sheet 4: Hazard Types & Solutions

| Hazard Category | Cause in Code | Hardware Fix / Penalty |
| :--- | :--- | :--- |
| **RAW (ALU to ALU)** | `add r1, r2, r3` followed by `sub r4, r1, r5` | Forwarding Unit (`EX->EX` bypass) $\rightarrow$ **0 stalls**! |
| **RAW (Load-Use)** | `ld r1, [r2]` followed by `add r3, r1, r4` | Hazard Unit injects **1 bubble stall** |
| **Control (Branch)** | `cmp` followed by `beq target` | Predictor guesses; if wrong: flush **1 cycle (4-stage)** or **2 cycles (6-stage)** |
| **Structural (Memory)** | `IF` and `MA` requesting memory at same cycle | Split L1 caches (Harvard L1I + L1D) $\rightarrow$ **0 stalls**! |

---

## Cheat Sheet 5: Horizontal vs Vertical Microcode

| Dimension | Horizontal Control Store | Vertical Control Store |
| :--- | :---: | :---: |
| **Control Word Width** | **32 bits** (Unencoded) | **16 bits** (Encoded) |
| **Hardware Overhead** | **0 decode logic** | Requires 4-to-16 hardware decoders |
| **ROM Memory Size** | 512 bytes ($128 \times 32$) | **256 bytes ($128 \times 16$)** (50% smaller) |
| **Speed** | Maximum (parallel wires) | $-0.4\text{ ns}$ decoder delay |

---

## Cheat Sheet 6: Cache Address Slicing Formulas

Given: Cache Size $C$, Block Size $B$, Associativity $W$:
1. **Number of Blocks**: $N_{\text{blocks}} = C / B$
2. **Number of Sets**: $S = N_{\text{blocks}} / W$
3. **Byte Offset Bits**: $b = \log_2(B)$
4. **Set Index Bits**: $s = \log_2(S)$
5. **Tag Bits**: $t = \text{Address Bits} - (s + b)$

*Example (32 KB, 8-way, 64-byte line, 32-bit address)*:
- $B = 64 \rightarrow b = 6\text{ bits}$
- $S = (32768 / 64) / 8 = 64\text{ sets} \rightarrow s = 6\text{ bits}$
- $t = 32 - (6 + 6) = \mathbf{20\text{ tag bits}}$

---

## Cheat Sheet 7: DRAM Access Timings (DDR4-3200)

| DRAM Case | Physical Sequence | Latency in Cycles |
| :--- | :--- | :---: |
| **Row Hit** | Data already in Row Buffer $\rightarrow$ Issue `READ` ($t_{CL}$) | **~15 cycles** |
| **Row Empty (Idle)** | Open Row $\rightarrow$ Issue `ACTIVATE` ($t_{RCD}$) + `READ` ($t_{CL}$) | **~40 cycles** |
| **Row Conflict** | Close old row ($t_{RP}$) + Open new row ($t_{RCD}$) + `READ` ($t_{CL}$) | **~80-120 cycles** |

---

## Cheat Sheet 8: Core Performance Formulas

1. **Cycles Per Instruction (CPI)**:
   $$\text{CPI} = \frac{\text{Total Cycles}}{\text{Retired Instructions}}$$
2. **Execution Time (Iron Law)**:
   $$\text{Time } T = \text{Instruction Count } (IC) \times \text{CPI} \times \frac{1}{\text{Frequency } f}$$
3. **Average Memory Access Time (AMAT)**:
   $$\text{AMAT} = \text{Hit Time}_{L1} + (\text{Miss Rate}_{L1} \times \text{Miss Penalty}_{L1})$$
4. **Speedup**:
   $$\text{Speedup } S = \frac{T_{\text{baseline}}}{T_{\text{optimized}}}$$

---

## Cheat Sheet 9: Master Command Reference

```bash
# Build everything from clean:
make clean && make all

# Run comprehensive test suite (78 tests):
make test

# Run tests under AddressSanitizer & UndefinedBehaviorSanitizer:
make test-asan

# Verify 231 golden regression checks:
make verify-golden

# Check ISA single source of truth across all 43 files:
make lint-isa

# Run the 3 presentation demos:
make demo-pipe    # 6-stage pipeline trace & forward arrows
make demo-mem     # 4-stage cache grid & DRAM bank status
make demo-vm      # 6-stage Radix-4 Virtual Memory & TLBs

# Launch interactive debugging console:
./risc201_sim benchmarks/factorial_iter.s -i
```
