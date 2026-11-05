# Chapter 5: Software Toolchain: Assembler, Disassembler, and Macros

## 5.1 Two-Pass Assembler Architecture (`assembler/Assembler.hpp`)

The RISC201 assembler (`risc201_as`) translates human-readable SimpleRisc assembly programs into machine code. To support forward-referencing branch labels without complex back-patching, it executes a clean two-pass algorithm.

### 5.1.1 Pass 1: Symbol Table & Macro Preprocessing
Pass 1 scans the input source file sequentially to build the symbol table and determine exact byte alignments:
1. **Comment Stripping**: Removes single-line comments denoted by `;` or `//`.
2. **Macro Expansion**: Expands macro invocations (`push`, `pop`) into equivalent hardware instructions via `MacroPreprocessor::expandLine()`.
3. **Directive Processing**:
   * `.text`: Sets current segment to code (starts at `0x00000000`).
   * `.data`: Sets current segment to data storage.
   * `.word <val>`: Allocates a 32-bit constant at the current memory location and increments the location counter by 4 bytes.
4. **Label Discovery**:
   * Matches tokens ending in `:` (e.g. `loop:`, `skip:`).
   * Maps `labelName` $\rightarrow$ `currentAddress` in the symbol table (`std::unordered_map<std::string, Word> symbolTable`).
   * Does **not** increment `currentAddress` for label declarations.
5. **Location Counter**: For each emitted instruction word, advances `currentAddress += 4`.

### 5.1.2 Pass 2: Binary Word Assembly & Operand Encoding
Pass 2 re-scans the expanded instruction list and generates 32-bit binary words:
1. **Mnemonic Resolution**: Matches the instruction mnemonic against the canonical opcode table in `common/ISA.hpp`.
2. **Register Parsing**:
   * Parses register tokens (`r0` through `r15`, `sp` for `r14`, `ra` for `r15`).
   * Validates register indices are in the range $[0, 15]$.
3. **Branch Target Resolution**:
   * For branch instructions (`beq`, `bgt`, `b`, `call`), looks up the destination label in `symbolTable`.
   * Computes the PC-relative word offset:
     $$\text{Offset Words} = \frac{\text{Target Address} - \text{Current Instruction Address}}{4}$$
   * Validates offset fits within 27 signed bits ($[-2^{26}, 2^{26}-1]$).
4. **Immediate Modifier Selection**:
   * Generates modifier bits $[1:0]$: `00` for unsigned immediate (`MOD_U`), `01` for high immediate (`MOD_H`), `10` for signed immediate (`MOD_S`).
5. **Bit Packaging & Output Generation**:
   * Encodes bitfields into a 32-bit integer word (`uint32_t`).
   * Formats into two file deliverables:
     * **ASCII Hexadecimal (`.hex`)**: Formatted as uppercase 8-character hex words per line (e.g. `00C48000`).
     * **Raw Binary (`.bin`)**: Little-endian 32-bit binary words.

---

## 5.2 Macro Preprocessor & Full-Descending Stack (`assembler/MacroPreprocessor.hpp`)

The RISC201 architecture enforces a **full-descending stack convention**:
* **Stack Pointer (`SP` / `r14`)**: Points to the newest occupied word at the top of the stack.
* **Stack Growth**: Decrements towards lower memory addresses.

### 5.2.1 `PUSH` Macro Expansion
When the programmer writes `push rx`:
```assembly
; Source assembly
push r4

; Preprocessed Hardware Instructions
sub r14, r14, 4         ; Step 1: Pre-decrement SP by 4 bytes
st  r4, 0[r14]          ; Step 2: Store r4 at new top of stack
```

### 5.2.2 `POP` Macro Expansion
When the programmer writes `pop rx`:
```assembly
; Source assembly
pop r4

; Preprocessed Hardware Instructions
ld  r4, 0[r14]          ; Step 1: Load word from top of stack into r4
add r14, r14, 4         ; Step 2: Post-increment SP by 4 bytes
```

### 5.2.3 Pseudo-Instructions
* `nop` $\implies$ Expanded to `add r0, r0, 0` (binary `0x00000000`).
* `mov rd, imm` $\implies$ Automatically selects `MOD_U` or `MOD_S` based on immediate sign.

---

## 5.3 Disassembler Architecture (`assembler/Disassembler.hpp`)

The disassembler (`risc201_disasm`) decodes 32-bit machine code words back into formatted SimpleRisc assembly:
1. **Opcode Extraction**: Uses `sliceField(bits, 31, 27)` to isolate the 5-bit opcode string.
2. **Metadata Lookup**: Queries `INSTRUCTION_TABLE[opcode]` in `common/ISA.hpp` to retrieve format type (R, I, B, PRIV) and syntax rules.
3. **Field Extraction**: Unpacks $r_d$, $r_{s1}$, $r_{s2}$, immediate modifier, and 27-bit branch offset.
4. **Synthetic Label Reconstruction**:
   * Calculates branch target address: $\text{Target} = \text{Current PC} + (\text{Offset} \times 4)$.
   * Emits synthetic labels (e.g. `label_0028`) so disassembled code can be re-assembled directly!
5. **Reserved Opcode Guard**: Words matching reserved opcodes (`"11100"` through `"11111"`) are safely disassembled as `.word 0xXXXXXXXX ; [ILLEGAL OPCODE]`.

---

## 5.4 Command-Line Usage & Real Execution Outputs

### 5.4.1 Assembling a Program
```bash
$ ./risc201_as benchmarks/factorial_iter.s -o benchmarks/factorial_iter.hex
[ASSEMBLER] Input file: benchmarks/factorial_iter.s
[PASS 1] Discovered 2 labels: loop (0x00000004), exit (0x00000028)
[PASS 2] Emitted 11 instruction words (44 bytes).
[OUTPUT] Wrote ASCII hex to benchmarks/factorial_iter.hex
[OUTPUT] Wrote raw binary to benchmarks/factorial_iter.bin
```

### 5.4.2 Disassembling Machine Code
```bash
$ ./risc201_disasm benchmarks/factorial_iter.hex
0x00000000: 04400016    mov r1, 5
0x00000004: 04800006    mov r2, 1
0x00000008: 04040006    cmp r1, 0
0x0000000C: 80000018    beq label_0024
0x00000010: 00888000    mul r2, r2, r1
0x00000014: 04440006    sub r1, r1, 1
0x00000018: 93FFFFF8    b label_0008
0x0000001C: 01100000    nop
0x00000020: 01100000    nop
```

### 5.4.3 Round-Trip Verification Fidelity
The automated test suite (`tests/test_all.cpp`, Test 1.2) verifies the round-trip identity:
$$\text{Assembly} \xrightarrow{\text{Assembler}} \text{Binary}_1 \xrightarrow{\text{Disassembler}} \text{Assembly}_{\text{regen}} \xrightarrow{\text{Assembler}} \text{Binary}_2$$
In `tools/verify_golden.cpp` (Check 1), **$\text{Binary}_1$ and $\text{Binary}_2$ match byte-for-byte across 100% of the 12 benchmarks**.

