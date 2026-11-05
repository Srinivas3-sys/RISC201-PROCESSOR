# Complete Architectural Glossary

Alphabetical reference of every technical abbreviation and term used in this project:

| Term / Abbrev | Plain-Language Definition | Chapter Reference |
| :--- | :--- | :---: |
| **ALU** | Arithmetic Logic Unit: the combinational circuit executing math, logic, and comparisons. | Chapter G1 |
| **AMAT** | Average Memory Access Time: average clock cycles for memory access across all cache levels. | Chapter H3 |
| **ASID** | Address Space Identifier: process tag in TLBs preventing cross-process address conflicts. | Chapter H5 |
| **Bimodal Predictor**| Branch predictor using 2-bit saturating up-down counters (`00` to `11`). | Chapter E3 |
| **BTB** | Branch Target Buffer: on-chip cache storing calculated target branch addresses. | Chapter E3 |
| **Bubble** | A dummy `nop` instruction injected into a pipeline latch to stall execution for 1 cycle. | Chapter E2 |
| **Bypass** | See *Forwarding*. Routing results directly between execution latches. | Chapter E2 |
| **CISC** | Complex Instruction Set Computer: variable-length instructions with complex hardware. | Chapter A2 |
| **CLA** | Carry Lookahead Adder: high-speed adder using generate/propagate trees ($O(\log N)$ delay). | Chapter G2 |
| **Clock Cycle** | One tick of the processor clock; fundamental unit of execution time. | Chapter A1 |
| **Cold Miss** | The compulsory first-time access to a memory block. | Chapter H2 |
| **Conflict Miss** | Miss occurring when multiple addresses map to the exact same cache set. | Chapter H2 |
| **Control Hazard** | Pipeline stall caused by not knowing the target of a branch instruction. | Chapter E1 |
| **CPI** | Cycles Per Instruction: average number of clock cycles to retire one instruction. | Chapter J1 |
| **CSLA** | Carry Select Adder: speculative adder computing sums for carry=0 and carry=1 in parallel. | Chapter G2 |
| **Data Hazard** | Pipeline stall caused by an instruction depending on an in-flight result (e.g. RAW). | Chapter E1 |
| **Disassembler** | Software translating 32-bit binary machine code words back into assembly text. | Chapter C2 |
| **DRAM** | Dynamic Random Access Memory: off-chip main memory storing data as capacitor charges. | Chapter H4 |
| **FR-FCFS** | First-Ready First-Come First-Served: memory controller scheduler prioritizing row hits. | Chapter H4 |
| **Full-Descending**| Stack model where SP points to the latest valid data and grows toward lower addresses. | Chapter C3 |
| **GShare** | Two-level branch predictor XORing branch address with global branch history register. | Chapter E3 |
| **Guard Page** | An unmapped or no-access memory page placed beneath stack memory to detect overflow. | Chapter I2 |
| **Horizontal Microcode**| Unencoded 32-bit control words where each bit directly drives a physical hardware wire. | Chapter F3 |
| **IPC** | Instructions Per Cycle: reciprocal of CPI ($1 / \text{CPI}$); measures throughput. | Chapter J1 |
| **Iron Law** | Performance equation: $\text{Time} = \text{Instruction Count} \times \text{CPI} \times t_{\text{clk}}$. | Chapter J1 |
| **ISA** | Instruction Set Architecture: the contract between software and hardware. | Chapter A2 |
| **ISS** | Instruction Set Simulator: unpipelined, single-cycle reference model used for verification. | Chapter D5 |
| **Latch** | Inter-stage register holding intermediate data between clock cycles (e.g., `OF_EX`). | Chapter D2 |
| **Load-Store CPU** | Architecture where only `ld` and `st` access memory; arithmetic requires registers. | Chapter A2 |
| **Load-Use Delay** | 1-cycle unavoidable stall when an instruction immediately uses loaded memory data. | Chapter E2 |
| **Locality** | Tendency of programs to reuse recently used data (temporal) or adjacent data (spatial). | Chapter H1 |
| **Microcode** | Internal low-level micro-operations stored in control ROM that control CPU datapath. | Chapter F1 |
| **MicroPC** | Program counter indexing microinstructions inside the Control Store ROM. | Chapter F2 |
| **MSHR** | Miss Status Holding Register: hardware queue enabling non-blocking cache hits during a miss. | Chapter H2 |
| **Page Table** | Operating system data structure mapping virtual page numbers to physical frame numbers. | Chapter H5 |
| **PC** | Program Counter: register holding the byte address of the next instruction to fetch. | Chapter A1 |
| **Prefetcher** | Hardware engine predicting future memory accesses and loading lines into cache early. | Chapter H2 |
| **PWC** | Page-Walk Cache: on-chip cache storing intermediate Radix page table directory pointers. | Chapter H5 |
| **Radix-4 VM** | 4-level hierarchical page table (PML4, PDPT, PD, PT) for 48-bit virtual address spaces. | Chapter H5 |
| **RAS** | Return Address Stack: small LIFO buffer predicting return target addresses (`ret`). | Chapter E3 |
| **RAW Hazard** | Read-After-Write: true data dependency where instruction $i+1$ reads register written by $i$. | Chapter E1 |
| **RCA** | Ripple Carry Adder: basic adder chaining full adders sequentially ($O(N)$ delay). | Chapter G2 |
| **RISC** | Reduced Instruction Set Computer: fixed-length instructions, load-store architecture. | Chapter A2 |
| **Row Buffer** | Internal DRAM SRAM sensing latch caching an open 8 KB DRAM page. | Chapter H4 |
| **Squash** | Converting instructions on an incorrectly predicted branch path into `nop` bubbles. | Chapter E3 |
| **SSO** | Small String Optimization: C++ feature storing short strings without heap allocation. | Chapter B2 |
| **Structural Hazard**| Hardware resource conflict (e.g. single memory port serving both IF and MA). | Chapter E1 |
| **TLB** | Translation Lookaside Buffer: high-speed cache of virtual-to-physical address translations. | Chapter H5 |
| **Two-Pass Assembler**| Assembler that discovers labels in Pass 1 and encodes machine code in Pass 2. | Chapter C1 |
| **Vertical Microcode**| Encoded 16-bit control words using hardware decoders to save ROM space. | Chapter F3 |
| **Wallace Tree** | High-speed multiplier tree using carry-save adders to reduce partial products in parallel. | Chapter G2 |
