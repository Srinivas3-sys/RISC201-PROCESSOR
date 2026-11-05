# Part H: The Memory Subsystem (Caches, DRAM, Virtual Memory)

This section provides a complete, zero-knowledge explanation of the memory hierarchy, caching mechanics (tags, sets, lines, MSHRs), DRAM architecture (rows, columns, banks, timings), 4-level Radix-4 Virtual Memory, TLBs, and memory presets.

---

# Chapter H1: Why Memory Is Slow & The Locality Principle

### 1. In One Sentence
CPUs compute in fractions of a nanosecond, but retrieving data from physical DRAM takes dozens of nanoseconds (the "Memory Wall"); the memory hierarchy bridges this speed gap by keeping frequently used data in small, ultra-fast on-chip caches.

### 2. Start from Zero: Everyday Analogy
Imagine studying for final exams at university:
- **The CPU Registers** are your short-term memory (instant, but holds only 5-7 thoughts).
- **The L1 Cache** is your desk surface (you reach for your open notebook in 1 second).
- **The L2/L3 Cache** is the bookshelf in your dorm room (takes 10-30 seconds to walk over and grab a textbook).
- **The Physical DRAM** is the campus university library (takes a 15-minute walk across campus).
- **The SSD Swap Space** is ordering an out-of-print book via inter-library loan from another country (takes 3 days!).
- If you had to walk to the university library for *every single word* you wanted to read, you would never finish homework! Instead, you carry a backpack of books to your dorm desk and read there.

#### Course Slide Analogy: The Music Playlist
- **Temporal Locality (Locality in Time)**: When you find a favorite song, you hit "Repeat" and listen to that same song again and again. If an item was referenced recently, it will likely be referenced again soon.
- **Spatial Locality (Locality in Space)**: When you finish Track 3 on an album, you usually listen to Track 4 next. If an item was referenced, items at adjacent memory addresses will likely be referenced soon.

---

### 3. The Classic Example: Array Traversal
```cpp
int sum = 0;
for (int i = 0; i < 1000; i++) {
    sum += arr[i];
}
```
- **`sum`** exhibits **Temporal Locality**: The variable `sum` is read and updated 1,000 times consecutively in the exact same memory/register location!
- **`arr[i]`** exhibits **Spatial Locality**: Each loop iteration accesses `arr[0]`, then `arr[1]`, then `arr[2]`. When `arr[0]` is fetched from DRAM, the hardware cache brings in an entire 64-byte block (holding 16 integers). The next 15 loop iterations hit in the fast L1 cache with **0 DRAM trips**!

---

# Chapter H2: Caches from Zero (Lines, Sets, Tags, and MSHRs)

### 1. In One Sentence
A cache is a small, hardware-managed SRAM array that stores recently accessed 64-byte blocks of memory, indexed by bit-slicing physical memory addresses.

---

### 2. How an Address is Split into Tag, Index, and Offset

Every physical address is decomposed into three bit fields:

```
Physical Address (32 bits):
 31                                       11 10                  6 5                 0
+-------------------------------------------+---------------------+-------------------+
|                   TAG                     |        INDEX        |    BYTE OFFSET    |
|                 (21 bits)                 |       (5 bits)      |     (6 bits)      |
+-------------------------------------------+---------------------+-------------------+
```

1. **Byte Offset (Bits [5:0] = 6 bits)**:
   - Identifies which exact byte inside the 64-byte cache line is requested ($2^6 = 64\text{ bytes}$).
2. **Set Index (Bits [10:6] = 5 bits)**:
   - Selects which row (set) in the cache array to check ($2^5 = 32\text{ sets}$).
3. **Tag (Bits [31:11] = 21 bits)**:
   - The unique identifier stored in cache directory memory to verify if the cached block belongs to this exact memory address.

---

### 3. Cache Organization: Direct-Mapped vs Set-Associative

