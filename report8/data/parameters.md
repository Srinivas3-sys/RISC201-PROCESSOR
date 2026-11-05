# RISC201 Architectural & Physical Parameters (Sarangi Textbook Scope)

This document contains all real parameters extracted directly from the C++ codebase with exact `file:line` citations.

---

## 1. Pipelines & Execution Timing

| Parameter | 4-Stage Pipeline | 6-Stage Pipeline | Code Reference |
| :--- | :---: | :---: | :--- |
| **Pipeline Stages** | 4 (`IF`, `OF`, `EX`, `MA_RW`) | 6 (`IF`, `ID`, `RF`, `EX`, `MA`, `WB`) | `processor_4stage/Pipeline4Stage.hpp:12-25`, `processor_6stage/Pipeline6Stage.hpp:15-30` |
| **Clock Frequency ($f$)** | **500 MHz** | **800 MHz** | `processor_4stage/Pipeline4Stage.hpp:45`, `processor_6stage/Pipeline6Stage.hpp:48` |
| **Clock Period ($t_{\mathrm{clk}}$)** | **2.00 ns** | **1.25 ns** | $t_{\mathrm{clk}} = 1 / f$ |
| **Operand Read Stage** | `OF` (Stage 2) | `RF` (Stage 3) | `processor_4stage/Pipeline4Stage.hpp:185`, `processor_6stage/Pipeline6Stage.hpp:215` |
| **ALU Result Available** | End of `EX` (Stage 3) | End of `EX` (Stage 4) | `processor_4stage/Pipeline4Stage.hpp:210`, `processor_6stage/Pipeline6Stage.hpp:240` |
| **Memory Data Available** | End of `MA_RW` (Stage 4) | End of `MA` (Stage 5) | `processor_4stage/Pipeline4Stage.hpp:245`, `processor_6stage/Pipeline6Stage.hpp:270` |
| **Register Writeback** | End of `MA_RW` (Stage 4) | End of `WB` (Stage 6) | `processor_4stage/Pipeline4Stage.hpp:255`, `processor_6stage/Pipeline6Stage.hpp:290` |
| **Branch Resolution Stage** | `EX` (Stage 3) | `EX` (Stage 4) | `processor_4stage/Pipeline4Stage.hpp:220`, `processor_6stage/Pipeline6Stage.hpp:250` |
| **Branch Penalty (Flush)** | **1 cycle** (squashes `IF_OF`) | **2 cycles** (squashes `IF_ID` & `ID_RF`) | `common/HazardUnit.hpp:56-60`, `common/HazardUnit.hpp:115-120` |
| **Pipeline Fill Latency** | $4 - 1 = \mathbf{3\text{ cycles}}$ | $6 - 1 = \mathbf{5\text{ cycles}}$ | `common/PerformanceTracker.hpp:48` |
| **Ideal CPI** | **1.000** | **1.000** | Retires 1 instruction per cycle under zero hazards |

---

## 2. Hazard Handling & Forwarding Network

| Parameter | 4-Stage Core | 6-Stage Core | Code Reference |
| :--- | :---: | :---: | :--- |
| **Forwarding Modes** | `NONE`, `BASIC`, `FULL` | `NONE`, `BASIC`, `FULL` | `common/ForwardingUnit.hpp:13-18` |
| **EX-to-EX Bypass Path** | `EX_MARW -> OF_EX` | `EX_MA -> RF_EX` | `common/ForwardingUnit.hpp:153-166`, `common/ForwardingUnit.hpp:205-225` |
| **MEM-to-EX Bypass Path**| Merged in `EX_MARW` | `MA_RW -> RF_EX` | `common/ForwardingUnit.hpp:226-245` |
| **WB-to-EX Bypass Path** | (Not needed: merged RW) | `RW -> RF_EX` | `common/ForwardingUnit.hpp:246-260` |
| **Priority Rule** | Youngest producer first | `EX_MA` > `MA_RW` > `RW` | `common/ForwardingUnit.hpp:202-265` |
| **Store Data Forwarding** | `EX_MARW -> MARW` | `EX_MA / MA_RW -> MA` | `common/ForwardingUnit.hpp:169-174` |
| **RAW ALU-to-ALU Stall** | 0 cycles (with FULL fwd) | 0 cycles (with FULL fwd) | `common/ForwardingUnit.hpp:155` |
| **RAW Without Forwarding** | 1 stall cycle | 2 stall cycles | `common/HazardUnit.hpp:75-82`, `common/HazardUnit.hpp:144-168` |
| **Load-Use Stall** | **1 bubble cycle** | **1 bubble cycle** | `common/HazardUnit.hpp:88-95`, `common/HazardUnit.hpp:126-140` |
| **Stall Reason Codes** | `RAW_LOAD_USE`, `RAW_NOFWD`, `BRANCH_MISPREDICT`, `STRUCTURAL_MEM` | `common/PerformanceTracker.hpp:14-32` |

