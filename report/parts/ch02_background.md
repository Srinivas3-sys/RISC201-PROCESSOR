# Chapter 2: Theoretical Background

This chapter provides the theoretical foundation for the microarchitectural mechanisms implemented in the RISC201 simulator. Each concept is tied directly to its corresponding C++ implementation in the repository.

---

## 2.1 Pipelining Principles & Performance Metrics

Instruction pipelining partitions the execution of CPU instructions into a sequence of discrete processing stages separated by clocked register latches (`processor_4stage/StageRegisters4.hpp`, `processor_6stage/StageRegisters6.hpp`). Pipelining exploits **Instruction-Level Parallelism (ILP)**: while instruction $N$ executes arithmetic in `EX`, instruction $N+1$ reads operands in `OF`/`RF`, and instruction $N+2$ is fetched in `IF`.

### 2.1.1 The Fundamental Iron Law of Processor Performance
The wall-clock execution time $T$ required to execute a program is governed by the three-factor product:

$$T = \text{Instruction Count (IC)} \times \text{Cycles Per Instruction (CPI)} \times \text{Clock Cycle Time } t_{\text{clk}} = \frac{\text{IC} \times \text{CPI}}{\text{Clock Frequency } f}$$

* **Instruction Count ($\text{IC}$)**: Determined by the compiler/assembler (`assembler/Assembler.hpp`) and ISA expressiveness.
* **CPI**: The average number of clock cycles required to retire an instruction. For an ideal $K$-stage in-order pipeline with zero hazards, $\text{CPI}_{\text{ideal}} = 1.0$. Stalls and bubbles inflate CPI:
  $$\text{CPI} = 1.0 + \sum \text{Stall Cycles per Instruction}$$
* **Clock Period ($t_{\text{clk}}$)**: Constrained by the longest combinational path between consecutive pipeline latches (the *critical path*):
  $$t_{\text{clk}} \ge \max_{i} (t_{\text{latch\_propagation}} + t_{\text{stage\_logic}, i} + t_{\text{setup}} + t_{\text{clock\_skew}})$$

### 2.1.2 Depth Trade-Off: 4-Stage vs. 6-Stage Pipelines
As pipeline depth increases:
1. Combinational work per stage decreases, shortening $t_{\text{clk}}$ and permitting higher clock frequencies ($f_{6\text{-stage}} > f_{4\text{-stage}}$).
2. However, hazard penalties (branch misprediction latency, load-use delays) span more stages, increasing CPI ($\text{CPI}_{6\text{-stage}} > \text{CPI}_{4\text{-stage}}$).
3. The design wins if the frequency gain exceeds the CPI inflation penalty:
   $$\frac{T_{4\text{-stage}}}{T_{6\text{-stage}}} = \frac{\text{CPI}_{4\text{-stage}} \times f_{6\text{-stage}}}{\text{CPI}_{6\text{-stage}} \times f_{4\text{-stage}}} > 1.0$$

---

## 2.2 Pipeline Hazards & Hazard Mitigation

A pipeline hazard prevents the next instruction in the instruction stream from executing in its designated clock cycle (`common/HazardUnit.hpp`).

### 2.2.1 Data Hazards (RAW, WAR, WAW)
* **Read-After-Write (RAW, True Dependency)**: Instruction $J$ attempts to read operand $r_s$ before instruction $I$ writes $r_s$. In SimpleRisc, RAW hazards represent the primary cause of pipeline stalls.
* **Write-After-Read (WAR, Anti-Dependency)**: Instruction $J$ writes $r_d$ before instruction $I$ reads $r_d$. In an in-order pipeline, WAR cannot cause hazards because operand read always precedes writeback.
* **Write-After-Write (WAW, Output Dependency)**: Instruction $J$ writes $r_d$ before instruction $I$ writes $r_d$. In-order retirement guarantees write serialism.

### 2.2.2 Data Forwarding (Bypassing)
Rather than stalling the pipeline until the producing instruction writes back to the register file, data forwarding routes the computed result directly from intermediate pipeline latches (`EX/MEM` or `MEM/WB`) to the inputs of the functional unit (`EX`) (`common/ForwardingUnit.hpp`).

* **Load-Use Penalty**: A load instruction (`ld`) retrieves data from memory during stage `MA`. If the immediately following instruction consumes the loaded word in `EX`, data cannot be forwarded backwards in time. In the 6-stage core, this necessitates **1 bubble cycle**.

### 2.2.3 Control Hazards & Branch Prediction
Control hazards occur because the next program counter cannot be definitively known until branch condition evaluation and target calculation complete in `EX` (`common/BranchPredictor.hpp`).

* **Static Prediction**: Always Taken, Always Not-Taken (Flush), or BTFN (Backward Taken, Forward Not-Taken).
* **Dynamic Bimodal Prediction (Smith 2-Bit Counter)**: Uses a 2-bit saturating counter automaton (`00`, `01`, `10`, `11`) requiring two consecutive mispredictions to flip prediction direction.
* **Branch Target Buffer (BTB)**: A cache of branch target addresses indexed by lower PC bits, enabling zero-bubble target prediction during `IF`.
* **Return Address Stack (RAS)**: A hardware LIFO buffer that predicts subroutine return targets (`ret`) with near 100% accuracy.

---

## 2.3 Control Unit Architectures: Hardwired vs. Microprogrammed

The control unit generates all datapath multiplexer selects, register write-enables, memory commands, and ALU function codes (`microcode/`).

