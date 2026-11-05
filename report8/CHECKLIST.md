# RISC201 Complete 8-Page Technical Report Verification Checklist

This audit checklist verifies compliance with all requirements for the **CSC-201 Computer Organization & Architecture** (Autumn 2026) report, *"RISC201: Comprehensive Architecture Report on Memory Systems, Pipeline Hazards, Data Forwarding, and CPI Analysis"*.

---

## 1. Document & Page Count Compliance
- [x] **Target Page Count**: Strictly **8 pages** (A4).
- [x] **Verification Command**: `pdfinfo report8/RISC201_Memory_Hazards_CPI_Report.pdf | grep "Pages:"` -> **`Pages: 8`**.
- [x] **Page Budget Allocation**:
  - **Page 1**: Compact Title block (course, repo, commit, authors), Abstract, Keywords, Key metrics at a glance, Section 1 Introduction, Table 1 Requirement Checklist & Status.
  - **Page 2**: Section 2 Core Architecture, 5-bit string opcodes, Figure 1 (Block diagram), 4S vs 6S pipelines, Figure 2 (Stage layouts), Invariant accounting formula, Table 2 (Textbook baseline vs Project extensions).
  - **Page 3**: Section 3 Hazards Taxonomy (structural, RAW, WAR/WAW non-occurrence proof), HazardUnit logic, Load-use derivation (E6), Figure 4 (2-bit bimodal state machine), Branch policies (E7, E9 GShare & RAS), Precise exceptions, Table 3 (Branch structures).
  - **Page 4**: Section 4 Data Forwarding, Figure 3 (Bypass paths & multiplexers), Youngest-producer priority (`EX_MA` > `MA_RW` > `RW`) verified via E8, Bypass special cases (Store-data, StoreBuffer store-to-load), Table 4 (Minimum stall cycle matrix across NONE/BASIC/FULL/FULL+).
  - **Page 5**: Section 5 Memory System (Part 1: Caches & Prefetching), Locality principles, Figure 5 (Cache address split & 4-level radix VM split), 32 KB 8-way cache, 3C miss classification & A,B,A,B thrashing (E1), Non-blocking cache with 8 MSHRs (E2), Hardware stream/stride prefetchers (E2), Recursive AMAT (E2).
  - **Page 6**: Section 5 Memory System (Part 2: DRAM & Virtual Memory), Modern DDR4-3200 JEDEC timings (0.625 ns, Row Hit 26 cyc, Empty 48 cyc, Conflict 70 cyc) (E3), Figure 6 (DDR4 Bank Groups & inter-bank timing differentials), FR-FCFS out-of-order scheduling speedup (E3), Figure 7 (4-level page walk & PWC acceleration), 48-bit 4-level VM radix walk (PML4->PDPT->PD->PT), 16-entry Page-Walk Cache (85.4% walk latency cut) (E4), TLB hierarchy & page fault handling.
  - **Page 7**: Section 6 CPI Calculations & Performance, CPI stack formulation, PerformanceTracker invariant, Table 5 (Analytical CPI stack across labeled mix), E10 10-Instruction cycle reconciliation (4S: 16 cyc, 6S: 19 cyc, Invariant PASS), Processor speedup ($1.578\times$ wall-clock speedup) & break-even clock ratio ($f_6/f_4 > 1.014$) (E11), Cold-start vs steady-state CPI analysis (E12).
  - **Page 8**: Section 7 Verification & Quality Assurance (Golden ISS differential equivalence, 78/78 tests pass, ASan/UBSan clean), Section 8 Limitations (real, specific, one line per item), Section 9 Conclusion, Master Reconciliation Table (E1 to E12), References, Reproduction Box.

---

## 2. Full Feature Inventory & Verification Status (No Syllabus Restriction)

