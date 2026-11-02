# SimpleRisc Architectural ISA Specification: 5-Bit Binary-String Opcodes

## 1. Architectural Philosophy & Overview
The RISC201 SimpleRisc processor is a 32-bit load-store architecture modeled after modern RISC principles (such as MIPS, RISC-V, and DLX). Inside the Python simulator, the architectural identity of every instruction is strictly represented as a **5-bit fixed-width binary string** (`"00000"` through `"11001"` and `"11111"`), rather than an integer or arbitrary enum.

This representation reflects physical hardware control signals where opcodes are routed as 5 parallel bitlines to instruction decoders, ALU control units, and hazard detection logic.

---

## 2. Instruction Word Formats & Bitfields

All SimpleRisc instructions are 32 bits wide, aligned to 4-byte boundaries, with bit indexing documented MSB-first ($[31:0]$):

### Format R (Register-Register)
```
 31       27 26 25       22 21       18 17       14 13          0
+-----------+--+-----------+-----------+-----------+--------------+
|   Opcode  |I |    rd     |    rs1    |    rs2    |   Unused     |
|   (5 b)   |0 |   (4 b)   |   (4 b)   |   (4 b)   |    (14 b)    |
+-----------+--+-----------+-----------+-----------+--------------+
```

### Format I (Register-Immediate)
```
 31       27 26 25       22 21       18 17 16 15                0
+-----------+--+-----------+-----------+-----+------------------+
|   Opcode  |I |    rd     |    rs1    | Mod |    Imm16 (16 b)  |
|   (5 b)   |1 |   (4 b)   |   (4 b)   | (2b)| (Signed/Unsigned)|
+-----------+--+-----------+-----------+-----+------------------+
```

### Format B (Branch & Jump)
```
 31       27 26                                                 0
+-----------+---------------------------------------------------+
|   Opcode  |               Branch Offset (27 b)                |
|   (5 b)   |             (Signed, Word-Shifted << 2)           |
+-----------+---------------------------------------------------+
```

### Bitfield Definitions:
- **`Opcode` $[31:27]$ (5 bits):** Primary operation selector as binary string.
- **`I` $[26:26]$ (1 bit):** Immediate operand flag (`0` = register operand $rs2$, `1` = 16-bit immediate $imm16$).
- **`rd` $[25:22]$ (4 bits):** Destination register ($r0 - r15$).
- **`rs1` $[21:18]$ (4 bits):** First source operand register ($r0 - r15$).
- **`rs2` $[17:14]$ (4 bits):** Second source operand register (when $I = 0$).
- **`Mod` $[17:16]$ (2 bits):** Immediate modifier (when $I = 1$):
  - `00` (Default): 16-bit signed immediate (sign-extended to 32 bits).
  - `01` (`'u'`): 16-bit unsigned immediate (zero-extended to 32 bits).
  - `10` (`'h'`): 16-bit high immediate (shifted left by 16 bits: `imm16 << 16`).
- **`Imm16` $[15:0]$ (16 bits):** Literal numerical immediate value.
- **`Offset` $[26:0]$ (27 bits):** Relative branch target offset, calculated as $\text{PC}_{\text{target}} = \text{PC} + (\text{Offset} \ll 2)$.

---

## 3. Complete Architectural Instruction Table

