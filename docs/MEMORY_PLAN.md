# RISC201-PROCESSOR: Comprehensive Memory-Hierarchy & Virtual-Memory Simulation Subsystem
## Architectural Design Plan & Implementation Roadmap (CSC-201)

---

## 1. Executive Summary & Existing Codebase Analysis

### 1.1 Existing Codebase Overview
The `RISC201-PROCESSOR` project is a cycle-accurate educational processor modeling the SimpleRisc instruction set architecture (Prof. Smruti R. Sarangi, *Computer Organisation and Architecture*). The current codebase implements:
- **4-Stage Pipelined Processor** (`processor_4stage/`): Stages `IF -> OF -> EX -> MA_RW`. Memory Access and Register Writeback are merged into a single `MA_RW` stage.
- **6-Stage Pipelined Processor** (`processor_6stage/`): Stages `IF -> ID -> RF -> EX -> MA -> RW`. Operand fetch is decoupled into Instruction Decode (`ID`) and Register Fetch (`RF`), with a dedicated `MA` (Memory Access) stage prior to `RW` (Register Writeback).
- **Microprogrammed Control Unit** (`microcode/`): Supports both Horizontal (65-bit wide control words) and Vertical (45-bit encoded micro-instructions decoded via PLA/ROM) microprogramming with a shared internal datapath bus.
- **Arithmetic Logic Unit (ALU)** (`alu/`): Gate-level algorithms for adders (Ripple Carry, Carry Select, Carry Lookahead), multipliers (Iterative Shift-Add, Radix-2 Booth, Wallace Tree), and dividers (Restoring and Non-Restoring).
- **Assembler & Disassembler** (`assembler/`): 2-pass assembler with macro preprocessor supporting pseudoinstructions (`push`, `pop`, `pushm`, `popm`), immediate modifiers (`u` unsigned, `h` high 16-bit), and label symbol tables.
- **Interactive CLI Debugger** (`debugger/`): Interactive execution REPL with breakpoints, step cycle, step instruction, register dump, disassembly, memory hex dump, and algorithmic ALU selection.
- **Active Visualizers** (`visualizer/`): ASCII pipeline diagram renderer and stack frame visualizer.
- **Stack Boundary Safety & Exception Framework** (`common/Exception.hpp`, `common/Memory.hpp`): Precise architectural detection of `STACK_OVERFLOW` ($sp < 0x80000$), `STACK_UNDERFLOW` ($sp > 0xFFFFC$), `STACK_CORRUPTION` (writes inside a 64-byte guard band below $0x80000$), `UNALIGNED_MEMORY_ACCESS`, and `OUT_OF_BOUNDS_MEMORY`.
- **Evaluation Suite** (`evaluation/CompareProcessors.cpp`): Automated testing across 6 benchmark programs (`factorial_iter`, `factorial_rec`, `prime_test`, `array_sum`, `hazard_test`, `stack_test`) evaluating 5 execution engines (4-stage with/without forwarding, 6-stage with/without forwarding, microprogrammed MCU) yielding 30 verified passing test cases.

### 1.2 Pipeline Memory Access, Stalls, and Interfaces
1. **Instruction Fetch (IF Stage)**:
   - In both pipelines, `IF` reads a 32-bit machine word from memory at address `pc`: `memory.readWord(pc, &currentException)`.
   - On a branch taken (evaluated in `EX`), the pipeline flushes in-flight instructions:
     - 4-Stage: Injects 2 bubble cycles (`bubbleCycles += 2`), flushes `next_IF_OF` and `next_OF_EX`.
     - 6-Stage: Injects 3 bubble cycles (`bubbleCycles += 3`), flushes `next_IF_ID`, `next_ID_RF`, and `next_RF_EX`.
2. **Data Memory Access (MA Stage)**:
   - 4-Stage (`MA_RW`): Load reads `memory.readWord(aluResult, &currentException)`; store writes `memory.writeWord(aluResult, op2, &currentException)`. Load data is immediately latched and written to the register file in the same phase.
   - 6-Stage (`MA`): Load reads `memory.readWord(aluResult, ...)`; store writes `memory.writeWord(aluResult, storeData, ...)`. Data is held in `Latch_MA_RW` and committed to the register file during stage 6 (`RW`).
