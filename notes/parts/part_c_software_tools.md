# Part C: Software Toolchain & Development Tools

This section explains the software tools provided in the repository: the two-pass assembler (`risc201_as`), the disassembler (`risc201_disasm`), the stack macro preprocessor, and the interactive simulator CLI (`risc201_sim`).

---

# Chapter C1: The Two-Pass Assembler

### 1. In One Sentence
The two-pass assembler translates human-readable assembly text files (`.s`) into executable 32-bit machine code binary files (`.bin` and `.hex`), resolving forward branch labels in two distinct scans.

### 2. Start from Zero: Everyday Analogy
Imagine writing a table of contents for an unwritten book:
- You cannot write the page numbers for Chapter 5 while you are still on Chapter 1 because you don't know how long Chapters 2, 3, and 4 will be!
- **Pass 1 (Drafting)**: You write the book from start to finish. Whenever you start a chapter, you write down its starting page number in a notebook (the **Symbol Table**).
- **Pass 2 (Publishing)**: You flip back to the Table of Contents, look up the page numbers in your notebook, and print the finished book with exact page numbers.

### 3. The Concept in Simple Steps

```
                              [ Source Code (.s) ]
                                       |
                                       v
        +-------------------------------------------------------------+
        | PASS 1: Symbol Discovery & Address Assignment                |
        | - Read file line by line.                                   |
        | - Track current memory address (Address Counter += 4).       |
        | - When a label is found (e.g., "loop:"), record:            |
        |     SymbolTable["loop"] = currentAddress                    |
        +------------------------------+------------------------------+
                                       |
                                       v  (Symbol Table Populated)
        +-------------------------------------------------------------+
        | PASS 2: Machine Code Generation                             |
        | - Re-read file line by line.                                |
        | - For each instruction, look up 5-bit opcode string.        |
        | - For branch labels (e.g., "beq loop"), compute:            |
        |     Relative Offset = (SymbolTable["loop"] - currentPC) / 4 |
        | - Pack bits into 32-bit binary Word.                        |
        +------------------------------+------------------------------+
                                       |
                                       v
                         [ Output: .bin and .hex files ]
```

---

### 4. How THIS Project Implements It
In `assembler/Assembler.hpp`:

#### Pass 1: Symbol Resolution
```cpp
// File: assembler/Assembler.hpp (Pass 1 snippet)
Word currentAddress = 0;
for (const auto& line : sourceLines) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '/') continue;
    
    // Check for label definition
    size_t colonPos = trimmed.find(':');
    if (colonPos != std::string::npos) {
        std::string labelName = trimmed.substr(0, colonPos);
        symbolTable[labelName] = currentAddress;
        trimmed = trim(trimmed.substr(colonPos + 1));
    }
    if (!trimmed.empty()) {
        currentAddress += 4; // Each instruction advances PC by 4 bytes
    }
}
```

#### Pass 2: Binary Word Assembly
```cpp
// File: assembler/Assembler.hpp (Pass 2 snippet)
Word assembleInstruction(const std::string& mnemonic, const std::vector<std::string>& tokens, Word currentPC) {
    std::string opString = isa::getOpcode(mnemonic);
    const auto& meta = isa::getMeta(opString);
    Word word = 0;
    
    // 1. Pack 5-bit opcode into bits [31:27]
    uint32_t opInt = isa::opcodeStringToInt(opString);
    word |= (opInt & 0x1F) << 27;
    
    // 2. Pack operands based on format
    if (meta.is_branch) {
        std::string label = tokens[0];
        Word targetAddr = symbolTable.at(label);
        int32_t relativeOffset = (static_cast<int32_t>(targetAddr) - static_cast<int32_t>(currentPC)) / 4;
        word |= (relativeOffset & 0x07FFFFFF); // 27-bit signed offset
    } else {
        // Pack rd, rs1, rs2, and imm
        word |= (rd & 0x0F) << 22;
        word |= (rs1 & 0x0F) << 18;
        if (isImm) {
            word |= (1 << 26); // Set I-bit
            word |= (imm & 0x3FFFF); // 18-bit imm
        } else {
            word |= (rs2 & 0x0F) << 14;
        }
    }
    return word;
}
```

