# Chapter 10: Memory System & Virtual Memory Subsystem

## 10.1 Multi-Level Cache Hierarchy (`memory/Cache.hpp`)

The RISC201 memory subsystem implements a multi-level set-associative cache hierarchy supporting configurable capacity, line sizes, associativity, write policies, and replacement schemes.

### 10.1.1 Architectural Parameters Across Presets
Table 10.1 documents the exact cache parameters implemented in `memory/Config.hpp`:

| Cache Level | Modern Preset (DDR4) | Modern-DDR5 Preset | Associativity | Line Size | Hit Latency | Write Policy | Indexing |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`L1I` Cache** | 32 KB | 32 KB | 8-way | 64 Bytes | **1 cycle** | N/A (Read-only) | VIPT / PIPT |
| **`L1D` Cache** | 32 KB | 32 KB | 8-way | 64 Bytes | **1 cycle** | Write-Back / Write-Alloc | PIPT |
| **`L2` Cache** | 512 KB | 1024 KB | 8-way | 64 Bytes | **12 cycles** | Write-Back / Write-Alloc | PIPT |
| **`L3` Cache** | 8192 KB | 16384 KB | 16-way | 64 Bytes | **38 cycles** | Write-Back / Write-Alloc | PIPT |

### 10.1.2 Cache Address Decomposition & Worked Example
For an 8-way set-associative, 32 KB cache with 64-byte lines:
* Total sets:
  $$S = \frac{\text{Capacity}}{\text{Associativity} \times \text{LineSize}} = \frac{32768}{8 \times 64} = 64\text{ sets}$$
* Bitfield widths:
  * $\text{Offset Bits} = \log_2(64) = \mathbf{6\text{ bits}}$ ($[5:0]$)
  * $\text{Index Bits} = \log_2(64) = \mathbf{6\text{ bits}}$ ($[11:6]$)
  * $\text{Tag Bits} = 64 - (6 + 6) = \mathbf{52\text{ bits}}$ ($[63:12]$)

**Worked Address Example**: Physical Address `0x000000000001A488`:
1. Binary: `... 0001 1010 0100 1000 1000`
2. **Offset** $[5:0] = 001000_2 = \mathbf{0x08}$ (Byte 8 within cache line).
3. **Index** $[11:6] = 010010_2 = \mathbf{18}$ (Set index 18).
4. **Tag** $[63:12] = \text{0x000000000001A}$.
5. Lookup: Searches all 8 ways in Set 18 simultaneously for matching valid tag `0x000000000001A`.

### 10.1.3 Replacement Algorithms & 3C Miss Classification
* **Replacement Algorithms**:
  * **LRU**: Access timestamps determine least-recently used way.
  * **FIFO**: First-in, first-out circular pointer per set.
  * **Random**: Uniform pseudo-random eviction.
  * **Tree-PLRU**: $N-1$ flag bits per set traversing a binary decision tree.
  * **SRRIP**: 2-bit Re-Reference Prediction Values (RRPV). Lines inserted with distant prediction (`2`), promoted to near (`0`) on hit.
* **3C Miss Telemetry**:
  * **Compulsory (Cold)**: First access to this line address in application lifetime.
  * **Capacity**: Miss occurs even if cache were fully associative of the same size.
  * **Conflict**: Miss caused by set-way collision that a fully-associative cache would have hit.

### 10.1.4 Non-Blocking MSHRs & Hardware Data Prefetchers
* **MSHRs (`MissStatusHoldingRegister`)**: Allows the processor to continue issuing hits and concurrent misses while pending line refills are serviced by L2/L3/DRAM (Memory-Level Parallelism).
* **Hardware Prefetchers (`memory/Prefetcher.hpp`)**:
  * **Next-Line**: Prefetches block $K+1$ upon accessing block $K$.
  * **Stride Prefetcher**: Reference Prediction Table (RPT) with 4-state saturating automaton tracking consistent strides $\Delta = A_i - A_{i-1}$.
  * **Stream Buffer**: FIFO queue prefetching streams of sequential blocks ahead of demand requests.
  * **Page-Boundary Guard**: Prefetch requests are strictly forbidden from crossing 4 KB page boundaries, preventing spurious page faults!

---

## 10.2 Cycle-Accurate DRAM Model (`memory/DRAM.hpp`)

The main memory model simulates command-level DDR4-3200 and DDR5-4800 dynamic random access memory.

### 10.2.1 JEDEC Timing Parameters Modeled
Table 10.2 lists the exact DRAM specifications implemented in `memory/Config.hpp`:

