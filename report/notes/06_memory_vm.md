# Module Notes: Memory Hierarchy & Virtual Memory Subsystem

## 1. Multi-Level Memory Hierarchy (`memory/MemoryHierarchy.hpp`)
* **Modified Harvard Architecture**:
  * Level 1: Split **L1I Cache** (32 KB, 8-way) and **L1D Cache** (32 KB, 8-way).
  * Level 2: Unified **L2 Cache** (512 KB, 8-way, 12 cycles).
  * Level 3: Unified **L3 Cache** (8 MB, 16-way, 38 cycles).
  * Main Memory: Cycle-accurate DDR4/DDR5 DRAM controller.
* **Store Buffer**: 4-entry FIFO with CAM address matching. Bypasses stored data directly to matching loads with 1-cycle latency before writing to cache.

---

## 2. Cache Architecture (`memory/Cache.hpp`)
* **Address Decomposition**:
  $$\text{Tag} = \text{Addr}[63 : (\text{IndexBits} + \text{OffsetBits})]$$
  $$\text{Index} = \text{Addr}[(\text{IndexBits} + \text{OffsetBits} - 1) : \text{OffsetBits}]$$
  $$\text{Offset} = \text{Addr}[\text{OffsetBits} - 1 : 0]$$
  For 32 KB, 8-way, 64-byte line: 64 sets $\implies 6$ index bits, 6 offset bits, 52 tag bits.
* **Replacement Algorithms**:
  * **LRU**: True Least Recently Used using access timestamps.
  * **FIFO**: First-In First-Out.
  * **Random**: Uniform pseudo-random line eviction.
  * **Tree-PLRU**: Binary tree approximation using $N-1$ flag bits per set.
  * **SRRIP**: Static Re-Reference Interval Prediction using 2-bit RRPV counters per line.
* **3C Miss Telemetry**: Dynamically classifies every miss as **Compulsory** (cold line never seen before), **Capacity** (fully-associative cache of same size would miss), or **Conflict** (associativity limitation).
* **Non-Blocking MSHRs**: Tracks outstanding line requests to allow memory-level parallelism (MLP).

---

## 3. Virtual Memory & TLB Subsystem (`memory/VirtualMemory.hpp`)
* **Radix-4 Page Table Translation**:
  * 48-bit Virtual Address space decomposed into:
    * $[47:39]$: PML4 Index (9 bits $\implies$ 512 entries)
    * $[38:30]$: PDPT Index (9 bits $\implies$ 512 entries)
    * $[29:21]$: PD Index (9 bits $\implies$ 512 entries)
    * $[20:12]$: PT Index (9 bits $\implies$ 512 entries)
    * $[11:0]$: Page Offset (12 bits $\implies$ 4 KB standard page)
  * Supports **2 MB Huge Pages** (skips PT stage, bits $[20:0]$ become page offset).
* **Multi-Level TLB Hierarchy**:
  * **L1 ITLB**: 64 entries, 4-way set-associative (1-cycle hit).
  * **L1 DTLB**: 64 entries, 4-way set-associative (1-cycle hit).
  * **L2 STLB**: 1024 entries, 8-way set-associative (unified, 8-cycle hit).
* **Page-Walk Cache (PWC)**:
  * Caches intermediate upper-level page table pointers (PML4, PDPT, PD).
  * Cuts standard 4-level page table walk latency by up to **74%**!
* **Demand Paging & Swap**:
  * Minor page faults allocate a physical frame from the free list (zero-fill).
  * Major page faults evict victim frames to NVMe SSD swap space using Clock/LRU.
  * Enforces stack guard page boundary protection (raises `STACK_OVERFLOW` exception).

---

## 4. Cycle-Accurate DRAM Model (`memory/DRAM.hpp`)
* **DRAM Organization**:
  $$\text{Channels} \rightarrow \text{Ranks} \rightarrow \text{Bank Groups} \rightarrow \text{Banks} \rightarrow \text{Row Buffers}$$
* **JEDEC Timings Modeled**:
  * $t_{\text{RCD}}$: Row-to-Column Delay (ACTIVATE $\rightarrow$ READ/WRITE, 14.06 ns).
  * $t_{\text{RP}}$: Row Precharge (PRECHARGE $\rightarrow$ ACTIVATE, 14.06 ns).
  * $t_{\text{CL}}$: CAS Latency (COLUMN READ $\rightarrow$ DATA BUS, 14.06 ns).
  * $t_{\text{RAS}}$: Row Active Time (ACTIVATE $\rightarrow$ PRECHARGE, 32.0 ns).
  * $t_{\text{RC}} = t_{\text{RAS}} + t_{\text{RP}}$: Full row cycle time (46.06 ns).
  * $t_{\text{RFC}}$: Refresh Cycle Duration (350 ns stall).
  * $t_{\text{REFI}}$: Average Refresh Interval (7.8 $\mu\text{s}$).
* **Request Schedulers**:
  * **FCFS**: First-Come First-Served.
  * **FR-FCFS (First-Ready First-Come First-Served)**: Prioritizes row-buffer hits over row-buffer conflicts, maximizing memory bus throughput.

---

## 5. Architectural Memory Presets (`memory/Config.hpp`)

| Preset | CPU Freq | VM / TLB | L1 Caches | L2 Cache | L3 Cache | DRAM Subsystem | Prefetcher |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`ideal`** | 3.2 GHz | Disabled (1 cyc) | None | None | None | 1-cycle instant | None |
| **`simple`** | 3.2 GHz | Disabled (Direct PA) | 32 KB I/D (1 cyc) | None | None | DDR4-3200 (1 channel) | None |
| **`modern`** | 3.2 GHz | 48b Radix-4, L1/L2 TLB, PWC | 32 KB I/D (1 cyc) | 512 KB (12 cyc) | 8 MB (38 cyc) | DDR4-3200 (1 channel) | Stream |
| **`modern-ddr5`**| 3.6 GHz | 48b Radix-4, L1/L2 TLB, PWC | 32 KB I/D (1 cyc) | 1 MB (12 cyc) | 16 MB (36 cyc) | DDR5-4800 (2 subchan) | Stream |
| **`lecture`** | 3.2 GHz | 32b 2-Level | 64 B direct (1 cyc) | None | None | SDR SDRAM (133 MHz) | None |

