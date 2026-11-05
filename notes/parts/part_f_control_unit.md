# Part F: The Control Unit (Hardwired vs Microprogrammed)

This section explains processor control units from scratch, contrasts hardwired control with microprogrammed control, explores Horizontal and Vertical control words, and demonstrates the interactive microPC stepping engine.

---

# Chapter F1: Hardwired vs Microprogrammed Control

### 1. In One Sentence
The control unit is the brain of the processor that directs traffic by asserting enable signals on registers, multiplexers, and ALU operation lines, implemented either as fast fixed logic gates (Hardwired) or as a miniature internal computer running microcode from ROM (Microprogrammed).

### 2. Start from Zero: Everyday Analogy
Think of a **mechanical music box vs an electronic synthesizer**:
- **Hardwired Control (The Music Box)**:
  - Inside a music box, a metal cylinder has physical brass pins welded onto its surface. As it spins, the pins pluck steel combs directly.
  - It is blindingly fast and has zero software overhead.
  - But if you want to change the song or fix a wrong note, **you must melt down the brass cylinder and rebuild the entire box**!
- **Microprogrammed Control (The Synthesizer)**:
  - A keyboard synthesizer reads musical notes from an internal digital memory chip (ROM).
  - It takes a few extra clock cycles to read the memory and decode the sound.
  - But if you want to add a new song or fix a note, **you just update the ROM memory**!

---

### 3. Comparison: Hardwired vs Microprogrammed

| Feature | Hardwired Control Unit | Microprogrammed Control Unit |
| :--- | :--- | :--- |
| **Implementation** | Fixed combinational logic gates & flip-flop state machines | Control Memory ROM storing microinstructions |
| **Speed** | **Ultra-fast (1 clock cycle)** | Slower (requires ROM lookup latency) |
| **Silicon Area** | Grows rapidly with complex ISAs | Regular, compact, predictable array structure |
| **Modification / Bug Fixes**| Impossible without re-fabricating silicon chips | Easy: patch the microcode ROM |
| **Best Suited For** | Modern high-speed RISC processors | Complex CISC processors (x86) and legacy mainframes |
| **Location in Project** | `processor_4stage/` and `processor_6stage/` | `microcode/MicroController.hpp` |

---

# Chapter F2: Microinstructions, MicroPC, and Dispatch

### 1. In One Sentence
In a microprogrammed processor, every high-level machine instruction (like `add` or `ld`) triggers an internal micro-routine executed step-by-step by an internal Micro-Program Counter (`microPC`).

### 2. The Concept in Simple Steps

```
  High-Level Machine Instruction: add r1, r2, r3 (Opcode: "00000")
                                  |
                                  v
  DISPATCH ROM: Opcode "00000" maps to starting MicroPC = 0x04
                                  |
                                  v
  CONTROL STORE ROM (Microcode Memory):
  +--------------------------------------------------------------------+
  | MicroPC | Micro-Operation               | Next MicroPC Address     |
  +---------+-------------------------------+--------------------------+
  |  0x00   | FETCH: Read inst from PC      | Next -> 0x01             |
  |  0x01   | DECODE: Read Opcode & Regs    | Dispatch to Opcode addr  |
  |  ...    |                               |                          |
  |  0x04   | EXEC: ALU Add; Assert RegWr   | Return to FETCH (0x00)   |
  +--------------------------------------------------------------------+
```

1. **MicroPC**: Points to the currently executing microinstruction inside Control Memory.
2. **Dispatch Table**: Maps the 5-bit opcode string directly to the starting microPC address.
3. **Sequencing**: Each microinstruction specifies its successor: `NEXT` (microPC + 1), `BRANCH_ON_CONDITION`, or `FETCH` (restart instruction cycle).

---

# Chapter F3: Horizontal vs Vertical Microcode

### 1. In One Sentence
Horizontal microcode uses wide, unencoded control words where each bit directly drives a hardware control wire, whereas Vertical microcode encodes control signals into compact bit fields that require hardware decoders.

---

### 2. Comparison: Word Geometry and Trade-Offs

```
1. Horizontal Control Word (32 bits, unencoded):
 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|PC|IR|Reg|Reg|ALU|ALU|ALU|ALU|Mem|Mem|Br |Br |Flag|...       | Next MicroPC Address (6 bits)    |
|Wr|Wr|Rd |Wr |Op3|Op2|Op1|Op0|Rd |Wr |Tk |Cnd|Wr |          |                                  |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
(Every bit is a dedicated physical copper wire!)

2. Vertical Control Word (16 bits, encoded with decoders):
 15       12 11      9 8       6 5     4 3       0
+-----------+---------+---------+-------+---------+
| ALU Field | Reg Dst | Mem Cmd | BrCmd | NextAddr|
|  (4 bits) | (3 bits)| (3 bits)|(2 bits)| (4 bits)|
+-----------+---------+---------+-------+---------+
     |
     v Hardware Decoder (4-to-16 line decoder)
     +--> 16 individual control lines
```

#### Detailed Trade-Off Matrix

| Metric | Horizontal Microcode (`HorizontalControl.hpp`) | Vertical Microcode (`VerticalControl.hpp`) |
| :--- | :--- | :--- |
| **Word Width** | **32 bits** (Wide) | **16 bits** (Narrow, 50% smaller!) |
| **Control Store ROM** | Large ($128\text{ words} \times 32\text{ bits} = 512\text{ bytes}$) | Compact ($128\text{ words} \times 16\text{ bits} = 256\text{ bytes}$) |
| **Decode Logic** | **Zero logic delay** (direct connection to gates) | Requires hardware decoders ($+0.4\text{ ns}$ delay) |
| **Concurrency** | Multiple independent functional units can fire at once | Limited to actions allowed by encoded fields |
| **Simplicity** | High conceptual simplicity | Requires careful field partitioning |

---

### 3. How THIS Project Implements It (`microcode/MicroController.hpp`)
In `debugger/Debugger.hpp`, switching to microcoded mode allows stepping microcycle by microcycle:

```bash
$ ./risc201_sim benchmarks/factorial_iter.s -i
(risc201-dbg) mode micro
Switched to MICROCODED mode.
(risc201-dbg) step 1
MicroCycle 1: microPC=0x00 [FETCH] IR <= Mem[PC], PC <= PC + 4
(risc201-dbg) step 1
MicroCycle 2: microPC=0x01 [DECODE] Dispatching opcode "01001" (mov) -> target microPC=0x18
(risc201-dbg) step 1
MicroCycle 3: microPC=0x18 [EXEC_MOV] Reg[r1] <= 30, Next -> 0x00 (FETCH)
```

---

### 4. Summary: If You Only Remember 5 Things
1. **Hardwired** uses logic gates (faster); **Microprogrammed** uses ROM memory (flexible).
2. The **microPC** sequences micro-operations inside the control store.
3. **Horizontal microcode** is **32 bits wide, unencoded, and fast**.
4. **Vertical microcode** is **16 bits wide, encoded, and saves 50% ROM space**.
5. The interactive debugger (`mode micro`) lets you step through internal microPC states one by one.