---

## 3. Memory Hierarchy (Textbook Baseline)

| Subsystem | Architectural Parameter | Configured Value | Code Reference |
| :--- | :--- | :--- | :--- |
| **L1 Instruction Cache** | Organization | 32 KB, 8-way set-associative | `memory/Config.hpp:128-132` |
| | Line Size ($B$) | 64 Bytes ($b = 6\text{ offset bits}$) | `memory/Cache.hpp:66` |
| | Number of Sets ($S$) | 64 Sets ($s = 6\text{ index bits}$) | `memory/Cache.hpp:65` |
| | Tag Width ($t$) | 20 bits ($32 - (6 + 6)$) | `memory/Cache.hpp:68` |
| | Hit Latency | **1 clock cycle** | `memory/Config.hpp:131` |
| | Write / Replacement Policy| Write-Back + Write-Allocate, LRU | `memory/Config.hpp:18-20` |
| **L1 Data Cache** | Organization | 32 KB, 8-way set-associative | `memory/Config.hpp:134-140` |
| | Line Size / Hit Latency | 64 Bytes, **1 clock cycle** | `memory/Config.hpp:137-138` |
| **Main Memory (DRAM)** | Cell Architecture | 1T-1C capacitive storage cell | `memory/DRAM.hpp:15-30` |
| | Clock Cycle ($t_{\mathrm{CK}}$) | 1.0 ns (Textbook SimpleDRAM) | `memory/Config.hpp:196` |
| | $t_{\mathrm{RCD}}$ (Row-to-Column) | 15 DRAM cycles (15 ns) | `memory/Config.hpp:197` |
| | $t_{\mathrm{RP}}$ (Row Precharge) | 15 DRAM cycles (15 ns) | `memory/Config.hpp:198` |
| | $t_{\mathrm{CAS}}$ / $t_{\mathrm{CL}}$ (Column Latency)| 15 DRAM cycles (15 ns) | `memory/Config.hpp:199` |
| | $t_{\mathrm{BURST}}$ (Burst Duration) | 4 DRAM cycles (8-word burst) | `memory/Config.hpp:200` |
| | Row Hit Latency | $t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = \mathbf{19\text{ ns}}$ | `memory/DRAM.hpp:120-140` |
| | Row Empty Latency | $t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = \mathbf{34\text{ ns}}$ | `memory/DRAM.hpp:141-160` |
| | Row Conflict Latency | $t_{\mathrm{RP}} + t_{\mathrm{RCD}} + t_{\mathrm{CAS}} + t_{\mathrm{BURST}} = \mathbf{49\text{ ns}}$ | `memory/DRAM.hpp:161-180` |
| **Virtual Memory** | Virtual Address Width | 32 bits ($4\text{ GB}$ address space) | `memory/Config.hpp:69` |
| | Page Size | 4096 Bytes ($4\text{ KB}$, 12-bit offset) | `memory/Config.hpp:71` |
| | Page Table Structure | 2-level hierarchical radix tree | `memory/Config.hpp:72` |
| | Page Directory (PD) | 10 bits ($2^{10} = 1024$ entries) | `docs/VIRTUAL_MEMORY.md:70-74` |
| | Page Table (PT) | 10 bits ($2^{10} = 1024$ entries) | `docs/VIRTUAL_MEMORY.md:72` |
| | Address Split | `[31:22] PD | [21:12] PT | [11:0] Offset` | `docs/VIRTUAL_MEMORY.md:74` |
| | Page Table Entry (PTE) | 4 bytes (PFN: 20 bits, Flags: 12 bits)| `memory/VirtualMemory.hpp:16-27` |
| | TLB Organization | 64 entries, 4-way associative | `memory/Config.hpp:75-78` |
| | TLB Hit Latency | **1 clock cycle** | `memory/Config.hpp:84` |

---

## 4. Performance Accounting Invariant

$$\text{Total Execution Cycles} = (\text{Stages} - 1) + IC + \sum \text{Stall Cycles} + \text{Branch Bubbles}$$
* Reference: `common/PerformanceTracker.hpp:92-105`.
* Enforces that every single CPU cycle is attributed to **exactly one** canonical reason.