```
8-Way Set-Associative Cache:
Set Index ---> Set [ 5 ]: [ Way 0 ][ Way 1 ][ Way 2 ][ Way 3 ][ Way 4 ][ Way 5 ][ Way 6 ][ Way 7 ]
                            |        |        |        |        |        |        |        |
                            v        v        v        v        v        v        v        v
                         Compare all 8 Tags in parallel against Address Tag -> 1 Hit!
```
- **Direct-Mapped**: Each memory block can go into exactly ONE cache slot ($1\text{-way}$). High conflict misses!
- **Fully-Associative**: A memory block can go into ANY slot in the cache. Complex, expensive comparator hardware.
- **$N$-Way Set-Associative (Modern Standard)**: Each set holds $N$ slots (ways). RISC201 L1 caches are **8-way associative**, virtually eliminating conflict misses while keeping access latency to **1 cycle**.

---

### 4. The 3C Classification of Misses
1. **Compulsory (Cold) Miss**: The very first time a block is accessed. Unavoidable unless prefetched.
2. **Capacity Miss**: The cache was full, so the block was evicted earlier and now had to be re-fetched.
3. **Conflict Miss**: Multiple memory addresses mapped to the *exact same set*, evicting each other even though other sets were empty.

---

### 5. Advanced Cache Features in RISC201 (`memory/Cache.hpp`)
- **Non-Blocking MSHR (Miss Status Holding Register)**:
  - In old processors, a cache miss froze the entire cache.
  - RISC201 uses non-blocking caches with MSHRs: while a miss is fetching from DRAM, subsequent instructions that hit in L1 continue executing without stalling!
- **Stream Prefetcher**:
  - Monitors sequential memory access patterns and proactively streams consecutive cache lines from DRAM before the CPU even requests them, converting compulsory misses into 1-cycle hits!

---

# Chapter H3: Average Memory Access Time (AMAT)

### 1. In One Sentence
AMAT is the average number of clock cycles the CPU spends on a memory access across the entire multi-level cache hierarchy.

### 2. The Formula

$$\text{AMAT} = \text{Hit Time}_{L1} + (\text{Miss Rate}_{L1} \times \text{Miss Penalty}_{L1})$$

In a multi-level hierarchy (L1, L2, L3, DRAM):
$$\text{Miss Penalty}_{L1} = \text{Hit Time}_{L2} + (\text{Miss Rate}_{L2} \times \text{Miss Penalty}_{L2})$$
$$\text{Miss Penalty}_{L2} = \text{Hit Time}_{L3} + (\text{Miss Rate}_{L3} \times \text{DRAM Latency})$$

#### Real Numbers from RISC201 `modern` Preset:
- L1 Hit Time: 1 cycle (Miss rate: $4\%$)
- L2 Hit Time: 12 cycles (Miss rate: $15\%$)
- L3 Hit Time: 38 cycles (Miss rate: $20\%$)
- DRAM Latency: 120 cycles

$$\text{AMAT} = 1 + 0.04 \times (12 + 0.15 \times (38 + 0.20 \times 120)) = \mathbf{1.85\text{ cycles}}$$
> **Key Takeaway**: Even though physical DRAM takes 120 cycles, the memory hierarchy delivers an effective access time of less than **2 cycles**!

---

# Chapter H4: DRAM Controller & Geometry

### 1. In One Sentence
Physical DRAM is an array of capacitive memory cells organized into channels, ranks, bank groups, and rows, managed by a cycle-accurate memory controller that prioritizes row buffer hits.

---

### 2. DRAM Architecture & Access Steps (`memory/DRAM.hpp`)

```
+--------------------------------------------------------------------------+
|                        DDR4-3200 DRAM CHANNEL                            |
+--------------------------------------------------------------------------+
|  Bank Group 0        Bank Group 1        Bank Group 2        Bank Group 3|
|  [Bank 0][Bank 1]    [Bank 4][Bank 5]    [Bank 8][Bank 9]    [Bank 12].. |
|      |                                                                   |
|      v                                                                   |
|  [ ROW BUFFER (Cached 8 KB Row) ]  <=== Row Hit: Read in tCL = 14 ns!    |
|      |                                                                   |
|  [ Array of 65,536 Rows ]          <=== Row Conflict: Precharge + Active|
+--------------------------------------------------------------------------+
```

1. **Row Hit**: The requested address is already open in the Bank's Row Buffer.
   - Action: Issue `READ` column command.
   - Latency: $t_{CL} = 14\text{ ns}$ (~15 CPU cycles). Fast!