3. **Hazard Detection and Stall Interlocks**:
   - Load-Use Hazard: When an instruction in `EX` is a load (`OP_LD`) and the consuming instruction in `OF` (4-stage) or `RF` (6-stage) reads that destination register, the hazard detection unit asserts:
     - `stall_IF = true; stall_OF = true; bubble_EX = true; stallCycles++;` (4-stage)
     - `stall_IF = true; stall_ID = true; stall_RF = true; bubble_EX = true; stallCycles++;` (6-stage)
   - When forwarding is disabled, interlocks stall for *any* RAW data dependency until previous writeback clears `EX`/`MA`.
4. **Current Memory Limitations**:
   - Currently, memory is a flat byte vector with fixed 1-cycle access latency and physical addressing only. No cache lines, no DRAM timing, no virtual memory translation, no TLBs, and no page faults.

---

## 2. Target Modern Architecture Design Principles

The new memory hierarchy subsystem models modern computer architecture with deep parameterization and cycle accuracy:
1. **Transfer Granularity**: Cache line size defaults to 64 bytes (configurable 32/64/128 B). All cache fills transfer full lines.
2. **Burst DRAM Transfers**: Line fill from DDR4 DRAM is one burst (64-bit bus $\times$ BL8 = 64 B). DDR5 uses two independent 32-bit subchannels $\times$ BL16 = 64 B. Burst transfer time ($t_{\text{BURST}}$) is added to every DRAM access.
3. **Critical-Word-First & Early Restart**: Requested 32-bit word is served to the CPU as soon as its chunk arrives, allowing execution to resume early while the rest of the cache line fills in the background.
4. **Non-Blocking Caches (MSHRs)**: Miss Status Holding Registers support hit-under-miss and miss-under-miss with request merging and Memory-Level Parallelism (MLP) tracking.
5. **Hardware Prefetchers**: Next-line, stride (reference prediction table), and stream prefetchers with degree and distance. Prefetches never cross 4 KB page boundaries.
6. **Virtual Memory & Two-Level TLB**:
   - 32-bit and 48-bit virtual address support; 4 KB default page size (huge pages: 2 MB, 4 MB).
   - Multi-level page tables (2-level $10+10+12$; 4-level $9+9+9+9+12$).
   - Separate L1 ITLB and DTLB (4-way, 64 entries) + unified L2 STLB (8-way, 1024 entries).
   - VIPT L1 cache with hardware aliasing/synonym detection ($(\text{CacheSize}/\text{Assoc}) \le \text{PageSize}$).
   - Hardware page table walker whose PTE lookups traverse L2/L3 caches and DRAM.
   - Demand paging with physical frame allocator, swap space on SSD/HDD, and replacement algorithms (FIFO, LRU, Clock, Belady Optimal).
7. **Precise Exception Handling**: Faulting instruction and younger instructions are squashed; older instructions commit; faulting address and PC are recorded; execution can resume or abort.

---

## 3. Subsystem Architecture & Module Breakdown

### 3.1 `memory/config.py`
Unified configuration schema using Python dataclasses, supporting export/import to/from JSON and YAML.
- **Hierarchy Presets**:
  - `ideal`: 0 extra latency, 1-cycle memory, VM disabled (exact legacy behavior).
  - `simple`: L1 (32 KB, 4-cycle) + DRAM (flat 50ns), VM off.
  - `modern`: Split L1I/L1D (32 KB, 8-way, 4-cycle), L2 (512 KB, 8-way, 12-cycle), L3 (8 MB, 16-way, 40-cycle), DDR4-3200 CL22 (channel/rank/bank-group/bank), stream prefetcher, 2-level TLB (ITLB 64, DTLB 64, STLB 1024), 48-bit 4-level page table, demand paging on.
  - `modern-ddr5`: Same as modern but with DDR5-4800 (subchannels, BL16, 8 bank groups).
  - `lecture`: Latency model matching lecture slides (RF 0.3ns, L1 1.1ns, L2 3.3ns, L3 12.8ns, L4 42.4ns, DRAM 62.9ns, SSD 0.1ms, Disk 3ms).
- CPU Frequency: Configurable 1.0 GHz to 4.0 GHz for exact nanosecond $\leftrightarrow$ cycle conversions.

### 3.2 `memory/hierarchy.py`
End-to-end memory hierarchy coordinator:
- Levels: Register File $\to$ TLB $\to$ L1 $\to$ L2 $\to$ L3 $\to$ DRAM $\to$ Swap Storage.
- Recursive latency model:
  $$T_i = t_i + m_i \cdot T_{i+1}$$
  where $m_i$ is the local miss rate at level $i$.
