"""
common/isa.py: Architectural single source of truth for the RISC201 / SimpleRisc ISA.
Represents all opcodes, ALU select codes, and register fields as fixed-width BINARY STRINGS.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Any

# =====================================================================
# 1. FIELD WIDTH CONSTANTS (Single Source of Truth)
# =====================================================================
WORD_BITS: int = 32
OPCODE_BITS: int = 5
I_BITS: int = 1
REG_BITS: int = 4
MOD_BITS: int = 2
IMM16_BITS: int = 16
OFFSET27_BITS: int = 27
UNUSED_R_BITS: int = 14
ALU_OP_BITS: int = 5
FWD_SEL_BITS: int = 2

# Architectural Register Counts
NUM_REGISTERS: int = 16
REG_ZERO: int = 0
REG_SP: int = 14
REG_RA: int = 15

REG_ZERO_BITS: str = "0000"
REG_SP_BITS: str = "1110"
REG_RA_BITS: str = "1111"

# Immediate Modifiers as 2-bit binary strings
MOD_DEFAULT_BITS: str = "00"   # Signed 16-bit
MOD_UNSIGNED_BITS: str = "01"  # 'u' Unsigned 16-bit
MOD_HIGH_BITS: str = "10"      # 'h' High 16-bit (shifted left by 16)
MOD_RESERVED_BITS: str = "11"

# Forwarding Mux Select Codes as 2-bit binary strings
FWD_SEL_REG: str   = "00"  # Read from Register File / Latch directly
FWD_SEL_EXMEM: str = "01"  # Bypass from EX/MEM (or EX/MA)
FWD_SEL_MEMWB: str = "10"  # Bypass from MEM/WB (or MA/RW)
FWD_SEL_WBID: str  = "11"  # Bypass from WB to ID/RF

# =====================================================================
# 2. NAMED BINARY STRING CONSTANTS FOR EVERY OPCODE (5 BITS)
# =====================================================================
OP_ADD: str     = "00000"  # 0:  add  rd, rs1, (rs2/imm)
OP_SUB: str     = "00001"  # 1:  sub  rd, rs1, (rs2/imm)
OP_MUL: str     = "00010"  # 2:  mul  rd, rs1, (rs2/imm)
OP_DIV: str     = "00011"  # 3:  div  rd, rs1, (rs2/imm)
OP_MOD: str     = "00100"  # 4:  mod  rd, rs1, (rs2/imm)
OP_CMP: str     = "00101"  # 5:  cmp  rs1, (rs2/imm)
OP_AND: str     = "00110"  # 6:  and  rd, rs1, (rs2/imm)
OP_OR: str      = "00111"  # 7:  or   rd, rs1, (rs2/imm)
OP_NOT: str     = "01000"  # 8:  not  rd, (rs2/imm)
OP_MOV: str     = "01001"  # 9:  mov  rd, (rs2/imm)
OP_LSL: str     = "01010"  # 10: lsl  rd, rs1, (rs2/imm)
OP_LSR: str     = "01011"  # 11: lsr  rd, rs1, (rs2/imm)
OP_ASR: str     = "01100"  # 12: asr  rd, rs1, (rs2/imm)
OP_NOP: str     = "01101"  # 13: nop
OP_LD: str      = "01110"  # 14: ld   rd, imm[rs1]
OP_ST: str      = "01111"  # 15: st   rd, imm[rs1]
OP_BEQ: str     = "10000"  # 16: beq  offset
OP_BGT: str     = "10001"  # 17: bgt  offset
OP_B: str       = "10010"  # 18: b    offset
OP_CALL: str    = "10011"  # 19: call offset
OP_RET: str     = "10100"  # 20: ret  (PC <- ra)
OP_MOVZ: str    = "10101"  # 21: movz (transfer privileged <-> general)
OP_RETZ: str    = "10110"  # 22: retz (PC <- oldPC, CPL <- 1)
OP_MIN: str     = "10111"  # 23: min  rd, rs1, (rs2/imm)
OP_MAX: str     = "11000"  # 24: max  rd, rs1, (rs2/imm)
OP_ROTS: str    = "11001"  # 25: rots rd, rs1, (rs2/imm)
OP_INVALID: str = "11111"  # 31: illegal / invalid opcode

# Dedicated NOP/Bubble binary string
OP_BUBBLE: str  = OP_NOP

# =====================================================================
# 3. ALU OPERATION CODES AS 5-BIT BINARY STRINGS
# =====================================================================
ALU_OP_ADD: str  = "00000"
ALU_OP_SUB: str  = "00001"
ALU_OP_MUL: str  = "00010"
ALU_OP_DIV: str  = "00011"
ALU_OP_MOD: str  = "00100"
ALU_OP_CMP: str  = "00101"
ALU_OP_AND: str  = "00110"
ALU_OP_OR: str   = "00111"
ALU_OP_NOT: str  = "01000"
ALU_OP_MOV: str  = "01001"
ALU_OP_LSL: str  = "01010"
ALU_OP_LSR: str  = "01011"
ALU_OP_ASR: str  = "01100"
ALU_OP_MIN: str  = "10111"
ALU_OP_MAX: str  = "11000"
ALU_OP_ROTS: str = "11001"
ALU_OP_PASS: str = "11111"

# =====================================================================
# 4. BIT-STRING UTILITY API (Fully Validated)
# =====================================================================

def int_to_bits(value: int, width: int) -> str:
    """
    Converts an integer to a fixed-width binary string of '0' and '1'.
    Supports both unsigned integers [0, 2^width - 1] and signed two's complement [-2^(width-1), 2^(width-1) - 1].
    Raises ValueError if value cannot fit into width bits or width <= 0.
    """
    if width <= 0:
        raise ValueError(f"Width must be positive, got {width}")
    min_signed = -(1 << (width - 1))
    max_unsigned = (1 << width) - 1

    if value < min_signed or value > max_unsigned:
        raise ValueError(f"Value {value} out of range for {width}-bit representation ([{min_signed}, {max_unsigned}])")

    masked = value & ((1 << width) - 1)
    return f"{masked:0{width}b}"


def bits_to_int(bitstr: str, signed: bool = False) -> int:
    """
    Converts a binary string of '0' and '1' to an integer.
    If signed=True, treats the MSB as sign bit in two's complement.
    Raises ValueError if string contains non-'0'/'1' characters or is empty.
    """
    if not bitstr:
        raise ValueError("Cannot convert empty bit string to int")
    if not all(c in ('0', '1') for c in bitstr):
        raise ValueError(f"Invalid bit string contains non-binary characters: '{bitstr}'")

    val = int(bitstr, 2)
    if signed and bitstr[0] == '1':
        val -= (1 << len(bitstr))
    return val


def word_to_bits(word: int, width: int = WORD_BITS) -> str:
    """Converts a 32-bit (or custom width) integer word to a binary string."""
    return int_to_bits(word, width)


def bits_to_word(bitstr: str) -> int:
    """Converts a binary string to an unsigned 32-bit integer word."""
    return bits_to_int(bitstr, signed=False)


def slice_field(word_bits: str, hi: int, lo: int) -> str:
    """
    Extracts a bit field from hi down to lo (inclusive) from an MSB-first bit string.
    Convention: Bit 31 is the leftmost MSB (index 0 in 32-bit string),
                Bit 0 is the rightmost LSB (index 31 in 32-bit string).
    For a string of length N, bit k corresponds to character index (N - 1 - k).
    """
    n = len(word_bits)
    if hi < lo or lo < 0 or hi >= n:
        raise ValueError(f"Invalid slice indices [hi={hi}, lo={lo}] for bit string of length {n}")
    start_idx = n - 1 - hi
    end_idx = n - 1 - lo + 1
    return word_bits[start_idx:end_idx]


# =====================================================================
# 5. ILLEGAL INSTRUCTION EXCEPTION
# =====================================================================

class IllegalInstructionError(Exception):
    """Raised when an instruction contains an unrecognized opcode bit string."""
    def __init__(self, opcode_bits: str, raw_word: int):
        self.opcode_bits = opcode_bits
        self.raw_word = raw_word
        super().__init__(
            f"IllegalInstructionError: Opcode '{opcode_bits}' not recognized in table. "
            f"Word: 0x{raw_word:08X} (binary: {raw_word:032b})"
        )


# =====================================================================
# 6. INSTRUCTION METADATA TABLE DEFINITION
# =====================================================================

@dataclass(frozen=True)
class InstructionInfo:
    mnemonic: str
    opcode: str                 # 5-bit binary string
    funct: Optional[str]        # Optional funct string (None for standard SimpleRisc)
    format: str                 # "R", "I", "B", "PRIV"
    operand_syntax: str         # Human-readable syntax e.g. "rd, rs1, rs2"
    reads_rs1: bool
    reads_rs2: bool
    writes_rd: bool
    uses_imm: bool
    is_load: bool
    is_store: bool
    is_branch: bool
    is_jump: bool
    is_call: bool
    is_return: bool
    is_alu: bool
    alu_op: str                 # 5-bit binary string for ALU selection
    alu_name: str               # "ADD", "SUB", etc.
    exec_latency: int           # Latency in cycles
    functional_unit: str        # "ALU", "MUL", "DIV", "MEM", "BRANCH", "CONTROL"
    can_raise_exception: bool
    earliest_result_stage: str  # "EX" or "MEM" or "WB"


# Canonical metadata specifications for all 26 SimpleRisc instructions
_TABLE_ENTRIES: List[InstructionInfo] = [
    InstructionInfo(
        mnemonic="add", opcode=OP_ADD, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_ADD, alu_name="ADD", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="sub", opcode=OP_SUB, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_SUB, alu_name="SUB", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="mul", opcode=OP_MUL, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_MUL, alu_name="MUL", exec_latency=2, functional_unit="MUL",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="div", opcode=OP_DIV, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_DIV, alu_name="DIV", exec_latency=4, functional_unit="DIV",
        can_raise_exception=True, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="mod", opcode=OP_MOD, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_MOD, alu_name="MOD", exec_latency=4, functional_unit="DIV",
        can_raise_exception=True, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="cmp", opcode=OP_CMP, funct=None, format="R/I",
        operand_syntax="rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=False, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_CMP, alu_name="CMP", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="and", opcode=OP_AND, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_AND, alu_name="AND", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="or", opcode=OP_OR, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_OR, alu_name="OR", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="not", opcode=OP_NOT, funct=None, format="R/I",
        operand_syntax="rd, (rs2/imm)",
        reads_rs1=False, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_NOT, alu_name="NOT", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="mov", opcode=OP_MOV, funct=None, format="R/I",
        operand_syntax="rd, (rs2/imm)",
        reads_rs1=False, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_MOV, alu_name="MOV", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="lsl", opcode=OP_LSL, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_LSL, alu_name="LSL", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="lsr", opcode=OP_LSR, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_LSR, alu_name="LSR", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="asr", opcode=OP_ASR, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_ASR, alu_name="ASR", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="nop", opcode=OP_NOP, funct=None, format="B",
        operand_syntax="",
        reads_rs1=False, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="NOP", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="ld", opcode=OP_LD, funct=None, format="I",
        operand_syntax="rd, imm[rs1]",
        reads_rs1=True, reads_rs2=False, writes_rd=True, uses_imm=True,
        is_load=True, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_ADD, alu_name="ADDR_ADD", exec_latency=1, functional_unit="MEM",
        can_raise_exception=True, earliest_result_stage="MEM"
    ),
    InstructionInfo(
        mnemonic="st", opcode=OP_ST, funct=None, format="I",
        operand_syntax="rd, imm[rs1]",
        reads_rs1=True, reads_rs2=False, writes_rd=False, uses_imm=True,
        is_load=False, is_store=True, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_ADD, alu_name="ADDR_ADD", exec_latency=1, functional_unit="MEM",
        can_raise_exception=True, earliest_result_stage="MEM"
    ),
    InstructionInfo(
        mnemonic="beq", opcode=OP_BEQ, funct=None, format="B",
        operand_syntax="offset",
        reads_rs1=False, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="BRANCH", exec_latency=1, functional_unit="BRANCH",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="bgt", opcode=OP_BGT, funct=None, format="B",
        operand_syntax="offset",
        reads_rs1=False, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="BRANCH", exec_latency=1, functional_unit="BRANCH",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="b", opcode=OP_B, funct=None, format="B",
        operand_syntax="offset",
        reads_rs1=False, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=True, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="BRANCH", exec_latency=1, functional_unit="BRANCH",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="call", opcode=OP_CALL, funct=None, format="B",
        operand_syntax="offset",
        reads_rs1=False, reads_rs2=False, writes_rd=True, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=True, is_call=True, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="CALL", exec_latency=1, functional_unit="BRANCH",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="ret", opcode=OP_RET, funct=None, format="R",
        operand_syntax="",
        reads_rs1=True, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=True, is_call=False, is_return=True,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="RET", exec_latency=1, functional_unit="BRANCH",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="movz", opcode=OP_MOVZ, funct=None, format="PRIV",
        operand_syntax="rd, priv_reg",
        reads_rs1=False, reads_rs2=False, writes_rd=True, uses_imm=False,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="PRIV", exec_latency=1, functional_unit="CONTROL",
        can_raise_exception=True, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="retz", opcode=OP_RETZ, funct=None, format="PRIV",
        operand_syntax="",
        reads_rs1=False, reads_rs2=False, writes_rd=False, uses_imm=False,
        is_load=False, is_store=False, is_branch=True, is_jump=True, is_call=False, is_return=True,
        is_alu=False, alu_op=ALU_OP_PASS, alu_name="RETZ", exec_latency=1, functional_unit="CONTROL",
        can_raise_exception=True, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="min", opcode=OP_MIN, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_MIN, alu_name="MIN", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="max", opcode=OP_MAX, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_MAX, alu_name="MAX", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
    InstructionInfo(
        mnemonic="rots", opcode=OP_ROTS, funct=None, format="R/I",
        operand_syntax="rd, rs1, (rs2/imm)",
        reads_rs1=True, reads_rs2=True, writes_rd=True, uses_imm=True,
        is_load=False, is_store=False, is_branch=False, is_jump=False, is_call=False, is_return=False,
        is_alu=True, alu_op=ALU_OP_ROTS, alu_name="ROTS", exec_latency=1, functional_unit="ALU",
        can_raise_exception=False, earliest_result_stage="EX"
    ),
]

# Primary Instruction Table keyed by 5-bit binary string opcode
INSTRUCTION_TABLE: Dict[str, InstructionInfo] = {entry.opcode: entry for entry in _TABLE_ENTRIES}

# Mnemonic to opcode mapping table
MNEMONIC_TO_OPCODE: Dict[str, str] = {entry.mnemonic: entry.opcode for entry in _TABLE_ENTRIES}


def validate_opcode_bits(opcode_bits: str) -> None:
    """Validates that an opcode bit string is strictly OPCODE_BITS long and contains only '0' and '1'."""
    if len(opcode_bits) != OPCODE_BITS or not all(c in ('0', '1') for c in opcode_bits):
        raise ValueError(f"Invalid opcode bit string: '{opcode_bits}'. Must be {OPCODE_BITS}-bit string of '0'/'1'.")


def mnemonic_of(opcode_bits: str) -> str:
    """Returns the mnemonic for a binary opcode string. Raises IllegalInstructionError if unknown."""
    validate_opcode_bits(opcode_bits)
    if opcode_bits not in INSTRUCTION_TABLE:
        raise IllegalInstructionError(opcode_bits, bits_to_word(opcode_bits + "0" * 27))
    return INSTRUCTION_TABLE[opcode_bits].mnemonic


def opcode_of(mnemonic: str) -> str:
    """Returns the 5-bit binary opcode string for a mnemonic. Raises KeyError if unknown."""
    m = mnemonic.lower().strip()
    if m not in MNEMONIC_TO_OPCODE:
        raise KeyError(f"Unknown mnemonic: '{mnemonic}'")
    return MNEMONIC_TO_OPCODE[m]


# =====================================================================
# 7. DECODED INSTRUCTION STRUCTURE
# =====================================================================

@dataclass
class Instruction:
    raw_word: int = 0x68000000
    opcode: str = OP_NOP
    mnemonic: str = "nop"
    format: str = "B"
    is_immediate: bool = False
    rd: int = 0
    rd_bits: str = "0000"
    rs1: int = 0
    rs1_bits: str = "0000"
    rs2: int = 0
    rs2_bits: str = "0000"
    imm_modifier: int = 0
    imm_modifier_bits: str = "00"
    imm16: int = 0
    immx: int = 0
    branch_offset: int = 0
    branch_target: int = 0
    info: InstructionInfo = field(default_factory=lambda: INSTRUCTION_TABLE[OP_NOP])
    is_bubble: bool = False
    disassembly: str = "nop"

    # Convenience Query Methods driven by table metadata
    def is_branch(self) -> bool:
        return self.info.is_branch

    def is_conditional_branch(self) -> bool:
        return self.opcode in (OP_BEQ, OP_BGT)

    def is_call(self) -> bool:
        return self.info.is_call

    def is_ret(self) -> bool:
        return self.info.is_return

    def is_load(self) -> bool:
        return self.info.is_load

    def is_store(self) -> bool:
        return self.info.is_store

    def is_nop(self) -> bool:
        return self.opcode == OP_NOP or self.is_bubble

    def writes_register(self) -> bool:
        if self.is_bubble:
            return False
        return self.info.writes_rd

    def get_dest_reg(self) -> int:
        if self.opcode == OP_CALL:
            return REG_RA
        return self.rd

    def get_dest_reg_bits(self) -> str:
        if self.opcode == OP_CALL:
            return REG_RA_BITS
        return self.rd_bits

    def reads_rs1(self) -> bool:
        if self.is_bubble:
            return False
        return self.info.reads_rs1

    def reads_rs2(self) -> bool:
        if self.is_bubble or self.is_immediate:
            return False
        return self.info.reads_rs2

    def reads_rd(self) -> bool:
        # Store instructions read rd as source data operand
        return self.info.is_store


# Cached Bubble Instruction Singleton
_BUBBLE_INFO = INSTRUCTION_TABLE[OP_NOP]
BUBBLE_INSTRUCTION = Instruction(
    raw_word=0x68000000, # OP_NOP << 27
    opcode=OP_NOP,
    mnemonic="nop",
    format="B",
    is_immediate=False,
    rd=0, rd_bits="0000",
    rs1=0, rs1_bits="0000",
    rs2=0, rs2_bits="0000",
    imm_modifier=0, imm_modifier_bits="00",
    imm16=0, immx=0,
    branch_offset=0, branch_target=0,
    info=_BUBBLE_INFO,
    is_bubble=True,
    disassembly="nop [bubble]"
)


# =====================================================================
# 8. DECODE AND ENCODE ENGINES
# =====================================================================

def decode(word: int, pc: int = 0) -> Instruction:
    """
    Decodes a 32-bit machine word into an Instruction object.
    Decodes ONCE per instruction and caches the binary-string opcode and fields.
    Raises IllegalInstructionError if opcode bit string is not in the instruction table.
    """
    word_bits = word_to_bits(word & 0xFFFFFFFF, WORD_BITS)

    # Slice fields (MSB-first)
    op_bits  = slice_field(word_bits, 31, 27)
    i_bit    = slice_field(word_bits, 26, 26)
    rd_bits  = slice_field(word_bits, 25, 22)
    rs1_bits = slice_field(word_bits, 21, 18)
    rs2_bits = slice_field(word_bits, 17, 14)
    mod_bits = slice_field(word_bits, 17, 16)
    imm_bits = slice_field(word_bits, 15, 0)
    off_bits = slice_field(word_bits, 26, 0)

    validate_opcode_bits(op_bits)
    if op_bits not in INSTRUCTION_TABLE:
        raise IllegalInstructionError(op_bits, word)

    info = INSTRUCTION_TABLE[op_bits]
    is_imm = (i_bit == "1")
    rd = bits_to_int(rd_bits)
    rs1 = bits_to_int(rs1_bits)
    rs2 = bits_to_int(rs2_bits)
    mod = bits_to_int(mod_bits)
    imm16_val = bits_to_int(imm_bits, signed=True)

    # Immediate Expansion
    if mod == 1:      # unsigned 'u'
        immx = bits_to_int(imm_bits, signed=False)
    elif mod == 2:    # high 'h'
        immx = (bits_to_int(imm_bits, signed=False) << 16) & 0xFFFFFFFF
    else:             # default signed
        immx = imm16_val & 0xFFFFFFFF

    # Branch Offset Expansion
    branch_offset = bits_to_int(off_bits, signed=True)
    branch_target = (pc + (branch_offset << 2)) & 0xFFFFFFFF

    if op_bits == OP_RET:
        rs1 = REG_RA
        rs1_bits = REG_RA_BITS

    # Build disassembly representation
    if op_bits == OP_NOP:
        disasm = "nop"
    elif op_bits == OP_RET:
        disasm = "ret"
    elif op_bits == OP_RETZ:
        disasm = "retz"
    elif info.is_branch:
        disasm = f"{info.mnemonic} 0x{branch_target:08X}"
    elif op_bits in (OP_MOV, OP_NOT):
        arg2 = f"#{imm16_val}" if is_imm else f"r{rs2}"
        disasm = f"{info.mnemonic} r{rd}, {arg2}"
    elif op_bits == OP_CMP:
        arg2 = f"#{imm16_val}" if is_imm else f"r{rs2}"
        disasm = f"cmp r{rs1}, {arg2}"
    elif op_bits == OP_LD:
        disasm = f"ld r{rd}, {imm16_val}[r{rs1}]"
    elif op_bits == OP_ST:
        disasm = f"st r{rd}, {imm16_val}[r{rs1}]"
    else:
        arg2 = f"#{imm16_val}" if is_imm else f"r{rs2}"
        disasm = f"{info.mnemonic} r{rd}, r{rs1}, {arg2}"

    return Instruction(
        raw_word=word,
        opcode=op_bits,
        mnemonic=info.mnemonic,
        format=info.format,
        is_immediate=is_imm,
        rd=rd,
        rd_bits=rd_bits,
        rs1=rs1,
        rs1_bits=rs1_bits,
        rs2=rs2,
        rs2_bits=rs2_bits,
        imm_modifier=mod,
        imm_modifier_bits=mod_bits,
        imm16=imm16_val,
        immx=immx,
        branch_offset=branch_offset,
        branch_target=branch_target,
        info=info,
        is_bubble=False,
        disassembly=disasm
    )


def encode(
    mnemonic: str,
    rd: int = 0,
    rs1: int = 0,
    rs2: int = 0,
    imm: Optional[int] = None,
    imm_modifier: int = 0,
    branch_offset: Optional[int] = None
) -> int:
    """
    Assembles an instruction by concatenating field bit strings into a 32-bit machine word.
    Guarantees bit-identical encoding to SimpleRisc specifications.
    """
    op_bits = opcode_of(mnemonic)
    if mnemonic in ("nop", "ret", "retz"):
        return bits_to_word(op_bits + ("0" * 27))

    info = INSTRUCTION_TABLE[op_bits]

    if info.is_branch and branch_offset is not None:
        off_bits = int_to_bits(branch_offset, OFFSET27_BITS)
        word_bits = op_bits + off_bits
        return bits_to_word(word_bits)

    if imm is not None:
        # Immediate format: opcode(5) + "1" + rd(4) + rs1(4) + mod(2) + imm16(16)
        rd_bits = int_to_bits(rd, REG_BITS)
        rs1_bits = int_to_bits(rs1, REG_BITS)
        mod_bits = int_to_bits(imm_modifier & 0x3, MOD_BITS)
        imm16_bits = int_to_bits(imm & 0xFFFF, IMM16_BITS)
        word_bits = op_bits + "1" + rd_bits + rs1_bits + mod_bits + imm16_bits
        return bits_to_word(word_bits)
    else:
        # Register format: opcode(5) + "0" + rd(4) + rs1(4) + rs2(4) + unused(14)
        rd_bits = int_to_bits(rd, REG_BITS)
        rs1_bits = int_to_bits(rs1, REG_BITS)
        rs2_bits = int_to_bits(rs2, REG_BITS)
        word_bits = op_bits + "0" + rd_bits + rs1_bits + rs2_bits + ("0" * UNUSED_R_BITS)
        return bits_to_word(word_bits)


def print_isa_table() -> str:
    """Prints a formatted ASCII table of the complete SimpleRisc ISA."""
    lines = [
        "==================================================================================================",
        "                               RISC201 / SimpleRisc ARCHITECTURAL ISA TABLE                      ",
        "==================================================================================================",
        f"{'Mnemonic':<10} {'Opcode':<8} {'Format':<8} {'Syntax':<24} {'ALU Op':<8} {'Latency':<8} {'Stage':<6} {'Exception?'}",
        "--------------------------------------------------------------------------------------------------"
    ]
    for op, info in INSTRUCTION_TABLE.items():
        lines.append(
            f"{info.mnemonic:<10} {info.opcode:<8} {info.format:<8} {info.operand_syntax:<24} "
            f"{info.alu_op:<8} {info.exec_latency:<8} {info.earliest_result_stage:<6} {'YES' if info.can_raise_exception else 'NO'}"
        )
    lines.append("==================================================================================================")
    return "\n".join(lines)