---

### 5. Worked Example (Hand-Tracing a Program)
Consider this 3-line assembly program:
```assembly
0x0000:   mov r1, 5
0x0004:   bgt target
0x0008:   add r1, r1, 1
0x000C: target:
0x000C:   sub r1, r1, 2
```

1. **Pass 1:**
   - Line 1 (`mov r1, 5`): `currentAddress = 0x0000`, advances to `0x0004`.
   - Line 2 (`bgt target`): `currentAddress = 0x0004`, advances to `0x0008`.
   - Line 3 (`add r1, r1, 1`): `currentAddress = 0x0008`, advances to `0x000C`.
   - Line 4 (`target: sub r1, r1, 2`): Records `symbolTable["target"] = 0x000C`. Advances to `0x0010`.

2. **Pass 2 (Assembling the forward branch on Line 2):**
   - Current PC = `0x0004`.
   - Target Address = `symbolTable["target"] = 0x000C`.
   - Byte Distance = `0x000C - 0x0004 = +8 bytes`.
   - Instruction Word Offset = `+8 / 4 = +2`.
   - Opcode for `bgt` = `isa::OP_BGT = "10001"` ($17_{10} = 0x11$).
   - Machine Word:
     - Bits `[31:27]` = `10001`
     - Bits `[26:0]` = `+2` (`0x0000002`)
     - Hex Result = `0x88000002`.

---

### 6. Design Choices & Trade-Offs
- **Relative Branch Offsets vs Absolute Addresses**:
  - RISC201 encodes branches as relative offsets: `(target - PC) / 4`.
  - **Advantage**: Position-Independent Code (PIC). The code can be relocated anywhere in memory without changing branch target words!

### 7. Numbers & Results
- Verified across all 12 benchmarks:
  - Assembled `benchmarks/hazard_test.s` (19 instructions) in $< 1\text{ ms}$.
  - Symbol table size: handles up to thousands of labels with $O(1)$ lookup time using `std::unordered_map`.

### 8. Common Mistakes to Avoid
- **Mistake**: Calculating branch offset in bytes instead of instruction words.
  - **Correction**: Because every instruction is 4 bytes, the offset in the machine word is always divided by 4: `offset = (target - PC) / 4`.

### 9. Likely Professor Questions
* **Q**: Why are two passes necessary? Why can't you assemble in a single pass?
  * **Answer**: Because of **forward references**. An instruction on line 5 might branch to `target`, but `target:` is defined on line 50. In a single pass, the assembler cannot know what address `target` will have when it encounters line 5. Pass 1 discovers all label addresses; Pass 2 encodes them.

### 10. Summary: If You Only Remember 5 Things
1. Pass 1 finds all **label addresses** and stores them in the **Symbol Table**.
2. Pass 2 emits the **32-bit machine words**.
3. Branch offsets are **relative** and measured in **words** (`bytes / 4`).
4. Two passes solve the **forward reference problem**.
5. The output is saved as raw binary (`.bin`) and hexadecimal text (`.hex`).

### 11. Self-Check Quiz
1. What data structure stores label names and addresses during Pass 1?
2. If a branch target is at `0x0020` and the current branch instruction is at `0x0010`, what is the relative offset encoded into the word?
3. What happens if a branch references a label that was never defined?
4. How many bytes does each assembled instruction occupy?
5. Why are comments stripped during assembly?

*(Answers: 1. The Symbol Table; 2. `(0x20 - 0x10) / 4 = +4`; 3. The assembler raises an "Undefined Symbol" error; 4. Exactly 4 bytes; 5. Comments are for humans and have no hardware machine representation)*

---

# Chapter C2: The Disassembler

### 1. In One Sentence
The disassembler performs the exact inverse of the assembler: it reads 32-bit machine code words, extracts the bit fields, and reconstructs the original assembly mnemonics and operands.

### 2. Start from Zero: Everyday Analogy
Think of a **barcode scanner at a supermarket**:
- The barcode on a cereal box is a series of black and white bars (machine code).
- The scanner reads the bars, extracts the product code, and looks it up in a database to display "Crispy Flakes: \$3.99" on the screen (assembly language).
- The disassembler is the barcode scanner for the CPU.

