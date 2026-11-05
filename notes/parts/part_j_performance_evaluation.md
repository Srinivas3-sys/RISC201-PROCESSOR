# Part J: Performance Accounting & Empirical Evaluation

This section explains performance metrics from first principles (CPI, IPC, Speedup, Wall-Clock Execution Time), breaks down the CPI stack, explains why cold-start short programs show high CPI while long programs reach near-ideal 1.12 CPI, and presents empirical results across all 12 benchmarks and 30 experiments.

---

# Chapter J1: CPI, IPC, and the Iron Law of Processor Performance

### 1. In One Sentence
Processor performance is governed by the "Iron Law" of computer architecture, which states that total execution time is the product of Instruction Count, Cycles Per Instruction (CPI), and Clock Period.

---

### 2. The Formulas

1. **Cycles Per Instruction (CPI)**:
   $$\text{CPI} = \frac{\text{Total Clock Cycles}}{\text{Total Retired Instructions}}$$
2. **Instructions Per Cycle (IPC)**:
   $$\text{IPC} = \frac{1}{\text{CPI}}$$
3. **The Iron Law (Execution Time $T$)**:
   $$\text{Execution Time } (T) = \text{Instruction Count } (IC) \times \text{CPI} \times \frac{1}{\text{Frequency } (f)}$$
4. **Speedup ($S$)**:
   $$\text{Speedup } (S) = \frac{\text{Execution Time of Baseline}}{\text{Execution Time of Optimized}}$$

---

# Chapter J2: The CPI Stack (Cold-Start vs Steady-State)

### 1. In One Sentence
The CPI Stack decomposes total CPI into the ideal base execution plus stall contributions from RAW data hazards, branch mispredicts, and memory hierarchy misses.

---

### 2. The Great CPI Discrepancy Explained (Cold-Start vs Steady-State)
In older hand-typed documentation, a theoretical CPI table showed `2.560 CPI`.
When running fresh cycle-accurate simulations, short benchmarks showed cold CPIs like `6.125`, while extended benchmarks showed steady-state CPIs like `1.125`.

**Why the difference?**
1. **Cold-Start Startup Overhead**:
   - On simulation boot, the very first instruction fetch misses compulsory in both L1 ITLB and L1I Cache.
   - The Radix-4 page walker must traverse all 4 table levels in DRAM, costing **$117\text{--}126\text{ stall cycles}$**.
   - If a short benchmark executes only 24 instructions (like `factorial_iter.s`), that single 120-cycle startup penalty gets divided by 24 instructions:
     $$\Delta \text{CPI}_{\text{cold}} = \frac{120}{24} = +\mathbf{5.000\text{ CPI}}!$$
   - This creates an artificially inflated cold CPI of $1.250 + 5.000 = \mathbf{6.250}$.
2. **Steady-State Reality**:
   - In real-world software, programs execute millions of instructions.
   - On scaled benchmarks (`*_long.s`), that initial 120-cycle startup penalty gets divided across $> 100,000$ instructions:
     $$\Delta \text{CPI}_{\text{startup}} = \frac{120}{100,000} = +\mathbf{0.0012\text{ CPI}} \approx 0$$
   - Once caches and TLBs are warm, the true steady-state CPI is **1.02 to 1.15**!

---

### 3. Steady-State CPI Stack Breakdown (Real Runs on `matrix_mult_long.s`)

```
4-Stage Pipeline Steady-State CPI Stack (Total: 1.125):
+------------------------------------+-----------+------------+
| Component                          | Delta CPI | Percentage |
+------------------------------------+-----------+------------+
| Base Execution (Ideal Pipeline)    |   1.000   |    88.9%   |
| RAW Load-Use Bubble Stalls         |   0.070   |     6.2%   |
| Memory Hierarchy Latency Stalls    |   0.043   |     3.8%   |
| Branch Mispredict Flush Penalty    |   0.012   |     1.1%   |
+------------------------------------+-----------+------------+
| Total CPI                          |   1.125   |   100.0%   |
+------------------------------------+-----------+------------+
```

---

# Chapter J3: Complete Benchmark Suite Results

Real simulation data captured from `./audit_cpi`:

| Benchmark | Purpose / Workload | Instructions | 4-Stage CPI (Cold) | 4-Stage CPI (Steady) | 6-Stage CPI (Steady) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **`factorial_iter`** | Iterative loop (tests branch prediction) | 24 | 6.125 | **1.250** | 1.417 |
| **`factorial_rec`** | Recursive stack frame calls | 143 | 1.902 | **1.084** | 1.154 |
| **`prime_test`** | Nested trial division loops | 167 | 1.743 | **1.042** | 1.072 |
| **`array_sum`** | Sequential array memory reads | 58 | 3.207 | **1.190** | 1.259 |
| **`hazard_test`** | Intensive back-to-back RAW dependencies | 19 | 7.421 | **1.263** | 1.529 |
| **`stack_test`** | Deep push/pop sequences | 37 | 4.405 | **1.243** | 1.351 |
| **`array_sum_long`** | Scaled array traversal (>100k inst) | 100,004 | 1.191 | **1.190** | 1.259 |
| **`matrix_mult_long`**| Triple-nested matrix multiply | 189,050 | 1.126 | **1.125** | 1.131 |
| **`prime_test_long`** | Prime sieve (>100k inst) | 100,002 | 1.043 | **1.042** | 1.072 |

---

# Chapter J4: Key Experimental Findings from `experiments/`

The project includes 30 automated experiment sweeps. Here are the top architectural takeaways:

1. **Forwarding Effectiveness (Exp 1)**:
   - Without forwarding, every RAW hazard stalls for 2-3 cycles ($\text{CPI} = 2.45$).
   - Full forwarding reduces RAW stalls by **87%**, dropping CPI to **1.12**.
2. **Branch Predictor Comparison (Exp 2)**:
   - Static Not-Taken: 62% accuracy.
   - 2-Bit Bimodal: **93.4% accuracy**, cutting branch bubbles in half.
3. **Cache Line Size Trade-Off (Exp 1)**:
   - 16 B lines suffer from high compulsory miss rates.
   - 64 B lines achieve the lowest AMAT by maximizing spatial locality.
   - 256 B lines cause memory bus pollution and increase eviction latency.
4. **Page-Walk Caching (Exp 15)**:
   - Reduces average page table walk latency from 124 cycles down to **32 cycles (a 74% reduction)**.
5. **ASID Tagging During Context Switches (Exp 19)**:
   - Without ASID tagging, every process switch flushes the entire TLB, causing massive translation stall spikes.
   - 8-bit ASID tags preserve resident translations, cutting switch overhead by **84%**.

---

### 5. Summary: If You Only Remember 5 Things
1. $\text{Execution Time } T = (IC \times \text{CPI}) / f$.
2. **Cold-Start CPI** is high on tiny micro-benchmarks because 120-cycle compulsory misses dominate small instruction counts.
3. **Steady-State CPI** on realistic workloads is **1.02 to 1.15**.
4. Full data forwarding cuts RAW data stalls by **87%**.
5. 6-Stage has slightly higher CPI ($1.131$ vs $1.125$), but finishes **59% faster in wall-clock time** due to 800 MHz clocking.
