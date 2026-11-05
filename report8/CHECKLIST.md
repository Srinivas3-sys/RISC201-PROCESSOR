# RISC201 8-Page Technical Report Verification Checklist

This checklist documents the complete verification and quality audit performed for the 8-page examination report for **CSC-201 Computer Organization & Architecture** (Autumn 2026), titled *"RISC201: Memory System, Pipeline Hazards, Data Forwarding, and CPI Analysis"*.

---

## 1. Document & Page Count Compliance
- [x] **Target Page Count**: Exactly **8 pages** (A4).
- [x] **Verification Command**: `pdfinfo report8/RISC201_Memory_Hazards_CPI_Report.pdf | grep "Pages:"` -> **`Pages: 8`**.
- [x] **Page Budget Distribution**:
  - **Page 1**: Title block, contributor attribution, abstract (~100 words), keywords, key architectural metrics at a glance, 1. Introduction & textbook scope, course requirements mapping table.
  - **Page 2**: 2. Processor architecture & pipeline organization, 2.1 Canonical SimpleRisc ISA & 5-bit string opcodes (`"00000"` to `"11011"`), Figure 1 (system block diagram), 2.2 4-stage vs 6-stage core microarchitectures, Figure 2 (pipeline stage layouts), 2.3 Cycle accounting invariant & ideal CPI ($1.000$).
  - **Page 3**: 3. Pipeline hazards & resolution taxonomy, 3.1 Classification (structural, RAW true data dependency, WAR/WAW non-occurrence proof in in-order pipelines), 3.2 HazardUnit detection logic, Figure 4 (2-bit bimodal saturating counter state machine), 3.3 Control hazards & branch penalties (E7 static, E9 bimodal with hysteresis).
  - **Page 4**: 4. Data forwarding network & priority logic, 4.1 Forwarding bypass architecture, Figure 3 (forwarding bypass paths & producer-consumer matrix), 4.2 Youngest-producer priority rule (`EX_MA` > `MA_RW` > `RW`) verified via E8, 4.3 Minimum stall cycle matrix (None vs Basic vs Full forwarding), E5 RAW chain.
  - **Page 5**: 5. Memory system organization & cache hierarchy (Part 1), 5.1 Temporal & spatial locality principles, Figure 5 (32 KB 8-way cache bitfield decomposition & 2-level VM split), 5.2 L1 Cache architecture ($t=20, s=6, b=6$), write-back/allocate, LRU, 3C miss categorization (E1), 5.3 AMAT recursive formula & numerical verification (E2).
  - **Page 6**: 5. Memory system (Part 2: DRAM & Virtual Memory), 5.4 SimpleDRAM physical architecture (1T-1C cells, destructive read & refresh), Open-Page timing derivations (Row Hit 19 ns, Row Empty 34 ns, Row Conflict 49 ns), clock domain crossing to CPU cycles (500 MHz vs 800 MHz) verified via E3, 5.5 Virtual memory architecture, 32-bit VA, 2-level page table ($10+10+12$ bits), 64-entry 4-way TLB, step-by-step translation trace (E4).
  - **Page 7**: 6. CPI calculations, performance analysis & speedup, 6.1 Analytical CPI stack formulation ($T_{\mathrm{exec}} = IC \times \mathrm{CPI} \times t_{\mathrm{clk}}$), 6.2 Analytical CPI stack across representative instruction mix table, 6.3 Micro-program E10 cycle-by-cycle reconciliation (11 instructions retired, 4-stage 16 cycles $\mathrm{CPI}=1.455$, 6-stage 19 cycles $\mathrm{CPI}=1.727$, Invariant PASS), 6.4 Processor speedup ($1.347\times$ wall-clock speedup) & Amdahl's law analysis.
  - **Page 8**: 7. Verification & quality assurance (differential Golden ISS equivalence, 78/78 tests pass, ASan/UBSan clean), 8. Limitations & one-sentence scope note, 9. Conclusion, Master Verification Reconciliation Table (E1–E10), References, Reproduction box.

---

## 2. Scope & Syllabus Boundary Enforcement
- [x] **Curriculum Scope**: Strictly Smruti R. Sarangi's *Computer Organisation and Architecture* (Chapters 8 & 9).
- [x] **Negative Scope Statement**: Present on Page 8 (Section 8) as exactly ONE single sentence:
  > *"Textbook Scope Limitation: Advanced architectural mechanisms beyond the Sarangi textbook syllabus—specifically 4-level 48-bit page tables, non-blocking caches, hardware prefetchers, out-of-order DRAM scheduling, and branch predictors beyond 2-bit bimodal—are strictly out of scope for this baseline evaluation."*
- [x] **Zero Out-of-Scope Concepts in Technical Analysis**: No mention of PML4, PDPT, radix-4, page-walk caches, bank groups, dual subchannels, FR-FCFS, MSHRs, prefetchers, GShare, RAS, BTB, or Newton-Raphson division in any technical description or calculations.
- [x] **No External Benchmark Suites Used**: All performance figures and tables are derived strictly from analytical formulas, physical hardware parameters, and 10 purpose-written micro-programs (E1–E10).

---

## 3. Micro-Program Verification (E1 - E10) Reconciled Matrix

