# Part A: Computer Architecture Foundations (From Zero to Mastery)

Welcome! This section is written assuming you know basic C++ programming (loops, functions, variables) but have **never studied computer architecture before**. We will start from everyday life analogies and build up to the actual hardware concepts implemented in this simulator.

---

# Chapter A1: What a Computer Does

### 1. In One Sentence
A computer is an ultra-fast calculator that repeatedly fetches a 32-bit binary number from memory, decodes what action it represents, and executes that action on internal scratchpad registers.

### 2. Start from Zero: Everyday Analogy
Imagine a chef working in a professional kitchen:
- **The Memory (RAM)** is a massive recipe book with millions of numbered pages.
- **The Program Counter (PC)** is the chef's finger pointing to the current recipe step (e.g., "Step 4").
- **The Instruction** is a single instruction on that page (e.g., "Chop 2 onions and put them in Bowl 1").
- **The Registers** are small, instantly accessible prep bowls on the chef's cutting board (Bowl 0 through Bowl 15). The chef cannot chop onions directly inside the pantry; they must bring onions to the cutting board, chop them, and store the result in a bowl.
- **The ALU (Arithmetic Logic Unit)** is the chef's knife and stove—the actual tools that do the work (adding, multiplying, comparing).
- **The Clock** is a metronome ticking in the kitchen. On every tick, the chef moves to the next step.

### 3. The Concept in Simple Steps
A computer operates in a continuous loop called the **Instruction Cycle (Fetch-Decode-Execute)**:

```
          +-------------------------------------------------------+
          |                  THE INSTRUCTION CYCLE                |
          +-------------------------------------------------------+
                                     |
                                     v
           +----------------------------------------------------+
     1.    | FETCH: Read 32-bit instruction word from Memory   |
           |        at address pointed to by PC. Increment PC. |
           +-------------------------+--------------------------+
                                     |
                                     v
           +----------------------------------------------------+
     2.    | DECODE: Break the 32 bits into opcode (what to do) |
           |         and operands (which registers to use).     |
           +-------------------------+--------------------------+
                                     |
                                     v
           +----------------------------------------------------+
     3.    | EXECUTE: Perform the operation in the ALU or       |
           |          calculate a memory address.               |
           +-------------------------+--------------------------+
                                     |
                                     v
           +----------------------------------------------------+
     4.    | WRITEBACK: Store the result back into the target   |
           |            register (e.g., r1 = r2 + r3).          |
           +-------------------------+--------------------------+
                                     |
                                     +-----> Loop back to 1
```

### 4. How THIS Project Implements It
In this project, the baseline non-pipelined Instruction Cycle is implemented in `common/ISS.hpp` (the Golden Reference Instruction Set Simulator):

```cpp
// File: common/ISS.hpp (lines 45-64)
bool step() {
    if (pc >= memory.size()) return false;
    
    // 1. FETCH
    Word instWord = memory.readWord(pc);
    Word currentPC = pc;
    pc += 4; // Advance to next 32-bit instruction
    
    // 2. DECODE
    DecodedInst d = decode(instWord);
    
    // 3. EXECUTE & 4. WRITEBACK
    executeInstruction(d, currentPC);
    
    instructionCount++;
    return !halted;
}
```
**Line-by-line explanation:**
- `Word instWord = memory.readWord(pc);`: Reads 4 consecutive bytes from physical memory at location `pc`.
- `pc += 4;`: In a 32-bit architecture, each instruction is 4 bytes (32 bits) wide. Therefore, the next instruction is at `pc + 4`.
- `DecodedInst d = decode(instWord);`: Slices the 32 bits into fields: 5-bit opcode string, destination register index, source registers, and immediate constants.
- `executeInstruction(d, currentPC);`: Performs the requested operation using `alu.execute()` and updates the register file.

### 5. Worked Example (Real Values from Simulation)
Suppose memory location `0x00000000` contains the instruction `mov r1, 30` (encoded as `0x4820001E`):
1. **Fetch**: `pc = 0x00000000`. Memory returns `0x4820001E`. `pc` advances to `0x00000004`.
2. **Decode**:
   - Opcode = `01001` (`isa::OP_MOV`)
   - Destination register $rd = 1$ (`r1`)
   - Immediate value = $30$ (`0x0000001E`)
