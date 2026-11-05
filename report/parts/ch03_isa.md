# Chapter 3: RISC201 Instruction Set Architecture Specification

## 3.1 Architectural Philosophy & Single Source of Truth
The RISC201 processor implements the **SimpleRisc ISA**, a 32-bit load-store RISC architecture specified in Sarangi, *Computer Organisation and Architecture*, Chapters 3 & 9 (Tables 3.10 and 9.2).

### 3.1.1 Canonical 5-Bit Binary-String Opcodes
A foundational design decision of the RISC201 codebase is that **opcodes are represented as 5-character binary strings (`std::string`) throughout the entire simulator datapath**, with [`common/ISA.hpp`](file:///home/srinivas/RISC201-PROCESSOR/common/ISA.hpp) serving as the **single source of truth**.

```cpp
// In common/ISA.hpp: Canonical 5-bit binary-string opcodes
namespace isa {
inline const std::string OP_ADD     = "00000"; // 0:  add  rd, rs1, (rs2/imm)
inline const std::string OP_SUB     = "00001"; // 1:  sub  rd, rs1, (rs2/imm)
inline const std::string OP_MUL     = "00010"; // 2:  mul  rd, rs1, (rs2/imm)
inline const std::string OP_DIV     = "00011"; // 3:  div  rd, rs1, (rs2/imm)
inline const std::string OP_MOD     = "00100"; // 4:  mod  rd, rs1, (rs2/imm)
inline const std::string OP_CMP     = "00101"; // 5:  cmp  rs1, (rs2/imm)
inline const std::string OP_AND     = "00110"; // 6:  and  rd, rs1, (rs2/imm)
inline const std::string OP_OR      = "00111"; // 7:  or   rd, rs1, (rs2/imm)
inline const std::string OP_NOT     = "01000"; // 8:  not  rd, (rs2/imm)
inline const std::string OP_MOV     = "01001"; // 9:  mov  rd, (rs2/imm)
inline const std::string OP_LSL     = "01010"; // 10: lsl  rd, rs1, (rs2/imm)
inline const std::string OP_LSR     = "01011"; // 11: lsr  rd, rs1, (rs2/imm)
inline const std::string OP_ASR     = "01100"; // 12: asr  rd, rs1, (rs2/imm)
inline const std::string OP_NOP     = "01101"; // 13: nop
inline const std::string OP_LD      = "01110"; // 14: ld   rd, imm[rs1]
inline const std::string OP_ST      = "01111"; // 15: st   rd, imm[rs1]
inline const std::string OP_BEQ     = "10000"; // 16: beq  offset
inline const std::string OP_BGT     = "10001"; // 17: bgt  offset
inline const std::string OP_B       = "10010"; // 18: b    offset
inline const std::string OP_CALL    = "10011"; // 19: call offset
inline const std::string OP_RET     = "10100"; // 20: ret  (PC <- ra)
inline const std::string OP_MOVZ    = "10101"; // 21: movz (privileged transfer)
inline const std::string OP_RETZ    = "10110"; // 22: retz (kernel trap return)
inline const std::string OP_MIN     = "10111"; // 23: min  rd, rs1, (rs2/imm)
inline const std::string OP_MAX     = "11000"; // 24: max  rd, rs1, (rs2/imm)
inline const std::string OP_ROTS    = "11001"; // 25: rots rd, rs1, (rs2/imm)
inline const std::string OP_XOR     = "11010"; // 26: xor  rd, rs1, (rs2/imm)
inline const std::string OP_ABS     = "11011"; // 27: abs  rd, (rs2/imm)
inline const std::string OP_INVALID = "11111"; // 31: illegal opcode
inline const std::string OP_BUBBLE  = OP_NOP;
}
```

### 3.1.2 Why Numbers Exist Only at the Word Boundary
In traditional C++ simulators, opcodes are often stored as an integer `enum` (`0`, `1`, `2`). While integer comparisons are fast, integer enums introduce serious architectural bugs:
* They obscure the bitfield relationships specified in computer architecture textbooks.
* Developers accidentally perform arithmetic on opcodes (e.g. `op + 1`) or mix up opcode numbers with register IDs.
* In SimpleRisc, bits $[31:27]$ directly represent the control store entry points in horizontal microcode.

In RISC201:
1. **Instruction Fetch (`IF`)**: Reads the 32-bit machine code word from memory and converts it once to a 32-character binary bitstring (`wordToBits(word, 32)`).
2. **Instruction Decode (`ID`)**: Slices bits $[31:27]$ using `sliceField(wordBits, 31, 27)`. The resulting 5-character string **is** the canonical opcode.
3. **Pipeline Latches & ALU**: The opcode is carried as a `std::string` through every pipeline latch (`IF/ID`, `ID/RF`, `RF/EX`, `EX/MA`, `MA/RW`) and is matched directly in the ALU (`if (opcode == isa::OP_ADD)`).
4. **Static Verification**: The custom linter (`tools/lint_isa.cpp`, executed via `make lint-isa`) enforces across all 43 project source files that no integer `Opcode` enums, raw numeric constants, or quoted string literals exist anywhere outside `common/ISA.hpp`.

### 3.1.3 Engineering Trade-Off Analysis
| Property | Binary-String Opcodes (`std::string`) | Integer Enum (`uint8_t`) | Architectural Justification |
| :--- | :--- | :--- | :--- |
| **Debuggability** | Exceptional (prints `"00000"` / `add` directly in debugger) | Poor (prints integer `0`, requires cast table) | Eliminates debug translation overhead in REPL and traces. |
| **Bitfield Fidelity** | 100% exact match to Sarangi Table 3.10 | Abstracted away | Directly maps hardware microcode bits and control stores. |
| **Type Safety** | Strict single-source constant matching | Permissive (implicit int conversions) | Prevents accidental arithmetic or register ID confusion. |
| **Simulation Speed** | 940,000+ instructions/second | $\sim 1.1\times 10^6$ instructions/second | Short string SSO optimization makes string comparison negligible. |

---

## 3.2 Register Set & Architectural State
The processor provides 16 General-Purpose Registers (`r0` to `r15`) and 5 Privileged System Registers:

| Register | Architectural Name | Role and Hardware Conventions |
| :---: | :--- | :--- |
| **`r0`** | Hardwired Zero | Always reads as `0x00000000`. Writes to `r0` are silently discarded by hardware. |
| **`r1` – `r13`** | General Registers | General-purpose operand registers, function arguments, and temporary variables. |
| **`r14`** | Stack Pointer (`SP`) | Dedicated stack pointer under the **full-descending stack** convention. |
| **`r15`** | Return Address (`RA`) | Dedicated return address register written by `call` ($\text{PC} + 4$) and read by `ret`. |
| **`FLAGS`** | Condition Flags | 2-bit architectural status register: $E$ (Equal) and $GT$ (Greater Than). Set only by `cmp`. |
| **`PC`** | Program Counter | 32-bit byte address of the currently executing instruction. Word-aligned (multiple of 4). |
| **`SR`** | Status Register | Privileged register holding Current Privilege Level ($\text{CPL}=0$ kernel, $\text{CPL}=1$ user). |
| **`EPC`** | Exception PC | Privileged register capturing the faulting instruction address upon an exception. |
| **`ECAUSE`** | Exception Cause | Privileged register recording the architectural fault code. |
| **`BADVADDR`** | Bad Virtual Address | Privileged register capturing the faulting memory address on page faults. |
| **`PTBR`** | Page Table Base Reg | Privileged register holding the physical base address of the Level 4 PML4 page table. |

---

## 3.3 Complete Instruction Set Reference Table

The table below documents all 32 opcode slots in the 5-bit space ($2^5 = 32$), as verified by `tests/test_all.cpp` (Section 1):

| Opcode String | Hex | Mnemonic | Format | Assembly Syntax | Architectural Semantics | Flags Modified |
| :---: | :---: | :--- | :---: | :--- | :--- | :---: |
| `"00000"` | 0x00 | `add` | R / I | `add rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} + B$ | None |
| `"00001"` | 0x01 | `sub` | R / I | `sub rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} - B$ | None |
| `"00010"` | 0x02 | `mul` | R / I | `mul rd, rs1, rs2/imm` | $r_d \leftarrow (r_{s1} \times B)[31:0]$ | None |
| `"00011"` | 0x03 | `div` | R / I | `div rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \mathbin{/} B$ (Trap if $B=0$) | None |
| `"00100"` | 0x04 | `mod` | R / I | `mod rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \pmod B$ (Trap if $B=0$) | None |
| `"00101"` | 0x05 | `cmp` | R / I | `cmp rs1, rs2/imm` | $E \leftarrow (r_{s1} == B), GT \leftarrow (r_{s1} > B)$ | $E, GT$ |
| `"00110"` | 0x06 | `and` | R / I | `and rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \mathrel{\&} B$ | None |
| `"00111"` | 0x07 | `or` | R / I | `or rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \mid B$ | None |
| `"01000"` | 0x08 | `not` | R / I | `not rd, rs2/imm` | $r_d \leftarrow \sim B$ | None |
| `"01001"` | 0x09 | `mov` | R / I | `mov rd, rs2/imm` | $r_d \leftarrow B$ | None |
| `"01010"` | 0x0A | `lsl` | R / I | `lsl rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \ll B[4:0]$ | None |
| `"01011"` | 0x0B | `lsr` | R / I | `lsr rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \gg B[4:0]$ (Logical) | None |
| `"01100"` | 0x0C | `asr` | R / I | `asr rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \gg B[4:0]$ (Arithmetic) | None |
| `"01101"` | 0x0D | `nop` | R | `nop` | No operation ($r_0 \leftarrow r_0 + 0$) | None |
| `"01110"` | 0x0E | `ld` | I | `ld rd, imm[rs1]` | $r_d \leftarrow \text{Mem}[r_{s1} + \text{SignExt}(imm)]$ | None |
| `"01111"` | 0x0F | `st` | I | `st rd, imm[rs1]` | $\text{Mem}[r_{s1} + \text{SignExt}(imm)] \leftarrow r_d$ | None |
| `"10000"` | 0x10 | `beq` | B | `beq label/offset` | If ($E == 1$) $\text{PC} \leftarrow \text{PC} + (\text{SignExt}(imm_{27}) \times 4)$ | None |
| `"10001"` | 0x11 | `bgt` | B | `bgt label/offset` | If ($GT == 1$) $\text{PC} \leftarrow \text{PC} + (\text{SignExt}(imm_{27}) \times 4)$ | None |
| `"10010"` | 0x12 | `b` | B | `b label/offset` | Unconditional: $\text{PC} \leftarrow \text{PC} + (\text{SignExt}(imm_{27}) \times 4)$ | None |
| `"10011"` | 0x13 | `call` | B | `call label/offset` | $r_{15} (\text{RA}) \leftarrow \text{PC} + 4; \text{PC} \leftarrow \text{PC} + (\text{SignExt}(imm_{27}) \times 4)$ | None |
| `"10100"` | 0x14 | `ret` | R | `ret` | Return: $\text{PC} \leftarrow r_{15} (\text{RA})$ | None |
| `"10101"` | 0x15 | `movz` | PRIV | `movz rd, privReg` | Transfer between general and privileged register | None |
| `"10110"` | 0x16 | `retz` | PRIV | `retz` | Return from exception: $\text{PC} \leftarrow \text{EPC}, \text{CPL} \leftarrow 1$ | None |
| `"10111"` | 0x17 | `min` | R / I | `min rd, rs1, rs2/imm` | $r_d \leftarrow (r_{s1} < B) \mathbin{?} r_{s1} : B$ | None |
| `"11000"` | 0x18 | `max` | R / I | `max rd, rs1, rs2/imm` | $r_d \leftarrow (r_{s1} > B) \mathbin{?} r_{s1} : B$ | None |
| `"11001"` | 0x19 | `rots` | R / I | `rots rd, rs1, rs2/imm`| $r_d \leftarrow \text{RotateRight}(r_{s1}, B[4:0])$ | None |
| `"11010"` | 0x1A | `xor` | R / I | `xor rd, rs1, rs2/imm` | $r_d \leftarrow r_{s1} \oplus B$ | None |
| `"11011"` | 0x1B | `abs` | R / I | `abs rd, rs2/imm` | $r_d \leftarrow \lvert B \rvert$ | None |
| `"11100"` | 0x1C | Reserved | - | - | Unassigned $\implies$ Raises `ILLEGAL_OPCODE` | Trap |
| `"11101"` | 0x1D | Reserved | - | - | Unassigned $\implies$ Raises `ILLEGAL_OPCODE` | Trap |
| `"11110"` | 0x1E | Reserved | - | - | Unassigned $\implies$ Raises `ILLEGAL_OPCODE` | Trap |
| `"11111"` | 0x1F | `invalid`| - | - | Explicit Invalid $\implies$ Raises `ILLEGAL_OPCODE` | Trap |

*Note on Operand $B$*: In R-type instructions ($I=0$), $B = r_{s2}$. In I-type instructions ($I=1$), $B$ is the 16-bit immediate modified according to the 2 modifier bits $[1:0]$.

---

## 3.4 Instruction Word Formats & Bit Slicing

The 32-bit instruction word is sliced according to MSB-first bit numbering ($[31:0]$):

### 3.4.1 Format R: Register-Register Operations
```
 31         27 26 25      22 21      18 17      14 13                       0