- Average Memory Access Time (AMAT):
  $$\text{AMAT} = \text{hit\_rate} \cdot t_{\text{hit}} + \text{miss\_rate} \cdot t_{\text{miss}}$$
- Effective Access Time with Virtual Memory (EAT):
  $$\text{EAT} = T_{\text{translation}} + T_{\text{cache\_hierarchy}}$$
  $$T_{\text{translation}} = t_{\text{TLB}} + m_{\text{TLB}} \cdot t_{\text{pagewalk}} + p_{\text{fault}} \cdot t_{\text{fault\_service}}$$
- Inclusion policies per cache level: Inclusive (back-invalidations on eviction), Exclusive (swap on hit, allocate only on miss), NINE (Non-Inclusive, Non-Exclusive).

### 3.3 `memory/cache.py`
Generic set-associative cache engine:
- **Geometry**: Configurable capacity, line size (32/64/128 B), associativity (1 to 64, fully associative, non-power-of-2).
- **Replacement Policies**: LRU, FIFO, Random (deterministic seed), Tree-PLRU, Static/Dynamic RRIP (SRRIP).
- **Insertion Policies**: MRU insert, LRU insert, Bimodal Insertion Policy (BIP).
- **Write Policies**: Write-Back + Write-Allocate (default), Write-Through + No-Write-Allocate.
- **Indexing & Tagging**:
  - PIPT (Physical Index, Physical Tag)
  - VIPT (Virtual Index, Physical Tag): Compares $(\text{set\_index\_bits} + \text{offset\_bits})$ against page offset bits. If greater, flags synonym/aliasing condition.
  - VIVT (Virtual Index, Virtual Tag): Requires ASID tag and flush on context switch.
- **Miss Classification (3C Model)**:
  - Compulsory (cold miss): First reference to line.
  - Capacity: Miss would occur even in a fully associative cache of same capacity.
  - Conflict: Miss caused by set contention that would hit in fully associative cache.
- **Non-blocking MSHRs**: Configurable MSHR entry count; tracks primary misses, secondary misses (merging), and Memory-Level Parallelism (MLP).
- **Line Straddling**: Memory requests crossing a 64-byte boundary are decomposed into two independent requests.

### 3.4 `memory/dram.py`
Cycle-accurate DRAM controller and memory subsystem:
- **Hierarchical Geometry**: Channel $\to$ Rank $\to$ Chip $\to$ Bank Group $\to$ Bank $\to$ Row Buffer $\to$ Cell Array.
- **Timing Parameters (DDR4 & DDR5)**:
  - $t_{\text{RCD}}$ (Row to Column Delay), $t_{\text{RP}}$ (Row Precharge), $t_{\text{CL}}$ (CAS Latency), $t_{\text{RAS}}$ (Row Active Time), $t_{\text{RC}} = t_{\text{RAS}} + t_{\text{RP}}$, $t_{\text{WR}}$, $t_{\text{WTR}}$, $t_{\text{RTP}}$, $t_{\text{FAW}}$ (Four-Activate Window).
  - Bank group timings: $t_{\text{RRD\_S}}$ vs $t_{\text{RRD\_L}}$, $t_{\text{CCD\_S}}$ vs $t_{\text{CCD\_L}}$.
  - Burst time: $t_{\text{BURST}} = \text{BL} / 2$ (DDR).
- **Row-Buffer Policies**:
  - Open-page: Row Hit $= t_{\text{CL}} + t_{\text{BURST}}$; Row Conflict $= t_{\text{RP}} + t_{\text{RCD}} + t_{\text{CL}} + t_{\text{BURST}}$.
  - Closed-page: Precharge automatically scheduled after access; Row Empty $= t_{\text{RCD}} + t_{\text{CL}} + t_{\text{BURST}}$.
- **Refresh Scheduling**: Periodic refresh bursts every $t_{\text{REFI}}$ (7.8 $\mu$s) taking $t_{\text{RFC}}$ cycles. Performance loss percentage tracked.
- **Memory Schedulers**: First-Come First-Served (FCFS) and First-Ready First-Come-First-Served (FR-FCFS) row-hit prioritizing scheduler with read/write queues and write draining.
- **Bandwidth Metrics**: Peak theoretical bandwidth, effective bandwidth, bus utilization percentage.