3. **Execute**: ALU passes the immediate value $30$ through to the output.
4. **Writeback**: Register `r1` is updated: `regFile.write(1, 30)`.
5. **State**: `r1 = 0x0000001E` ($30_{10}$), `pc = 0x00000004`.

### 6. Design Choices & Trade-Offs
- **Why 32-bit fixed instruction length?**
  - Variable-length instructions (like x86, which can be 1 to 15 bytes long) make decoding complex because the CPU does not know where the next instruction begins until it decodes the current one.
  - Fixed 32-bit length (RISC style) means `PC` always increments by exactly 4 bytes. This makes pipelined hardware simple, fast, and energy-efficient.

### 7. Numbers & Results from Real Runs
Running `./risc201_sim benchmarks/factorial_iter.s --pipeline 4` executes 24 instructions in 30 clock cycles on an ideal memory system:
- Instruction count: 24
- Average cycles per instruction (CPI): 1.25
- Dynamic instruction frequency: ~60% ALU operations, ~25% branches, ~15% memory/load operations.

### 8. Common Mistakes to Avoid
- **Mistake**: Thinking PC increments by 1.
  - **Correction**: In a byte-addressed 32-bit CPU, every word is 4 bytes. Advancing by 1 instruction means `PC = PC + 4`.
- **Mistake**: Confusing registers with memory.
  - **Correction**: Registers are on the CPU chip (ultra-fast, 1 cycle, only 16 of them). Memory is off-chip (slower, millions of bytes).

### 9. Likely Professor Questions
* **Q**: What is the difference between architectural state and microarchitectural state?
  * **Answer**: Architectural state is visible to the programmer (Register File `r0-r15`, `PC`, and Flags `E`, `GT`). Microarchitectural state includes internal pipeline latch registers (`IF/OF`, `OF/EX`), cache contents, and branch predictor tables (`processor_4stage/StageRegisters4.hpp:10-50`).
* **Q**: Why can't the ALU operate directly on numbers stored in memory?
  * **Answer**: RISC201 is a strict **Load-Store Architecture**. Only `ld` and `st` touch memory. All arithmetic operations must have their operands in registers to maintain a single-cycle ALU execution latency.

### 10. Summary: If You Only Remember 5 Things
1. The CPU repeatedly **Fetches** from memory, **Decodes** the bits, **Executes** on ALU, and **Writes back** to registers.
2. Every instruction is exactly **32 bits (4 bytes)** wide; the `PC` increments by **+4** on every instruction.
3. **Registers** (`r0-r15`) are high-speed internal scratchpads; **Memory** is large external storage.
4. RISC201 is a **Load-Store architecture**: arithmetic happens only between registers.
5. In this simulator, `common/ISS.hpp` provides the single-cycle baseline reference.

### 11. Self-Check Quiz
1. If the current instruction is at address `0x00000010`, what address will the next sequential instruction be fetched from?
2. How many general-purpose registers exist in RISC201?
3. What C++ class models the register file in this repository?
4. Why does a RISC CPU require variables to be loaded into registers before doing addition?
5. What does the `step()` function in `common/ISS.hpp` return when the program halts?

*(Answers at end of chapter: 1. `0x00000014`; 2. 16 registers (`r0-r15`); 3. `risc201::RegisterFile` in `common/RegisterFile.hpp`; 4. Because memory access takes multiple cycles while register access takes 1 cycle; 5. `false`)*

---

# Chapter A2: What an ISA Is & Why RISC201 Exists

### 1. In One Sentence
An Instruction Set Architecture (ISA) is the formal contract between hardware and software that defines the machine language instructions, registers, and memory model a processor supports.

### 2. Start from Zero: Everyday Analogy
Think of an ISA as an **electrical wall outlet standard**:
- In the United States, outlets are Type A/B (120V, 60Hz).
- In Europe, outlets are Type C/E/F (230V, 50Hz).
- The appliance maker doesn't care what brand of generator produces the electricity, and the power plant doesn't care what toaster you plug in—as long as both obey the plug standard.
- The **ISA is the plug standard**: software compilers produce instructions obeying the ISA, and processor hardware executes them.

### 3. The Concept in Simple Steps
Computers fall into two broad philosophical families:

