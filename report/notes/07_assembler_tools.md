# Module Notes: Assembler, Disassembler, and Macro Preprocessor

## 1. Two-Pass Assembler Architecture (`assembler/Assembler.hpp`)

The assembler implements a classic two-pass design to support forward jump labels and macro expansion without back-patching complexity:

### Pass 1: Symbol Table & Macro Expansion
1. Strips comments (`;` or `//`) and trims whitespace.
2. Expands preprocessor macros via `MacroPreprocessor::expandLine()`.
3. Processes assembler directives:
   * `.text`: Sets active emission segment to code section (address 0x00000000).
   * `.data`: Sets active emission segment to data section.
   * `.word <val>`: Emits raw 32-bit integer constant into current memory location.
4. Identifies label definitions (e.g. `loop:`, `exit:`).
   * Records the symbol name and its current byte address in `std::unordered_map<std::string, Word> symbolTable`.
   * Advances the location counter (`currentAddress += 4`) for every emitted instruction word.

### Pass 2: Binary Word Encoding
1. Re-scans the expanded instruction list.
2. Extracts mnemonic and matches it to `common/ISA.hpp` definitions.
3. Parses operands (destination register $r_d$, source registers $r_{s1}, r_{s2}$, immediate constants, or branch target labels).
4. Calculates branch offsets:
   $$\text{Offset Words} = \frac{\text{Target Address} - \text{Current Instruction Address}}{4}$$
   * Bounds-checks the offset to ensure it fits within 27 signed bits ($[-2^{26}, 2^{26}-1]$).
5. Assembles bitfields into a 32-bit machine code word and converts to binary string via `wordToBits()`.
6. Emits both ASCII hexadecimal (`.hex`) and binary byte (`.bin`) outputs.

---

## 2. Macro Preprocessor (`assembler/MacroPreprocessor.hpp`)

### 2.1 Full-Descending Stack Macros
The architecture implements a **full-descending stack** convention ($r_{14}$ is `SP`, growing downwards towards lower addresses):

1. **`PUSH <reg>` Macro Expansion**:
   ```assembly
   ; Source assembly
   push r1
   
   ; Expanded two-instruction sequence
   sub r14, r14, 4        ; Decrement SP first (pre-decrement)
   st  r1, 0[r14]         ; Store register value at new top of stack
   ```

2. **`POP <reg>` Macro Expansion**:
   ```assembly
   ; Source assembly
   pop r1
   
   ; Expanded two-instruction sequence
   ld  r1, 0[r14]         ; Load word from current top of stack
   add r14, r14, 4        ; Increment SP after read (post-increment)
   ```

### 2.2 Immediate Modifier Preprocessing
* SimpleRisc encodes immediate values up to 16 bits in I-type format.
* To support full 32-bit constants without memory loads:
  * Low 16 bits: `mov r1, imm16` (uses `MOD_U` or `MOD_S`).
  * High 16 bits: `movh r1, imm16` $\implies$ sets `MOD_H` ($[1:0] = 01$), shifting the immediate into bits $[31:16]$.

---

## 3. Disassembler (`assembler/Disassembler.hpp`)
* **Inverse Mapping**: Translates raw 32-bit words back into clean SimpleRisc assembly.
* **Bitfield Extraction**: Uses `sliceField(wordBits, hi, lo)` to cleanly unpack opcode, register fields, and immediate values.
* **Synthetic Label Generation**: Computes target addresses for branch instructions (`beq`, `bgt`, `b`, `call`) and prints synthetic labels (e.g. `beq label_0018`) for enhanced readability.
* **Illegal Word Handling**: Any word matching reserved opcode bits ($28-30$ or $31$) is clearly labeled `.word 0xXXXXXXXX ; [ILLEGAL OPCODE]`.

---

## 4. Round-Trip Verification Fidelity
* **Invariance Property**:
  $$\text{Assembly}_1 \xrightarrow{\text{Assembler}} \text{Binary} \xrightarrow{\text{Disassembler}} \text{Assembly}_2 \xrightarrow{\text{Assembler}} \text{Binary}$$
* Verified across all 12 benchmark programs by `tools/verify_golden.cpp` (Check 1): byte-for-byte binary identity is preserved across disassembly round trips!

