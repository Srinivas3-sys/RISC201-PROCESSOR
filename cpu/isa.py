"""
cpu/isa.py: Architectural types, opcodes, instruction formats, and decoders
for the RISC201 / SimpleRisc processor.
"""

from __future__ import annotations
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional


class Opcode(IntEnum):
    OP_ADD  = 0b00000 # 0
    OP_SUB  = 0b00001 # 1
    OP_MUL  = 0b00010 # 2
    OP_DIV  = 0b00011 # 3
    OP_MOD  = 0b00100 # 4
    OP_CMP  = 0b00101 # 5
    OP_AND  = 0b00110 # 6
    OP_OR   = 0b00111 # 7
    OP_NOT  = 0b01000 # 8
    OP_MOV  = 0b01001 # 9
    OP_LSL  = 0b01010 # 10
    OP_LSR  = 0b01011 # 11
    OP_ASR  = 0b01100 # 12
    OP_NOP  = 0b01101 # 13
    OP_LD   = 0b01110 # 14
    OP_ST   = 0b01111 # 15
    OP_BEQ  = 0b10000 # 16
    OP_BGT  = 0b10001 # 17
    OP_B    = 0b10010 # 18
    OP_CALL = 0b10011 # 19
    OP_RET  = 0b10100 # 20
    OP_MOVZ = 0b10101 # 21
    OP_RETZ = 0b10110 # 22
    # Extensions
    OP_MIN  = 0b10111 # 23
    OP_MAX  = 0b11000 # 24
    OP_ROTS = 0b11001 # 25
    OP_INVALID = 0b11111


REG_ZERO = 0
REG_SP = 14
REG_RA = 15
NUM_REGISTERS = 16


@dataclass
class DecodedInst:
    raw_word: int = 0
    opcode: Opcode = Opcode.OP_NOP
    is_immediate: bool = False
    rd: int = 0
    rs1: int = 0
    rs2: int = 0
    imm_modifier: int = 0                  # 0: default signed, 1: unsigned 'u', 2: high 'h'
    imm16: int = 0
    immx: int = 0                          # Expanded 32-bit immediate
    branch_offset: int = 0                 # Signed 27-bit word offset
    branch_target: int = 0                 # Target address = PC + (offset << 2)

    def is_branch(self) -> bool:
        return self.opcode in (
            Opcode.OP_B, Opcode.OP_BEQ, Opcode.OP_BGT,
            Opcode.OP_CALL, Opcode.OP_RET, Opcode.OP_RETZ
        )

    def is_conditional_branch(self) -> bool:
        return self.opcode in (Opcode.OP_BEQ, Opcode.OP_BGT)

    def is_call(self) -> bool:
        return self.opcode == Opcode.OP_CALL

    def is_ret(self) -> bool:
        return self.opcode in (Opcode.OP_RET, Opcode.OP_RETZ)

    def is_load(self) -> bool:
        return self.opcode == Opcode.OP_LD

    def is_store(self) -> bool:
        return self.opcode == Opcode.OP_ST

    def is_nop(self) -> bool:
        return self.opcode == Opcode.OP_NOP

    def writes_register(self) -> bool:
        if self.opcode in (
            Opcode.OP_NOP, Opcode.OP_ST, Opcode.OP_CMP,
            Opcode.OP_B, Opcode.OP_BEQ, Opcode.OP_BGT,
            Opcode.OP_RET, Opcode.OP_RETZ
        ):
            return False
        return True

    def get_dest_reg(self) -> int:
        if self.opcode == Opcode.OP_CALL:
            return REG_RA
        return self.rd

    def reads_rs1(self) -> bool:
        if self.opcode in (
            Opcode.OP_NOP, Opcode.OP_B, Opcode.OP_BEQ,
            Opcode.OP_BGT, Opcode.OP_CALL, Opcode.OP_NOT, Opcode.OP_MOV
        ):
            return False
        return True

    def reads_rs2(self) -> bool:
        if self.is_immediate:
            return False
        if self.opcode in (
            Opcode.OP_NOP, Opcode.OP_B, Opcode.OP_BEQ,
            Opcode.OP_BGT, Opcode.OP_CALL, Opcode.OP_RET, Opcode.OP_RETZ,
            Opcode.OP_LD
        ):
            return False
        return True

    def reads_rd(self) -> bool:
        return self.opcode == Opcode.OP_ST


