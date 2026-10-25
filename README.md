# RISC201-PROCESSOR: Cycle-Accurate SimpleRisc Simulation Subsystem & Modern Memory Hierarchy

**Course**: CSC-201 Computer Organization & Architecture  
**Branch**: `memory-hierarchy`  

A cycle-accurate, deeply parameterized modern computer systems simulator extending the SimpleRisc architecture. Models multi-level virtual memory, hardware TLBs, radix tree page tables, set-associative cache hierarchies, hardware data prefetchers, non-blocking MSHRs, and DDR4/DDR5 DRAM subsystems with command-level timing.

---

## 1. System Architecture Diagram

```
+-------------------------------------------------------------------------------------------------------+
|                                    RISC201 PIPELINED CPU CORE                                         |
|                                                                                                       |
|     +-------------------------+                                 +---------------------------------+   |
|     |  Instruction Fetch (IF) |                                 |       Memory Access (MA)        |   |
|     +------------+------------+                                 +----------------+----------------+   |
+------------------|---------------------------------------------------------------|--------------------+
                   | Virtual PC                                                    | Virtual Address (VA)
                   v                                                               v
          +-----------------+                                             +-----------------+
          |     L1 ITLB     |                                             |     L1 DTLB     |
          | (64-entry, 4-way)|                                             | (64-entry, 4-way)|
          +--------+--------+                                             +--------+--------+
                   | Miss                                                          | Miss
                   +------------------------------+--------------------------------+
                                                  |
                                                  v
                                        +--------------------+
                                        |      L2 STLB       |
                                        | (1024-entry, 8-way)|
                                        +---------+----------+
                                                  | Miss
                                                  v
                                        +--------------------+
                                        |   Page-Walk Cache  |
                                        |      (PWC)         |
                                        +---------+----------+
                                                  | Miss / Page Walk
                                                  v
                                        +--------------------+
                                        |  Physical Frames / |
                                        |  Swap Space (SSD)  |
                                        +--------------------+
                   +------------------------------+
                   | Physical Address (PA)
                   v
          +-----------------+                                             +-----------------+
          |    L1I Cache    |                                             |    L1D Cache    |
          | (32 KB, 8-way)  |                                             | (32 KB, 8-way)  |
          +--------+--------+                                             +--------+--------+
                   | Line Miss                                                     | Line Miss
                   +------------------------------+--------------------------------+
                                                  |
                                                  v
                                        +--------------------+
                                        |      L2 Cache      |
                                        |  (512 KB, 8-way)   |
                                        +---------+----------+
                                                  | Line Miss
                                                  v
                                        +--------------------+
                                        |      L3 Cache      |
                                        |  (8192 KB, 16-way) |
                                        +---------+----------+
                                                  | Line Miss (64 Bytes)
                                                  v
                                        +--------------------+
                                        |  Memory Controller |
                                        | (FR-FCFS Scheduler)|
                                        +---------+----------+
                                                  |
             +-------------------+----------------+-------------------+--------------------+
             |                   |                                    |                    |
             v                   v                                    v                    v
       Channel 0 (64b)     Channel 1 (64b)                      Bank Group 0         Bank Group 1
       Rank 0 (Chips)      Rank 0 (Chips)                       Row Buffer (Hits)    Row Buffer (Conflicts)
```

---

## 2. Key Features

- **Virtual Memory & Multi-Level Translation (`memory/vm.py`)**:
  - Configurable 32-bit and 48-bit address spaces (4-level x86-64 radix tree).
  - 4 KB standard and 2 MB huge page support.
  - Separate L1 ITLB, L1 DTLB, and unified L2 STLB.
  - Page-Walk Cache (PWC) cutting walk latency by up to 74%.
  - Demand paging with frame allocator and page replacement (FIFO, LRU, Clock, Belady Optimal).
  - Minor faults (zero-fill free frames) and major faults (NVMe SSD swap backing).
  - Precise page fault exceptions and stack guard page boundary enforcement.
- **Cache Subsystem (`memory/cache.py`)**:
  - Arbitrary capacity, line sizes (32, 64, 128 B), and associativity ($N$-way, direct-mapped, fully-associative).
  - Multiple replacement policies: LRU, FIFO, Random, Tree-PLRU, SRRIP.
  - Write-Back + Write-Allocate and Write-Through + No-Write-Allocate.
  - Indexing: PIPT, VIPT (with automatic page-offset aliasing check), and VIVT.
  - 3C Miss classification: Compulsory, Capacity, Conflict.
  - Non-blocking MSHRs with request merging and memory-level parallelism (MLP) tracking.
