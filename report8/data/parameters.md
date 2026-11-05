# RISC201 Architectural & Microarchitectural Parameters (Complete Feature Inventory)

Extracted directly from the C++ codebase with exact `file:line` citations. Covers both textbook baselines and advanced architectural extensions.

---

## 1. Pipelines & Execution Timing

| Architectural Parameter | 4-Stage Pipeline Core | 6-Stage Pipeline Core | Code Citation |
| :--- | :---: | :---: | :--- |
| **Pipeline Stage Breakdown** | `IF`, `OF`, `EX`, `MA_RW` | `IF`, `ID`, `RF`, `EX`, `MA`, `WB` | `processor_4stage/Pipeline4Stage.hpp:12-25`, `processor_6stage/Pipeline6Stage.hpp:15-30` |
| **Clock Frequency ($f$)** | **500 MHz** | **800 MHz** | `processor_4stage/Pipeline4Stage.hpp:45`, `processor_6stage/Pipeline6Stage.hpp:48` |
| **Clock Period ($t_{\mathrm{clk}}$)** | **2.00 ns** | **1.25 ns** | Configured architectural clock period |
| **Operand Fetch Stage** | `OF` (Stage 2) | `RF` (Stage 3) | `processor_4stage/Pipeline4Stage.hpp:185`, `processor_6stage/Pipeline6Stage.hpp:215` |
| **ALU Execution Stage** | `EX` (Stage 3) | `EX` (Stage 4) | `processor_4stage/Pipeline4Stage.hpp:210`, `processor_6stage/Pipeline6Stage.hpp:240` |
| **Memory Access Stage** | `MA_RW` (Stage 4) | `MA` (Stage 5) | `processor_4stage/Pipeline4Stage.hpp:245`, `processor_6stage/Pipeline6Stage.hpp:270` |
| **Register Writeback Stage**| `MA_RW` (Stage 4) | `WB` (Stage 6) | `processor_4stage/Pipeline4Stage.hpp:255`, `processor_6stage/Pipeline6Stage.hpp:290` |
| **Branch Resolution Stage** | `EX` (Stage 3) | `EX` (Stage 4) | `processor_4stage/Pipeline4Stage.hpp:220`, `processor_6stage/Pipeline6Stage.hpp:250` |
| **Branch Penalty (Flush)** | **1 bubble** (squashes `IF_OF`) | **2 bubbles** (squashes `IF_ID` & `ID_RF`) | `common/HazardUnit.hpp:56-60`, `common/HazardUnit.hpp:115-120` |
| **Pipeline Fill Latency** | $4 - 1 = \mathbf{3\text{ cycles}}$ | $6 - 1 = \mathbf{5\text{ cycles}}$ | `common/PerformanceTracker.hpp:48` |
| **Ideal CPI** | **1.000** | **1.000** | Single-issue in-order baseline throughput |
| **ISA Binary String Opcodes**| 28 canonical 5-bit strings | 28 canonical 5-bit strings | `common/ISA.hpp:20-65` (`isa::OP_ADD = "00000"` to `"11011"`) |

---

## 2. Hazards, Stall Logic & Branch Predictors

