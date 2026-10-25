# RISC201-PROCESSOR: Comprehensive Memory Hierarchy & Virtual Memory Experiments
## Computer Organization & Architecture (CSC-201) Empirical Evaluation Report

This report presents quantitative data, comparative trends, and architectural explanations for 20 rigorous memory hierarchy and virtual memory experiments simulated on the cycle-accurate RISC201 processor subsystem.

---

### Experiment 1: Cache Size, Line Size, Associativity, and Replacement Sweeps

| Size_KB | Line_B | Assoc | Policy | Hits | Misses | HitRate_Pct | Compulsory | Capacity | Conflict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 8 | 16 | 1 | lru | 12 | 988 | 1.20 | 500 | 0 | 488 |
| 8 | 16 | 1 | fifo | 12 | 988 | 1.20 | 500 | 0 | 488 |
| 8 | 16 | 1 | random | 12 | 988 | 1.20 | 500 | 0 | 488 |
| 8 | 16 | 1 | plru | 12 | 988 | 1.20 | 500 | 0 | 488 |
| 8 | 16 | 1 | srrip | 12 | 988 | 1.20 | 500 | 0 | 488 |
| 8 | 16 | 2 | lru | 0 | 1000 | 0.00 | 500 | 0 | 500 |
| 8 | 16 | 2 | fifo | 0 | 1000 | 0.00 | 500 | 0 | 500 |
| 8 | 16 | 2 | random | 83 | 917 | 8.30 | 500 | 0 | 417 |
| 8 | 16 | 2 | plru | 0 | 1000 | 0.00 | 500 | 0 | 500 |
| 8 | 16 | 2 | srrip | 0 | 1000 | 0.00 | 500 | 0 | 500 |
| 8 | 16 | 4 | lru | 0 | 1000 | 0.00 | 500 | 0 | 500 |

*(Table truncated: showing top 11 of 320 data rows. Complete data in `experiments/results/exp01_cache_sweeps.csv`)*


---

### Experiment 2: Direct-Mapped Conflict Miss Thrashing (A, B, A, B) vs 2-Way Associativity

| Config | Access_Pattern | Accesses | Hits | Misses | HitRate_Pct | ConflictMisses |
| --- | --- | --- | --- | --- | --- | --- |
| 1-Way Direct-Mapped | A,B,A,B (Same Index) | 100 | 0 | 100 | 0.00 | 98 |
| 2-Way Set-Associative | A,B,A,B (Same Index) | 100 | 98 | 2 | 98.00 | 0 |

---

### Experiment 3: DRAM Row-Buffer Policy: Open-Page vs Closed-Page

| Policy | Pattern | Total_Accesses | Row_Hits | Row_Empties | Row_Conflicts | HitRate_Pct | Avg_DRAM_Latency |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OPEN | Sequential | 100 | 99 | 1 | 0 | 99.00 | 3363.00 |
| OPEN | Strided_4KB | 100 | 93 | 7 | 0 | 93.00 | 3619.00 |
| OPEN | Random | 100 | 0 | 16 | 84 | 0.00 | 8526.74 |
| CLOSED | Sequential | 100 | 0 | 100 | 0 | 0.00 | 7112.54 |
| CLOSED | Strided_4KB | 100 | 0 | 100 | 0 | 0.00 | 7106.78 |
| CLOSED | Random | 100 | 0 | 100 | 0 | 0.00 | 6929.54 |

---

### Experiment 4: DRAM Timing Sensitivity: Independent Sweeps of tRCD, tRP, and tCL

| Varying_Param | Param_Value | tRCD | tRP | tCL | Avg_Access_Latency_Cycles | AMAT_Cycles |
| --- | --- | --- | --- | --- | --- | --- |
| tRCD | 14 | 14 | 22 | 22 | 106.56 | 86.00 |
| tRCD | 18 | 18 | 22 | 22 | 106.72 | 86.00 |
| tRCD | 22 | 22 | 22 | 22 | 106.88 | 86.00 |
| tRCD | 26 | 26 | 22 | 22 | 107.04 | 86.00 |
| tRP | 14 | 22 | 14 | 22 | 106.88 | 86.00 |
| tRP | 18 | 22 | 18 | 22 | 106.88 | 86.00 |
| tRP | 22 | 22 | 22 | 22 | 106.88 | 86.00 |
| tRP | 26 | 22 | 26 | 22 | 106.88 | 86.00 |
| tCL | 14 | 22 | 22 | 14 | 90.88 | 86.00 |
| tCL | 18 | 22 | 22 | 18 | 98.88 | 86.00 |
| tCL | 22 | 22 | 22 | 22 | 106.88 | 86.00 |

