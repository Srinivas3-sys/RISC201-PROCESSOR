# Module Notes: Instruction Set Architecture (RISC201)

## 1. Architectural Overview & Design Philosophy
* **Specification Source**: SimpleRisc ISA as defined by Sarangi, *Computer Organisation and Architecture*, Chapters 3 & 9 (Tables 3.10 and 9.2).
* **Single Source of Truth**: All opcode constants, bitfields, and instruction metadata reside strictly in [`common/ISA.hpp`](file:///home/srinivas/RISC201-PROCESSOR/common/ISA.hpp).
* **Binary-String Representation**:
  * Opcodes are stored, compared, and routed across the entire simulator as 5-character binary strings (`std::string`), such as `"00000"` (`OP_ADD`), `"01110"` (`OP_LD`), and `"11010"` (`OP_XOR`).
  * **Rationale**: Direct bitfield inspection without bit-mask extraction errors during decode; enables clean string matching in the ALU and microcode sequencing; matches Sarangi's hardware control bit notation directly.
  * Integer bitwise conversions occur only at the instruction-word boundary:
    * Fetch: `wordToBits()` splits 32-bit words into binary strings.
    * Decode: `sliceField(wordBits, 31, 27)` extracts the 5-bit opcode string.

## 2. Bitfield Layout & Instruction Formats
Word Width: 32 bits ($[31:0]$, MSB-first).

1. **R-Type (Register-Register)**:
   * $[31:27]$: Opcode (5 bits)
   * $[26]$: Immediate Flag $I = 0$ (1 bit)
   * $[25:22]$: Destination Register $r_d$ (4 bits, $0-15$)
   * $[21:18]$: Source Register 1 $r_{s1}$ (4 bits, $0-15$)
   * $[17:14]$: Source Register 2 $r_{s2}$ (4 bits, $0-15$)
   * $[13:0]$: Unused / Zero padding (14 bits)

2. **I-Type (Register-Immediate)**:
   * $[31:27]$: Opcode (5 bits)
   * $[26]$: Immediate Flag $I = 1$ (1 bit)
   * $[25:22]$: Destination Register $r_d$ (4 bits, $0-15$)
   * $[21:18]$: Source Register 1 $r_{s1}$ (4 bits, $0-15$)
   * $[17:2]$: Immediate value $imm_{16}$ (16 bits)
   * $[1:0]$: Immediate modifier $mod$ (2 bits):
     * `00` (`MOD_U`): Unsigned 16-bit zero-extension.
     * `01` (`MOD_H`): High 16-bit shift (`imm << 16`).
     * `10` (`MOD_S`): Signed 16-bit sign-extension (bit 17 sign bit).

3. **B-Type (Branch / Call)**:
   * $[31:27]$: Opcode (5 bits)
   * $[26:0]$: Signed Branch Offset $imm_{27}$ (27 bits, 2's complement word offset).
   * Target calculation: $\text{Target PC} = \text{Current PC} + (\text{SignExtend}(imm_{27}) \times 4)$.

4. **Memory Instructions (`ld`, `st`)**:
   * Uses I-type format: effective address $\text{EA} = r_{s1} + \text{SignExtend}(imm_{16})$.
   * `ld rd, imm[rs1]`: Loads 32-bit word from memory at EA into $r_d$.
   * `st rd, imm[rs1]`: Stores 32-bit word from $r_d$ into memory at EA.

5. **Privileged Instructions (`movz`, `retz`)**:
   * `movz`: Transfers data between General Purpose Registers ($r0-r15$) and Privileged Registers (`SR`, `EPC`, `ECAUSE`, `BADVADDR`, `PTBR`).
   * `retz`: Returns from kernel exception handler, restores PC from `EPC`, and demotes CPL to user mode (1).

## 3. Complete Opcode Map (`common/ISA.hpp`)

| Opcode String | Hex | Mnemonic | Format | Semantics | Flags Set |
| :--- | :---: | :--- | :---: | :--- | :---: |
| `"00000"` | 0x00 | `add` | R / I | $r_d \leftarrow r_{s1} + (r_{s2} \mid imm)$ | None |
| `"00001"` | 0x01 | `sub` | R / I | $r_d \leftarrow r_{s1} - (r_{s2} \mid imm)$ | None |
| `"00010"` | 0x02 | `mul` | R / I | $r_d \leftarrow (r_{s1} \times (r_{s2} \mid imm))[31:0]$ | None |
| `"00011"` | 0x03 | `div` | R / I | $r_d \leftarrow r_{s1} \mathbin{/} (r_{s2} \mid imm)$ | DivisionByZero Exception |
| `"00100"` | 0x04 | `mod` | R / I | $r_d \leftarrow r_{s1} \pmod{r_{s2} \mid imm}$ | DivisionByZero Exception |
| `"00101"` | 0x05 | `cmp` | R / I | Flags updated: $E \leftarrow (r_{s1} == B), GT \leftarrow (r_{s1} > B)$ | $E, GT$ |
| `"00110"` | 0x06 | `and` | R / I | $r_d \leftarrow r_{s1} \mathrel{\&} (r_{s2} \mid imm)$ | None |
| `"00111"` | 0x07 | `or` | R / I | $r_d \leftarrow r_{s1} \mid (r_{s2} \mid imm)$ | None |
| `"01000"` | 0x08 | `not` | R / I | $r_d \leftarrow \sim(r_{s2} \mid imm)$ | None |
| `"01001"` | 0x09 | `mov` | R / I | $r_d \leftarrow (r_{s2} \mid imm)$ | None |
| `"01010"` | 0x0A | `lsl` | R / I | $r_d \leftarrow r_{s1} \ll (r_{s2} \mid imm)[4:0]$ | None |
| `"01011"` | 0x0B | `lsr` | R / I | $r_d \leftarrow r_{s1} \gg (r_{s2} \mid imm)[4:0]$ (logical) | None |
| `"01100"` | 0x0C | `asr` | R / I | $r_d \leftarrow r_{s1} \gg (r_{s2} \mid imm)[4:0]$ (arithmetic) | None |
| `"01101"` | 0x0D | `nop` | R | No operation | None |
| `"01110"` | 0x0E | `ld` | I | $r_d \leftarrow \text{Mem}[r_{s1} + imm]$ | None |
| `"01111"` | 0x0F | `st` | I | $\text{Mem}[r_{s1} + imm] \leftarrow r_d$ | None |
| `"10000"` | 0x10 | `beq` | B | if ($E == 1$) $\text{PC} \leftarrow \text{Target}$ | None |
| `"10001"` | 0x11 | `bgt` | B | if ($GT == 1$) $\text{PC} \leftarrow \text{Target}$ | None |
| `"10010"` | 0x12 | `b` | B | Unconditional: $\text{PC} \leftarrow \text{Target}$ | None |
| `"10011"` | 0x13 | `call` | B | $r_{15} (\text{RA}) \leftarrow \text{PC} + 4; \text{PC} \leftarrow \text{Target}$ | None |
| `"10100"` | 0x14 | `ret` | R | $\text{PC} \leftarrow r_{15} (\text{RA})$ | None |
| `"10101"` | 0x15 | `movz` | PRIV | Privileged register transfer | Privileged Check |
| `"10110"` | 0x16 | `retz` | PRIV | Kernel trap return, restore PC from `EPC` | Privileged Check |
| `"10111"` | 0x17 | `min` | R / I | $r_d \leftarrow \min(r_{s1}, r_{s2} \mid imm)$ | None |
| `"11000"` | 0x18 | `max` | R / I | $r_d \leftarrow \max(r_{s1}, r_{s2} \mid imm)$ | None |
| `"11001"` | 0x19 | `rots` | R / I | $r_d \leftarrow \text{RotateRight}(r_{s1}, (r_{s2} \mid imm)[4:0])$ | None |
| `"11010"` | 0x1A | `xor` | R / I | $r_d \leftarrow r_{s1} \oplus (r_{s2} \mid imm)$ | None |
| `"11011"` | 0x1B | `abs` | R / I | $r_d \leftarrow \lvert r_{s2} \mid imm \rvert$ | None |
| `"11100"` | 0x1C | Reserved | - | Illegal opcode $\rightarrow$ Trap | Illegal Opcode |
| `"11101"` | 0x1D | Reserved | - | Illegal opcode $\rightarrow$ Trap | Illegal Opcode |
| `"11110"` | 0x1E | Reserved | - | Illegal opcode $\rightarrow$ Trap | Illegal Opcode |
| `"11111"` | 0x1F | `invalid` | - | Illegal opcode $\rightarrow$ Trap | Illegal Opcode |

## 4. Special Register Usage
* **$r_0$**: Constant Zero. Writes to $r_0$ are discarded; reads always yield 0.
* **$r_{14}$ (`SP`)**: Stack Pointer. Points to the top of the stack under the full-descending convention.
* **$r_{15}$ (`RA`)**: Return Address. Written automatically by `call`; read by `ret`.

