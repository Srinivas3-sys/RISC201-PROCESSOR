# 120 Comprehensive Examiner Questions & Model Answers

Grouped by difficulty into **Easy**, **Medium**, **Hard**, and **Examiner Trap** questions. Every answer includes concrete source file line citations.

---

## Category 1: Easy Questions (Fundamentals & Architecture)

#### Q1: What does RISC mean, and how does RISC201 follow RISC principles?
**Answer**: RISC stands for Reduced Instruction Set Computer. RISC201 follows RISC by using fixed 32-bit instruction words, a regular 4-format layout, and a strict Load-Store architecture where only `ld` and `st` touch memory (`common/ISA.hpp:1-50`).  
*Reference: Chapter A2*

#### Q2: What are the four stages of the 4-stage pipeline?
**Answer**: The four stages are Instruction Fetch (`IF`), Operand Fetch & Decode (`OF`), Execute (`EX`), and Memory Access & Register Writeback (`MA_RW`) (`processor_4stage/Pipeline4Stage.hpp:12-25`).  
*Reference: Chapter D2*

#### Q3: What are the six stages of the 6-stage pipeline?
**Answer**: The six stages are Instruction Fetch (`IF`), Instruction Decode (`ID`), Register Read (`RF`), Execute (`EX`), Memory Access (`MA`), and Writeback (`WB`) (`processor_6stage/Pipeline6Stage.hpp:15-30`).  
*Reference: Chapter D3*

#### Q4: How many general-purpose registers does RISC201 have?
**Answer**: RISC201 has 16 general-purpose registers (`r0` through `r15`), where `r14` is conventionally the Stack Pointer (`sp`) and `r15` is the Return Address (`ra`) (`common/RegisterFile.hpp:18-25`).  
*Reference: Chapter B1*

#### Q5: What is ideal CPI for an in-order scalar pipeline?
**Answer**: The ideal CPI is 1.000, which corresponds to retiring exactly one instruction per clock cycle when there are no hazard stalls, memory misses, or branch flushes.  
*Reference: Chapter D1*

#### Q6: How are condition flags updated in RISC201?
**Answer**: Flags `E` (Equal) and `GT` (Greater Than) are updated strictly by the `cmp` instruction. Other arithmetic instructions like `add` or `sub` do NOT modify flags (`alu/ALU.hpp:45-55`).  
*Reference: Chapter B1*

#### Q7: What is the opcode width in RISC201?
**Answer**: Exactly 5 bits (`bits[31:27]`), allowing up to 32 distinct opcode patterns, of which 25 are assigned in this architecture (`common/ISA.hpp:20-40`).  
*Reference: Chapter B1*

#### Q8: What does the Immediate Flag (bit 26) do?
**Answer**: If bit 26 is `0`, the second operand comes from register `rs2`. If bit 26 is `1`, the second operand is an 18-bit signed immediate constant (`assembler/Disassembler.hpp:38-44`).  
*Reference: Chapter A3*

#### Q9: What is the purpose of the Golden ISS?
**Answer**: The Golden ISS (`common/ISS.hpp`) is an unpipelined, single-cycle reference model used to verify that the complex pipelined cores execute instructions with 100% behavioral correctness.  
*Reference: Chapter D5*

#### Q10: What tool assembles `.s` files in this project?
**Answer**: `risc201_as`, built from `assembler/main_as.cpp` and `assembler/Assembler.hpp`.  
*Reference: Chapter C1*

#### Q11: What is a pipeline bubble?
**Answer**: A bubble is a dummy `nop` instruction injected into an inter-stage latch that advances through the pipeline without modifying architectural state, stalling subsequent instructions (`common/HazardUnit.hpp:80-95`).  
*Reference: Chapter E2*

#### Q12: What is the full-descending stack convention?
**Answer**: "Full" means `sp` points to the last valid written data item. "Descending" means the stack grows toward lower memory addresses (decrements on push, increments on pop) (`assembler/MacroPreprocessor.hpp:25-45`).  
*Reference: Chapter C3*

#### Q13: What does the disassembler do?
**Answer**: The disassembler (`risc201_disasm`) reads 32-bit binary words, extracts bit fields, and outputs human-readable assembly text (`assembler/Disassembler.hpp:55-80`).  
*Reference: Chapter C2*