*(Table truncated: showing top 11 of 12 data rows. Complete data in `experiments/results/exp04_dram_timing_sensitivity.csv`)*


---

### Experiment 5: DRAM Banking, Channels, and XOR-Hash Interleaving

| Channels | Banks | Mapping | Conflicts | Max_Bank_Accesses | Avg_Latency_Cycles |
| --- | --- | --- | --- | --- | --- |
| 1 | 16 | row_bg_bank_col_chan_byte | 0 | 16 | 2268.00 |
| 1 | 16 | xor_hash | 48 | 4 | 5524.31 |
| 2 | 16 | row_bg_bank_col_chan_byte | 0 | 32 | 2197.00 |
| 2 | 16 | xor_hash | 48 | 4 | 5508.53 |
| 4 | 16 | row_bg_bank_col_chan_byte | 0 | 64 | 2175.00 |
| 4 | 16 | xor_hash | 48 | 4 | 5519.62 |

---

### Experiment 6: Hardware Prefetchers Across Sequential, Strided, and Pointer-Chasing Traffic

| Workload | Prefetcher | Issued | Useful | Accuracy_Pct | L1D_Hits | L1D_Misses | L1D_HitRate_Pct |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sequential | none | 0 | 0 | 0.00 | 0 | 120 | 0.00 |
| Sequential | next_line | 120 | 118 | 98.33 | 118 | 2 | 98.33 |
| Sequential | stride | 112 | 104 | 92.86 | 104 | 16 | 86.67 |
| Sequential | stream | 115 | 109 | 94.78 | 109 | 11 | 90.83 |
| Strided | none | 0 | 0 | 0.00 | 0 | 120 | 0.00 |
| Strided | next_line | 237 | 116 | 48.95 | 116 | 4 | 96.67 |
| Strided | stride | 104 | 96 | 92.31 | 96 | 24 | 80.00 |
| Strided | stream | 223 | 107 | 47.98 | 107 | 13 | 89.17 |
| Pointer_Chasing | none | 0 | 0 | 0.00 | 0 | 120 | 0.00 |
| Pointer_Chasing | next_line | 183 | 51 | 27.87 | 51 | 69 | 42.50 |
| Pointer_Chasing | stride | 86 | 80 | 93.02 | 80 | 40 | 66.67 |

*(Table truncated: showing top 11 of 12 data rows. Complete data in `experiments/results/exp06_prefetcher_eval.csv`)*


---

### Experiment 7: Non-Blocking Caches: MSHR Depth, Request Merging, and MLP

| MSHR_Count | Total_Accesses | MSHR_Merges | Average_MLP |
| --- | --- | --- | --- |
| 1 | 200 | 0 | 1.00 |
| 2 | 200 | 0 | 1.00 |
| 4 | 200 | 0 | 1.00 |
| 8 | 200 | 0 | 1.00 |
| 16 | 200 | 0 | 1.00 |

---

### Experiment 8: DRAM Refresh Overhead vs Retention Window and Row Counts

| Retention_ms | tREFI_cycles | tRFC_cycles | Total_Refresh_Stalls | Overhead_Pct |
| --- | --- | --- | --- | --- |
| 64 | 12480 | 560 | 44240 | 4.42 |
| 32 | 6240 | 560 | 85680 | 8.57 |
| 16 | 3120 | 560 | 159600 | 15.96 |

---

### Experiment 9: 4-Stage vs 6-Stage CPI Across Ideal, Simple, and Modern Presets

