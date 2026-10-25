"""
cpu/exceptions.py: Exception definitions and stack boundary checker.
Directly compatible with common/Exception.hpp.
"""

from __future__ import annotations
from enum import IntEnum
from dataclasses import dataclass


class ExceptionType(IntEnum):
    NONE = 0
    STACK_OVERFLOW = 1
    STACK_UNDERFLOW = 2
    STACK_CORRUPTION = 3
    UNALIGNED_MEMORY_ACCESS = 4
    OUT_OF_BOUNDS_MEMORY = 5
    DIVISION_BY_ZERO = 6
    ILLEGAL_OPCODE = 7
    PRIVILEGE_VIOLATION = 8


@dataclass
class CpuException:
    type: ExceptionType = ExceptionType.NONE
    faulting_pc: int = 0
    faulting_address: int = 0
    details: str = ""

    def has_occurred(self) -> bool:
        return self.type != ExceptionType.NONE

    def clear(self) -> None:
        self.type = ExceptionType.NONE
        self.faulting_pc = 0
        self.faulting_address = 0
        self.details = ""


@dataclass
class StackBounds:
    stack_base: int = 0x000FFFFC   # Highest address where stack begins
    stack_limit: int = 0x00080000  # Lowest allowed stack pointer address (512 KB)
    guard_band_size: int = 64      # 64-byte guard zone below stack limit

    def verify_sp(self, sp: int) -> ExceptionType:
        if sp > self.stack_base:
            return ExceptionType.STACK_UNDERFLOW
        if sp < self.stack_limit:
            return ExceptionType.STACK_OVERFLOW
        return ExceptionType.NONE

    def verify_write_address(self, addr: int) -> ExceptionType:
        if (self.stack_limit - self.guard_band_size) <= addr < self.stack_limit:
            return ExceptionType.STACK_CORRUPTION
        return ExceptionType.NONE