2. **Row Empty (Closed)**: The bank is idle.
   - Action: Issue `ACTIVATE` command, wait $t_{RCD}$, then issue `READ`.
   - Latency: $t_{RCD} + t_{CL} \approx 28\text{ ns}$ (~40 CPU cycles).
3. **Row Conflict**: A *different* row is currently open in that bank.
   - Action: Issue `PRECHARGE` (write back open row, $t_{RP}$), issue `ACTIVATE` (open new row, $t_{RCD}$), then issue `READ` ($t_{CL}$).
   - Latency: $t_{RP} + t_{RCD} + t_{CL} \approx 42\text{ ns}$ (~80-120 CPU cycles). Worst case!

#### FR-FCFS Memory Scheduler:
The memory controller uses **First-Ready First-Come First-Served** scheduling: it scans the request queue and serves requests that hit in open row buffers first, boosting memory bandwidth by up to **$2.5\times$**.

---

# Chapter H5: 4-Level Radix-4 Virtual Memory & TLBs

### 1. In One Sentence
The Virtual Memory system maps 48-bit virtual addresses to physical DRAM frames via a 4-level radix page table (PML4, PDPT, PD, PT), accelerated by L1/L2 TLBs and a Page-Walk Cache (PWC).

---

### 2. The 48-Bit Virtual Address Split

```
48-bit Virtual Address:
+---------------+---------------+---------------+---------------+--------------------+
| PML4 (9 bits) | PDPT (9 bits) |  PD (9 bits)  |  PT (9 bits)  | Page Offset (12b)  |
+---------------+---------------+---------------+---------------+--------------------+
```
- Each table level holds 512 entries ($2^9$).
- Each Page Table Entry (PTE) is 64 bits (8 bytes).
- $512 \times 8\text{ bytes} = 4096\text{ bytes} = 4\text{ KB}$ (each table fits exactly into one physical memory page!).

---

### 3. The Page-Walk Cache (PWC)
- Walking all 4 levels in DRAM on a TLB miss takes 4 separate memory trips (~120 cycles).
- The **Page-Walk Cache** caches the upper PML4 and PDPT translation pointers.
- On a PWC hit, the MMU skips levels 4 and 3, dropping page walk overhead from 120 cycles to **32 cycles (a 74% reduction!)**.

---

# Chapter H6: Memory Hierarchy Presets in RISC201

The simulator includes 5 built-in presets (`--mem-preset`):

| Preset | CPU Freq | VM / TLB | L1 Caches | L2 / L3 Caches | DRAM Controller | Best For |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **`ideal`** | 3.2 GHz | Disabled | None | None | Instant (1 cycle) | Baseline CPU core debugging |
| **`simple`** | 3.2 GHz | Disabled | 32 KB, 8-way | None | DDR4-3200 (1 channel) | Single-level cache studies |
| **`modern`** | 3.2 GHz | 48b Radix-4, TLBs, PWC | 32 KB I/D (1-cyc) | 512 KB / 8 MB | DDR4-3200 (FR-FCFS) | **Realistic contemporary PC** |
| **`modern-ddr5`**| 3.6 GHz | 48b Radix-4, TLBs, PWC | 32 KB I/D (1-cyc) | 1 MB / 16 MB | DDR5-4800 (Dual subch) | Next-gen server evaluation |
| **`lecture`** | 3.2 GHz | 32b 2-level | 64 B direct | None | SDR SDRAM (133 MHz) | Educational textbook exercises |

---

### 4. Summary: If You Only Remember 5 Things
1. **Locality of Reference** (Temporal & Spatial) makes caches work.
2. Physical addresses are split into **Tag, Set Index, and Byte Offset**.
3. **AMAT** measures average memory latency; caches drop access time from 120 cycles to **< 2 cycles**.
4. **DRAM Row Conflicts** take $3\times$ longer than Row Hits ($t_{RP} + t_{RCD} + t_{CL}$).
5. **Radix-4 VM** uses 4 levels (PML4, PDPT, PD, PT); the **PWC** cuts walk latency by **74%**.