### 3. The Concept in Simple Steps
```
  32-bit Hex Word:         0x0048C000
                                |
                                v
  Bit-Slicing:             [31:27]=00000 | [26]=0 | [25:22]=0001 | [21:18]=0010 | [17:14]=0011
                                |
                                v
  Field Mapping:           Opcode: "00000" -> "add"
                           Imm flag: 0 (Register mode)
                           rd:  1 -> "r1"
                           rs1: 2 -> "r2"
                           rs2: 3 -> "r3"
                                |
                                v
  Reconstructed Text:      add r1, r2, r3
```

---

### 4. How THIS Project Implements It
In `assembler/Disassembler.hpp`:

```cpp
// File: assembler/Disassembler.hpp (lines 55-80)
std::string disassemble(Word word, Word currentPC) {
    uint32_t opInt = (word >> 27) & 0x1F;
    std::string opString = isa::intToOpcodeString(opInt);
    const auto& meta = isa::getMeta(opString);
    
    if (opString == isa::OP_INVALID) {
        return ".word 0x" + toHex(word) + " ; ILLEGAL OPCODE";
    }
    
    if (meta.is_branch) {
        int32_t relOffset = static_cast<int32_t>(word & 0x07FFFFFF);
        // Sign extend 27-bit offset
        if (relOffset & 0x04000000) relOffset |= 0xF8000000;
        Word target = currentPC + (relOffset * 4);
        return meta.mnemonic + " 0x" + toHex(target);
    }
    
    bool isImm = ((word >> 26) & 0x01) != 0;
    uint32_t rd = (word >> 22) & 0x0F;
    uint32_t rs1 = (word >> 18) & 0x0F;
    
    if (isImm) {
        int32_t imm = static_cast<int32_t>(word & 0x3FFFF);
        if (imm & 0x20000) imm |= 0xFFFC0000; // Sign extend 18 bits
        return meta.mnemonic + " r" + std::to_string(rd) + ", r" + std::to_string(rs1) + ", " + std::to_string(imm);
    } else {
        uint32_t rs2 = (word >> 14) & 0x0F;
        return meta.mnemonic + " r" + std::to_string(rd) + ", r" + std::to_string(rs1) + ", r" + std::to_string(rs2);
    }
}
```

---

### 5. Worked Example (Handling an Illegal Word)
Suppose memory contains `0xF8000000`:
1. Slicing bits `[31:27]`: `(0xF8000000 >> 27) & 0x1F = 31` (`"11111"`).
2. Querying `isa::getMeta("11111")` returns `OP_INVALID`.
3. The disassembler does not crash; it prints:
   `.word 0xF8000000 ; ILLEGAL OPCODE`
   This allows debugging corrupt memory cleanly.

---

### 6. Design Choices & Trade-Offs
- **Sign-Extension Handling**:
  - In a 32-bit system, negative numbers have a $1$ in their highest bit.
  - When extracting an 18-bit immediate, bit 17 is the sign bit. If bit 17 is $1$, the disassembler sets all bits from `[31:18]` to $1$.
  - This ensures that `-4` does not accidentally print as `+262140`!

### 7. Numbers & Results
- Verified by `./verify_golden` Check 1/4:
  - All 12 benchmarks were assembled to `.bin` and disassembled back to text.
  - **Result: 0 mismatches**. 100% round-trip fidelity.

### 8. Common Mistakes to Avoid
- **Mistake**: Forgetting to sign-extend negative immediate values.
- **Mistake**: Assuming the original label names (like `loop:`) can be retrieved from raw machine words. Machine words only store numbers, not variable names. The disassembler displays the calculated target address (e.g. `0x00000024`).

### 9. Likely Professor Questions
* **Q**: Can the disassembler restore original comments and macro names?
  * **Answer**: No. Comments and macro names are discarded by the assembler during Pass 1 and do not exist in the 32-bit machine binary.

### 10. Summary: If You Only Remember 5 Things
1. The disassembler converts **32-bit binary words back into assembly mnemonics**.
2. It slices bits `[31:27]` to identify the opcode.
3. It sign-extends 18-bit immediates and 27-bit branch offsets.
4. Unassigned or invalid words print cleanly as `.word 0x... ; ILLEGAL OPCODE`.
5. The CLI tool is `risc201_disasm`.