- **Hardware Data Prefetchers (`memory/prefetch.py`)**:
  - Next-line, Stride (Reference Prediction Table with 4-state automaton), and Stream prefetchers.
  - Page boundary guard preventing prefetches across 4 KB boundaries.
- **Cycle-Accurate DRAM Model (`memory/dram.py`, `memory/mapping.py`)**:
  - Channel $\rightarrow$ Rank $\rightarrow$ Chip $\rightarrow$ Bank Group $\rightarrow$ Bank $\rightarrow$ Row Buffer.
  - Timings: $t_{\text{RCD}}, t_{\text{RP}}, t_{\text{CL}}, t_{\text{RAS}}, t_{\text{RC}}, t_{\text{WR}}, t_{\text{FAW}}, t_{\text{RRD\_S/L}}, t_{\text{CCD\_S/L}}$.
  - Open-page and closed-page row-buffer management policies.
  - Periodic refresh stalls ($t_{\text{RFC}}$ injected every $t_{\text{REFI}}$).
  - FCFS and FR-FCFS (row-hit-first) memory controller request schedulers.
  - Bit-field address mapping and XOR-hash bank interleaving.
- **Dual Pipeline Integration (`cpu/pipeline4.py`, `cpu/pipeline6.py`)**:
  - Direct integration into 4-stage (IF $\rightarrow$ OF $\rightarrow$ EX $\rightarrow$ MA_RW) and 6-stage (IF $\rightarrow$ ID $\rightarrow$ RF $\rightarrow$ EX $\rightarrow$ MA $\rightarrow$ RW) pipelines.
  - Precise multi-cycle bubble insertion for IF and MA cache/DRAM misses.

---

## 3. Presets & Configuration Guide

Configurations can be passed via command line flags or selected via `--mem-preset`:

| Preset | CPU Freq | VM / TLB | L1 Caches | L2 Cache | L3 Cache | DRAM Subsystem | Prefetcher |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`ideal`** | 3.2 GHz | Disabled (1-cyc) | None | None | None | Instant (1 cycle) | None |
| **`simple`** | 3.2 GHz | Disabled (Direct PA) | 32 KB, 8-way, 4-cyc | None | None | DDR4-3200 (1 channel) | None |
| **`modern`** | 3.2 GHz | 48b Radix-4, L1/L2 TLB, PWC | 32 KB I/D, 8-way, 4-cyc | 512 KB, 8-way, 12-cyc | 8 MB, 16-way, 38-cyc | DDR4-3200 (1 channel) | Stream |
| **`modern-ddr5`**| 3.6 GHz | 48b Radix-4, L1/L2 TLB, PWC | 32 KB I/D, 8-way, 4-cyc | 1 MB, 8-way, 12-cyc | 16 MB, 16-way, 36-cyc | DDR5-4800 (2 subchan) | Stream |
| **`lecture`** | 3.2 GHz | 32b 2-Level | 64 B direct (2/3/3 split)| None | None | SDR SDRAM (133 MHz) | None |

---

## 4. Quick Start & CLI Usage

### Running the Python Cycle-Accurate Simulator
```bash
# Run a SimpleRisc assembly benchmark with modern memory hierarchy
./sim.py benchmarks/factorial_iter.s --mem-preset modern

# Run with 6-stage pipeline and custom cache geometry
./sim.py benchmarks/prime_test.s --pipeline 6 --l1-size 65536 --assoc 4 --replacement plru

# Run in interactive debugger REPL mode
./sim.py benchmarks/factorial_iter.s --interactive
```

### Interactive REPL Commands
When launched with `--interactive`, the simulator provides an architectural debugging console:
- `step [N]` / `s [N]`: Advance pipeline by $N$ clock cycles.
- `run` / `r`: Run to program completion.
- `cache`: Display ASCII set-way grid and line status for L1I, L1D, L2, and L3.
- `dram`: Display DRAM channel, bank group, and bank buffer states.
- `tlb`: View ITLB, DTLB, and STLB cached virtual-to-physical translations.
- `pagetable [va]`: Perform a walk of the radix page table for a virtual address.
- `amat`: Show recursive AMAT and Effective Access Time (EAT) breakdown.
- `stats`: Print comprehensive pipeline and memory hierarchy telemetry.