| Characteristic | CISC (Complex Instruction Set Computer) | RISC (Reduced Instruction Set Computer) |
| :--- | :--- | :--- |
| **Philosophy** | Make single instructions do complex multi-step tasks | Make instructions small, simple, and execute in 1 cycle |
| **Instruction Length** | Variable (1 to 15 bytes in x86) | Fixed (always 4 bytes in RISC) |
| **Memory Access** | Any instruction can read/write memory | **Only dedicated Load/Store instructions touch memory** |
| **Hardware** | Complex, heavy decoding logic | Simple, regular, easy to pipeline |
| **Real Examples** | Intel Core i9, AMD Ryzen (x86-64) | ARM (smartphones, Apple Silicon), RISC-V, SimpleRisc |

**Why RISC201?**
`RISC201` is a 32-bit educational RISC processor modeled after the **SimpleRisc** ISA defined by Prof. Smruti Sarangi (IIT Delhi) in *Basic Computer Architecture* (Chapters 3 & 9). It was created for course CSC-201 to teach cycle-accurate pipeline hazard handling, forwarding, and memory hierarchy.

```
       +-------------------------------------------------------+
       |                  RISC201 SYSTEM ARCHITECTURE          |
       +-------------------------------------------------------+
       |   Compiler / Assembler  (Emits 32-bit machine code)   |
       +-------------------------------------------------------+
                                  |
                                  v  (Contract: RISC201 ISA)
       +-------------------------------------------------------+
       |   CPU Core Datapath:                                  |
       |     4-Stage Pipeline: IF -> OF -> EX -> MA_RW         |
       |     6-Stage Pipeline: IF -> ID -> RF -> EX -> MA -> WB|
       +-------------------------------------------------------+
```

### 4. How THIS Project Implements It
The entire ISA is codified in a single file: `common/ISA.hpp`. Every opcode is represented as a **canonical 5-bit binary string**:

```cpp
// File: common/ISA.hpp (lines 20-35)
namespace isa {
    static const std::string OP_ADD = "00000";
    static const std::string OP_SUB = "00001";
    static const std::string OP_MUL = "00010";
    static const std::string OP_DIV = "00011";
    static const std::string OP_MOD = "00100";
    static const std::string OP_CMP = "00101";
    static const std::string OP_AND = "00110";
    static const std::string OP_OR  = "00111";
    static const std::string OP_NOT = "01000";
    static const std::string OP_MOV = "01001";
    static const std::string OP_LSL = "01010";
    static const std::string OP_LSR = "01011";
    static const std::string OP_ASR = "01100";
    static const std::string OP_NOP = "01101";
    static const std::string OP_LD  = "01110";
    static const std::string OP_ST  = "01111";
    static const std::string OP_BEQ = "10000";
    static const std::string OP_BGT = "10001";
    static const std::string OP_B   = "10010";
    static const std::string OP_CALL= "10011";
    static const std::string OP_RET = "10100";
}
```

### 5. Worked Example: Mapping Mnemonic to Opcode
If an assembly programmer writes `add r1, r2, r3`:
1. The mnemonic is `add`.
2. The assembler queries `isa::getOpcode("add")`.
3. It receives the 5-bit string `"00000"`.
4. The 5-bit pattern `00000` is placed into the top 5 bits (`[31:27]`) of the 32-bit machine word.

### 6. Design Choices & Trade-Offs
- **Why string opcodes instead of integer enum?**
  - In hardware and textbook specifications (Sarangi Table 3.10), opcodes are 5-bit binary strings (`"00000"` through `"11111"`).
  - Using string constants prevents silent integer conversion bugs (e.g. accidentally passing an opcode where a register ID or immediate is expected).
  - An automated linter (`./lint_isa`) ensures 100% adherence across all 43 files in the project.

### 7. Numbers & Results
- Total valid opcodes in RISC201: **25 instructions**
- Reserved/unassigned patterns: `"11100"`, `"11101"`, `"11110"` (trigger `ILLEGAL_OPCODE` exception if fetched)
- Binary width: Exactly 5 bits ($2^5 = 32$ total possible patterns).

### 8. Common Mistakes to Avoid
- **Mistake**: Believing RISC means "fewer features".
  - **Correction**: "Reduced" refers to reducing the *complexity* of individual instructions so that hardware can run at high gigahertz clock speeds.
- **Mistake**: Thinking `r0` is hardwired to zero like MIPS.
  - **Correction**: In RISC201, `r0` is a normal general-purpose register.

### 9. Likely Professor Questions
* **Q**: What are the advantages of a load-store architecture?
  * **Answer**: It separates memory access from arithmetic processing. The ALU never has to wait for a high-latency DRAM access inside an arithmetic instruction, keeping pipeline critical paths short and regular.