| Benchmark | Pipeline | Preset | Cycles | Retired_Insts | CPI | IPC | Memory_Stalls | Branch_Bubbles |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Factorial_Iter | 4-Stage | IDEAL | 35 | 24 | 1.458 | 0.686 | 0 | 8 |
| Factorial_Iter | 4-Stage | SIMPLE | 136 | 29 | 4.690 | 0.213 | 116 | 4 |
| Factorial_Iter | 4-Stage | MODERN | 285 | 29 | 9.828 | 0.102 | 265 | 4 |
| Factorial_Iter | 6-Stage | IDEAL | 41 | 24 | 1.708 | 0.585 | 0 | 12 |
| Factorial_Iter | 6-Stage | SIMPLE | 136 | 29 | 4.690 | 0.213 | 116 | 6 |
| Factorial_Iter | 6-Stage | MODERN | 285 | 29 | 9.828 | 0.102 | 265 | 6 |
| Factorial_Rec | 4-Stage | IDEAL | 208 | 143 | 1.455 | 0.688 | 0 | 62 |
| Factorial_Rec | 4-Stage | SIMPLE | 748 | 149 | 5.020 | 0.199 | 644 | 36 |
| Factorial_Rec | 4-Stage | MODERN | 240 | 10 | 24.000 | 0.042 | 230 | 4 |
| Factorial_Rec | 6-Stage | IDEAL | 241 | 143 | 1.685 | 0.593 | 0 | 93 |
| Factorial_Rec | 6-Stage | SIMPLE | 88 | 5 | 17.600 | 0.057 | 80 | 6 |

*(Table truncated: showing top 11 of 24 data rows. Complete data in `experiments/results/exp09_pipeline_cpi_comparison.csv`)*


---

### Experiment 10: Memory Controller Scheduling: FCFS vs FR-FCFS

| Scheduler | Total_Requests | Row_Hits | Row_Empties | Row_Conflicts | HitRate_Pct | Total_DRAM_Cycles |
| --- | --- | --- | --- | --- | --- | --- |
| FCFS | 60 | 59 | 1 | 0 | 98.33 | 2024 |
| FRFCFS | 60 | 59 | 1 | 0 | 98.33 | 2024 |

---

### Experiment 11: DDR4-3200 vs DDR5-4800 Bandwidth, Latency, and Subchannel Evaluation

| DRAM_Type | Bus_Clock_MHz | Data_Rate_MTs | Subchannels | Bus_Width_Bits | Peak_Bandwidth_GBs | tCL_ns | Row_Hit_Latency_ns |
| --- | --- | --- | --- | --- | --- | --- | --- |
| DDR4-3200 | 1600 | 3200 | 1 | 64 | 25.60 | 13.75 | 16.25 |
| DDR5-4800 | 2404 | 4808 | 2 | 32 | 38.46 | 16.64 | 19.97 |

---

### Experiment 12: TLB Size, Associativity, and Reach vs Working Set Size

| TLB_Entries | TLB_Reach_KB | WorkingSet_KB | Lookups | Hits | Misses | HitRate_Pct |
| --- | --- | --- | --- | --- | --- | --- |
| 8 | 32 | 512 | 1000 | 0 | 1000 | 0.00 |
| 16 | 64 | 512 | 1000 | 0 | 1000 | 0.00 |
| 32 | 128 | 512 | 1000 | 0 | 1000 | 0.00 |
| 64 | 256 | 512 | 1000 | 0 | 1000 | 0.00 |
| 128 | 512 | 512 | 1000 | 872 | 128 | 87.20 |
| 256 | 1024 | 512 | 1000 | 872 | 128 | 87.20 |
| 512 | 2048 | 512 | 1000 | 872 | 128 | 87.20 |
| 1024 | 4096 | 512 | 1000 | 872 | 128 | 87.20 |
| 2048 | 8192 | 512 | 1000 | 872 | 128 | 87.20 |

---

### Experiment 13: Page Size Evaluation: 4 KB vs 2 MB Huge Pages

| Page_Size | TLB_Entries | TLB_Reach_MB | TLB_Hits | TLB_Misses | HitRate_Pct | Page_Table_Overhead_KB |
| --- | --- | --- | --- | --- | --- | --- |
| 4 KB | 64 | 0.25 | 0 | 500 | 0.00 | 8 |
| 2 MB | 64 | 128.00 | 498 | 2 | 99.60 | 1 |

---

### Experiment 14: Multi-Level Page Table Depth and Page-Walk Cache (PWC) Latency Reduction

