"""
cpu/microcode.py: Microprogrammed Control Unit integrated with MemoryHierarchy.
Models microcode execution, control-store fields as fixed-width binary strings,
and memory wait states where microPC stalls waiting on memory/TLB/page walk.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from common.isa import (
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR,
    OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_NOP, OP_LD, OP_ST,
    OP_BEQ, OP_BGT, OP_B, OP_CALL, OP_RET, OP_MOVZ, OP_RETZ,
    OP_MIN, OP_MAX, OP_ROTS,
    ALU_OP_ADD, ALU_OP_SUB, ALU_OP_MUL, ALU_OP_DIV, ALU_OP_MOD,
    ALU_OP_CMP, ALU_OP_AND, ALU_OP_OR, ALU_OP_NOT, ALU_OP_MOV,
    ALU_OP_LSL, ALU_OP_LSR, ALU_OP_ASR, ALU_OP_MIN, ALU_OP_MAX, ALU_OP_ROTS,
    decode, REG_SP, REG_RA, int_to_bits, bits_to_int
)
from cpu.regfile import RegisterFile, Flags
from cpu.alu import ALU
from memory.hierarchy import MemoryHierarchy

# =====================================================================
# MICROCODE FIELD WIDTHS AND CONTROL WORD DEFINITION
# =====================================================================
UCODE_ALU_BITS: int     = 5   # ALU select code (5 bits)
UCODE_REG_WR_BITS: int  = 1   # Register write enable (1 bit)
UCODE_MEM_RD_BITS: int  = 1   # Memory read enable (1 bit)
UCODE_MEM_WR_BITS: int  = 1   # Memory write enable (1 bit)
UCODE_FLAGS_WR_BITS: int= 1   # Flags write enable (1 bit)
UCODE_BRANCH_BITS: int  = 3   # Branch condition (3 bits)
UCODE_NEXT_UPC_BITS: int= 4   # Next micro-PC (4 bits)

UCODE_WORD_BITS: int    = (
    UCODE_ALU_BITS + UCODE_REG_WR_BITS + UCODE_MEM_RD_BITS +
    UCODE_MEM_WR_BITS + UCODE_FLAGS_WR_BITS + UCODE_BRANCH_BITS + UCODE_NEXT_UPC_BITS
) # 16 bits


@dataclass(frozen=True)
class MicroControlWord:
    alu_sel: str         # 5 bits
    reg_wr: str          # 1 bit ("1" or "0")
    mem_rd: str          # 1 bit
    mem_wr: str          # 1 bit
    flags_wr: str        # 1 bit
    branch_cond: str     # 3 bits ("000"=none, "001"=uncond, "010"=E, "011"=GT, "100"=call, "101"=ret)
    next_upc: str        # 4 bits

    def to_bitstr(self) -> str:
        return (
            self.alu_sel + self.reg_wr + self.mem_rd +
            self.mem_wr + self.flags_wr + self.branch_cond + self.next_upc
        )

    def to_word(self) -> int:
        return int(self.to_bitstr(), 2)


# Mapping ROM: Opcode binary string -> Micro-address entry point
MICRO_DISPATCH_ROM: Dict[str, int] = {
    OP_ADD:  10,
    OP_SUB:  11,
    OP_MUL:  12,
    OP_DIV:  13,
    OP_MOD:  14,
    OP_CMP:  15,
    OP_AND:  16,
    OP_OR:   17,
    OP_NOT:  18,
    OP_MOV:  19,
    OP_LSL:  20,
    OP_LSR:  21,
    OP_ASR:  22,
    OP_NOP:  23,
    OP_LD:   24,
    OP_ST:   25,
    OP_BEQ:  26,
    OP_BGT:  27,
    OP_B:    28,
    OP_CALL: 29,
    OP_RET:  30,
    OP_MIN:  31,
    OP_MAX:  32,
    OP_ROTS: 33,
}


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
            # Preamble 1: Fetch IR from memory
            word, lat = self.mem.read_word(self.pc, is_fetch=True, pc=self.pc, current_cycle=self.total_micro_cycles)
            self.ir = word
            self.upc = 1
            if lat > 1:
                self.mem_wait_cycles = lat - 1
                self.waiting_on = "IF"
            return False

        elif self.upc == 1:
            # Preamble 2: PC increment & Decode
            self.pc = (self.pc + 4) & 0xFFFFFFFF
            self.upc = 2
            return False

        elif self.upc == 2:
            # Dispatch via Mapping ROM keyed by binary-string opcode
            d = decode(self.ir, self.pc - 4)
            op = d.opcode

            if op == OP_ADD:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == OP_SUB:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == OP_MUL:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op in (OP_DIV, OP_MOD):
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == OP_CMP:
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                out = self.alu.execute(op, a, b, self.reg_file.flags)
                self.reg_file.flags = out.flags
                self.upc = 0
                return True

            elif op in (OP_AND, OP_OR, OP_NOT, OP_LSL, OP_LSR, OP_ASR, OP_MIN, OP_MAX, OP_ROTS):
                a = self.reg_file.read_port1(d.rs1)
                b = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                res = self.alu.execute(op, a, b, self.reg_file.flags).result
                self.reg_file.write_port(d.rd, res)
                self.upc = 0
                return True

            elif op == OP_MOV:
                val = d.immx if d.is_immediate else self.reg_file.read_port2(d.rs2)
                self.reg_file.write_port(d.rd, val)
                self.upc = 0
                return True

            elif op == OP_LD:
                base = self.reg_file.read_port1(d.rs1)
                self.mar = (base + d.imm16) & 0xFFFFFFFF
                word, lat = self.mem.read_word(self.mar, is_fetch=False, pc=self.pc - 4, current_cycle=self.total_micro_cycles)
                self.reg_file.write_port(d.rd, word)
                self.upc = 0
                if lat > 1:
                    self.mem_wait_cycles = lat - 1
                    self.waiting_on = "MEM_LOAD"
                return True

            elif op == OP_ST:
                base = self.reg_file.read_port1(d.rs1)
                self.mar = (base + d.imm16) & 0xFFFFFFFF
                self.mdr = self.reg_file.read_port2(d.rd)
                lat = self.mem.write_word(self.mar, self.mdr, pc=self.pc - 4, current_cycle=self.total_micro_cycles)
                self.upc = 0
                if lat > 1:
                    self.mem_wait_cycles = lat - 1
                    self.waiting_on = "MEM_STORE"
                return True

            elif op == OP_B:
                self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == OP_BEQ:
                if self.reg_file.flags.E:
                    self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == OP_BGT:
                if self.reg_file.flags.GT:
                    self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == OP_CALL:
                self.reg_file.set_ra(self.pc)
                self.pc = d.branch_target
                self.upc = 0
                return True

            elif op == OP_RET:
                self.pc = self.reg_file.get_ra()
                self.upc = 0
                return True

            else: # NOP or unhandled
                self.upc = 0
                return True

        return False