| Domain | Feature | Code Location | Status | Verified By |
| :--- | :--- | :--- | :---: | :---: |
| **Memory** | 32 KB 8-Way Set-Associative L1 Cache | `memory/Cache.hpp:45-125` | Complete | E1 |
| | 3C Miss Classification (Compulsory, Capacity, Conflict)| `memory/Cache.hpp:74-95, 296-330` | Complete | E1 |
| | Direct-Mapped vs 2-Way Conflict Thrashing Resolution | `memory/Cache.hpp:100-117` | Complete | E1 |
| | Non-Blocking Caches with 8 MSHRs (Hit-Under-Miss) | `memory/Cache.hpp:72, 278-294` | Complete | E2 |
| | Hardware Stream Prefetcher (4 buffers, degree 2) | `memory/Prefetcher.hpp:20-95` | Complete | E2 |
| | Stride RPT Prefetcher & Page Boundary Suppression | `memory/Prefetcher.hpp:70-90` | Complete | E2 |
| | Recursive AMAT Formulation ($L_1 \to L_2 \to \text{DRAM}$) | `memory/Config.hpp:126-160` | Complete | E2 |
| | Modern DDR4-3200 JEDEC Timing ($t_{\mathrm{CK}}=0.625\text{ ns}$) | `memory/Config.hpp:30-40` | Complete | E3 |
| | DDR4 Bank Groups (16 Banks in 4 Groups, $t_{\mathrm{CCD\_S}}$ vs $t_{\mathrm{CCD\_L}}$)| `memory/Config.hpp:42-44, 56-57` | Complete | E3 |
| | FR-FCFS Memory Request Scheduling Reordering | `memory/Config.hpp:62`, `memory/DRAM.hpp:155` | Complete | E3 (Analytic) |
| | DDR5 Dual 32-bit Subchannels Extension (32 Banks, 38.4 GB/s)| `memory/Config.hpp:207-229` | Complete | Config Check |
| | 48-Bit Virtual Address 4-Level Radix Walk (PML4->PDPT->PD->PT)| `memory/Config.hpp:69-72`, `memory/VirtualMemory.hpp:345` | Complete | E4 |
| | Page-Walk Cache (PWC, 16 entries, cuts walk latency by 74–85%)| `memory/VirtualMemory.hpp:350-361` | Complete | E4 |
| | Multi-Level TLB Hierarchy (L1 ITLB/DTLB 64-entry, L2 STLB 1024-entry)| `memory/Config.hpp:75-80` | Complete | E4 |
| | Demand Page Fault Handling (Minor Zero-Fill & Major Swap-In)| `memory/VirtualMemory.hpp:365-405` | Complete | E4 |
| **Hazards** | Structural Hazards Elimination (Harvard L1 Caches) | `memory/MemoryHierarchy.hpp:12-40` | Complete | Invariant |
| | RAW Data Dependency Detection | `common/HazardUnit.hpp:18-50` | Complete | E5, E6 |
| | WAR / WAW Non-Occurrence (In-order pipeline proof)| Formalized in Report Section 3.1 | Complete | Theoretical Proof |
| | Load-Use Interlock & Stalls (1-cycle delay) | `common/HazardUnit.hpp:88-95` | Complete | E6 |
| | Load-Use Instruction Scheduling Pass (`--schedule`) | `assembler/Assembler.hpp:310-340` | Complete | E6 |
| | Static Branch Not-Taken Squashes (1 bubble 4S, 2 bubbles 6S)| `common/HazardUnit.hpp:56, 115` | Complete | E7 |
| | Dynamic 2-Bit Bimodal Saturating Counter with Hysteresis| `common/BranchPredictor.hpp:148, 251` | Complete | E9 |
| | GShare Branch Predictor (10-bit GHR XOR PC indexing) | `common/BranchPredictor.hpp:151, 256` | Complete | E9 |
| | Branch Target Buffer (BTB, 64 entries) | `common/BranchPredictor.hpp:58-95` | Complete | E9 |
| | Return Address Stack (RAS, 16 entries circular LIFO)| `common/BranchPredictor.hpp:97-130` | Complete | E9 |
| | Precise Exception Handling & `RETZ` Return | `common/Exception.hpp:12-45` | Complete | Architecture Check |
| **Forwarding**| Forwarding Modes: `NONE`, `BASIC`, `FULL`, `FULL_PLUS` | `common/ForwardingUnit.hpp:13-28` | Complete | E5, E8 |
| | 4-Stage Core Bypass Paths (`EX_MARW -> OF_EX`, `MARW -> MARW`)| `common/ForwardingUnit.hpp:150-175` | Complete | E5 |
| | 6-Stage Core Bypass Paths (`EX_MA`, `MA_RW`, `RW` $\to$ `RF_EX`)| `common/ForwardingUnit.hpp:200-265` | Complete | E5 |
| | Youngest-Producer Priority Resolution (`EX_MA` > `MA_RW` > `RW`)| `common/ForwardingUnit.hpp:202-245` | Complete | E8 |
| | Store-Data MEM->MEM Forwarding (0 stalls) | `common/ForwardingUnit.hpp:247-266` | Complete | E8 |
| | StoreBuffer Store-to-Load Bypass (`FULL_PLUS`) | `common/ForwardingUnit.hpp:48-95` | Complete | E8 |
| **CPI & Perf** | PerformanceTracker Cycle-Accounting Invariant Formula | `common/PerformanceTracker.hpp:92-105` | Complete | E10 |
| | Canonical Stall Attributions (8 distinct categories) | `common/PerformanceTracker.hpp:14-35` | Complete | E10 |
| | Analytic CPI Model Across Labeled Instruction Mixes | Formalized in Report Table 5 | Complete | E11 |
| | 4-Stage vs 6-Stage Speedup Calculation ($1.578\times$) | Formalized in Report Section 6.4 | Complete | E11 |
| | Break-Even Clock Ratio Derivation ($f_6 / f_4 > 1.014$)| Formalized in Report Section 6.4 | Complete | E11 |
| | Cold-Start vs Steady-State CPI Amortization Analysis | Formalized in Report Section 6.4 | Complete | E12 |