| ID | Concept Tested | Hand-Calculated Analytical Value | Simulator Output (`./report8/data/run_e1_e10`) | Verification Status |
| :---: | :--- | :--- | :--- | :---: |
| **E1** | Cache Split & 3C Misses | $t=20, s=6, b=6$; Set 0 ways 0..7 | Compulsory miss on cold, Hit on same line, Conflict miss on 9th distinct block | **PASS (100% Match)** |
| **E2** | AMAT Calculation | $\text{AMAT} = 1 + 0.05 \times t_{\mathrm{miss}}$ | 4-Stage: $2.25\text{ cycles}$ ($4.50\text{ ns}$); 6-Stage: $3.00\text{ cycles}$ ($3.75\text{ ns}$) | **PASS (100% Match)** |
| **E3** | DRAM Timings | Hit $19\text{ ns}$, Empty $34\text{ ns}$, Conflict $49\text{ ns}$ | 4-Stage ($2.0\text{ ns}$): $10, 17, 25\text{ cyc}$; 6-Stage ($1.25\text{ ns}$): $16, 28, 40\text{ cyc}$ | **PASS (100% Match)** |
| **E4** | 2-Level VM & TLB | Split: $\text{PD}=1, \text{PT}=1, \text{Offset}=4$ | First access: DTLB miss -> minor fault -> PA `0x1004`; Second: DTLB hit ($1\text{ cyc}$) | **PASS (100% Match)** |
| **E5** | RAW Chain Forwarding | 3 ALU insts: None vs Full | 4-Stage: $6\text{ cyc}$ ($0\text{ stall}$) vs $8\text{ cyc}$ ($2\text{ stall}$); 6-Stage: $8\text{ cyc}$ ($0\text{ stall}$) vs $12\text{ cyc}$ ($4\text{ stall}$) | **PASS (100% Match)** |
| **E6** | Load-Use Scheduling | Adjacent ($1\text{ bubble}$) vs Separated ($0\text{ stall}$) | Case A: $7\text{ cyc}$ ($1\text{ load-use stall}$); Case B: $7\text{ cyc}$ ($0\text{ stalls}$, 4 insts retired) | **PASS (100% Match)** |
| **E7** | Branch Penalty | 4S squashes 1 inst; 6S squashes 2 insts | 4-Stage: $8\text{ cyc}$ ($1\text{ bubble}$); 6-Stage: $11\text{ cyc}$ ($2\text{ bubbles}$) | **PASS (100% Match)** |
| **E8** | Forwarding Priority | $\text{EX\_MA} > \text{MA\_RW}$ (Youngest wins) | Producer writes 10 then 20; consumer receives $20 \implies r2 = 25$ (`0x19`) | **PASS (100% Match)** |
| **E9** | 2-Bit Bimodal Predictor| Transitions: $10 \to 11 \to 11 \to 10$ | 3-iteration loop: exactly 2 mispredictions across loop entry and exit | **PASS (100% Match)** |
| **E10**| Full CPI Reconciliation | Total Cycles $= (S-1) + IC + \sum \text{Stalls}$ | 4-Stage: $16\text{ cyc}$ ($\text{CPI} = 1.455$); 6-Stage: $19\text{ cyc}$ ($\text{CPI} = 1.727$); Invariant PASS | **PASS (100% Match)** |

---

## 4. Code Citations & Implementation Integrity
- [x] **Canonical 5-Bit String Opcodes**: Verified in `common/ISA.hpp:20-65` (`isa::OP_ADD = "00000"`, `isa::OP_SUB = "00001"`, `isa::OP_INVALID = "11111"`).
- [x] **Hazard Unit Interlocks**: Verified in `common/HazardUnit.hpp:18-120`.
- [x] **Forwarding Unit Multiplexers & Priority**: Verified in `common/ForwardingUnit.hpp:40-165`.
- [x] **Cache Hierarchy**: Verified in `memory/Cache.hpp:45-125` and `memory/Config.hpp:128-140`.
- [x] **SimpleDRAM Controller**: Verified in `memory/DRAM.hpp:20-180` and `memory/Config.hpp:195-205`.
- [x] **Virtual Memory Subsystem**: Verified in `memory/VirtualMemory.hpp:15-165` and `memory/Config.hpp:68-85`.
- [x] **PerformanceTracker Invariant**: Verified in `common/PerformanceTracker.hpp:92-105`.
- [x] **Simulator Source Code Preserved**: Simulator source code completely untouched; documentation and verification tooling only.

---

## 5. Contributor Attribution
- [x] **Derived directly from Git commit history**:
  - `git shortlog -sn`:
    - `36  bandlapalliyss-max` (Core architecture, 4-stage and 6-stage pipelines, hazard and forwarding units, memory subsystem, DRAM controller, virtual memory, CPI auditing)
    - ` 1  Varshith Teja` (Interactive debugger user interface and execution visualization)
  - Both contributors accurately credited on Page 1 title block.

---

## 6. Build & Reproduction Tooling
- [x] **Makefile Target**: `make report8` cleanly builds `report8/RISC201_Memory_Hazards_CPI_Report.pdf` and outputs `Pages: 8`.
- [x] **Automated Tests**: `make test` executes 78/78 tests with 100% success.
- [x] **Micro-Kernel Suite**: `report8/data/run_e1_e10` programmatically executes and logs all micro-benchmarks.