#### Q14: What is the default starting Stack Pointer address?
**Answer**: `0x000FFFFC`, located at the top of the 1 MB default physical address space (`common/RegisterFile.hpp:22`).  
*Reference: Chapter I1*

#### Q15: What is AMAT?
**Answer**: Average Memory Access Time: the average number of clock cycles required to perform a memory access across all levels of the cache and DRAM hierarchy (`notes/parts/part_h_memory.md`).  
*Reference: Chapter H3*

---

## Category 2: Medium Questions (Pipelining, Hazards, Memory)

#### Q16: Why are two passes required in the assembler?
**Answer**: Pass 1 scans the source code to find all label addresses and store them in the Symbol Table. Pass 2 generates machine code words, using the Symbol Table to resolve forward branch references (`assembler/Assembler.hpp:60-120`).  
*Reference: Chapter C1*

#### Q17: How does data forwarding eliminate RAW hazards?
**Answer**: The Forwarding Unit compares destination registers in `EX/MA` and `MA/RW` latches with source registers in `EX`. If there is a match, it routes the newly computed value directly to the ALU inputs, bypassing the Register File in 0 cycles (`common/ForwardingUnit.hpp:45-80`).  
*Reference: Chapter E2*

#### Q18: Why does a Load-Use hazard still require 1 bubble even with forwarding?
**Answer**: Memory data from a `ld` instruction is only returned at the end of the `MA` stage. A dependent instruction entering `EX` needs the value at the start of `EX`. Because hardware cannot forward backwards in time, a 1-cycle bubble must be inserted (`common/HazardUnit.hpp:35-55`).  
*Reference: Chapter E2*

#### Q19: What is the branch penalty in 4-stage vs 6-stage?
**Answer**: In the 4-stage core, branches resolve at `EX` (stage 3), creating a **1-cycle flush penalty**. In the 6-stage core, branches resolve at `EX` (stage 4), creating a **2-cycle flush penalty** (`processor_6stage/Pipeline6Stage.hpp:180-210`).  
*Reference: Chapter D4*

#### Q20: Why is L1 split into ITLB/DTLB and I-Cache/D-Cache?
**Answer**: To eliminate the single-port structural hazard. In every cycle, `IF` fetches an instruction while `MA` may read/write data. Splitting L1 allows both stages to access memory simultaneously with zero stalls (`memory/MemoryHierarchy.hpp:35-60`).  
*Reference: Chapter H2*

#### Q21: What is a 2-bit bimodal branch predictor?
**Answer**: It uses a 2-bit saturating counter (`00` to `11`) for each branch address. It requires two consecutive mispredicts before changing direction, preventing single loop exit anomalies from flipping the prediction (`common/BranchPredictor.hpp:55-80`).  
*Reference: Chapter E3*

#### Q22: How does the basic-block instruction scheduler eliminate bubbles?
**Answer**: Enabled via `--schedule`, the assembler reorders independent instructions to fill the slot immediately following a `ld`, separating the load from its user and eliminating the load-use stall (`assembler/Assembler.hpp:210-250`).  
*Reference: Chapter C1*

#### Q23: What is the difference between Horizontal and Vertical microcode?
**Answer**: Horizontal microcode uses wide, unencoded 32-bit control words with zero decode delay. Vertical microcode encodes control lines into compact 16-bit fields, cutting control store ROM in half but requiring hardware decoders (`microcode/HorizontalControl.hpp` and `VerticalControl.hpp`).  
*Reference: Chapter F3*

#### Q24: What is an MSHR in a cache?
**Answer**: Miss Status Holding Register: a hardware queue that tracks in-flight cache misses, allowing subsequent memory hits to proceed without stalling the processor (`memory/Cache.hpp:45-75`).  
*Reference: Chapter H2*

#### Q25: How does the Stack Guard Page prevent memory corruption?
**Answer**: An inaccessible page (`Present=1, Read=0, Write=0`) is mapped beneath the stack space. Any stack overflow attempts to write into this page, immediately triggering an unmaskable exception trap (`memory/VirtualMemory.hpp:180-210`).  
*Reference: Chapter I2*