| Feature | Configured Value / Implementation | Code Citation |
| :--- | :--- | :--- |
| **Hazard Interlock Unit** | Detects RAW Load-Use, asserts freeze PC & IF/OF, inserts bubble into EX | `common/HazardUnit.hpp:18-120` |
| **Stall Reason Attribution** | `RAW_LOAD_USE`, `RAW_NOFWD`, `BRANCH_MISPREDICT`, `STRUCTURAL_MEM`, `ICACHE_MISS`, `DCACHE_MISS`, `TLB_MISS_PAGE_WALK`, `DRAM_LATENCY` | `common/PerformanceTracker.hpp:14-35` |
| **Load-Use Delay Slot** | 1 cycle hardware stall bubble; completely eliminated via compiler instruction scheduling pass (`--schedule`) | `common/HazardUnit.hpp:88-95`, `assembler/Assembler.hpp:310-340` |
| **Branch Predictor Policies** | 8 policies: `STALL`, `FLUSH` (predict not-taken), `PREDICT_TAKEN`, `STATIC_BTFN`, `BIMODAL_1BIT`, `BIMODAL_2BIT`, `GSHARE`, `PERFECT` | `common/BranchPredictor.hpp:13-36` |
| **Bimodal 2-Bit Predictor** | 256-entry table of 2-bit saturating counters (00, 01, 10, 11) with hysteresis | `common/BranchPredictor.hpp:148, 165` |
| **GShare Predictor** | 10-bit Global History Register (GHR) XOR with PC: `index = ((pc >> 2) ^ ghr) % tableSize` | `common/BranchPredictor.hpp:151, 256` |
| **Branch Target Buffer (BTB)**| 64-entry direct-mapped BTB with tag comparison | `common/BranchPredictor.hpp:58-95` |
| **Return Address Stack (RAS)**| 16-entry circular call-return target prediction stack | `common/BranchPredictor.hpp:97-130` |
| **Precise Exceptions** | Faulting PC captured, pipeline drained, control transferred to kernel handler via `RETZ` | `common/Exception.hpp:12-45`, `processor_4stage/Pipeline4Stage.hpp:280-285` |

---

## 3. Data Forwarding Bypass Network

| Bypass Network Property | Configured Value / Implementation | Code Citation |
| :--- | :--- | :--- |
| **Forwarding Modes** | `NONE` (interlocks only), `BASIC` (`EX->EX`), `FULL` (`EX->EX`, `MEM->EX`, `WB->EX`), `FULL_PLUS` (+StoreBuffer bypass) | `common/ForwardingUnit.hpp:13-28` |
| **4-Stage Core Bypass Paths** | `EX_MARW -> OF_EX` (ALU-to-ALU), `MARW -> MARW` (Store Data) | `common/ForwardingUnit.hpp:150-175` |
| **6-Stage Core Bypass Paths** | `EX_MA -> RF_EX` (Priority 1), `MA_RW -> RF_EX` (Priority 2), `RW -> RF_EX` (Priority 3) | `common/ForwardingUnit.hpp:200-265` |
| **Priority Resolution Rule** | Youngest producer strictly overrides older pipeline registers (`EX_MA` > `MA_RW` > `RW`) | `common/ForwardingUnit.hpp:202-245` |
| **Store-Data Forwarding** | MEM-to-MEM forwarding for `load -> store` dependency with 0 stalls | `common/ForwardingUnit.hpp:247-266` |
| **Store-to-Load Bypass** | 16-entry `StoreBuffer` bypasses pending data to subsequent load to same address before cache write | `common/ForwardingUnit.hpp:48-95` |

---

## 4. Memory System Architecture (Textbook + Advanced)