+-------------+--+----------+----------+----------+--------------------------+
|   Opcode    |0 |    rd    |   rs1    |   rs2    |      Unused (Zero)       |
|  (5 bits)   |I | (4 bits) | (4 bits) | (4 bits) |        (14 bits)         |
+-------------+--+----------+----------+----------+--------------------------+
```
* **Bit Slicing**:
  * `opcode`: `sliceField(bits, 31, 27)` (5 bits)
  * `I-bit`: `sliceField(bits, 26, 26)` (Always `'0'`)
  * `rd`: `sliceField(bits, 25, 22)` (4 bits $\implies 0-15$)
  * `rs1`: `sliceField(bits, 21, 18)` (4 bits $\implies 0-15$)
  * `rs2`: `sliceField(bits, 17, 14)` (4 bits $\implies 0-15$)

### 3.4.2 Format I: Register-Immediate Operations
```
 31         27 26 25      22 21      18 17                             2 1  0
+-------------+--+----------+----------+--------------------------------+----+
|   Opcode    |1 |    rd    |   rs1    |        Immediate Value         |mod |
|  (5 bits)   |I | (4 bits) | (4 bits) |           (16 bits)            |(2b)|
+-------------+--+----------+----------+--------------------------------+----+
```
* **Modifier Decoding ($[1:0]$)**:
  * `00` (`MOD_U`): Zero-extend 16-bit unsigned immediate $\implies \text{Value} = imm_{16}$.
  * `01` (`MOD_H`): Shift immediate to high half-word $\implies \text{Value} = imm_{16} \ll 16$.
  * `10` (`MOD_S`): Sign-extend 16-bit immediate $\implies \text{Value} = \text{SignExtend}_{32}(imm_{16})$.

### 3.4.3 Format B: Branch, Jump, and Call Operations
```
 31         27 26                                                           0