| Levels | PWC_Enabled | Translations | PWC_Hits | Avg_Walk_Latency_Cycles |
| --- | --- | --- | --- | --- |
| 1 | False | 100 | 0 | 20.00 |
| 1 | True | 100 | 99 | 12.08 |
| 2 | False | 100 | 0 | 32.00 |
| 2 | True | 100 | 99 | 12.20 |
| 4 | False | 100 | 0 | 56.00 |
| 4 | True | 100 | 99 | 12.44 |

---

### Experiment 15: Hardware Page-Walk Latency when PTEs Hit in Cache vs DRAM

| PTE_Served_By | Walk_Levels | Per_Level_Latency_Cycles | Total_Walk_Cost_Cycles |
| --- | --- | --- | --- |
| L1D (Cache Hit) | 4 | 4 | 17 |
| L2 (Cache Hit) | 4 | 12 | 49 |
| L3 (Cache Hit) | 4 | 38 | 153 |
| DRAM (Cache Miss) | 4 | 70 | 281 |

---

### Experiment 16: Page Replacement Algorithms and Belady's Anomaly Demonstration

| Frames | FIFO_Faults | LRU_Faults | Clock_Faults | Belady_Anomaly_Detected |
| --- | --- | --- | --- | --- |
| 2 | 12 | 12 | 12 | NO |
| 3 | 9 | 10 | 10 | NO |
| 4 | 10 | 8 | 8 | YES (Belady Anomaly!) |
| 5 | 5 | 5 | 5 | NO |
| 6 | 5 | 5 | 5 | NO |
| 7 | 5 | 5 | 5 | NO |

---

### Experiment 17: Minor vs Major Page Fault Costs and Thrashing Impact

| Fault_Type | Backing_Medium | Typical_Latency_ns | CPU_Cycles_at_3GHz | Relative_DRAM_Multiple |
| --- | --- | --- | --- | --- |
| Minor Fault | Zero-fill / Free Page Allocator | 500.0 | 1500 | 7.9x |
| Major Fault (SSD) | PCIe NVMe SSD Swap Partition | 100000.0 | 300000 | 1589.8x |
| Major Fault (HDD) | Mechanical Hard Disk Swap Partition | 3000000.0 | 9000000 | 47694.8x |

---

### Experiment 18: VIPT Cache Synonym Aliasing Condition Demo

| Cache_Size_KB | Assoc | Bytes_Per_Way_KB | Page_Size_KB | Aliasing_Detected | Hardware_Cost |
| --- | --- | --- | --- | --- | --- |
| 16 | 2 | 8 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 16 | 4 | 4 | 4 | NO (Safe) | Zero hardware overhead (VIPT transparent) |
| 16 | 8 | 2 | 4 | NO (Safe) | Zero hardware overhead (VIPT transparent) |
| 32 | 2 | 16 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 32 | 4 | 8 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 32 | 8 | 4 | 4 | NO (Safe) | Zero hardware overhead (VIPT transparent) |
| 64 | 2 | 32 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 64 | 4 | 16 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 64 | 8 | 8 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 128 | 2 | 64 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |
| 128 | 4 | 32 | 4 | YES (Aliasing!) | Synonym bits needed / Page color tag |

*(Table truncated: showing top 11 of 12 data rows. Complete data in `experiments/results/exp18_vipt_aliasing.csv`)*


---

### Experiment 19: Context Switching Penalty With and Without ASID-Tagged TLBs

| Scheme | Switches | Total_Translations | TLB_Hits | TLB_Misses | HitRate_Pct | Page_Walk_Cycles |
| --- | --- | --- | --- | --- | --- | --- |
| No ASID (Flush on Switch) | 100 | 100 | 0 | 100 | 0.00 | 1244 |
| With ASID | 100 | 100 | 84 | 16 | 84.00 | 236 |

---

### Experiment 20: Row-Major vs Column-Major Matrix Traversal on Cache and TLB Misses

| Traversal_Algorithm | Matrix_Dim | L1D_Hits | L1D_Misses | L1D_HitRate_Pct | TLB_Hits | TLB_Misses | TLB_HitRate_Pct |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Row-Major (Sequential Stride) | 32x32 | 1019 | 5 | 99.51 | 1023 | 1 | 99.90 |
| Column-Major (Strided Stride) | 32x32 | 1018 | 6 | 99.41 | 1023 | 1 | 99.90 |

---