| Parameter | Meaning | DDR4-3200 Value | DDR5-4800 Value |
| :--- | :--- | :---: | :---: |
| **Channels** | Independent 64-bit memory channels | 1 | 2 (Dual 32-bit subchannels) |
| **Ranks** | Independent chip ranks per channel | 1 | 1 |
| **Bank Groups** | Bank groups per rank | 4 | 8 |
| **Banks** | Banks per bank group | 4 (16 banks total) | 4 (32 banks total) |
| **$t_{\text{RCD}}$** | RAS-to-CAS Delay (Row Activate to Column Command) | 14.06 ns (45 CPU cyc) | 13.75 ns (49 CPU cyc) |
| **$t_{\text{RP}}$** | Row Precharge (Close active row) | 14.06 ns (45 CPU cyc) | 13.75 ns (49 CPU cyc) |
| **$t_{\text{CL}}$** | CAS Latency (Read Column to Data Bus) | 14.06 ns (45 CPU cyc) | 13.75 ns (49 CPU cyc) |
| **$t_{\text{RAS}}$** | Row Active Time | 32.00 ns (102 CPU cyc)| 32.00 ns (115 CPU cyc)|
| **$t_{\text{RFC}}$** | Refresh Cycle Time (Stall duration) | 350.00 ns | 295.00 ns |
| **$t_{\text{REFI}}$** | Average Refresh Interval | 7.80 $\mu\text{s}$ | 3.90 $\mu\text{s}$ |

### 10.2.2 Address Mapping Schemes (`memory/AddressMapping.hpp`)
* **Physical Address Slicing**:
  $$\text{Address} \implies \text{Row} \mathbin{\Vert} \text{Bank Group} \mathbin{\Vert} \text{Bank} \mathbin{\Vert} \text{Rank} \mathbin{\Vert} \text{Channel} \mathbin{\Vert} \text{Column} \mathbin{\Vert} \text{ByteOffset}$$
* **XOR Interleaving**: Slices bank group and bank bits using an XOR hash with upper row bits ($\text{Bank} \oplus \text{Row}[17:14]$). This eliminates consecutive stride bank conflicts in matrix algorithms!
* **Memory Schedulers**:
  * **FCFS**: First-Come First-Served.
  * **FR-FCFS (First-Ready First-Come First-Served)**: Prioritizes row-buffer hits over row-buffer conflicts to maximize DRAM bus bandwidth.

---

## 10.3 Virtual Memory & Multi-Level TLB Subsystem (`memory/VirtualMemory.hpp`)

### 10.3.1 4-Level Radix-4 Translation Architecture
* **Address Space**: 48-bit Virtual Addresses mapped to 32-bit / 48-bit Physical Addresses.
* **Radix-4 Hierarchy**:
  * Level 4: PML4 (Page Map Level 4, bits $[47:39]$ $\implies 512$ entries)
  * Level 3: PDPT (Page Directory Pointer Table, bits $[38:30]$ $\implies 512$ entries)
  * Level 2: PD (Page Directory, bits $[29:21]$ $\implies 512$ entries)
  * Level 1: PT (Page Table, bits $[20:12]$ $\implies 512$ entries)
  * Offset: bits $[11:0]$ ($4\text{ KB}$ page size).
* **Huge Pages (2 MB)**: Bypasses Level 1 (PT). Bits $[20:0]$ form a 2 MB direct frame offset.

### 10.3.2 Multi-Level TLB & Page-Walk Cache (PWC)
* **L1 ITLB**: 64 entries, 4-way set-associative. Serves instruction fetch in **1 cycle**.
* **L1 DTLB**: 64 entries, 4-way set-associative. Serves data load/store in **1 cycle**.
* **L2 STLB**: 1024 entries, 8-way set-associative. Serves L1 TLB misses in **8 cycles**.
* **Page-Walk Cache (PWC)**: Caches upper-level radix translations (PML4, PDPT, PD). Reduces 4-level page table walk penalty by **up to 74%**!

### 10.3.3 Demand Paging, SSD Swap, and Protection
* **Minor Faults**: If a page is unmapped, the allocator binds a free physical frame (zero-fill).
* **Major Faults**: When physical frames are exhausted, the frame allocator evicts dirty pages to simulated NVMe SSD swap space (10 $\mu\text{s}$ penalty) using Clock or LRU replacement.
* **Protection & Exceptions**: Enforces Read/Write and User/Kernel permissions. Accessing a stack guard page raises a precise `STACK_OVERFLOW` CPU exception.

---

## 10.4 Effective Access Time (EAT) & AMAT Calculation
The total effective memory latency per memory reference is calculated recursively:
$$\text{EAT} = \text{TLB\_Access\_Time} + P(\text{TLB\_Miss}) \times t_{\text{PageWalk}} + \text{AMAT}_{\text{Cache}}$$

Where:
$$\text{AMAT}_{\text{Cache}} = t_{\text{L1}} + \text{MR}_{\text{L1}} \times \left( t_{\text{L2}} + \text{MR}_{\text{L2}} \times \left( t_{\text{L3}} + \text{MR}_{\text{L3}} \times t_{\text{DRAM}} \right) \right)$$

* In **`ideal`** preset: $\text{EAT} = \mathbf{1.0\text{ cycle}}$ (zero stall cycles).
* In **`modern`** preset (steady-state): L1 hit rate $>99\%$, TLB hit rate $>99.9\% \implies \text{EAT} \approx \mathbf{1.05 - 1.15\text{ cycles}}$.