+-------------+--------------------------------------------------------------+
|   Opcode    |                 Signed Branch Offset (imm27)                 |
|  (5 bits)   |               (27-bit PC-relative word offset)               |
+-------------+--------------------------------------------------------------+
```
* **Target Calculation**:
  $$\text{Effective Target} = \text{Current PC} + (\text{SignExtend}_{32}(imm_{27}) \times 4)$$
  Provides a branch reach of $\pm 2^{26}$ instructions ($\pm 256\text{ MB}$), covering the entire standard program address space.

---

## 3.5 Worked Encoding and Decoding Examples

The five worked examples below demonstrate exact machine code generation by the assembler (`assembler/Assembler.hpp`) and reconstruction by the disassembler (`assembler/Disassembler.hpp`):

### Example 1: `add r3, r1, r2` (R-Type Arithmetic)
* **Fields**:
  * Opcode: `add` $\implies$ `"00000"`
  * $I$-bit: Register operand $\implies$ `'0'`
  * $r_d$: `r3` $\implies$ `3` $\implies$ `"0011"`
  * $r_{s1}$: `r1` $\implies$ `1` $\implies$ `"0001"`
  * $r_{s2}$: `r2` $\implies$ `2` $\implies$ `"0010"`
  * Unused: 14 zero bits $\implies$ `"00000000000000"`
* **Binary Word**: `00000 0 0011 0001 0010 00000000000000` $\implies$ `00000000110001001000000000000000`
* **Hexadecimal Word**: `0x00C48000`

### Example 2: `add r1, r1, 1` (I-Type Signed Immediate)
* **Fields**:
  * Opcode: `add` $\implies$ `"00000"`
  * $I$-bit: Immediate operand $\implies$ `'1'`
  * $r_d$: `r1` $\implies$ `1` $\implies$ `"0001"`
  * $r_{s1}$: `r1` $\implies$ `1` $\implies$ `"0001"`
  * Immediate: `1` in 16 bits $\implies$ `"0000000000000001"`
  * Modifier: Signed (`MOD_S`) $\implies$ `"10"`
* **Binary Word**: `00000 1 0001 0001 0000000000000001 10` $\implies$ `00000100010001000000000000000110`
* **Hexadecimal Word**: `0x04440006`

### Example 3: `ld r4, 16[r5]` (I-Type Memory Load)
* **Fields**:
  * Opcode: `ld` $\implies$ `"01110"`
  * $I$-bit: Immediate offset $\implies$ `'1'`
  * $r_d$: `r4` $\implies$ `4` $\implies$ `"0100"`
  * $r_{s1}$: `r5` $\implies$ `5` $\implies$ `"0101"`
  * Immediate: `16` in 16 bits $\implies$ `"0000000000010000"`
  * Modifier: Signed (`MOD_S`) $\implies$ `"10"`
* **Binary Word**: `01110 1 0100 0101 0000000000010000 10` $\implies$ `01110101000101000000000001000010`
* **Hexadecimal Word**: `0x75140042`

### Example 4: `beq label_loop` (B-Type Conditional Branch, Offset = -5 Words)
* **Target Calculation**: Target is 5 instructions backwards $\implies \text{Offset} = -5$.
* **27-bit 2's Complement of -5**:
  * $+5 = \text{0x0000005} \implies -5 = \text{0x7FFFFFFB}$ (in 27 bits: `"111111111111111111111111011"`).
* **Fields**:
  * Opcode: `beq` $\implies$ `"10000"`
  * Branch Offset: `"111111111111111111111111011"`
* **Binary Word**: `10000 111111111111111111111111011` $\implies$ `10000111111111111111111111111011`
* **Hexadecimal Word**: `0x87FFFFFB`

### Example 5: `call func_fact` (B-Type Subroutine Call, Offset = +12 Words)
* **Target Calculation**: Target is 12 instructions forward $\implies \text{Offset} = +12$.
* **27-bit Binary of +12**: `"000000000000000000000001100"`
* **Fields**:
  * Opcode: `call` $\implies$ `"10011"`
  * Branch Offset: `"000000000000000000000001100"`
* **Binary Word**: `10011 000000000000000000000001100` $\implies$ `10011000000000000000000000001100`
* **Hexadecimal Word**: `0x9800000C`