### 11. Self-Check Quiz
1. What does the disassembler do when it encounters opcode `"11111"`?
2. Why does the disassembler display addresses instead of label names for branches?
3. Which bit determines whether an operand is a register or an immediate?
4. What binary file does `risc201_disasm` read?
5. How wide is the immediate field in an I-format instruction?

*(Answers: 1. Prints `.word ... ; ILLEGAL OPCODE`; 2. Because label names are not stored in machine code; 3. Bit 26; 4. A 32-bit binary `.bin` file; 5. 18 bits)*

---

# Chapter C3: Stack Macros & Full-Descending Convention

### 1. In One Sentence
The macro preprocessor expands convenient high-level stack operations (`push`, `pop`, `call`, `ret`) into primitive load, store, and arithmetic instructions following the standard full-descending stack convention.

### 2. Start from Zero: Everyday Analogy
Think of a **spring-loaded cafeteria plate dispenser**:
- When you add a clean plate, you push it down onto the stack.
- The plate at the very top is the first one taken off (Last-In, First-Out: LIFO).
- In computer memory, a **Full-Descending Stack** means:
  - **Full**: The Stack Pointer (`sp`) points directly to the last piece of valid data placed on the stack.
  - **Descending**: The stack grows *downward* toward lower memory addresses (from `0x000FFFFC` down toward `0x00000000`).

```
                HIGH MEMORY (0x000FFFFC)
             +----------------------------+
             |    Previous Stack Frame    |
             +----------------------------+
             |    Saved Return Addr (ra)  |
             +----------------------------+
             |    Local Variable 1        |
  sp ------> +----------------------------+  <--- sp points to current top of stack
             |       (Empty Space)        |
             v                            v
             Stack grows DOWNWARD toward 0x0
```

---

### 3. The Concept in Simple Steps

#### Why Full-Descending?
There are 4 theoretical stack models in computer science:
1. **Full-Ascending (FA)**: Points to valid data, grows up.
2. **Empty-Ascending (EA)**: Points to first free slot, grows up.
3. **Empty-Descending (ED)**: Points to first free slot, grows down.
4. **Full-Descending (FD)**: **Points to valid data, grows down** (Standard across ARM, x86, RISC-V, and RISC201).

#### How `push` and `pop` Operate
- **PUSH (Put data on stack)**:
  1. Decrement `sp` by 4 bytes: `sp = sp - 4`
  2. Store data into the newly allocated slot: `Memory[sp] = register`
- **POP (Remove data from stack)**:
  1. Load data from the current slot: `register = Memory[sp]`
  2. Increment `sp` by 4 bytes: `sp = sp + 4`

---

### 4. How THIS Project Implements It
In `assembler/MacroPreprocessor.hpp`:

```cpp
// File: assembler/MacroPreprocessor.hpp (lines 25-55)
std::vector<std::string> expandMacro(const std::string& line) {
    std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) return {line};
    
    std::string mnemonic = toLower(tokens[0]);
    
    // 1. PUSH reg
    if (mnemonic == "push") {
        std::string reg = tokens[1];
        return {
            "sub sp, sp, 4",       // Decrement SP (full-descending)
            "st " + reg + ", [sp]" // Store data at new SP
        };
    }
    
    // 2. POP reg
    if (mnemonic == "pop") {
        std::string reg = tokens[1];
        return {
            "ld " + reg + ", [sp]", // Read data from current SP
            "add sp, sp, 4"         // Increment SP (free slot)
        };
    }
    
    return {line}; // Not a macro, pass through unchanged
}
```

---

### 5. Worked Example (Recursive Factorial Stack Frames)
In `benchmarks/factorial_rec.s`, the recursive function saves its caller's `ra` and local parameter `r0`:
```assembly
fact_rec:
    push  ra            ; Expanded to: sub sp, sp, 4 ; st ra, [sp]
    push  r0            ; Expanded to: sub sp, sp, 4 ; st r0, [sp]
    ...
    ; Base case or recursion
    ...
    pop   r0            ; Expanded to: ld r0, [sp]   ; add sp, sp, 4
    pop   ra            ; Expanded to: ld ra, [sp]   ; add sp, sp, 4
    ret                 ; Return to caller
```
- When `fact_rec(3)` runs:
  - Call 1: saves `ra`, saves `3` (`sp` drops by 8 bytes)
  - Call 2: saves `ra`, saves `2` (`sp` drops by 8 bytes)
  - Call 3: saves `ra`, saves `1` (`sp` drops by 8 bytes)
  - Base case reached: unwinds and restores registers in exact reverse order!