* **Hardwired Control**: Implemented with combinational logic gates and decoders. It delivers minimal propagation delay but is inflexible to modify.
* **Microprogrammed Control**: Executes sequences of primitive micro-instructions stored in a dedicated **Control Memory (ROM)** addressed by a **MicroPC ($\mu\text{PC}$)**:
  * **Horizontal Microcode**: Unencoded, wide control words where each bit directly drives a hardware control wire (`microcode/HorizontalControl.hpp`). Delivers zero decode latency at the expense of large ROM bit width.
  * **Vertical Microcode**: Compact, field-encoded micro-instructions where related control signals are grouped into fields (`microcode/VerticalControl.hpp`). Hardware decoders (`MicroDecoder`) expand the fields into horizontal signals, reducing ROM area by $\sim 30\%$.

---

## 2.4 Arithmetic Logic Unit (ALU) Algorithms & Complexity

The ALU performs arithmetic, shift, and logical operations (`alu/`).

### 2.4.1 Adder Topologies
* **Ripple-Carry Adder (RCA)**: Cascaded 1-bit Full Adders. Area $O(N)$, Delay $O(N)$ due to serial carry propagation across 32 bits ($64\Delta$).
* **Carry-Lookahead Adder (CLA)**: Generates bit-level Generate ($G_i = A_i \cdot B_i$) and Propagate ($P_i = A_i \oplus B_i$) terms. Group carry generators compute carries in parallel: Delay $O(\log N) \approx 8\Delta$.
* **Carry-Select Adder (CSLA)**: Dual speculative carry chains assuming $C_{in} = 0$ and $C_{in} = 1$: Delay $O(\sqrt{N}) \approx 16\Delta$.

### 2.4.2 Multiplier Topologies
* **Shift-Add Array Multiplier**: Iterates over 32 bits sequentially (32 cycles).
* **Booth's Radix-4 Multiplier**: Overlapping 3-bit recoding cuts the number of partial products in half (16 cycles).
* **Wallace-Tree Multiplier**: Tree of Carry-Save Adders (CSA) reducing partial products in $O(\log_{1.5} N)$ tree levels.

### 2.4.3 Divider Topologies
* **Restoring Division**: Shifts dividend left, subtracts divisor; if negative, adds divisor back (restores). Latency: 32 cycles.
* **Non-Restoring Division**: Avoids the restoring addition by adding divisor in subsequent cycle if current remainder is negative. Latency: 32 cycles with reduced switching activity.
* **Newton-Raphson Iterative Division**: Computes reciprocal $X \approx 1/D$ via quadratic convergence:
  $$X_{k+1} = X_k (2 - D_{\text{norm}} X_k)$$
  Achieves full 32-bit integer precision in only **5 iterations** ($O(\log(\text{bits}))$).

---

## 2.5 Memory Hierarchy & Locality Principles

The memory system bridges the massive speed gap between fast processor clocks (0.3 – 2.0 ns) and slow main memory (50 – 100 ns) (`memory/`).

### 2.5.1 The Principle of Locality
* **Temporal Locality**: Memory locations accessed recently are likely to be accessed again in the near future (e.g. loop variables, stack frames).
* **Spatial Locality**: Memory locations physically adjacent to recently accessed addresses are likely to be accessed soon (e.g. sequential instruction streams, array traversals).

### 2.5.2 Modified Harvard vs. Von Neumann
* **Von Neumann**: Unified instruction and data memory. In an in-order pipeline, simultaneous instruction fetch (`IF`) and data memory access (`MA`) collide on the shared memory port, creating a structural stall of $+0.25$ to $+0.30$ CPI!
* **Modified Harvard Architecture**: Instruction (`L1I`) and Data (`L1D`) caches are split at Level 1, allowing concurrent, stall-free 1-cycle access by `IF` and `MA`. Lower levels (L2, L3, DRAM) are unified for capacity efficiency.

### 2.5.3 Average Memory Access Time (AMAT)
The effective memory latency experienced by the processor core is modeled recursively:

$$\text{AMAT} = \text{Hit Time}_{L1} + \text{Miss Rate}_{L1} \times \left( \text{Hit Time}_{L2} + \text{Miss Rate}_{L2} \times \left( \text{Hit Time}_{L3} + \text{Miss Rate}_{L3} \times t_{\text{DRAM}} \right) \right)$$

### 2.5.4 DRAM Command-Level Timing Parameters
Main memory access latency depends dynamically on DRAM internal bank state:
* $t_{\text{RCD}}$ (Row-to-Column Delay): Time to open a row into the row buffer (14.06 ns).
* $t_{\text{RP}}$ (Row Precharge): Time to close an open row buffer (14.06 ns).
* $t_{\text{CL}}$ (CAS Latency): Time between column read command and first data word on the bus (14.06 ns).
* **Row-Buffer Hit**: Access to currently open row requires only $t_{\text{CL}}$.
* **Row-Buffer Conflict (Miss)**: Access to different row in same bank requires $t_{\text{RP}} + t_{\text{RCD}} + t_{\text{CL}}$ ($\sim 3\times$ longer!).
* **Scheduling**: FR-FCFS (First-Ready First-Come First-Served) prioritizes row-buffer hits to maximize memory bus utilization.

### 2.5.5 Virtual Memory & Multi-Level Translation
Virtual memory provides hardware isolation, protection, and demand paging (`memory/VirtualMemory.hpp`):
* **Radix-4 Page Tables**: Decomposes 48-bit virtual addresses into 4 hierarchical index levels (PML4, PDPT, PD, PT).
* **TLB (Translation Lookaside Buffer)**: Caches recent virtual-to-physical translations to avoid 4 DRAM memory lookups per access.
* **Page-Walk Cache (PWC)**: Caches intermediate page directory pointers, reducing page walk latency by up to 74%.