### 3.5 `memory/mapping.py`
Address translation to physical DRAM coordinates:
- Configurable interleaving:
  - `row:bank:bank_group:column:channel:byte`
  - `row:column:bank:channel`
  - Hash / XOR-based bank interleaving: $\text{bank} = \text{bits} \oplus (\text{row} \gg k)$ to break power-of-2 stride bank conflicts.
- Bank conflict detection, serialization, and Bank-Level Parallelism (BLP) tracking.

### 3.6 `memory/prefetch.py`
Pluggable hardware data prefetchers:
- **Next-Line Prefetcher**: Prefetches line $k+1$ on reference to line $k$.
- **Stride Prefetcher**: Reference Prediction Table (RPT) keyed on program counter (PC) to detect fixed positive/negative strides.
- **Stream Prefetcher**: History table tracking sequential access direction with configurable prefetch degree and lookahead distance.
- Prefetch filter ensuring prefetch requests never cross 4 KB page boundaries.
- Evaluation metrics: Coverage, Accuracy, Timeliness, Pollution, Bandwidth overhead.

### 3.7 `memory/vm.py`
Virtual Memory Subsystem:
- **Address Space**: 32-bit and 48-bit Virtual Addresses, 32-bit Physical Address; configurable page size (4 KB, 8 KB, 16 KB, 2 MB huge page).
- **Page Table Structures**: Single-level (flat for small spaces), 2-level ($10+10+12$ for 32-bit), 4-level radix tree ($9+9+9+9+12$ PML4/PDP/PD/PT for 48-bit).
- **PTE Format**: PFN, Present/Valid, Read, Write, Execute, User/Supervisor, Accessed, Dirty, SwapSlot.
- **Virtual Memory Loader**: Eager and lazy segment mapping for Code (R-X), Data (RW-), Heap (RW-), Stack (RW-), and Stack Guard Page (no permissions).
- **Two-Level TLB Hierarchy**:
  - L1 ITLB & DTLB: 64 entries, 4-way set associative, 1-cycle hit latency.
  - L2 Unified STLB: 1024 entries, 8-way set associative, 7-cycle hit latency.
  - VIPT parallel lookup: L1 cache indexing occurs concurrently with TLB lookup.
  - TLB Invalidation: Complete flush, flush by ASID, flush single page (INVLPG).
