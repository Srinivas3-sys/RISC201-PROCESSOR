# RISC201 Complete 18-Program Verification Test Suite (E1 - E18)

Generated programmatically from actual C++ classes, pipeline models, and memory hierarchy.

---

========================================================================
E1: Foundations & Performance Equation: Ideal vs 4-Stage Pipeline
========================================================================
Program: 5 ALU/Register Instructions (IC = 5)
- Ideal Single-Cycle Machine: CPI = 1.000, Total Cycles = 5, Execution Time = 10.000 ns
- 4-Stage Pipelined Core: Stages = 4, Fill Latency = 3 cycles, Total Cycles = (4-1)+5 = 8
  Simulator Measured Cycles: 8 (Retired: 5, CPI: 1.6)
  Pipelined Execution Time at 500 MHz (t_clk = 2.0 ns): T = 5 * 1.600 * 2.0 ns = 16.000 ns
Verification: PASS (Performance equation T = IC * CPI / f confirmed)

========================================================================
E2: ISA Bit-by-Bit Instruction Encoding and Decoding Verification
========================================================================
1. Instruction: add r2, r1, 5
   - Encoded 32-bit Machine Word: 0x04840005
   - Decoded: Opcode="00000" (add), rd=2, rs1=1, imm16=5
   - Verification: PASS (Exact Bit-Level Round-Trip Confirmed)

2. Instruction: ld r3, 4[r0]
   - Encoded 32-bit Machine Word: 0x74c00004
   - Decoded: Opcode="01110" (ld), rd=3, rs1=0, imm16=4
   - Verification: PASS (Exact Bit-Level Round-Trip Confirmed)

========================================================================
E3: Two-Pass Assembler, Symbol Table, and Full-Descending Stack Macros
========================================================================
Input Assembly Source:
.main:
  mov r1, 10
  b .target
  mov r1, 20
.target:
  add r2, r1, 5

Full-Descending Macro Expansion:
- "push r2" expands to:
    sub sp, sp, 4
    st r2, 0[sp]
- "pop r3" expands to:
    ld r3, 0[sp]
    add sp, sp, 4

Pass 1 Symbol Table Resolution:
  .main00000 -> 0x0000
  .target000 -> 0xc000

Pass 2 Generated Machine Words (4 instructions):
  [00] PC=0x00 : 0x4c40000a
  [10] PC=0x40 : 0x90000002
  [20] PC=0x80 : 0x4c400014
  [30] PC=0xc0 : 0x48400050
Verification: PASS (Labels resolved and macros expanded cleanly)

========================================================================
E4: 4-Stage vs 6-Stage Execution Cycles and Fill Latencies
========================================================================
Program: 4 independent MOV instructions (IC = 4)
- 4-Stage Core: Cold Fill Latency = 4 - 1 = 3 cycles. Theoretical = 3 + 4 = 7 cycles.
  Simulator Measured Cycles: 7 (Retired: 4, CPI: 1.75)
- 6-Stage Core: Cold Fill Latency = 6 - 1 = 5 cycles. Theoretical = 5 + 4 = 9 cycles.
  Simulator Measured Cycles: 9 (Retired: 4, CPI: 2.25)
Verification: PASS (Cold fill latencies match formula N - 1 exactly)

========================================================================
E4: HazardUnit Interlocks: Load-Use Dependency and Compiler Scheduling
========================================================================
Case A (Adjacent Load-Use: ld r1; add r2, r1):
  Total Cycles: 8 | Stalls: 1
Case B (Compiler Scheduled: ld r1; mov r4, 42; add r2, r1):
  Total Cycles: 8 | Stalls: 0
Verification: PASS (1 stall bubble completely eliminated via scheduling)

========================================================================
E5: Active Forwarding Network: Modes, Priority, and Store-Data Bypass
========================================================================
3-Instruction RAW Chain (mov -> add -> sub):
- 4-Stage (NONE): 8 cycles (2 RAW stalls)
- 4-Stage (FULL): 6 cycles (0 RAW stalls) -> Saves 2 cycles
Youngest-Producer Priority Resolution (mov r1, 10; mov r1, 20; add r2, r1, 5):
  Final r2 = 25 (Expected 25 from younger mov r1, 20)
Verification: PASS (Youngest producer priority verified)

