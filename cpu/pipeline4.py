"""
cpu/pipeline4.py: Cycle-accurate 4-Stage Pipelined Processor (IF -> OF -> EX -> MA_RW).
Integrated with MemoryHierarchy for Virtual Memory and cycle-accurate cache/DRAM stalls.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict
from cpu.isa import Opcode, DecodedInst, decode_instruction, disassemble_instruction, REG_SP, REG_RA
from cpu.regfile import RegisterFile
from cpu.alu import ALU
from cpu.exceptions import CpuException, ExceptionType, StackBounds
from memory.hierarchy import MemoryHierarchy
from memory.vm import PageFaultException


@dataclass
class Latch_IF_OF:
    pc: int = 0
    instruction: int = 0
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.is_bubble = True
        self.disassembly = "nop"


@dataclass
class Latch_OF_EX:
    pc: int = 0
    instruction: int = 0
    decoded: DecodedInst = field(default_factory=DecodedInst)
    op1: int = 0
    op2: int = 0
    immx: int = 0
    branch_target: int = 0
    is_immediate: bool = False
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.decoded = DecodedInst()
        self.op1 = 0
        self.op2 = 0
        self.immx = 0
        self.branch_target = 0
        self.is_immediate = False
        self.is_bubble = True
        self.disassembly = "nop"


@dataclass
class Latch_EX_MARW:
    pc: int = 0
    instruction: int = 0
    decoded: DecodedInst = field(default_factory=DecodedInst)
    alu_result: int = 0
    op2: int = 0
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.decoded = DecodedInst()
        self.alu_result = 0
        self.op2 = 0
        self.is_bubble = True
        self.disassembly = "nop"


class Pipeline4Stage:
    def __init__(self, memory_hierarchy: Optional[MemoryHierarchy] = None):
        self.mem = memory_hierarchy if memory_hierarchy is not None else MemoryHierarchy()
        self.reg_file = RegisterFile()
        self.alu = ALU()
        self.stack_bounds = StackBounds()

        self.pc = 0
        self.program_size = 0
        self.halted = False
        self.current_exception = CpuException()

        # Pipeline Latches
        self.l_if_of = Latch_IF_OF()
        self.l_of_ex = Latch_OF_EX()
        self.l_ex_marw = Latch_EX_MARW()

        self.next_if_of = Latch_IF_OF()
        self.next_of_ex = Latch_OF_EX()
        self.next_ex_marw = Latch_EX_MARW()

        # Performance & Stall Counters
        self.cycles = 0
        self.retired_instructions = 0
        self.raw_stall_cycles = 0
        self.bubble_cycles = 0
        self.branch_count = 0
        self.branch_taken_count = 0

        # Memory Stall Breakdown
        self.translation_stall_cycles = 0
        self.cache_stall_cycles = 0

        # Multi-cycle pending stalls
        self.if_pending_stall = 0
        self.mem_pending_stall = 0
        self.if_latched_inst = 0
        self.if_latched_pc = 0

        self.forwarding_enabled = True
        self.asid = 0

    def reset(self) -> None:
        self.reg_file.reset()
        self.pc = 0
        self.program_size = 0
        self.halted = False
        self.current_exception.clear()

        self.l_if_of.reset()
        self.l_of_ex.reset()
        self.l_ex_marw.reset()
        self.next_if_of.reset()
        self.next_of_ex.reset()
        self.next_ex_marw.reset()

        self.cycles = 0
        self.retired_instructions = 0
        self.raw_stall_cycles = 0
        self.bubble_cycles = 0
        self.branch_count = 0
        self.branch_taken_count = 0
        self.translation_stall_cycles = 0
        self.cache_stall_cycles = 0
        self.if_pending_stall = 0
        self.mem_pending_stall = 0
        self.if_latched_inst = 0
        self.if_latched_pc = 0

    def is_drained(self) -> bool:
        return (self.program_size > 0 and self.pc >= self.program_size) and \
               self.l_if_of.is_bubble and self.l_of_ex.is_bubble and self.l_ex_marw.is_bubble

    def step_cycle(self) -> None:
        if self.halted:
            return
        self.cycles += 1

        stall_if = False
        stall_of = False
        bubble_ex = False
        is_branch_taken = False
        branch_pc = 0

        # Handle pending memory stalls from previous cycles
        if self.mem_pending_stall > 0:
            self.mem_pending_stall -= 1
            self.cache_stall_cycles += 1
            return

        if self.if_pending_stall > 0:
            self.if_pending_stall -= 1
            self.cache_stall_cycles += 1
            stall_if = True
            if self.if_pending_stall == 0:
                self.next_if_of.pc = self.if_latched_pc
                self.next_if_of.instruction = self.if_latched_inst
                self.next_if_of.is_bubble = False
                self.next_if_of.disassembly = disassemble_instruction(self.if_latched_inst, self.if_latched_pc)
                self.pc += 4
            else:
                self.next_if_of.reset()

        # =====================================================================
        # STAGE 4: MA_RW (Memory Access and Register Writeback Merged)
        # =====================================================================
        if not self.l_ex_marw.is_bubble:
            d = self.l_ex_marw.decoded
            wb_data = self.l_ex_marw.alu_result

            if d.is_load():
                # Verify stack read boundary
                addr = self.l_ex_marw.alu_result
                try:
                    word_val, total_lat = self.mem.read_word(
                        vaddr=addr, is_fetch=False, pc=self.l_ex_marw.pc,
                        current_cycle=self.cycles, asid=self.asid
                    )
                    wb_data = word_val
                    if total_lat > 1:
                        # Extra cycles to stall
                        self.mem_pending_stall = total_lat - 1
                except PageFaultException as pfe:
                    self.current_exception.type = ExceptionType.STACK_CORRUPTION if pfe.fault_type == "guard_page" else ExceptionType.OUT_OF_BOUNDS_MEMORY
                    self.current_exception.faulting_pc = self.l_ex_marw.pc
                    self.current_exception.faulting_address = pfe.vaddr
                    self.current_exception.details = pfe.details
                    self.halted = True
                    return

            elif d.is_store():
                addr = self.l_ex_marw.alu_result
                # Verify stack boundary safety
                stack_ex = self.stack_bounds.verify_write_address(addr)
                if stack_ex != ExceptionType.NONE:
                    self.current_exception.type = stack_ex
                    self.current_exception.faulting_pc = self.l_ex_marw.pc
                    self.current_exception.faulting_address = addr
                    self.current_exception.details = f"Stack guard violation at 0x{addr:08X}"
                    self.halted = True
                    return
                try:
                    total_lat = self.mem.write_word(
                        vaddr=addr, val=self.l_ex_marw.op2, pc=self.l_ex_marw.pc,
                        current_cycle=self.cycles, asid=self.asid
                    )
                    if total_lat > 1:
                        self.mem_pending_stall = total_lat - 1
                except PageFaultException as pfe:
                    self.current_exception.type = ExceptionType.STACK_CORRUPTION
                    self.current_exception.faulting_pc = self.l_ex_marw.pc
                    self.current_exception.faulting_address = pfe.vaddr
                    self.halted = True
                    return

            elif d.is_call():
                wb_data = (self.l_ex_marw.pc + 4) & 0xFFFFFFFF

            if d.writes_register() and not self.current_exception.has_occurred():
                self.reg_file.write_port(d.get_dest_reg(), wb_data, True)

            self.retired_instructions += 1

        # =====================================================================
        # STAGE 3: EX (Execute Stage)
        # =====================================================================
        if not self.l_of_ex.is_bubble:
            d = self.l_of_ex.decoded
            opA = self.l_of_ex.op1

            # Forwarding for Operand A
            if self.forwarding_enabled and not self.l_ex_marw.is_bubble and self.l_ex_marw.decoded.writes_register():
                prev_dest = self.l_ex_marw.decoded.get_dest_reg()
                if d.reads_rs1() and d.rs1 == prev_dest:
                    opA = self.l_ex_marw.alu_result

            # Forwarding for Operand B
            opB = self.l_of_ex.immx if self.l_of_ex.is_immediate else self.l_of_ex.op2
            if not self.l_of_ex.is_immediate and self.forwarding_enabled and not self.l_ex_marw.is_bubble and self.l_ex_marw.decoded.writes_register():
                prev_dest = self.l_ex_marw.decoded.get_dest_reg()
                if d.reads_rs2() and d.rs2 == prev_dest:
                    opB = self.l_ex_marw.alu_result

            # Store data forwarding
            store_data = self.l_of_ex.op2
            if d.is_store() and self.forwarding_enabled and not self.l_ex_marw.is_bubble and self.l_ex_marw.decoded.writes_register():
                if d.rd == self.l_ex_marw.decoded.get_dest_reg():
                    store_data = self.l_ex_marw.alu_result

            # ALU computation
            alu_out = self.alu.execute(d.opcode, opA, opB, self.reg_file.flags)
            if d.opcode == Opcode.OP_CMP:
                self.reg_file.flags = alu_out.flags
            if alu_out.exception.has_occurred():
                self.current_exception = alu_out.exception
                self.current_exception.faulting_pc = self.l_of_ex.pc
                self.halted = True
                return

            # Branch condition evaluation
            if d.is_branch():
                self.branch_count += 1
                flg = self.reg_file.flags
                if d.opcode == Opcode.OP_B:
                    is_branch_taken = True
                    branch_pc = self.l_of_ex.branch_target
                elif d.opcode == Opcode.OP_BEQ:
                    is_branch_taken = flg.E
                    branch_pc = self.l_of_ex.branch_target
                elif d.opcode == Opcode.OP_BGT:
                    is_branch_taken = flg.GT
                    branch_pc = self.l_of_ex.branch_target
                elif d.opcode == Opcode.OP_CALL:
                    is_branch_taken = True
                    branch_pc = self.l_of_ex.branch_target
                elif d.opcode == Opcode.OP_RET:
                    is_branch_taken = True
                    branch_pc = opA
                elif d.opcode == Opcode.OP_RETZ:
                    is_branch_taken = True
                    branch_pc = self.reg_file.old_pc
                    self.reg_file.cpl = 1

                if is_branch_taken:
                    self.branch_taken_count += 1

            self.next_ex_marw.pc = self.l_of_ex.pc
            self.next_ex_marw.instruction = self.l_of_ex.instruction
            self.next_ex_marw.decoded = d
            self.next_ex_marw.alu_result = alu_out.result
            self.next_ex_marw.op2 = store_data
            self.next_ex_marw.is_bubble = False
            self.next_ex_marw.disassembly = self.l_of_ex.disassembly
        else:
            self.next_ex_marw.reset()

        # =====================================================================
        # STAGE 2: OF (Operand Fetch / Instruction Decode)
        # =====================================================================
        if not self.l_if_of.is_bubble:
            d = decode_instruction(self.l_if_of.instruction, self.l_if_of.pc)

            # Hazard Detection Unit:
            # Load-Use Hazard check
            load_use = False
            if not self.l_of_ex.is_bubble and self.l_of_ex.decoded.is_load():
                ld_dest = self.l_of_ex.decoded.rd
                if (d.reads_rs1() and d.rs1 == ld_dest) or \
                   (d.reads_rs2() and d.rs2 == ld_dest) or \
                   (d.is_store() and d.rd == ld_dest):
                    load_use = True

            raw_no_forward = False
            if not self.forwarding_enabled and not self.l_of_ex.is_bubble and self.l_of_ex.decoded.writes_register():
                dest = self.l_of_ex.decoded.get_dest_reg()
                if (d.reads_rs1() and d.rs1 == dest) or \
                   (d.reads_rs2() and d.rs2 == dest) or \
                   (d.is_store() and d.rd == dest):
                    raw_no_forward = True

            if load_use or raw_no_forward:
                stall_if = True
                stall_of = True
                bubble_ex = True
                self.raw_stall_cycles += 1
            else:
                op1 = self.reg_file.read_port1(REG_RA if d.is_ret() else d.rs1)
                op2 = self.reg_file.read_port2(d.rd if d.is_store() else d.rs2)

                # Forwarding from MA_RW to OF
                if self.forwarding_enabled and not self.l_ex_marw.is_bubble and self.l_ex_marw.decoded.writes_register():
                    wb_reg = self.l_ex_marw.decoded.get_dest_reg()
                    wb_val = self.l_ex_marw.alu_result
                    if self.l_ex_marw.decoded.is_call():
                        wb_val = (self.l_ex_marw.pc + 4) & 0xFFFFFFFF
                    if d.reads_rs1() and d.rs1 == wb_reg: op1 = wb_val
                    if d.reads_rs2() and d.rs2 == wb_reg: op2 = wb_val
                    if d.is_store() and d.rd == wb_reg: op2 = wb_val

                self.next_of_ex.pc = self.l_if_of.pc
                self.next_of_ex.instruction = self.l_if_of.instruction
                self.next_of_ex.decoded = d
                self.next_of_ex.op1 = op1
                self.next_of_ex.op2 = op2
                self.next_of_ex.immx = d.immx
                self.next_of_ex.branch_target = d.branch_target
                self.next_of_ex.is_immediate = d.is_immediate
                self.next_of_ex.is_bubble = False
                self.next_of_ex.disassembly = self.l_if_of.disassembly
        else:
            self.next_of_ex.reset()

        # =====================================================================
        # STAGE 1: IF (Instruction Fetch)
        # =====================================================================
        if is_branch_taken:
            self.pc = branch_pc
            self.next_if_of.reset()
            self.next_of_ex.reset()
            self.bubble_cycles += 2
        elif self.program_size > 0 and self.pc >= self.program_size:
            self.next_if_of.reset()
        elif not stall_if:
            try:
                inst_word, fetch_lat = self.mem.read_word(
                    vaddr=self.pc, is_fetch=True, pc=self.pc,
                    current_cycle=self.cycles, asid=self.asid
                )
                if fetch_lat > 1:
                    self.if_pending_stall = fetch_lat - 1
                    self.if_latched_inst = inst_word
                    self.if_latched_pc = self.pc
                    self.next_if_of.reset()
                else:
                    self.next_if_of.pc = self.pc
                    self.next_if_of.instruction = inst_word
                    self.next_if_of.is_bubble = False
                    self.next_if_of.disassembly = disassemble_instruction(inst_word, self.pc)
                    self.pc += 4
            except PageFaultException as pfe:
                self.current_exception.type = ExceptionType.OUT_OF_BOUNDS_MEMORY
                self.current_exception.faulting_pc = self.pc
                self.current_exception.faulting_address = pfe.vaddr
                self.current_exception.details = pfe.details
                self.halted = True
                return

        # Synchronous Latch Updates
        if bubble_ex:
            self.l_of_ex.reset()
        elif not stall_of:
            self.l_of_ex = self.next_of_ex

        if not stall_if or self.if_pending_stall == 0:
            self.l_if_of = self.next_if_of
        else:
            self.l_if_of.reset()
        self.l_ex_marw = self.next_ex_marw

    def run(self, max_cycles: int = 100000) -> None:
        while not self.halted and self.cycles < max_cycles:
            self.step_cycle()
            if self.is_drained():
                break

    def print_stats(self) -> str:
        cpi = (self.cycles / self.retired_instructions) if self.retired_instructions > 0 else 0.0
        ipc = (self.retired_instructions / self.cycles) if self.cycles > 0 else 0.0
        out = [
            "=== 4-Stage Pipeline Statistics ===",
            f"  Total Clock Cycles      : {self.cycles}",
            f"  Retired Instructions    : {self.retired_instructions}",
            f"  Cycles Per Inst (CPI)   : {cpi:.3f}",
            f"  Inst Per Cycle (IPC)    : {ipc:.3f}",
            f"  RAW Stall Cycles        : {self.raw_stall_cycles}",
            f"  Branch Bubble Cycles    : {self.bubble_cycles}",
            f"  Cache / DRAM Stalls     : {self.cache_stall_cycles}",
            f"  Translation Stalls      : {self.translation_stall_cycles}",
            f"  Total Branches          : {self.branch_count} (Taken: {self.branch_taken_count})",
            f"  Forwarding Enabled      : {'YES' if self.forwarding_enabled else 'NO (Interlocks only)'}",
        ]
        return "\n".join(out)