| Subsystem | Architectural Parameter | Configured Value | Code Citation |
| :--- | :--- | :--- | :--- |
| **L1 Caches (I & D)** | Capacity & Associativity | 32 KB, 8-way set-associative | `memory/Config.hpp:128-140` |
| | Line Size ($B$) | 64 Bytes ($b = 6\text{ offset bits}$) | `memory/Cache.hpp:66` |
| | Number of Sets ($S$) | 64 Sets ($s = 6\text{ index bits}$) | `memory/Cache.hpp:65` |
| | Tag Width ($t$) | 20 bits ($32 - (6 + 6)$) | `memory/Cache.hpp:68` |
| | Hit Latency | **1 clock cycle** | `memory/Config.hpp:131` |
| | Write & Replacement Policy| Write-Back + Write-Allocate (`wb_wa`), True LRU, SRRIP, BIP | `memory/Config.hpp:20-26` |
| | 3C Miss Categorization | Compulsory, Capacity (fully-associative shadow tag), Conflict | `memory/Cache.hpp:74-95, 296-330` |
| **Non-Blocking MSHRs** | MSHR Entries | **8 MSHR entries** (hit-under-miss & secondary hit merging) | `memory/Config.hpp:22`, `memory/Cache.hpp:72, 278-294` |
| **Hardware Prefetchers** | Supported Types | Next-Line, Stride (16-entry RPT), Stream (4 stream buffers, degree 2) | `memory/Config.hpp:94-102`, `memory/Prefetcher.hpp:20-95` |
| | Page Boundary Suppression | Prevents prefetch requests from crossing 4 KB page boundaries | `memory/Prefetcher.hpp:70-73` |
| **DRAM (DDR4-3200)** | Clock Period ($t_{\mathrm{CK}}$)| **0.625 ns** (1600 MHz clock, 3200 MT/s) | `memory/Config.hpp:30` |
| | JEDEC Timings | $t_{\mathrm{CL}}=22, t_{\mathrm{RCD}}=22, t_{\mathrm{RP}}=22, t_{\mathrm{RAS}}=52, t_{\mathrm{RC}}=74, t_{\mathrm{WR}}=24, t_{\mathrm{FAW}}=34$ | `memory/Config.hpp:31-40` |
| | Bank Architecture | 4 Bank Groups $\times$ 4 Banks = **16 Banks total** | `memory/Config.hpp:56-57` |
| | Bank Group Timings | Short $t_{\mathrm{CCD\_S}}=4, t_{\mathrm{RRD\_S}}=4$; Long $t_{\mathrm{CCD\_L}}=8, t_{\mathrm{RRD\_L}}=8$ | `memory/Config.hpp:42-44` |
| | Burst Length & Width | 64-bit bus (8 Bytes), $\text{BL}=8$ ($64\text{ B}$ burst), $t_{\mathrm{BURST}}=4\text{ cycles}$ | `memory/Config.hpp:45-46, 60` |
| | Peak Channel Bandwidth | $3200\text{ MT/s} \times 8\text{ B} = \mathbf{25.6\text{ GB/s}}$ | `docs/MEMORY_DESIGN.md:151` |
| | Refresh Parameters | $t_{\mathrm{RFC}}=350\text{ ns}, t_{\mathrm{REFI}}=7.8\ \mu\text{s}$ ($7800\text{ ns}$) | `memory/Config.hpp:47-48` |
| | Row Buffer Policy & Sched | Open-Page Policy, FR-FCFS out-of-order scheduling | `memory/Config.hpp:61-62`, `memory/DRAM.hpp:155-180` |
| **DDR5 Extension** | Subchannels & Banks | Dual independent 32-bit subchannels, 8 bank groups (32 banks), 38.4 GB/s | `memory/Config.hpp:207-229` |
| **Virtual Memory** | Address Widths | 32-bit baseline (SimpleRisc) & 48-bit modern x86-64/ARM64 | `memory/Config.hpp:69-70` |
| | Page Size & Structure | 4096 Bytes (4 KB, 12-bit offset); 2-level (10+10+12) or 4-level radix tree | `memory/Config.hpp:71-72` |
| | 4-Level Radix Split | `[47:39] PML4` (512 entries) $\to$ `[38:30] PDPT` $\to$ `[29:21] PD` $\to$ `[20:12] PT` | `memory/Config.hpp:72`, `docs/VIRTUAL_MEMORY.md:80` |
| | TLB Hierarchy | L1 ITLB & DTLB: 64 entries, 4-way, 1 cyc; L2 STLB: 1024 entries, 8-way, 7 cyc | `memory/Config.hpp:75-80, 84-85` |
| | Page-Walk Cache (PWC) | **16-entry PWC** caches intermediate directory levels (PML4, PDPT, PD), 3 cyc | `memory/Config.hpp:81, 86`, `memory/VirtualMemory.hpp:350-361` |
| | Page Fault Latencies | Minor fault (demand zero-fill): 1200 cyc; Major fault (swap-in): 300,000 cyc | `memory/Config.hpp:87-88` |

---

## 5. Performance Accounting Invariant

$$\text{Total Execution Cycles} = (\text{Stages} - 1) + IC + \sum_{k} \text{StallCycles}_k + \text{BranchBubbles}$$
* Reference: `common/PerformanceTracker.hpp:92-105`.
* Enforces that every single CPU cycle is strictly attributed to **exactly one** canonical reason with zero unallocated time.