---

## Category 3: Hard Questions (Deep Microarchitecture)

#### Q26: Explain the mathematical proof of why 6-stage is faster despite higher CPI.
**Answer**: Using the Iron Law $T = (IC \times \text{CPI}) / f$, on `matrix_mult_long`: 4-stage takes $(189,050 \times 1.125) / 500\text{ MHz} = 425.4\ \mu\text{s}$. 6-stage takes $(189,050 \times 1.131) / 800\text{ MHz} = 267.3\ \mu\text{s}$. The 60% frequency increase overwhelms the 0.5% CPI increase, yielding a **1.59x wall-clock speedup** (`notes/parts/part_d_processors.md`).  
*Reference: Chapter D4*

#### Q27: How does the Page-Walk Cache (PWC) reduce 4-level translation latency by 74%?
**Answer**: 4-level Radix translation requires 4 DRAM accesses (PML4, PDPT, PD, PT). The PWC caches upper-level PML4 and PDPT pointers. On a PWC hit, the MMU skips levels 4 and 3, descending directly to the final page table in 1 memory lookup instead of 4 (`memory/VirtualMemory.hpp:345-365`).  
*Reference: Chapter H5*

#### Q28: How does Booth's Radix-4 multiplier halve partial products?
**Answer**: By scanning 3 overlapping bits ($y_{2i+1}, y_{2i}, y_{2i-1}$), each 2-bit step is recoded into $\{0, \pm M, \pm 2M\}$. This reduces the number of addition steps from 32 down to 16 (`alu/Multipliers.hpp:80-120`).  
*Reference: Chapter G2*

#### Q29: What is the FR-FCFS memory scheduling algorithm?
**Answer**: First-Ready First-Come First-Served: the memory controller prioritizes queued requests that target already-open DRAM row buffers over older requests targeting closed banks, converting 120-cycle row conflicts into 15-cycle row hits (`memory/DRAM.hpp:110-150`).  
*Reference: Chapter H4*

#### Q30: What caused the 2.560 CPI discrepancy in older documentation?
**Answer**: The 2.560 table was a placeholder written before the cycle-accurate pipelines and caches were built. Furthermore, short benchmarks suffer from cold-start compulsory TLB/cache misses (120 cycles) that inflate apparent CPI. On extended benchmarks, steady-state CPI settles at **1.02 to 1.15** (`docs/CPI_AUDIT.md:1-50`).  
*Reference: Chapter J2*

---

## Category 4: Examiner Trap Questions (Answer Honestly!)

#### Q31: "Does your CPU support Out-of-Order execution?"
**Answer**: **No.** RISC201 is strictly an in-order processor. Instructions enter, execute, and retire in strict program order. We mitigate hazards using data forwarding, prefetching, and compiler scheduling rather than out-of-order reorder buffers (`processor_4stage/Pipeline4Stage.hpp`).

#### Q32: "Does your processor support multi-core cache coherence (MESI)?"
**Answer**: **No.** RISC201 models a high-performance single-core processor. Multi-core snooping and directory-based coherence protocols like MESI are outside the project scope (`memory/MemoryHierarchy.hpp`).

#### Q33: "Did you synthesize this processor on an FPGA?"
**Answer**: **No.** This project is a cycle-accurate software simulator written in pure C++17. The 500 MHz and 800 MHz frequencies are derived from standard 28nm standard-cell gate delay models (`docs/PERFORMANCE.md`).

#### Q34: "Can an instruction write to a register and memory in the same instruction?"
**Answer**: **No.** RISC201 is a strict Load-Store architecture. Arithmetic instructions write only to registers; `st` writes only to memory. This keeps instruction execution uniform and single-cycle (`common/ISA.hpp`).

#### Q35: "Why do you use string opcodes instead of integer enums?"
**Answer**: String opcodes (`isa::OP_ADD = "00000"`) prevent silent typecasting bugs, mirror physical 5-bit hardware wire lines, and serve as a single source of truth across the assembler, ISS, pipelines, and visualizers without translation layers (`common/ISA.hpp`). Verified by `./lint_isa`.