---

### 6. Design Choices & Trade-Offs
- **Macro Expansion vs Dedicated Hardware Stack**:
  - Some old architectures have dedicated hardware stack instructions that do multiple memory accesses in one instruction.
  - RISC201 expands macros into primitive `sub`, `st`, `ld`, `add` instructions.
  - **Advantage**: The CPU pipeline only needs to support single-cycle loads and stores. No multi-cycle microcoded instructions are needed for basic function calls!

### 7. Numbers & Results
- Initial Stack Pointer: `0x000FFFFC` (top of 1 MB address space).
- Stack Guard Page: Mapped beneath `0x00010000`. Any recursive function that exceeds stack depth triggers an immediate `STACK_OVERFLOW` exception.

### 8. Common Mistakes to Avoid
- **Mistake**: Popping in the same order as pushing:
  - `push r1; push r2; pop r1; pop r2;` $\rightarrow$ **WRONG!** `r1` gets `r2`'s value!
  - **Correction**: Always pop in **reverse order**: `push r1; push r2; pop r2; pop r1;`.

### 9. Likely Professor Questions
* **Q**: What does "full-descending" mean?
  * **Answer**: "Full" means `sp` points to the last written item in the stack (not the next empty slot). "Descending" means the stack grows toward lower memory addresses (subtracting 4 on push, adding 4 on pop).
* **Q**: What is a stack frame?
  * **Answer**: A stack frame is the block of memory on the stack allocated for a single function execution, containing saved return addresses, caller-saved registers, and local variables.

### 10. Summary: If You Only Remember 5 Things
1. RISC201 uses a **Full-Descending Stack**.
2. **`push reg`** expands to `sub sp, sp, 4` followed by `st reg, [sp]`.
3. **`pop reg`** expands to `ld reg, [sp]` followed by `add sp, sp, 4`.
4. The stack grows **downward** toward zero.
5. Registers must always be popped in the **exact reverse order** of pushing.

### 11. Self-Check Quiz
1. If `sp = 0x1000`, what is `sp` after executing `push r1`?
2. If `sp = 0x0FFC`, what is `sp` after executing `pop r1`?
3. Which architectural register serves as the Stack Pointer?
4. What happens if a function pushes `r1` then `r2`, but pops `r1` then `r2`?
5. Where does `sp` start in RISC201 by default?

*(Answers: 1. `0x0FFC`; 2. `0x1000`; 3. `r14` (`sp`); 4. Register values are swapped accidentally; 5. `0x000FFFFC`)*

---

# Chapter C4: Command-Line Simulator & Debugger Utilities

### 1. In One Sentence
The repository provides three primary CLI executables (`risc201_as`, `risc201_disasm`, `risc201_sim`) that automate assembling, disassembling, and cycle-accurate debugging.

### 2. Start from Zero: Everyday Analogy
Think of the command line tools like a **flight simulator**:
- `risc201_as`: Builds the flight plan (assembles the program).
- `risc201_disasm`: Translates the flight plan back into plain English.
- `risc201_sim`: The flight cockpit itself. You can fly at full speed (`run`), move forward one frame at a time (`step`), look at all the cockpit gauges (registers, caches, DRAM), and simulate storms (pipeline hazard stalls).

### 3. The Concept in Simple Steps
All executables are built directly with `make all`.

```
                  +-----------------------------------+
                  |           BUILD BINARIES          |
                  |             $ make all            |
                  +-----------------+-----------------+
                                    |
          +-------------------------+-------------------------+
          |                         |                         |
          v                         v                         v
   [ risc201_as ]           [ risc201_disasm ]         [ risc201_sim ]
  Assembles .s -> .bin     Disassembles .bin -> text   Cycle-accurate CPU
  and .hex files                                       simulator & debugger
```