def decode_instruction(word: int, pc: int = 0) -> DecodedInst:
    """Decodes a 32-bit machine word into a DecodedInst."""
    op_val = (word >> 27) & 0x1F
    try:
        opcode = Opcode(op_val)
    except ValueError:
        opcode = Opcode.OP_INVALID

    i_bit = bool((word >> 26) & 1)
    rd = (word >> 22) & 0xF
    rs1 = (word >> 18) & 0xF
    rs2 = (word >> 14) & 0xF
    mod = (word >> 16) & 0x3
    imm16 = word & 0xFFFF
    if imm16 & 0x8000:
        signed_imm16 = imm16 - 0x10000
    else:
        signed_imm16 = imm16

    # Immediate Expansion
    if mod == 0b01:      # unsigned 'u'
        immx = imm16 & 0xFFFF
    elif mod == 0b10:    # high 'h'
        immx = (imm16 & 0xFFFF) << 16
    else:                # default sign extended
        immx = signed_imm16 & 0xFFFFFFFF

    # Branch Offset Expansion (27-bit signed)
    offset27 = word & 0x07FFFFFF
    if offset27 & 0x04000000:
        offset27 -= 0x08000000 # Sign extend 27-bit
    branch_target = (pc + (offset27 << 2)) & 0xFFFFFFFF

    if opcode == Opcode.OP_RET:
        rs1 = REG_RA

    return DecodedInst(
        raw_word=word,
        opcode=opcode,
        is_immediate=i_bit,
        rd=rd,
        rs1=rs1,
        rs2=rs2,
        imm_modifier=mod,
        imm16=signed_imm16,
        immx=immx,
        branch_offset=offset27,
        branch_target=branch_target
    )


def disassemble_instruction(word: int, pc: int = 0) -> str:
    """Disassembles 32-bit machine word into SimpleRisc assembly string."""
    d = decode_instruction(word, pc)
    op = d.opcode

    if op == Opcode.OP_NOP:
        return "nop"
    if op == Opcode.OP_RET:
        return "ret"
    if op == Opcode.OP_RETZ:
        return "retz"

    if op in (Opcode.OP_B, Opcode.OP_BEQ, Opcode.OP_BGT, Opcode.OP_CALL):
        name = {Opcode.OP_B: "b", Opcode.OP_BEQ: "beq", Opcode.OP_BGT: "bgt", Opcode.OP_CALL: "call"}[op]
        return f"{name} 0x{d.branch_target:08X}"

    op_names = {
        Opcode.OP_ADD: "add", Opcode.OP_SUB: "sub", Opcode.OP_MUL: "mul",
        Opcode.OP_DIV: "div", Opcode.OP_MOD: "mod", Opcode.OP_CMP: "cmp",
        Opcode.OP_AND: "and", Opcode.OP_OR: "or", Opcode.OP_NOT: "not",
        Opcode.OP_MOV: "mov", Opcode.OP_LSL: "lsl", Opcode.OP_LSR: "lsr",
        Opcode.OP_ASR: "asr", Opcode.OP_LD: "ld", Opcode.OP_ST: "st",
        Opcode.OP_MIN: "min", Opcode.OP_MAX: "max", Opcode.OP_ROTS: "rots",
    }
    name = op_names.get(op, "unknown")

    if op == Opcode.OP_MOV:
        arg2 = f"{d.imm16}" if d.is_immediate else f"r{d.rs2}"
        return f"mov r{d.rd}, {arg2}"

    if op == Opcode.OP_NOT:
        arg2 = f"{d.imm16}" if d.is_immediate else f"r{d.rs2}"
        return f"not r{d.rd}, {arg2}"

    if op == Opcode.OP_CMP:
        arg2 = f"{d.imm16}" if d.is_immediate else f"r{d.rs2}"
        return f"cmp r{d.rs1}, {arg2}"

    if op == Opcode.OP_LD:
        return f"ld r{d.rd}, {d.imm16}[r{d.rs1}]"

    if op == Opcode.OP_ST:
        return f"st r{d.rd}, {d.imm16}[r{d.rs1}]"

    arg2 = f"{d.imm16}" if d.is_immediate else f"r{d.rs2}"
    return f"{name} r{d.rd}, r{d.rs1}, {arg2}"