| Mnemonic | Opcode Bits | Format | Syntax | Description | ALU Select | Latency | Earliest Stage | Can Fault? |
| :--- | :---: | :---: | :--- | :--- | :---: | :---: | :---: | :---: |
| `add`  | `"00000"` | R / I | `rd, rs1, (rs2/imm)` | Integer Addition | `"00000"` | 1 | EX | No |
| `sub`  | `"00001"` | R / I | `rd, rs1, (rs2/imm)` | Integer Subtraction | `"00001"` | 1 | EX | No |
| `mul`  | `"00010"` | R / I | `rd, rs1, (rs2/imm)` | Integer Multiplication | `"00010"` | 2 | EX | No |
| `div`  | `"00011"` | R / I | `rd, rs1, (rs2/imm)` | Signed Division | `"00011"` | 4 | EX | Yes (DivZero) |
| `mod`  | `"00100"` | R / I | `rd, rs1, (rs2/imm)` | Modulo Remainder | `"00100"` | 4 | EX | Yes (DivZero) |
| `cmp`  | `"00101"` | R / I | `rs1, (rs2/imm)` | Compare (sets E, GT flags) | `"00101"` | 1 | EX | No |
| `and`  | `"00110"` | R / I | `rd, rs1, (rs2/imm)` | Bitwise AND | `"00110"` | 1 | EX | No |
| `or`   | `"00111"` | R / I | `rd, rs1, (rs2/imm)` | Bitwise OR | `"00111"` | 1 | EX | No |
| `not`  | `"01000"` | R / I | `rd, (rs2/imm)` | Bitwise NOT | `"01000"` | 1 | EX | No |
| `mov`  | `"01001"` | R / I | `rd, (rs2/imm)` | Register Move / Imm Load | `"01001"` | 1 | EX | No |
| `lsl`  | `"01010"` | R / I | `rd, rs1, (rs2/imm)` | Logical Shift Left | `"01010"` | 1 | EX | No |
| `lsr`  | `"01011"` | R / I | `rd, rs1, (rs2/imm)` | Logical Shift Right | `"01011"` | 1 | EX | No |
| `asr`  | `"01100"` | R / I | `rd, rs1, (rs2/imm)` | Arithmetic Shift Right | `"01100"` | 1 | EX | No |
| `nop`  | `"01101"` | B | — | No Operation (Pipeline Bubble) | `"11111"` | 1 | EX | No |
| `ld`   | `"01110"` | I | `rd, imm[rs1]` | Load Word from Memory | `"00000"` | 1+ | MEM | Yes (PageFault, Guard) |
| `st`   | `"01111"` | I | `rd, imm[rs1]` | Store Word to Memory | `"00000"` | 1+ | MEM | Yes (PageFault, Guard) |
| `beq`  | `"10000"` | B | `offset` | Branch if Equal ($E = 1$) | `"11111"` | 1 | EX | No |
| `bgt`  | `"10001"` | B | `offset` | Branch if Greater ($GT = 1$) | `"11111"` | 1 | EX | No |
| `b`    | `"10010"` | B | `offset` | Unconditional Branch | `"11111"` | 1 | EX | No |
| `call` | `"10011"` | B | `offset` | Procedure Call ($ra \leftarrow \text{PC}+4$) | `"11111"` | 1 | EX | No |
| `ret`  | `"10100"` | R | — | Procedure Return ($\text{PC} \leftarrow ra$) | `"11111"` | 1 | EX | No |
| `movz` | `"10101"` | PRIV | `rd, priv_reg` | Privileged Register Move | `"11111"` | 1 | EX | Yes (PrivViolation) |
| `retz` | `"10110"` | PRIV | — | Return from Interrupt/Trap | `"11111"` | 1 | EX | Yes (PrivViolation) |
| `min`  | `"10111"` | R / I | `rd, rs1, (rs2/imm)` | Minimum of Operands | `"10111"` | 1 | EX | No |
| `max`  | `"11000"` | R / I | `rd, rs1, (rs2/imm)` | Maximum of Operands | `"11000"` | 1 | EX | No |
| `rots` | `"11001"` | R / I | `rd, rs1, (rs2/imm)` | Rotate Shift | `"11001"` | 1 | EX | No |
| `inv`  | `"11111"` | — | — | Reserved Illegal Opcode | `"11111"` | 1 | — | Yes (IllegalOpcode) |

---

## 4. Architectural Register Allocation

SimpleRisc provides sixteen 32-bit general-purpose registers ($r0 - r15$):

| Register | Architectural Alias | Hardware Width | Convention & Usage |
| :---: | :---: | :---: | :--- |
| `r0` | — | 32 bits | General Purpose Scratch (Note: NOT hardwired zero; arithmetic general register) |
| `r1` - `r13` | — | 32 bits | General Purpose Scratch / Function Arguments / Local Variables |
| `r14` | `sp` | 32 bits | **Stack Pointer** (Full-descending stack, default top: `0x000FFFFC`) |
| `r15` | `ra` | 32 bits | **Return Address Register** (Target for `call`, source for `ret`) |

---

## 5. Bit-String Helper API & Verification

The single source of truth for all binary-string operations resides in `common/isa.py`:

```python
from common.isa import (
    int_to_bits, bits_to_int, word_to_bits, bits_to_word,
    slice_field, encode, decode, validate_opcode_bits
)

# Convert integer to exactly 5-bit binary string
op_str = int_to_bits(14, 5)   # -> "01110" (ld)

# Slice fields from 32-bit binary instruction word
op_slice = slice_field("01110000010000000000000000000000", 31, 27)  # -> "01110"

# Decode 32-bit machine word into typed Instruction
inst = decode(0x70400000, pc=0x1000)
assert inst.opcode == "01110"
assert inst.is_load() == True
assert inst.rd == 1
```

All helpers strictly enforce that binary strings contain only `'0'` and `'1'` characters and match exact field widths, raising `ValueError` on any violation.