- **Hardware Page-Table Walker**: On TLB miss, walks radix tree. Each level's PTE read accesses L2/L3 caches and DRAM. Page-Walk Cache (PWC) caches intermediate directory levels to skip steps.
- **Demand Paging & Page Replacement**:
  - Frame Allocator with small configurable physical memory (e.g. 64 KB - 1 MB) to trigger page replacement in tests and demos.
  - Replacement Algorithms: FIFO, LRU, Clock (second chance), Optimal (Belady's offline algorithm).
  - Belady's Anomaly reproduction demo with FIFO.
  - Swap space modeling on SSD (~0.1 ms) and HDD (~3 ms).
- **Integration with RISC201 Stack Safety**:
  - Maps stack overflow and stack guard band writes to precise VM Page Faults with faulting address and PC.
  - Precise exception handling: squashes pipeline, saves architectural context, allows resumption or termination.

---

## 4. Pipeline Integration & Simulation Architecture

To provide full cycle-accurate simulation across both 4-stage and 6-stage pipelines:
- We provide a native Python execution engine (`risc201/` CPU simulator) implementing the complete SimpleRisc ISA, register file, ALU, pipeline latches, forwarding unit, hazard detection, branch flushes, and microcoded controller.
- The pipeline `IF` and `MA` stages interface with `memory.hierarchy.MemoryHierarchy`:
  - `IF`: Virtual PC $\to$ ITLB $\to$ L1I cache $\to$ DRAM.
  - `MA`: Virtual Address $\to$ DTLB $\to$ L1D cache $\to$ DRAM.
- Multi-cycle stalls freeze earlier pipeline stages and inject bubbles into subsequent stages for the exact cycle count returned by the memory hierarchy.
- Microcode controller wait states stall `uPC` on memory load/store operations.
- Comparison mode reports CPI and stall breakdowns: Translation Stalls vs Cache/DRAM Stalls vs RAW Hazard Stalls vs Branch Bubbles.

---

## 5. Interactive CLI, Debugger & Visualizer

- **CLI Driver** (`risc201_sim.py` and `sim.py`):
  Supports all requested arguments:
  `--mem-preset`, `--mem-config`, `--l1-size`, `--line-size`, `--assoc`, `--replacement`, `--write-policy`, `--prefetcher`, `--mshrs`, `--dram-type`, `--tRCD`, `--tRP`, `--tCL`, `--channels`, `--banks`, `--bank-groups`, `--addr-map`, `--page-policy`, `--sched`, `--pipeline`, `--vm`, `--page-size`, `--pt-levels`, `--va-bits`, `--pa-bits`, `--phys-mem-size`, `--itlb-entries`, `--dtlb-entries`, `--stlb-entries`, `--tlb-assoc`, `--tlb-replacement`, `--pwc`, `--asid`, `--page-replacement`, `--demand-paging`, `--cache-index`.
- **Interactive Debugger Commands**:
  - Existing: `s`, `si`, `r`, `c`, `b`, `db`, `regs`, `p`, `stack`, `micro`, `mode`, `stats`, `mem`, `disasm`, `set_n`, `help`, `q`.
  - Memory & Cache: `cache`, `cache stats`, `mshr`, `dram`, `dram timing`, `banks`, `addrmap <addr>`, `prefetch`, `mem trace`, `amat`, `locality`, `refresh`.
  - Virtual Memory: `tlb`, `tlb stats`, `translate <vaddr>`, `pagetable`, `frames`, `fault log`, `vmstat`, `flush tlb [asid|page]`, `switch <pid>`.
- **ASCII Visualizer Suite**:
  - Cache set & way dump with V/D/Tag and LRU counters.
  - DRAM bank grid with open/closed rows highlighted.
  - Memory hierarchy pyramid with live hit/miss counts and AMAT.
  - Pipeline diagram annotated with stall reasons (`[TLB-MISS]`, `[CACHE-MISS]`, `[PAGE-WALK]`).
  - Active MSHR occupancy timeline.
  - Virtual address space segment map and physical frame allocation table.

---

## 6. Experimental Campaign (20 Experiments)

All 20 experiments will be implemented as modular scripts in `experiments/` producing Markdown reports and CSV data:
1. `exp01_cache_geometry.py`: Cache size (4K-64K), line size (16-128B), associativity (1-8, fully), replacement policies (LRU, FIFO, Random, PLRU, SRRIP).
2. `exp02_conflict_miss.py`: A,B,A,B thrashing pattern in direct-mapped vs 2-way associative cache.
3. `exp03_page_policy.py`: Open-page vs Closed-page under sequential, strided, and random workloads.
4. `exp04_dram_timing_sensitivity.py`: Independent sensitivity sweep of $t_{\text{RCD}}$, $t_{\text{RP}}$, and $t_{\text{CL}}$ on AMAT and CPI.
5. `exp05_banking_and_mapping.py`: Bank count (1-16), bank groups, channels, power-of-2 stride conflict, and XOR hashing fix.
6. `exp06_prefetcher_eval.py`: Next-line vs Stride vs Stream on sequential, strided, and pointer chasing workloads.
7. `exp07_mshr_mlp.py`: MSHR depth sweep (1 to 16) and resulting MLP and CPI impact.
8. `exp08_refresh_overhead.py`: Refresh penalty vs retention window (32ms, 64ms) and bank/row count.
9. `exp09_pipeline_cpi_comparison.py`: 4-stage vs 6-stage CPI across 4 benchmarks under `ideal`, `simple`, and `modern` presets.
10. `exp10_dram_schedulers.py`: FCFS vs FR-FCFS scheduling performance under heavy mixed traffic.
11. `exp11_ddr4_vs_ddr5.py`: DDR4-3200 vs DDR5-4800 bandwidth, latency, and subchannel comparison.
12. `exp12_tlb_reach.py`: TLB size sweep (8 to 2048 entries), associativity, and hit rate vs working set size.
13. `exp13_page_size.py`: 4 KB vs 2 MB huge pages (TLB miss reduction vs internal fragmentation).
14. `exp14_page_table_depth.py`: 1-level vs 2-level vs 4-level page table walk overhead with and without Page-Walk Cache (PWC).
15. `exp15_page_walk_caching.py`: Page-walk latency when PTEs hit in L1/L2/L3 vs fetch from DRAM.
16. `exp16_page_replacement.py`: FIFO, LRU, Clock, Belady Optimal across frame counts, demonstrating Belady's Anomaly for FIFO.
17. `exp17_page_fault_costs.py`: Minor vs major page fault latency impact and thrashing behavior.
18. `exp18_vipt_aliasing.py`: VIPT synonym aliasing demo when cache size / associativity > page size.
19. `exp19_asid_context_switch.py`: Context switching penalty with and without ASID-tagged TLB entries.
20. `exp20_matrix_traversal.py`: Row-major vs column-major matrix traversal and cache-blocked multiplication on cache + TLB misses.

---

## 7. Complete List of Files to Touch / Create

### Existing Files Maintained Without Breaking
- `alu/*` (ALU, Adders, Multipliers, Dividers, LogicGates, Shifter) - intact.
- `assembler/*` (Assembler, Disassembler, MacroPreprocessor, main_as, main_disasm) - intact.
- `common/*` (Types, RegisterFile, Memory, Exception) - intact; C++ test suite passes.
- `debugger/*` (Debugger, main_sim) - intact; C++ test suite passes.
- `microcode/*` (MicroController, HorizontalControl, VerticalControl, MicroInstructions) - intact.
- `processor_4stage/*` (Pipeline4Stage, StageRegisters4) - intact.
- `processor_6stage/*` (Pipeline6Stage, StageRegisters6) - intact.
- `visualizer/*` (PipelineVisualizer, StackVisualizer) - intact.
- `evaluation/CompareProcessors.cpp` - intact; 30/30 tests pass.
- `Makefile` - extended with `test-mem`, `test-vm`, `exp`, `demo-mem`, `demo-vm`.

### New Modules to Create
- `memory/__init__.py`: Package export.
- `memory/config.py`: Dataclass and YAML/JSON configuration with presets.
- `memory/hierarchy.py`: Multi-level hierarchy coordinator, recursive latency, AMAT/EAT models.
- `memory/cache.py`: Set-associative cache, MSHRs, 3C classification, VIPT aliasing check.
- `memory/dram.py`: Cycle-accurate DDR4/DDR5 DRAM model, bank groups, FR-FCFS scheduler.
- `memory/mapping.py`: Physical address mapping and XOR hashing.
- `memory/prefetch.py`: Next-line, stride, and stream prefetchers.
- `memory/vm.py`: Radix page tables, TLBs, page walker, PWC, frame allocator, demand paging.
- `cpu/__init__.py`: Python RISC201 CPU simulator package.
- `cpu/isa.py`: Opcode definitions, formats, immediate modifiers, and instruction decoding.
- `cpu/assembler.py`: Python assembler and macro preprocessor compatible with SimpleRisc.
- `cpu/regfile.py`: Register file with flags and privileged registers.
- `cpu/alu.py`: Full ALU simulation with flags and exception reporting.
- `cpu/pipeline4.py`: Cycle-accurate 4-stage pipeline integrated with memory hierarchy.
- `cpu/pipeline6.py`: Cycle-accurate 6-stage pipeline integrated with memory hierarchy.
- `cpu/microcode.py`: Microprogrammed controller integrated with memory hierarchy.
- `cpu/exceptions.py`: Precise CPU exception records and stack boundary checker.
- `sim.py` / `risc201_sim.py`: Complete CLI driver, interactive debugger REPL, and visualizer.
- `tests/test_mem_cache.py`: Unit tests for cache indexing, replacement, writeback, 3C, MSHR.
- `tests/test_mem_dram.py`: Unit tests for DRAM timings, bank groups, refresh, FR-FCFS.
- `tests/test_mem_mapping.py`: Unit tests for address interleaving and XOR hashing.
- `tests/test_mem_prefetch.py`: Unit tests for prefetchers, boundary checks, and accuracy.
- `tests/test_vm_translation.py`: Unit tests for page tables, walks, TLB hits/misses, ASID.
- `tests/test_vm_paging.py`: Unit tests for frame allocator, page replacement, Belady anomaly.
- `tests/test_pipeline_mem.py`: Integration tests for 4-stage and 6-stage with memory stalls and faults.
- `experiments/run_all.py` and `experiments/exp01_*.py` through `experiments/exp20_*.py`: 20 experiment scripts.
- `docs/MEMORY_PLAN.md`: This comprehensive architecture plan.
- `docs/MEMORY_DESIGN.md`: Deep design formulas, derivations, and analysis.
- `docs/VIRTUAL_MEMORY.md`: Detailed translation, page tables, PTE formats, and fault handling guide.
- `README.md`: Updated architecture diagrams, quickstart, CLI options, and results.
