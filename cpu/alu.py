"""
cpu/alu.py: Full ALU operations supporting arithmetic, logic, shifting,
and condition flags generation matching SimpleRisc specification.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Tuple
from cpu.isa import Opcode
from cpu.regfile import Flags
from cpu.exceptions import CpuException, ExceptionType


@dataclass
class ALUOutput:
    result: int = 0
    flags: Flags = field(default_factory=Flags)
    exception: CpuException = field(default_factory=CpuException)


class ALU:
    @staticmethod
    def execute(op: Opcode, a: int, b: int, current_flags: Flags) -> ALUOutput:
        out = ALUOutput(result=0, flags=Flags(E=current_flags.E, GT=current_flags.GT))
        sa = a if a < 0x80000000 else a - 0x100000000
        sb = b if b < 0x80000000 else b - 0x100000000

        if op == Opcode.OP_ADD or op in (Opcode.OP_LD, Opcode.OP_ST):
            out.result = (a + b) & 0xFFFFFFFF

        elif op == Opcode.OP_SUB:
            out.result = (a - b) & 0xFFFFFFFF

        elif op == Opcode.OP_MUL:
            out.result = (a * b) & 0xFFFFFFFF

        elif op == Opcode.OP_DIV:
            if b == 0:
                out.exception.type = ExceptionType.DIVISION_BY_ZERO
                out.exception.details = "Division by zero in ALU"
                out.result = 0
            else:
                out.result = int(sa / sb) & 0xFFFFFFFF

        elif op == Opcode.OP_MOD:
            if b == 0:
                out.exception.type = ExceptionType.DIVISION_BY_ZERO
                out.exception.details = "Modulo by zero in ALU"
                out.result = 0
            else:
                out.result = int(sa % sb) & 0xFFFFFFFF

        elif op == Opcode.OP_CMP:
            diff = (a - b) & 0xFFFFFFFF
            out.result = diff
            out.flags.E = (a == b)
            out.flags.GT = (sa > sb)

        elif op == Opcode.OP_AND:
            out.result = (a & b) & 0xFFFFFFFF

        elif op == Opcode.OP_OR:
            out.result = (a | b) & 0xFFFFFFFF

        elif op == Opcode.OP_NOT:
            out.result = (~b) & 0xFFFFFFFF

        elif op == Opcode.OP_MOV:
            out.result = b & 0xFFFFFFFF

        elif op == Opcode.OP_LSL:
            shift = b & 0x1F
            out.result = (a << shift) & 0xFFFFFFFF

        elif op == Opcode.OP_LSR:
            shift = b & 0x1F
            out.result = (a >> shift) & 0xFFFFFFFF

        elif op == Opcode.OP_ASR:
            shift = b & 0x1F
            out.result = (sa >> shift) & 0xFFFFFFFF

        elif op == Opcode.OP_MIN:
            out.result = a if sa < sb else b
            out.flags.E = (out.result == 0)
            out.flags.GT = (sa > sb)

        elif op == Opcode.OP_MAX:
            out.result = a if sa > sb else b
            out.flags.E = (out.result == 0)
            out.flags.GT = (sa > sb)

        elif op == Opcode.OP_ROTS:
            shift = b & 0x1F
            out.result = ((a >> shift) | (a << (32 - shift))) & 0xFFFFFFFF
            out.flags.E = (out.result == 0)

        else: # default / pass-through
            out.result = b & 0xFFFFFFFF

        return out