========================================================================
E6: Branch Misprediction Penalties (4-Stage vs 6-Stage Squashes)
========================================================================
Taken Branch with Static Not-Taken Policy:
- 4-Stage Core (resolved in EX / Stage 3): Flush Latency = 1 bubble.
  Measured Branch Flushes: 1 bubble. Total Cycles: 8
- 6-Stage Core (resolved in EX / Stage 4): Flush Latency = 2 bubbles.
  Measured Branch Flushes: 2 bubbles. Total Cycles: 11
Verification: PASS (1 bubble on 4S, 2 bubbles on 6S verified)

========================================================================
E7: Dynamic Branch Predictors (2-Bit Bimodal, GShare) and Return Address Stack
========================================================================
GShare Predictor Global History (GHR) Indexing Trace (PC=0x40):
  1. Initial GHR = 0x000 -> Index = 16 ^ 0 = 16 -> Actual Taken -> GHR = 0x001
  2. GHR = 0x001 -> Index = 16 ^ 1 = 17 -> Actual Taken -> GHR = 0x003
  3. GHR = 0x003 -> Index = 16 ^ 3 = 19 -> Actual Not-Taken -> GHR = 0x006

Return Address Stack (RAS) Call/Return Interception:
  Pushed: 0x1004, then 0x2004
  Popped: 0x2004 (matches Call 2), then 0x1004 (matches Call 1)
Verification: PASS (GShare GHR updates and RAS exact return addresses verified)

========================================================================
E8: Microcode Control Stores: Horizontal (65-bit) vs Vertical (45-bit)
========================================================================
Control Store Geometry:
- Horizontal Control Word: 65 bits (33 discrete unencoded signals + 12b imm + 10b branch + 10b args)
- Vertical Control Word:   45 bits (3b type + 5b src + 5b dest + 12b imm + 10b branch + 10b args)
- Micro-instruction ROM capacity: 140 words
Total ROM Bit Storage:
  Horizontal ROM: 9100 bits
  Vertical ROM:   6300 bits
  Bit Storage Reduction: 30.77%
Verification: PASS (30.77% control store reduction confirmed)

========================================================================
E9: ALU Arithmetic: Flags, Barrel Shifter, and Booth Radix-4 Multiplier
========================================================================
1. Carry-Lookahead Adder (0x7FFFFFFF + 1):
   Result: 0x80000000 | Carry: 0 | Overflow: YES
2. Logarithmic Barrel Shifter (0x80000000 >> 4 ASR):
   Result: 0xf8000000 (Sign bit preserved)
3. Booth Multiplier (7 * -3):
   Result: -21 (Expected -21)
Verification: PASS (Arithmetic flags, barrel shifts, and Booth multiplier verified)

========================================================================
E10: Hardware Division: Restoring, Non-Restoring, and Newton-Raphson
========================================================================
1. Restoring Division (20 / 3):
   Quotient: 6 | Remainder: 2
2. Non-Restoring Division (20 / 3):
   Quotient: 6 | Remainder: 2
3. Newton-Raphson Division (20 / 3):
   Quotient: 6 | Remainder: 2
Verification: PASS (All three divider algorithms produce identical exact integer results)

========================================================================
E11: Full-Descending Stack Safety, ASCII Visualizer, and Exceptions
========================================================================
Rendered Stack Visualizer Frame:

+--------------------------------------------------------------------------+
|                   ACTIVE STACK MEMORY FRAME RENDERER                     |
+--------------------------------------------------------------------------+
  Stack Base : 0x000FFFFC | Current SP : 0x000FFFF8 | Stack Limit : 0x00080000
  Stack Boundary Status: SECURE & HEALTHY
+--------------+--------------+------------------+-------------------------+
|   Address    |  Value (Hex) |  Value (Signed)  | Annotation / Frame Role |
+--------------+--------------+------------------+-------------------------+
|  0x000FFFFC  |  0x00000000  |               0  |  [Stack Origin / Base]  |
|  0x000FFFF8  |  0x0000002A  |              42  |  <-- [SP] (Top of Stack)|
|  0x000FFFF4  |  0x00000000  |               0  |  [Unallocated Space]    |
|  0x000FFFF0  |  0x00000000  |               0  |  [Unallocated Space]    |
+--------------+--------------+------------------+-------------------------+

Boundary Safety Check for SP below stackLimit (0x7fffc):
  Exception Raised: STACK_OVERFLOW
