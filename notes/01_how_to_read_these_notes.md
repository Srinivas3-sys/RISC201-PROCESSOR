# Study Guide & Revision Plans

Welcome to the RISC201 study notes! This guide is designed to help you prepare whether you have 1 day, 3 days, or 1 week before your presentation or examination.

---

## 1. Suggested Reading Order

If you are starting with zero computer architecture background:
1. **Start with Foundations**: Read `notes/parts/part_a_foundations.md` (A1 to A4). This establishes mental models for the Instruction Cycle, the Register File, and bit-slicing.
2. **Understand the ISA & Assembly**: Read `notes/parts/part_b_isa.md` and `notes/parts/part_c_software_tools.md`. Learn how `mov`, `add`, `cmp`, and `ld`/`st` work, and why opcodes are 5-bit binary strings.
3. **Master Pipelining & Hazards**: Read `notes/parts/part_d_processors.md` and `notes/parts/part_e_hazards_forwarding.md`. This is 50% of what examiners ask about!
4. **Master the Memory Subsystem**: Read `notes/parts/part_h_memory.md`. Focus on Tag/Index/Offset address slicing and why L1 is split (Harvard) while L2 is unified.
5. **Practice Whiteboard Examples**: Practice the 10 hand-worked problems in `notes/worked_examples/WORKED_EXAMPLES.md`.

---

## 2. Revision Timetables

### Plan A: The 1-Day Emergency Crash Course (Night Before Presentation)
* **Hour 1 (08:00 - 09:00)**: Read `notes/cheat_sheets/CHEAT_SHEETS.md` (all 9 summary cards). Memorize the formulas and pipeline stages.
* **Hour 2 (09:00 - 10:00)**: Read `notes/parts/part_d_processors.md` (4-stage vs 6-stage) and Chapter D4 (why 6-stage is faster at 800 MHz despite higher CPI).
* **Hour 3 (10:00 - 11:00)**: Read `notes/parts/part_e_hazards_forwarding.md`. Memorize `EX->EX` forwarding and why load-use needs 1 bubble.
* **Hour 4 (11:00 - 12:00)**: Practice the Live Demo: run `make demo-pipe`, `make demo-mem`, and `make demo-vm`. Memorize what each screen shows.
* **Hour 5 (13:00 - 14:00)**: Read `notes/qa/120_EXAMINER_QUESTIONS.md` (focus on the "Hard" and "Trap" questions).
* **Hour 6 (14:00 - 15:00)**: Read `notes/presentation/SLIDES_AND_SCRIPTS.md`. Practice the 5-minute speaker script out loud in front of a mirror twice!

---

### Plan B: The 3-Day Comprehensive Preparation
* **Day 1: Foundations & Processors**:
  - Read Parts A, B, and C (Foundations, ISA, Two-Pass Assembler).
  - Read Part D (4-Stage vs 6-Stage Processors and Golden ISS).
  - Trace Example 1 and Example 2 in `notes/worked_examples/WORKED_EXAMPLES.md`.
* **Day 2: Hazards, Control Unit, and Memory**:
  - Read Part E (Hazards, Forwarding, 2-Bit Bimodal Predictor).
  - Read Part F (Horizontal vs Vertical Microcode).
  - Read Part H (Caches, DRAM Row Hits/Conflicts, Radix-4 Virtual Memory).
  - Practice Example 3 (Cache Slicing) and Example 4 (Page Walk) on paper.
* **Day 3: Performance, Testing, and Presentation Practice**:
  - Read Part J (CPI Stack, Cold-Start vs Steady-State) and Part K (Test suite and limitations).
  - Run all demo targets in terminal (`make demo-pipe`, `make demo-mem`, `make demo-vm`).
  - Read the 120 Examiner Questions in `notes/qa/120_EXAMINER_QUESTIONS.md`.
  - Rehearse the slide presentation and demo script with your teammate.

---

### Plan C: The 7-Day Mastery Curriculum
* **Day 1**: Part A (A1–A4) + Part B (B1–B2).
* **Day 2**: Part C (C1–C4) + Code Walkthrough of `Assembler.hpp` and `ISA.hpp`.
* **Day 3**: Part D (D1–D5) + Code Walkthrough of `Pipeline4Stage.hpp` and `Pipeline6Stage.hpp`.
* **Day 4**: Part E (E1–E5) + Code Walkthrough of `ForwardingUnit.hpp` and `HazardUnit.hpp`.
* **Day 5**: Part F (F1–F6) + Part G (G1–G3) + Code Walkthrough of `MicroController.hpp` and `ALU.hpp`.
* **Day 6**: Part H (H1–H6) + Part I (I1–I3) + Code Walkthrough of `Cache.hpp`, `DRAM.hpp`, `VirtualMemory.hpp`.
* **Day 7**: Full Mock Viva (run the 3 mock vivas in `notes/self_test/MOCK_VIVAS.md`) + Live Demo dress rehearsal.