---

## 5. Build & Makefile Targets

The Makefile supports both the original C++ processor evaluation suite and the new Python memory hierarchy subsystem:

```bash
# Compile and run legacy C++ simulator and all 30 benchmarks
make all
./risc201_eval

# Run Memory Subsystem Unit Tests (pytest)
make test-mem

# Run Virtual Memory & TLB Unit Tests (pytest)
make test-vm

# Run All Python Unit Tests (35 tests)
make test-all

# Execute All 20 Architectural Experiments (generates CSVs and Markdown report)
make exp

# Run Memory Hierarchy Demo
make demo-mem

# Run Virtual Memory 6-Stage Demo
make demo-vm
```

---

## 6. Experimental Suite Summary (20 Experiments)

All experiments run via `make exp` and save detailed CSV outputs in `experiments/results/`:

1. **Exp 01 (`exp01_cache_sweeps.csv`)**: Cache size, line size, associativity, and replacement sweeps.
2. **Exp 02 (`exp02_conflict_miss.csv`)**: Direct-mapped conflict miss thrashing ($A, B, A, B$) vs 2-way associativity.
3. **Exp 03 (`exp03_page_policy.csv`)**: DRAM row-buffer management: Open-page vs Closed-page.
4. **Exp 04 (`exp04_dram_timing_sensitivity.csv`)**: Independent sensitivity sweeps of $t_{\text{RCD}}, t_{\text{RP}}, t_{\text{CL}}$.
5. **Exp 05 (`exp05_banking_and_mapping.csv`)**: Banking, channels, and XOR-hash bank conflict elimination.
6. **Exp 06 (`exp06_prefetcher_eval.csv`)**: Next-line, stride, and stream prefetchers across access patterns.
7. **Exp 07 (`exp07_mshr_mlp.csv`)**: MSHR depth (1 to 16) and resulting memory-level parallelism.
8. **Exp 08 (`exp08_refresh_overhead.csv`)**: Periodic DRAM refresh overhead vs retention window.
9. **Exp 09 (`exp09_pipeline_cpi_comparison.csv`)**: 4-Stage vs 6-Stage CPI across Ideal, Simple, and Modern presets.
10. **Exp 10 (`exp10_dram_schedulers.csv`)**: Memory controller scheduling: FCFS vs FR-FCFS.
11. **Exp 11 (`exp11_ddr4_vs_ddr5.csv`)**: DDR4-3200 vs DDR5-4800 bandwidth and subchannel evaluation.
12. **Exp 12 (`exp12_tlb_reach.csv`)**: TLB size and reach vs working set thrashing threshold.
13. **Exp 13 (`exp13_page_size.csv`)**: Page size evaluation: 4 KB vs 2 MB huge pages.
14. **Exp 14 (`exp14_page_table_depth.csv`)**: Multi-level page table depth and Page-Walk Cache (PWC) latency reduction.
15. **Exp 15 (`exp15_page_walk_caching.csv`)**: Hardware page-walk latency when PTEs hit in L1/L2/L3 vs DRAM.
16. **Exp 16 (`exp16_page_replacement.csv`)**: Page replacement algorithms and Belady's Anomaly reproduction.
17. **Exp 17 (`exp17_page_fault_costs.csv`)**: Minor vs major page fault costs (SSD vs HDD swap).
18. **Exp 18 (`exp18_vipt_aliasing.csv`)**: VIPT cache synonym aliasing boundary conditions.
19. **Exp 19 (`exp19_asid_context_switch.csv`)**: Context switching penalty with and without ASID-tagged TLBs.
20. **Exp 20 (`exp20_matrix_traversal.csv`)**: Row-major vs column-major matrix traversal on cache and TLB misses.

See [`docs/MEMORY_DESIGN.md`](docs/MEMORY_DESIGN.md), [`docs/VIRTUAL_MEMORY.md`](docs/VIRTUAL_MEMORY.md), and [`experiments/results/EXPERIMENT_RESULTS.md`](experiments/results/EXPERIMENT_RESULTS.md) for full architectural documentation.