Verification: PASS (Stack visualizer and overflow exception verified)

========================================================================
E12: Cache Geometry, 3C Miss Classification, and Non-Blocking MSHRs
========================================================================
1. 32 KB 8-Way Cache Bitfield Split:
   - Offset: 6 bits [5:0]
   - Index:  6 bits [11:6] (64 sets)
   - Tag:   20 bits [31:12]
2. Cold access to 0x1000: Hit=NO, MissType=compulsory
3. Hit access to 0x1004: Hit=YES
4. Direct-Mapped A,B,A,B Accesses: Hit Rate = 0.00% (100% Conflict Thrashing)
Verification: PASS (3C misses and conflict thrashing verified)

========================================================================
E13: Cycle-Accurate DDR4-3200 DRAM Controller, Bank Groups, and FR-FCFS
========================================================================
DDR4-3200 JEDEC Parameters (tCK = 0.625 ns, 1600 MHz clock):
  tCL = 22, tRCD = 22, tRP = 22, tRAS = 52, tBURST = 4
Row Buffer Access Latencies:
- Row Hit:      26 DRAM cyc (16.25 ns)
- Row Empty:    48 DRAM cyc (30.00 ns)
- Row Conflict: 70 DRAM cyc (43.75 ns)
Bank Group Constraints:
- Different Bank Group (tCCD_S): 4 cycles (2.5 ns) -> 25.6 GB/s peak
- Same Bank Group (tCCD_L):      8 cycles (5.0 ns) -> 2.0x inter-burst delay
FR-FCFS Scheduling Speedup:
- FCFS [R1, R2, R1, R2]: 3 conflicts = 258 cycles
- FR-FCFS [R1, R1, R2, R2]: 2 row hits = 170 cycles
  Speedup: 258 / 170 = 1.518x (34.1% latency cut)
Verification: PASS (DDR4 timings and FR-FCFS speedup verified)

========================================================================
E14: 48-Bit 4-Level Virtual Memory Radix Walk and Page-Walk Cache
========================================================================
48-Bit Virtual Address Breakdown (0x00007FFFABCD1234):
- PML4: bits [47:39] = 255
- PDPT: bits [38:30] = 510
- PD:   bits [29:21] = 350
- PT:   bits [20:12] = 209
- Offset: bits [11:0] = 0x234
Cold Translation (PWC miss): Walk Latency = 48 cycles
Warm Translation (PWC hit):  Walk Latency = 1 cycles
Verification: PASS (4-level radix walk and translation verified)

========================================================================
E15: 10-Instruction Exact CPI Reconciliation & Invariant Check
========================================================================
4-Stage Cycle Breakdown (IC = 11):
- Cold Fill Latency: 4 - 1 = 3 cycles
- Base Retired:     11 cycles
- Load-Use Stalls:  1 cycle
- Branch Bubbles:   1 cycle
- Hand Calculated Sum: 3 + 11 + 1 + 1 = 16 cycles
- Simulator Measured: 16 cycles
Cycle Accounting Invariant: PASS (100% Cycles Accounted)

========================================================================
E16: 4-Stage vs 6-Stage Execution Time, Speedup, and Break-Even Frequency
========================================================================
Execution Time Derivation (per instruction):
- 4-Stage Core (500 MHz): T_4S = 1.075 * 2.00 ns = 2.15 ns
- 6-Stage Core (800 MHz): T_6S = 1.090 * 1.25 ns = 1.36 ns
Speedup: 2.15 / 1.36 = 1.578x (57.8% faster)
Break-Even Frequency Ratio: f_6 / f_4 > 1.014 ==> f_6 > 507.0 MHz
Since actual f_6 = 800 MHz (1.60x), the 6-stage pipeline decisively wins.
Verification: PASS (Analytic and empirical execution models cross-validated)

========================================================================
E18: Cold-Start vs Steady-State CPI Amortization
========================================================================
Short Loop Execution (5 iterations):
  Instructions Retired: 17
  Pipeline Fill: 3 cycles (17.6% of total execution cycles)
  Measured Total Cycles: 22, Measured CPI: 1.3
Steady-State Analysis (as N -> infinity):
  Cold Fill Overhead (3 / N) -> 0
  Loop branch predictor reaches 100% steady accuracy; Steady-state CPI -> 1.000
Verification: PASS (Cold-start unamortized fill overhead confirmed)

