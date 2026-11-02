"""
cpu/isa.py: Compatibility shim and re-export of common/isa.py.
Maintains backward compatibility for modules importing from cpu.isa.
"""

from __future__ import annotations
from common.isa import (
    WORD_BITS, OPCODE_BITS, I_BITS, REG_BITS, MOD_BITS, IMM16_BITS,
    OFFSET27_BITS, UNUSED_R_BITS, ALU_OP_BITS, FWD_SEL_BITS,
    NUM_REGISTERS, REG_ZERO, REG_SP, REG_RA,
    REG_ZERO_BITS, REG_SP_BITS, REG_RA_BITS,
    MOD_DEFAULT_BITS, MOD_UNSIGNED_BITS, MOD_HIGH_BITS, MOD_RESERVED_BITS,
    FWD_SEL_REG, FWD_SEL_EXMEM, FWD_SEL_MEMWB, FWD_SEL_WBID,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR,
    OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_NOP, OP_LD, OP_ST,
    OP_BEQ, OP_BGT, OP_B, OP_CALL, OP_RET, OP_MOVZ, OP_RETZ,
    OP_MIN, OP_MAX, OP_ROTS, OP_INVALID, OP_BUBBLE,
    ALU_OP_ADD, ALU_OP_SUB, ALU_OP_MUL, ALU_OP_DIV, ALU_OP_MOD,
    ALU_OP_CMP, ALU_OP_AND, ALU_OP_OR, ALU_OP_NOT, ALU_OP_MOV,
    ALU_OP_LSL, ALU_OP_LSR, ALU_OP_ASR, ALU_OP_MIN, ALU_OP_MAX,
    ALU_OP_ROTS, ALU_OP_PASS,
    int_to_bits, bits_to_int, word_to_bits, bits_to_word, slice_field,
    IllegalInstructionError, InstructionInfo, INSTRUCTION_TABLE,
    MNEMONIC_TO_OPCODE, validate_opcode_bits, mnemonic_of, opcode_of,
    Instruction, BUBBLE_INSTRUCTION, decode, encode, print_isa_table
)


class Opcode:
    """Namespace providing binary-string opcode constants."""
    OP_ADD     = OP_ADD
    OP_SUB     = OP_SUB
    OP_MUL     = OP_MUL
    OP_DIV     = OP_DIV
    OP_MOD     = OP_MOD
    OP_CMP     = OP_CMP
    OP_AND     = OP_AND
    OP_OR      = OP_OR
    OP_NOT     = OP_NOT
    OP_MOV     = OP_MOV
    OP_LSL     = OP_LSL
    OP_LSR     = OP_LSR
    OP_ASR     = OP_ASR
    OP_NOP     = OP_NOP
    OP_LD      = OP_LD
    OP_ST      = OP_ST
    OP_BEQ     = OP_BEQ
    OP_BGT     = OP_BGT
    OP_B       = OP_B
    OP_CALL    = OP_CALL
    OP_RET     = OP_RET
    OP_MOVZ    = OP_MOVZ
    OP_RETZ    = OP_RETZ
    OP_MIN     = OP_MIN
    OP_MAX     = OP_MAX
    OP_ROTS    = OP_ROTS
    OP_INVALID = OP_INVALID


# Backward-compatibility aliases
decode_instruction = decode
DecodedInst = Instruction


def disassemble_instruction(word: int, pc: int = 0) -> str:
    """Disassembles 32-bit machine word into SimpleRisc assembly string."""
    try:
        inst = decode(word, pc)
        return inst.disassembly
    except IllegalInstructionError:
        return f"illegal (0x{word:08X})"
