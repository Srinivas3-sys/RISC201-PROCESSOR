# Mock Vivas & Final Preparation Checklist

Run through these 3 simulated oral examinations (vivas) with a peer or in front of a mirror before your presentation.

---

## Mock Viva 1: The Fast-Paced Baseline Defense (10 Minutes)

* **Examiner**: "Welcome. In 30 seconds, explain what your project does."
  * **Model Answer**: "RISC201 is a cycle-accurate modern processor simulator written in pure C++17 modeling the SimpleRisc ISA. It compares a 4-stage core against an 800 MHz 6-stage core, backed by an advanced memory hierarchy including non-blocking caches, DDR4 DRAM, and 4-level Radix-4 Virtual Memory."
* **Examiner**: "Why did you separate instruction and data caches at L1?"
  * **Model Answer**: "To prevent the structural hazard where the `IF` stage fetches an instruction while the `MA` stage reads or writes data in the exact same clock cycle. Splitting L1 allows both stages to access memory simultaneously with zero stalls."
* **Examiner**: "Show me how `sub r1, r2, r3` executes in the 4-stage pipeline."
  * **Model Answer**: "Cycle 1: `IF` fetches the word from `PC`. Cycle 2: `OF` decodes opcode `'00001'` and reads `r2` and `r3` from the Register File. Cycle 3: `EX` subtracts `r3` from `r2` in the ALU. Cycle 4: `MA_RW` writes the result back into `r1`."
* **Examiner**: "What is the branch penalty in your 6-stage pipeline?"
  * **Model Answer**: "It is 2 cycles. Branches evaluate flags at `EX` (stage 4). If mispredicted, the two instructions fetched behind it in `IF` and `ID` are squashed into `nop` bubbles."

---

## Mock Viva 2: Deep Microarchitecture & Memory Defense (15 Minutes)

* **Examiner**: "Why is your 6-stage pipeline faster in real time if its CPI is higher?"
  * **Model Answer**: "Because execution time depends on both CPI and clock frequency: $T = (IC \times \text{CPI}) / f$. Splitting operand fetch into separate `ID` and `RF` stages cuts the critical path delay in half, allowing frequency to jump from 500 MHz to 800 MHz (a 1.6x increase). The tiny 0.5% CPI increase from the extra branch penalty cycle is overwhelmingly beaten by the 60% frequency boost, yielding a 1.59x wall-clock speedup."
* **Examiner**: "Explain how your memory controller handles a DRAM row conflict."
  * **Model Answer**: "When a request accesses an address in an already-active bank but in a different row, the controller must first issue a `PRECHARGE` command ($t_{RP}$) to close the active row, issue an `ACTIVATE` command ($t_{RCD}$) to latch the new row into the bank's Row Buffer, and then issue `READ` ($t_{CL}$). This takes ~120 cycles, compared to ~15 cycles for a row hit."
* **Examiner**: "What does your Page-Walk Cache do?"
  * **Model Answer**: "In a 4-level Radix page table, a TLB miss requires 4 sequential memory reads. The Page-Walk Cache caches intermediate PML4 and PDPT translation pointers. On a hit, the walker skips levels 4 and 3, descending to the final page table in 1 memory access instead of 4, cutting walk latency by 74%."

---

## Mock Viva 3: Software Tools & Edge Cases (10 Minutes)

* **Examiner**: "How does your assembler handle forward branches?"
  * **Model Answer**: "It uses two passes. Pass 1 calculates the memory address of every label and records them in a symbol table. Pass 2 emits machine code words, using the recorded label addresses to compute relative offsets: `(target - PC) / 4`."
* **Examiner**: "What prevents an infinite recursive function from crashing the simulator?"
  * **Model Answer**: "The Stack Guard Page. We map an unreadable and unwritable page directly beneath the stack boundary. When recursive calls push `sp` past the stack limit, the MMU catches the access, throws a `STACK_OVERFLOW_EXCEPTION`, and halts cleanly without corrupting heap or code memory."
* **Examiner**: "How do you know your pipelines don't have subtle bugs?"
  * **Model Answer**: "We built an unpipelined Golden Reference ISS (`common/ISS.hpp`). Our regression test suite runs 192 configurations against the ISS, comparing every register and memory word. All 231 golden checks pass with 100% agreement, and AddressSanitizer confirms zero memory leaks."

---

## Master "Can You Explain This?" Readiness Checklist

Tick each box once you can explain the concept out loud without looking at notes:

- [ ] **1. The Instruction Cycle**: Fetch $\rightarrow$ Decode $\rightarrow$ Execute $\rightarrow$ Writeback.
- [ ] **2. 4-Stage vs 6-Stage**: Where `OF` is split into `ID` and `RF`.
- [ ] **3. Why 6-Stage is Faster**: The math of 800 MHz vs 500 MHz ($1.59\times$ speedup).
- [ ] **4. RAW Data Hazards**: How `EX->EX` and `MEM->EX` forwarding paths bypass data in 0 cycles.
- [ ] **5. Load-Use Delay**: Why a load followed immediately by its user requires 1 bubble.
- [ ] **6. Branch Misprediction**: How squashing turns wrong-path instructions into bubbles.
- [ ] **7. 2-Bit Bimodal Predictor**: How saturating counters prevent loop exit mispredicts.
- [ ] **8. Split L1 Caches**: Why separate ITLB/DTLB and L1I/L1D prevents structural hazards.
- [ ] **9. DRAM Row Hit vs Conflict**: Why row hits take 15 cycles while conflicts take 120 cycles.
- [ ] **10. 4-Level Radix-4 Translation**: How a 48-bit address is sliced into PML4, PDPT, PD, PT, and Offset.
- [ ] **11. Page-Walk Cache**: Why caching upper directory pointers cuts walk latency by 74%.
- [ ] **12. Two-Pass Assembler**: Why Pass 1 is needed to resolve forward labels.
- [ ] **13. Full-Descending Stack**: Why `push` decrements `sp` and `pop` increments `sp`.
- [ ] **14. Stack Guard Page**: How boundary exceptions prevent memory corruption.
- [ ] **15. Single Source of Truth**: Why all opcodes are 5-bit strings in `common/ISA.hpp`.
- [ ] **16. The Cold-Start CPI Phenomenon**: Why short benchmarks show 6.1 CPI while long benchmarks show 1.12 CPI.
- [ ] **17. Horizontal vs Vertical Microcode**: 32-bit unencoded vs 16-bit encoded (50% ROM reduction).
- [ ] **18. Golden ISS**: How differential testing verifies pipelined execution correctness.
- [ ] **19. Live Demo Commands**: Ability to run `make demo-pipe`, `make demo-mem`, `make demo-vm` smoothly.
- [ ] **20. Project Limitations**: Confidently stating that the processor is in-order single-core without pretending otherwise.