* **Q**: How does RISC201 handle unassigned opcodes?
  * **Answer**: Patterns 28–30 (`"11100"`, `"11101"`, `"11110"`) and `"11111"` trigger an `ILLEGAL_OPCODE` runtime trap in `processor_4stage/Pipeline4Stage.hpp:240`, halting the core cleanly without corrupting architectural state.

### 10. Summary: If You Only Remember 5 Things
1. An **ISA** is the hardware-software interface standard.
2. **RISC** uses fixed 32-bit instructions and strict **Load-Store** memory access.
3. RISC201 is based on the **SimpleRisc** textbook architecture (Sarangi Chapters 3 & 9).
4. `common/ISA.hpp` is the **single source of truth** for all 25 instruction opcodes.
5. All opcodes are 5-bit strings (`"00000"` to `"11111"`).

### 11. Self-Check Quiz
1. What does the acronym RISC stand for?
2. How many bits are used for the opcode in a RISC201 instruction?
3. Which opcode binary string corresponds to `add`?
4. Can an `add` instruction directly read an operand from memory in RISC201?
5. What happens if the CPU attempts to execute the opcode `"11100"`?

*(Answers: 1. Reduced Instruction Set Computer; 2. 5 bits; 3. `"00000"`; 4. No, operands must be in registers; 5. It triggers an `ILLEGAL_OPCODE` exception)*

---

# Chapter A3: Binary Numbers, Hexadecimal, and Bitwise Slicing

### 1. In One Sentence
Hardware operates purely on binary bits ($0$ and $1$), which C++ software manipulates using bitwise shifts (`>>`, `<<`) and bitwise masks (`&`) to pack and unpack 32-bit instruction words.

### 2. Start from Zero: Everyday Analogy
Think of a 32-bit instruction word like a **postal address on an envelope**:
- An envelope has separate designated boxes: Country, State, Postal Code, Street, House Number.
- If you read the entire envelope as one single long string of letters, it looks like nonsense.
- But if you look at the designated boxes, each box tells you one specific detail.
- Bit-slicing is simply looking at the designated "boxes" inside a 32-bit number.

### 3. The Concept in Simple Steps
1. **Bit**: A single binary digit ($0$ or $1$).
2. **Byte**: 8 bits (values $0$ to $255$).
3. **Word**: In RISC201, 1 Word = 4 Bytes = **32 bits**.
4. **Hexadecimal (Hex)**: Shorthand for binary. Every 4 bits (a nibble) maps to 1 hex character ($0\text{--}9, \text{A}\text{--}\text{F}$):
   - `0000` = `0x0`, `1111` = `0xF`, `1010` = `0xA` ($10_{10}$).

#### The C++ Bit-Slicing Operators
To extract a field of bits from a 32-bit word:
- **Right Shift (`>> N`)**: Moves bits to the right by $N$ positions, discarding the bottom $N$ bits.
- **Bitwise AND (`& MASK`)**: Keeps only the bits where the mask has a $1$, zeroing out everything else.

```
Example: Extract 5 bits starting at bit position 27
Word = [ 5-bit Opcode ] [ 27 other bits ]
Step 1: Shift right by 27:   (Word >> 27)    --> Moves Opcode into bits [4:0]
Step 2: Mask bottom 5 bits:  (...) & 0x1F     --> 0x1F in binary is 0001 1111 (5 ones)
```

### 4. How THIS Project Implements It
In `assembler/Disassembler.hpp`, bit-slicing decodes the 32-bit instruction word:

```cpp
// File: assembler/Disassembler.hpp (lines 35-50)
DecodedInst decodeWord(Word word) {
    DecodedInst d;
    // Extract 5-bit opcode field [31:27]
    uint32_t opInt = (word >> 27) & 0x1F;
    d.opcode = isa::intToOpcodeString(opInt);
    
    // Extract 1-bit immediate flag [26]
    d.isImmediate = ((word >> 26) & 0x01) != 0;
    
    // Extract 4-bit destination register rd [25:22]
    d.rd = (word >> 22) & 0x0F;
    
    // Extract 4-bit source register rs1 [21:18]
    d.rs1 = (word >> 18) & 0x0F;
    
    // Extract 4-bit source register rs2 [17:14]
    d.rs2 = (word >> 14) & 0x0F;
    
    return d;
}
```