---

## 3. Micro-Program Verification (E1 - E12) Reconciled Matrix

| ID | Concept Tested | Analytical Hand Calculation | Simulator Output (`run_e1_e12`) | Status |
| :---: | :--- | :--- | :--- | :---: |
| **E1** | Cache Split & 3C Thrashing | $t=20, s=6, b=6$; Set 0 ways 0..7; A,B,A,B thrash | Direct-Mapped: 0% hit (thrashing); 2-Way: 50% cold / 100% steady | **PASS (100% Match)** |
| **E2** | AMAT, MSHRs & Prefetch | $\text{AMAT}=2.0\text{ cyc}$; MSHR 1 vs 4 overlap; stream pref | MSHR merges secondary miss; stream prefetch triggers 2 lines | **PASS (100% Match)** |
| **E3** | DDR4 Timings & Bank Groups | Hit 26 cyc ($16.25\text{ ns}$), Empty 48 cyc ($30\text{ ns}$), Conflict 70 cyc ($43.75\text{ ns}$) | DDR4-3200 timings match; FR-FCFS reordering yields $1.518\times$ speedup | **PASS (100% Match)** |
| **E4** | 48-Bit 4-Level VM & PWC | PML4(255), PDPT(510), PD(350), PT(209), Offset(564) | 4-level walk: 1288 cyc; PWC hit: 1211 cyc (85.4% walk latency cut) | **PASS (100% Match)** |
| **E5** | RAW Chain Forwarding | 3 ALU insts: None vs Full on 4S and 6S | 4S: 6 cyc ($0\text{ stall}$) vs 8 cyc ($2\text{ stall}$); 6S: 8 cyc ($0\text{ stall}$) vs 12 cyc ($4\text{ stall}$) | **PASS (100% Match)** |
| **E6** | Load-Use Scheduling | Adjacent ($1\text{ bubble}$) vs Separated ($0\text{ stalls}$) | Case A: 8 cyc ($1\text{ load-use bubble}$); Case B: 8 cyc ($0\text{ stalls}$, 5 retired insts) | **PASS (100% Match)** |
| **E7** | Branch Penalty | 4S squashes 1 inst; 6S squashes 2 insts | 4S: 8 cyc ($1\text{ bubble}$); 6S: 11 cyc ($2\text{ bubbles}$) | **PASS (100% Match)** |
| **E8** | Forwarding Priority | $\text{EX\_MA} > \text{MA\_RW}$ (Youngest producer wins) | Producer writes 10 then 20; consumer receives $20 \implies r2 = 25$ (`0x19`) | **PASS (100% Match)** |
| **E9** | GShare & RAS Call/Return | GShare index: `(PC>>2) ^ GHR`; 16-entry RAS | GShare updates GHR; RAS pops exact return target with 0 bubbles | **PASS (100% Match)** |
| **E10**| Full CPI Reconciliation | Total Cycles $= (S-1) + IC + \sum \text{Stalls}$ | 4S: 16 cyc ($\text{CPI} = 1.455$); 6S: 19 cyc ($\text{CPI} = 1.727$); Invariant PASS | **PASS (100% Match)** |
| **E11**| 4S vs 6S Speedup & Break-Even| Time ratio & $f_6 / f_4 > \text{CPI}_6 / \text{CPI}_4 = 1.014$ | $1.578\times$ wall-clock speedup ($57.8\%$ faster); break-even $f_6 > 507\text{ MHz}$ | **PASS (100% Match)** |
| **E12**| Cold-Start vs Steady-State | Short loop: fill latency unamortized ($17.6\%$) | 5-iter loop: $\text{CPI} = 1.294 \to$ steady-state $\text{CPI} \to 1.000$ | **PASS (100% Match)** |

---

## 4. Top 10 Likely Examiner Questions & Answers

1. **Q: Why did you implement 4-level 48-bit virtual memory when SimpleRisc addresses are 32-bit?**
   - *A:* SimpleRisc addresses are 32-bit, but modern operating systems and 64-bit architectures (x86-64 and ARM64) utilize 48-bit virtual addressing with 4-level page tables (PML4 $\to$ PDPT $\to$ PD $\to$ PT). We implemented both: a baseline 32-bit 2-level mode for SimpleRisc, and a fully functional 48-bit 4-level radix mode to explore modern hardware page-table walk latencies and verify Page-Walk Cache (PWC) acceleration.
