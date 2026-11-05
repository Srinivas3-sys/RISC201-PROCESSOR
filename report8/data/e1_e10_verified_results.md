# RISC201 Verified Micro-Program Test Suite (E1 - E10)

All results generated programmatically by running the actual C++ classes and simulator.

---

### E1: Cache Address Split & 3C Miss Categorization

- Configuration: Size = 32 KB, Line = 64 B, Ways = 8, Sets = 64
- Split: Offset = 6 bits [5:0], Index = 6 bits [11:6], Tag = 20 bits [31:12]

1. Access `0x00001000`: Set=0, Tag=0x1 -> Hit=NO, MissType=compulsory
2. Access `0x00001004`: Set=0, Tag=0x1 -> Hit=YES
   Fill way (Tag 2) `0x2000`: Hit=NO, MissType=compulsory
   Fill way (Tag 3) `0x3000`: Hit=NO, MissType=compulsory
   Fill way (Tag 4) `0x4000`: Hit=NO, MissType=compulsory
   Fill way (Tag 5) `0x5000`: Hit=NO, MissType=compulsory
   Fill way (Tag 6) `0x6000`: Hit=NO, MissType=compulsory
   Fill way (Tag 7) `0x7000`: Hit=NO, MissType=compulsory
   Fill way (Tag 8) `0x8000`: Hit=NO, MissType=compulsory
3. Access `0x9000` (9th distinct line in 8-way Set 0): Hit=NO, MissType=compulsory (Conflict Miss verified!)

### E2: AMAT Calculation

- L1 Hit Latency ($t_{\mathrm{hit}}$): 1 cycle (2.0 ns @ 500 MHz, 1.25 ns @ 800 MHz)
- L1 Miss Rate ($P_{\mathrm{miss}}$): 5.0%
- DRAM Miss Penalty ($t_{\mathrm{miss}}$): 25 cycles (50 ns @ 500 MHz) / 40 cycles (50 ns @ 800 MHz)
- AMAT Formula: $\text{AMAT} = t_{\mathrm{hit}} + P_{\mathrm{miss}} \times t_{\mathrm{miss}}$
- Result (4-Stage 500 MHz): $\text{AMAT} = 1 + 0.05 \times 25 = 2.25\text{ cycles}$ (4.5 ns)
- Result (6-Stage 800 MHz): $\text{AMAT} = 1 + 0.05 \times 40 = 3\text{ cycles}$ (3.75 ns)

### E3: DRAM Timing Calculations

- DRAM Timing parameters: $t_{\mathrm{CK}}=1.0\text{ ns}, t_{\mathrm{RCD}}=15\text{ cyc}, t_{\mathrm{RP}}=15\text{ cyc}, t_{\mathrm{CAS}}=15\text{ cyc}, t_{\mathrm{BURST}}=4\text{ cyc}$
1. Row Empty (first access to Bank 0, Row 0): Latency = 34 ns (Formula: $t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 15 + 4 = 34\text{ ns}$)
2. Row Hit (subsequent access to open Row 0 in Bank 0): Latency = 19 ns (Formula: $t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 4 = 19\text{ ns}$)
3. Row Conflict (different row targeting already open Bank 0): Latency = 49 ns (Formula: $t_{\mathrm{RP}} + t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = 15 + 15 + 15 + 4 = 49\text{ ns}$)

- 4-Stage Core (500 MHz, $t_{\mathrm{clk}}=2.0\text{ ns}$): Row Hit = 10 cycles (19 ns -> ceil(19/2)=10 cyc), Row Empty = 17 cycles (34 ns -> 17 cyc), Row Conflict = 25 cycles (49 ns -> ceil(49/2)=25 cyc)
- 6-Stage Core (800 MHz, $t_{\mathrm{clk}}=1.25\text{ ns}$): Row Hit = 16 cycles (19 ns -> ceil(19/1.25)=16 cyc), Row Empty = 28 cycles (34 ns -> ceil(34/1.25)=28 cyc), Row Conflict = 40 cycles (49 ns -> ceil(49/1.25)=40 cyc)

### E4: Virtual Memory 2-Level Translation

- VA: `0x00401004` -> Binary decomposition:
  - Page Directory Index (bits 31:22): `1` (`0x1`)
  - Page Table Index (bits 21:12): `1` (`0x1`)
  - Offset (bits 11:0): `4` (`0x4`)
1. First Access (DTLB Miss + Unmapped Page):
   - DTLB Hit: NO
   - Page Fault Occurred: YES (Type: minor)
   - Translated PA: `0x1004`
2. Second Access to same page:
   - DTLB Hit: YES (Latency: 1 cycle)
   - Translated PA: `0x1004`

### E10: 10-Instruction Micro-Program CPI Stack Reconciliation

#### 4-Stage Pipeline Run:
- Instructions Retired: 11
- Total Cycles: 16
- Fill Latency: 3 cycles
- Load-Use Stalls: 1
- Branch Bubbles: 1
- Invariant Check: Total Cycles = (4-1) + 11 + 1 + 1 = 16 (PASS)
- CPI: 1.455

#### 6-Stage Pipeline Run:
- Instructions Retired: 11
- Total Cycles: 19
- Fill Latency: 5 cycles
- Load-Use Stalls: 1
- Branch Bubbles: 2
- Invariant Check: Total Cycles = (6-1) + 11 + 1 + 2 = 19 (PASS)
- CPI: 1.727