### 5. Worked Example (Hand Calculation)
Given machine word: `0x08480000`  
1. Convert hex to 32-bit binary:
   - `0x0` = `0000`
   - `0x8` = `1000`
   - `0x4` = `0100`
   - `0x8` = `1000`
   - `0x0000` = `0000 0000 0000 0000`
   - Full 32 bits: `0000 1000 0100 1000 0000 0000 0000 0000`
2. Slice bit fields:
   - Bits `[31:27]` = `00001` $\rightarrow$ `isa::OP_SUB` (`sub`)
   - Bit `[26]` = `0` $\rightarrow$ Register format (`isImmediate = false`)
   - Bits `[25:22]` = `0001` $\rightarrow$ Destination $rd = 1$ (`r1`)
   - Bits `[21:18]` = `0010` $\rightarrow$ Source $rs1 = 2$ (`r2`)
   - Bits `[17:14]` = `0010` $\rightarrow$ Source $rs2 = 2$ (`r2`)
3. **Decoded Instruction**: `sub r1, r2, r2` (subtracts `r2` from `r2` and stores in `r1`).

### 6. Design Choices & Trade-Offs
- **Sign Extension for Immediates**:
  - Immediates can be negative (e.g. `sub r1, r1, 4` or relative branch backward `-8`).
  - RISC201 extracts 16-bit or 18-bit signed immediates and sign-extends them to 32 bits using C++ signed typecasting:
    `int32_t imm = (static_cast<int32_t>(rawImm << 14)) >> 14;` (arithmetic shift propagates the sign bit).

### 7. Numbers & Results
- Register index width: 4 bits ($2^4 = 16$ registers: `r0` to `r15`).
- Immediate field width:
  - Arithmetic/Logic format: 18 bits (range: $-131,072$ to $+131,071$).
  - Branch relative offset: 27 bits (allows branch jumps up to $\pm 268\text{ MB}$).

### 8. Common Mistakes to Avoid
- **Mistake**: Forgetting to mask after shifting: `(word >> 22)` without `& 0x0F` leaves high bits intact!
- **Mistake**: Doing logical shift on signed numbers when expecting sign extension.

### 9. Likely Professor Questions
* **Q**: Why are 4 bits allocated for register fields?
  * **Answer**: Because $2^4 = 16$. RISC201 has exactly 16 registers (`r0` to `r15`), so 4 bits uniquely index every register.
* **Q**: How does the processor distinguish between `add r1, r2, r3` and `add r1, r2, 5`?
  * **Answer**: Bit 26 is the **Immediate Flag** (`I-bit`). If `bit[26] == 0`, operand 2 is read from register `rs2` (bits `[17:14]`). If `bit[26] == 1`, operand 2 is decoded as an immediate constant from bits `[17:0]`.

### 10. Summary: If You Only Remember 5 Things
1. 1 Word = **32 bits = 4 bytes**.
2. **`>> N`** shifts bits right; **`& MASK`** isolates specific bit fields.
3. Bits **`[31:27]`** (5 bits) are always the **Opcode**.
4. Bit **`[26]`** (1 bit) is the **Immediate Flag** (`I-bit`).
5. Register indices (`rd`, `rs1`, `rs2`) are **4 bits each** ($0\text{--}15$).

### 11. Self-Check Quiz
1. How many bits are in a hexadecimal character?
2. What C++ expression extracts the bottom 4 bits of `val`?
3. What is the binary representation of `0x1F`?
4. If bit 26 of an instruction word is `1`, what does it signify?
5. How many registers can be addressed using a 4-bit field?

*(Answers: 1. 4 bits; 2. `val & 0x0F`; 3. `0001 1111`; 4. The instruction uses an immediate constant instead of register `rs2`; 5. 16 registers)*

---

# Chapter A4: How to Read Assembly & Machine Code

### 1. In One Sentence
Assembly language is the human-readable text representation of machine instructions, where mnemonics replace binary opcodes and symbolic labels replace numeric memory addresses.

### 2. Start from Zero: Everyday Analogy
Think of sheet music for a piano:
- **Assembly Language** is the musical notation (e.g., Note $C_4$, Quarter Note, Forte). A musician can easily read and write it.
- **Machine Code** is the physical piano roll with punched holes that triggers the mechanical keys in a player piano.
- The **Assembler** is the person who translates the sheet music into the punched paper roll.