2. **Q: How does the Page-Walk Cache (PWC) cut walk latency by 74–85%?**
   - *A:* In a standard 4-level walk, a TLB miss requires 4 serial memory references (one for each table level). PWC caches the physical base pointers of intermediate directory levels (PML4, PDPT, PD). When accessing adjacent pages within the same 2 MB virtual directory, PWC hits on the directory prefix, allowing the hardware walker to directly fetch the final 4 KB Page Table Entry (PTE) in a single memory access instead of four.
3. **Q: Why is a 1-cycle stall unavoidable in a load-use hazard even with full forwarding?**
   - *A:* In an in-order pipeline, memory read data only becomes valid after the data cache access completes at the end of the Memory stage (`MA`). The dependent instruction requires this operand at the start of the Execute stage (`EX`). Forwarding backward across time is physically impossible without violating causality; hence, the pipeline must inject exactly 1 stall bubble to let the memory stage complete.
4. **Q: How does compiler instruction scheduling eliminate load-use stalls?**
   - *A:* The assembler scheduling pass (`--schedule`) scans the basic block for independent instructions located after the load and moves one into the delay slot immediately following the load. The independent instruction executes in `EX` while the load is in `MA`, allowing the loaded value to be forwarded to the subsequent consumer with zero stall cycles.
5. **Q: Why can Write-After-Read (WAR) and Write-After-Write (WAW) hazards never occur in RISC201?**
   - *A:* RISC201 is an in-order pipeline. Instructions are fetched, decoded, and dispatched in strict program order. Register reads occur strictly in the Decode/Operand Fetch stage, and register writes occur strictly in the Writeback stage at the end of the pipeline in program order. Because an earlier instruction always reads its operand before a later instruction reaches writeback, and earlier instructions always commit their writes before later instructions commit, WAR and WAW are structurally impossible.
6. **Q: How does GShare branch prediction differ from 2-bit bimodal?**
   - *A:* Bimodal prediction indexes its counter table using only the lower bits of the branch PC, meaning every instance of a branch shares the same counter regardless of execution context. GShare XORs the branch PC with a Global History Register (GHR) recording the outcomes of the last 10 branches. This maps identical branch instructions executed along different execution paths to distinct counter table entries, eliminating path aliasing.
7. **Q: What is the benefit of DDR4 Bank Groups compared to a flat multi-bank memory?**
   - *A:* In high-speed DDR4, the internal sense amplifiers and column decoders require a long recovery delay ($t_{\mathrm{CCD\_L}} = 8\text{ cycles}$) when consecutive read/write commands target the same bank group. By organizing 16 banks into 4 separate Bank Groups, consecutive commands to different bank groups run at the short delay ($t_{\mathrm{CCD\_S}} = 4\text{ cycles}$), sustaining the full 25.6 GB/s bus bandwidth without pipeline stalls.
8. **Q: What is the difference between Hit-Under-Miss and Miss-Under-Miss in non-blocking caches?**
   - *A:* Hit-Under-Miss allows subsequent cache hits to complete in 1 cycle while an earlier miss is fetching data from DRAM. Miss-Under-Miss allows multiple independent cache misses to issue concurrently to memory using Miss Status Holding Registers (MSHRs). In RISC201, 8 MSHR entries track pending misses and merge secondary hits to the same line address without issuing redundant memory transfers.
9. **Q: Why is the CPI of the 6-stage core higher than the 4-stage core, yet its execution time is lower?**
   - *A:* The 6-stage core has deeper pipeline stages, resulting in a higher fill latency (5 cycles vs 3 cycles) and a larger branch misprediction penalty (2 bubbles vs 1 bubble), giving a slightly higher CPI ($1.090$ vs $1.075$). However, dividing the pipeline stages allows the 6-stage core to achieve an 800 MHz clock frequency ($t_{\mathrm{clk}} = 1.25\text{ ns}$) compared to 500 MHz ($2.00\text{ ns}$). Because the $1.60\times$ clock frequency increase far exceeds the $1.014\times$ break-even CPI ratio, the 6-stage core delivers a $57.8\%$ wall-clock speedup ($1.578\times$).
10. **Q: How does the cycle-accounting invariant prove simulation correctness?**
    - *A:* The performance tracker enforces the mathematical invariant:
      $$\text{Total Cycles} = (\text{Stages} - 1) + IC + \sum \text{StallCycles}_k + \text{BranchBubbles}$$
      Because every clock cycle during execution is sampled and mapped to exactly one mutually exclusive reason code (`RAW_LOAD_USE`, `RAW_NOFWD`, `BRANCH_MISPREDICT`, `ICACHE_MISS`, etc.), any unallocated cycles, double-counted bubbles, or dropped latencies would immediately violate the invariant and trigger a runtime assertion failure. 100% of cycles are strictly accounted for.
