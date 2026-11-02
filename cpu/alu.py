"""
cpu/alu.py: Full ALU operations supporting arithmetic, logic, shifting,
and condition flags generation matching SimpleRisc specification.
All operations dispatched via 5-bit BINARY STRING opcodes.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from common.isa import (
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR,
    OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_MIN, OP_MAX, OP_ROTS,
    OP_LD, OP_ST, ALU_OP_ADD, ALU_OP_SUB, ALU_OP_MUL, ALU_OP_DIV,
    ALU_OP_MOD, ALU_OP_CMP, ALU_OP_AND, ALU_OP_OR, ALU_OP_NOT, ALU_OP_MOV,
    ALU_OP_LSL, ALU_OP_LSR, ALU_OP_ASR, ALU_OP_MIN, ALU_OP_MAX, ALU_OP_ROTS
)
from cpu.regfile import Flags
from cpu.exceptions import CpuException, ExceptionType


@dataclass
class ALUOutput:
    result: int = 0
    flags: Flags = field(default_factory=Flags)
    exception: CpuException = field(default_factory=CpuException)


class ALU:
    @staticmethod
    def execute(op: str, a: int, b: int, current_flags: Flags) -> ALUOutput:
        """
        Executes ALU operation using binary-string opcode `op`.
        Returns ALUOutput containing computation result, updated condition flags, and exceptions.
        """
        out = ALUOutput(result=0, flags=Flags(E=current_flags.E, GT=current_flags.GT))
        sa = a if a < 0x80000000 else a - 0x100000000
        sb = b if b < 0x80000000 else b - 0x100000000

        if op == OP_ADD or op == ALU_OP_ADD or op in (OP_LD, OP_ST):
            out.result = (a + b) & 0xFFFFFFFF

        elif op == OP_SUB or op == ALU_OP_SUB:
            out.result = (a - b) & 0xFFFFFFFF

        elif op == OP_MUL or op == ALU_OP_MUL:
            out.result = (a * b) & 0xFFFFFFFF

        elif op == OP_DIV or op == ALU_OP_DIV:
            if b == 0:
                out.exception.type = ExceptionType.DIVISION_BY_ZERO
                out.exception.details = "Division by zero in ALU"
                out.result = 0
            else:
                out.result = int(sa / sb) & 0xFFFFFFFF

        elif op == OP_MOD or op == ALU_OP_MOD:
            if b == 0:
                out.exception.type = ExceptionType.DIVISION_BY_ZERO
                out.exception.details = "Modulo by zero in ALU"
                out.result = 0
            else:
                out.result = int(sa % sb) & 0xFFFFFFFF

        elif op == OP_CMP or op == ALU_OP_CMP:
            diff = (a - b) & 0xFFFFFFFF
            out.result = diff
            out.flags.E = (a == b)
            out.flags.GT = (sa > sb)

        elif op == OP_AND or op == ALU_OP_AND:
            out.result = (a & b) & 0xFFFFFFFF

        elif op == OP_OR or op == ALU_OP_OR:
            out.result = (a | b) & 0xFFFFFFFF

        elif op == OP_NOT or op == ALU_OP_NOT:
            out.result = (~b) & 0xFFFFFFFF

        elif op == OP_MOV or op == ALU_OP_MOV:
            out.result = b & 0xFFFFFFFF

        elif op == OP_LSL or op == ALU_OP_LSL:
            shift = b & 0x1F
            out.result = (a << shift) & 0xFFFFFFFF

        elif op == OP_LSR or op == ALU_OP_LSR:
            shift = b & 0x1F
            out.result = (a >> shift) & 0xFFFFFFFF

        elif op == OP_ASR or op == ALU_OP_ASR:
            shift = b & 0x1F
            out.result = (sa >> shift) & 0xFFFFFFFF

        elif op == OP_MIN or op == ALU_OP_MIN:
            out.result = a if sa < sb else b
            out.flags.E = (out.result == 0)
            out.flags.GT = (sa > sb)

        elif op == OP_MAX or op == ALU_OP_MAX:
            out.result = a if sa > sb else b
            out.flags.E = (out.result == 0)
            out.flags.GT = (sa > sb)

        elif op == OP_ROTS or op == ALU_OP_ROTS:
            shift = b & 0x1F
            out.result = ((a >> shift) | (a << (32 - shift))) & 0xFFFFFFFF
            out.flags.E = (out.result == 0)

        else: # default / pass-through
            out.result = b & 0xFFFFFFFF

        return out