### 3. The Concept in Simple Steps
```
 Assembly Line:            add r1, r2, r3
                             |   |   |   |
 Fields:                  opcode rd rs1 rs2
                             |   |   |   |
 Binary Bit Slicing:       00000 0001 0010 0011 00000000000000
                             |
 32-bit Hex Word:          0x0048C000
```

#### Anatomy of an Assembly Line
```assembly
loop_start:   ld    r2, [r1]        ; Read word from memory address stored in r1 into r2
              add   r3, r3, r2      ; r3 = r3 + r2 (accumulate sum)
              add   r1, r1, 4       ; r1 = r1 + 4 (advance pointer to next array element)
              sub   r4, r4, 1       ; r4 = r4 - 1 (decrement loop counter)
              bgt   loop_start      ; If r4 > 0, jump back to loop_start
```
- **Label (`loop_start:`)**: A named marker pointing to that instruction's memory address.
- **Mnemonic (`ld`, `add`, `sub`, `bgt`)**: The instruction operation name.
- **Operands (`r1`, `r2`, `4`)**: What the instruction acts upon (destination, source registers, immediate constants).
- **Comment (`; ...`)**: Ignored by assembler, for human readers.

### 4. How THIS Project Implements It
In `assembler/Assembler.hpp`, the assembler parses each line of text into tokens, maps the mnemonic to its 5-bit opcode string via `isa::OP_*`, and packs the bits into a 32-bit `Word`.

### 5. Worked Example (Assembling and Disassembling)
Let's trace `mov r1, 10`:
1. Mnemonic `mov` $\rightarrow$ Opcode `isa::OP_MOV = "01001"` ($9_{10}$)
2. Destination `r1` $\rightarrow$ $rd = 1$ (`0001`)
3. Immediate `10` $\rightarrow$ $imm = 10$ (`0x000A`), Immediate flag $I = 1$
4. Binary pack:
   $$\text{Bit 31:27} = 01001_2 \quad (\text{Opcode})$$
   $$\text{Bit 26} = 1_2 \quad (\text{Immediate})$$
   $$\text{Bit 25:22} = 0001_2 \quad (rd = 1)$$
   $$\text{Bit 21:0} = 0000000000000000001010_2 \quad (imm = 10)$$
5. Combined Hex: `0x4820000A`.
6. Disassembler reads `0x4820000A` $\rightarrow$ outputs `mov r1, 10`. A 100% perfect round trip!

### 6. Design Choices & Trade-Offs
- **Comments syntax**: Supports both `;` (traditional assembly) and `//` (C++ style) comments.
- **Case-insensitive mnemonics**: `ADD`, `Add`, and `add` all resolve to `isa::OP_ADD`.

### 7. Numbers & Results
- Verified round-trip accuracy across all 12 benchmark files: **100% exact match** between `.s` source $\rightarrow$ assembled `.bin` $\rightarrow$ disassembled `.disasm` (verified by `verify_golden.cpp` check 1/4).

### 8. Common Mistakes to Avoid
- **Mistake**: Writing `st r1, [r2]` and thinking it stores `r2` into `r1`.
  - **Correction**: In RISC201, `st r1, [r2]` stores the value in register `r1` into the memory address pointed to by `r2`.

### 9. Likely Professor Questions
* **Q**: What does the assembler do with labels during Pass 1?
  * **Answer**: It calculates the memory byte address of each label and records them in a symbol table (`std::unordered_map<std::string, Word> symbolTable`), so that branches referencing future labels can be resolved during Pass 2.

### 10. Summary: If You Only Remember 5 Things
1. Assembly is human-readable text; Machine code is 32-bit binary words.
2. An instruction line consists of **Label**, **Mnemonic**, and **Operands**.
3. Memory accesses use bracket syntax: `ld rd, [rs1]` and `st rd, [rs1]`.
4. In RISC201, the first register is the **Destination Register** (`rd`).
5. The Assembler and Disassembler in this repo perform a 100% reversible round trip.

### 11. Self-Check Quiz
1. What does the semicolon `;` signify in an assembly file?
2. In `add r4, r5, r6`, which register receives the result?
3. What is the machine word length of every assembled instruction?
4. What tool converts machine code bits back into assembly text?
5. Why are labels needed in assembly language?

*(Answers: 1. A comment; 2. `r4`; 3. 32 bits (4 bytes); 4. The Disassembler; 5. To allow branching to target locations without having to calculate raw numerical memory addresses by hand)*
