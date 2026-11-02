"""
cpu/iss.py: Non-pipelined architectural Instruction Set Simulator (ISS).
Serves as the golden reference interpreter for SimpleRisc / RISC201.
Driven strictly by binary-string opcodes and common/isa.py metadata.
"""

from __future__ import annotations
from typing import Optional, Tuple, Dict
from common.isa import (
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR,
    OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_NOP, OP_LD, OP_ST,
    OP_BEQ, OP_BGT, OP_B, OP_CALL, OP_RET, OP_MOVZ, OP_RETZ,
    OP_MIN, OP_MAX, OP_ROTS, OP_INVALID,
    decode, Instruction, REG_SP, REG_RA, IllegalInstructionError
)
from cpu.regfile import RegisterFile, Flags
from cpu.alu import ALU, ALUOutput
from cpu.exceptions import CpuException, ExceptionType, StackBounds
from memory.hierarchy import MemoryHierarchy
from memory.vm import PageFaultException


class InstructionSetSimulator:
    """
    Pure sequential reference interpreter (ISS) for SimpleRisc ISA.
    Executes one instruction to full architectural retirement with zero pipelining.
    Used for differential equivalence verification against pipelined cores.
    """
    def __init__(self, memory_hierarchy: Optional[MemoryHierarchy] = None):
        self.mem = memory_hierarchy if memory_hierarchy is not None else MemoryHierarchy()
        self.reg_file = RegisterFile()
        self.alu = ALU()
        self.stack_bounds = StackBounds()

        self.pc: int = 0
        self.program_size: int = 0
        self.halted: bool = False
        self.retired_instructions: int = 0
        self.cycles: int = 0
        self.current_exception = CpuException()
        self.asid: int = 0

    def reset(self) -> None:
        self.reg_file.reset()
        self.pc = 0
        self.program_size = 0
        self.halted = False
        self.retired_instructions = 0
        self.cycles = 0
        self.current_exception.clear()

    def step(self) -> bool:
        """
        Executes exactly one instruction sequentially.
        Returns True if an instruction was executed, False if halted or end of program.
        """
        if self.halted:
            return False
        if self.program_size > 0 and self.pc >= self.program_size:
            self.halted = True
            return False

        current_pc = self.pc
        self.cycles += 1

        # 1. Fetch
        try:
            raw_word, lat = self.mem.read_word(
                vaddr=current_pc, is_fetch=True, pc=current_pc,
                current_cycle=self.cycles, asid=self.asid
            )
        except PageFaultException as pfe:
            self.current_exception.type = ExceptionType.OUT_OF_BOUNDS_MEMORY
            self.current_exception.faulting_pc = current_pc
            self.current_exception.faulting_address = pfe.vaddr
            self.current_exception.details = pfe.details
            self.halted = True
            return False

        # 2. Decode
        try:
            inst = decode(raw_word, current_pc)
        except IllegalInstructionError as err:
            self.current_exception.type = ExceptionType.ILLEGAL_OPCODE
            self.current_exception.faulting_pc = current_pc
            self.current_exception.details = str(err)
            self.halted = True
            return False

        # Advance PC to next sequential word by default
        next_pc = (current_pc + 4) & 0xFFFFFFFF
        op = inst.opcode

        # 3. Read Operands
        opA = self.reg_file.read_port1(REG_RA if inst.is_ret() else inst.rs1)
        opB = inst.immx if inst.is_immediate else self.reg_file.read_port2(inst.rs2)
        store_data = self.reg_file.read_port2(inst.rd)

        # 4. Execute ALU / Address calculation
        alu_out = self.alu.execute(op, opA, opB, self.reg_file.flags)
        if op == OP_CMP:
            self.reg_file.flags = alu_out.flags
        elif op in (OP_MIN, OP_MAX, OP_ROTS):
            self.reg_file.flags = alu_out.flags

        if alu_out.exception.has_occurred():
            self.current_exception = alu_out.exception
            self.current_exception.faulting_pc = current_pc
            self.halted = True
            return False

        wb_data = (current_pc + 4) & 0xFFFFFFFF if inst.is_call() else alu_out.result

        # 5. Memory Access
        if inst.is_load():
            addr = alu_out.result
            try:
                word_val, _ = self.mem.read_word(
                    vaddr=addr, is_fetch=False, pc=current_pc,
                    current_cycle=self.cycles, asid=self.asid
                )
                wb_data = word_val
            except PageFaultException as pfe:
                self.current_exception.type = ExceptionType.STACK_CORRUPTION if pfe.fault_type == "guard_page" else ExceptionType.OUT_OF_BOUNDS_MEMORY
                self.current_exception.faulting_pc = current_pc
                self.current_exception.faulting_address = pfe.vaddr
                self.current_exception.details = pfe.details
                self.halted = True
                return False

        elif inst.is_store():
            addr = alu_out.result
            # Check stack bounds
            stack_ex = self.stack_bounds.verify_write_address(addr)
            if stack_ex != ExceptionType.NONE:
                self.current_exception.type = stack_ex
                self.current_exception.faulting_pc = current_pc
                self.current_exception.faulting_address = addr
                self.current_exception.details = f"Stack guard violation at 0x{addr:08X}"
                self.halted = True
                return False
            try:
                self.mem.write_word(
                    vaddr=addr, val=store_data, pc=current_pc,
                    current_cycle=self.cycles, asid=self.asid
                )
            except PageFaultException as pfe:
                self.current_exception.type = ExceptionType.STACK_CORRUPTION
                self.current_exception.faulting_pc = current_pc
                self.current_exception.faulting_address = pfe.vaddr
                self.halted = True
                return False

        # 6. Branch Handling
        if op == OP_B:
            next_pc = inst.branch_target
        elif op == OP_BEQ:
            if self.reg_file.flags.E:
                next_pc = inst.branch_target
        elif op == OP_BGT:
            if self.reg_file.flags.GT:
                next_pc = inst.branch_target
        elif op == OP_CALL:
            self.reg_file.set_ra(next_pc)
            next_pc = inst.branch_target
        elif op == OP_RET:
            next_pc = self.reg_file.get_ra()
        elif op == OP_RETZ:
            next_pc = self.reg_file.old_pc
            self.reg_file.cpl = 1

        # 7. Writeback
        if inst.writes_register() and not self.current_exception.has_occurred():
            self.reg_file.write_port(inst.get_dest_reg(), wb_data, True)

        self.pc = next_pc
        self.retired_instructions += 1
        return True

    def run(self, max_instructions: int = 100000) -> None:
        while not self.halted and self.retired_instructions < max_instructions:
            if not self.step():
                break
