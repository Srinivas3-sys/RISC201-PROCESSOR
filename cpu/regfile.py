"""
cpu/regfile.py: SimpleRisc register file (r0-r15, flags, and privileged registers).
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List


@dataclass
class Flags:
    E: bool = False   # Equal flag
    GT: bool = False  # Greater-Than flag

    def clear(self) -> None:
        self.E = False
        self.GT = False


class RegisterFile:
    def __init__(self):
        self.regs: List[int] = [0] * 16
        self.flags = Flags()
        self.old_pc: int = 0
        self.old_sp: int = 0
        self.old_flags = Flags()
        self.cpl: int = 1 # 0: privileged, 1: user mode
        self.reset()

    def reset(self) -> None:
        self.regs = [0] * 16
        self.regs[14] = 0x000FFFFC # SP default base
        self.flags.clear()
        self.old_pc = 0
        self.old_sp = 0
        self.old_flags.clear()
        self.cpl = 1

    def read_port1(self, r: int) -> int:
        return self.regs[r & 0xF]

    def read_port2(self, r: int) -> int:
        return self.regs[r & 0xF]

    def write_port(self, r: int, val: int, enable: bool = True) -> None:
        if enable:
            self.regs[r & 0xF] = val & 0xFFFFFFFF

    def get_sp(self) -> int:
        return self.regs[14]

    def set_sp(self, sp: int) -> None:
        self.regs[14] = sp & 0xFFFFFFFF

    def get_ra(self) -> int:
        return self.regs[15]

    def set_ra(self, ra: int) -> None:
        self.regs[15] = ra & 0xFFFFFFFF