---

### 4. How THIS Project Implements It: Flag Reference

#### 1. Assembler: `risc201_as`
```bash
./risc201_as <input.s> [-o output.bin] [--hex output.hex] [--schedule]
```
- `<input.s>`: Source assembly file.
- `-o <file>`: Emits raw 32-bit little-endian binary file.
- `--hex <file>`: Emits ASCII hexadecimal text format.
- `--schedule`: Enables basic-block instruction scheduling pass to eliminate load-use bubbles.

#### 2. Disassembler: `risc201_disasm`
```bash
./risc201_disasm <input.bin> [-o output.s]
```
- Disassembles binary machine words into human-readable text.

#### 3. Core Simulator: `risc201_sim`
```bash
./risc201_sim [assembly_file] [options]
```

| Flag | Purpose & Values | Example Command |
| :--- | :--- | :--- |
| **`--pipeline`** | Choose pipeline core: `4` or `6` | `./risc201_sim bench.s --pipeline 6` |
| **`--mem-preset`**| Memory model: `ideal`, `simple`, `modern`, `modern-ddr5`, `lecture` | `./risc201_sim bench.s --mem-preset modern` |
| **`--forwarding`**| Forwarding mode: `none`, `basic`, `full` | `./risc201_sim bench.s --forwarding full` |
| **`--branch-policy`**| Branch predictor: `not_taken`, `1bit`, `2bit`, `gshare` | `./risc201_sim bench.s --branch-policy 2bit` |
| **`--div-algo`** | Division unit: `rest`, `nonrest`, `newton` | `./risc201_sim bench.s --div-algo newton` |
| **`--diagram`** | Display ASCII pipeline timing diagram | `./risc201_sim bench.s --diagram` |
| **`--report`** | Print cycle count, CPI, and stall breakdown | `./risc201_sim bench.s --report` |
| **`-i, --interactive`**| Launch interactive REPL debugger console | `./risc201_sim bench.s -i` |

---

### 5. Interactive Debugger REPL Commands
When launched with `-i`, the simulator enters an interactive terminal shell:

```
(risc201-dbg) help
Available Commands:
  step [N] / s [N]  : Advance simulation by N clock cycles (default 1)
  run / r           : Run to completion or breakpoint
  break <addr>      : Set breakpoint at hexadecimal address
  regs              : Print all general registers (r0-r15) and flags
  pipe              : Display current pipeline latches and instructions
  stack             : Render ASCII visualization of active stack frame
  mem               : Display cache and TLB hit rates
  cache             : Render ASCII set-way line status grid
  dram              : Display DRAM bank state and row buffers
  mode <4|6|micro>  : Switch between 4-stage, 6-stage, or microcode mode
  quit / q          : Exit debugger
```

---

### 6. Worked Example: Step-by-Step Interactive Session
```bash
$ ./risc201_sim benchmarks/factorial_iter.s -i
Loaded assembly file: benchmarks/factorial_iter.s
Switched simulation mode to: 4-STAGE PIPELINE
(risc201-dbg) step 1
Cycle 1 completed (4-Stage).
[IF    ] : mov r1, 30 (PC: 0x00000000)
[OF_EX ] : -- BUBBLE --
[EX_MA ] : -- BUBBLE --
[MA_RW ] : -- BUBBLE --
(risc201-dbg) regs
r0  : 0x00000006  r1  : 0x00000000  r2  : 0x00000000  sp  : 0x000FFFFC
(risc201-dbg) run
Execution stopped after 29 cycles.
r1  : 0x000002D0 (720 decimal, correct 6!)
```

---

### 7. Summary: If You Only Remember 5 Things
1. **`risc201_as`** assembles; **`risc201_disasm`** disassembles; **`risc201_sim`** executes.
2. Use **`--pipeline 4`** or **`--pipeline 6`** to switch processor depth.
3. Use **`--mem-preset modern`** to activate caches, TLBs, and DDR4 DRAM.
4. Launch with **`-i`** to enter the interactive step-by-step debugger.
5. In the debugger, type **`pipe`**, **`regs`**, **`stack`**, and **`mem`** to inspect execution state.
