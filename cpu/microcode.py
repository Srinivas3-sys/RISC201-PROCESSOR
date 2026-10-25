"""
cpu/microcode.py: Microprogrammed Control Unit integrated with MemoryHierarchy.
Models microcode execution, shared internal datapath bus, and memory wait states
where microPC stalls waiting on memory/TLB/page walk.
"""

from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple
from cpu.isa import Opcode, decode_instruction, REG_SP, REG_RA
from cpu.regfile import RegisterFile, Flags
from cpu.alu import ALU
from memory.hierarchy import MemoryHierarchy


class MicroController:
    def __init__(self, memory_hierarchy: Optional[MemoryHierarchy] = None):
        self.mem = memory_hierarchy if memory_hierarchy is not None else MemoryHierarchy()
        self.reg_file = RegisterFile()
        self.alu = ALU()

        self.upc: int = 0
        self.pc: int = 0
        self.ir: int = 0
        self.total_micro_cycles: int = 0
        self.instruction_finished: bool = False

        # Micro-registers
        self.A: int = 0
        self.B: int = 0
        self.mar: int = 0
        self.mdr: int = 0
        self.alu_result: int = 0
        self.ld_result: int = 0

        # Memory wait-state timer
        self.mem_wait_cycles: int = 0
        self.waiting_on: str = "NONE" # "IF", "MEM_LOAD", "MEM_STORE"

        self.reset()

    def reset(self) -> None:
        self.reg_file.reset()
        self.upc = 0
        self.pc = 0
        self.ir = 0
        self.total_micro_cycles = 0
        self.instruction_finished = False
        self.A = 0
        self.B = 0
        self.mar = 0
        self.mdr = 0
        self.alu_result = 0
        self.ld_result = 0
        self.mem_wait_cycles = 0
        self.waiting_on = "NONE"

    def step_micro_cycle(self) -> bool:
        """
        Executes one micro-cycle. If waiting on memory, stalls microPC until memory responds.
        Returns True when a program instruction completes.
        """
        self.total_micro_cycles += 1

        if self.mem_wait_cycles > 0:
            self.mem_wait_cycles -= 1
            return False # MicroPC frozen in wait state

        # Microcode State Machine
        if self.upc == 0:
            # Preamble: Fetch IR from memory
            word, lat = self.mem.read_word(self.pc, is_fetch=True, pc=self.pc, current_cycle=self.total_micro_cycles)
            self.ir = word
            self.upc = 1
            if lat > 1:
                self.mem_wait_cycles = lat - 1
                self.waiting_on = "IF"
            return False

        elif self.upc == 1:
            # Decode & PC increment
            self.pc = (self.pc + 4) & 0xFFFFFFFF
            self.upc = 2
            return False

        elif self.upc == 2:
            # Dispatch to opcode handler
            d = decode_instruction(self.ir, self.pc - 4)
            op = d.opcode

            if op == Opcode.OP_ADD:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == Opcode.OP_SUB:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == Opcode.OP_CMP:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                out = self.alu.execute(op, a, b, self.reg_file.flags)
                self.reg_file.flags = out.flags
                self.upc = 0
                return True

            elif op == Opcode.OP_MOV:
                val = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                self.reg_file.write_port(d.rd, val)
                self.upc = 0
                return True

            elif op == Opcode.OP_LD:
                base = self.reg_file.read_port1(d.rs1)
                self.mar = (base + d.imm16) & 0xFFFFFFFF
                word, lat = self.mem.read_word(self.mar, is_fetch=False, pc=self.pc - 4, current_cycle=self.total_micro_cycles)
                self.reg_file.write_port(d.rd, word)
                self.upc = 0
                if lat > 1:
                    self.mem_wait_cycles = lat - 1
                    self.waiting_on = "MEM_LOAD"
                return True

            elif op == Opcode.OP_ST:
                base = self.reg_file.read_port1(d.rs1)
                self.mar = (base + d.imm16) & 0xFFFFFFFF
                self.mdr = self.reg_file.read_port2(d.rd)
                lat = self.mem.write_word(self.mar, self.mdr, pc=self.pc - 4, current_cycle=self.total_micro_cycles)
                self.upc = 0
                if lat > 1:
                    self.mem_wait_cycles = lat - 1
                    self.waiting_on = "MEM_STORE"
                return True

            elif op == Opcode.OP_B:
                self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == Opcode.OP_BEQ:
                if self.reg_file.flags.E:
                    self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == Opcode.OP_BGT:
                if self.reg_file.flags.GT:
                    self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == Opcode.OP_CALL:
                self.reg_file.set_ra(self.pc)
                self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == Opcode.OP_RET:
                self.pc = self.reg_file.get_ra()
                self.upc = 0
                return True

            else: # NOP or unhandled
                self.upc = 0
                return True

        return False
