"""
cpu/pipeline4.py: Cycle-accurate 4-Stage Pipelined Processor (IF -> OF -> EX -> MA_RW).
Integrated with MemoryHierarchy (VM, TLBs, Caches, DRAM), HazardUnit, ForwardingUnit,
BranchPredictor, and PerformanceTracker.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict, Any

from common.isa import (
    Instruction, decode, disassemble_instruction,
    OP_NOP, OP_LD, OP_ST, OP_B, OP_BEQ, OP_BGT, OP_CALL, OP_RET, OP_RETZ,
    OP_CMP, OP_MOVZ, REG_SP, REG_RA, REG_ZERO, IllegalInstructionError,
    int_to_bits, bits_to_int
)
from cpu.regfile import RegisterFile, Flags
from cpu.alu import ALU, ALUOutput
from cpu.exceptions import CpuException, ExceptionType, StackBounds
from memory.hierarchy import MemoryHierarchy
from memory.vm import PageFaultException
from common.hazard import HazardUnit4Stage
from common.forwarding import ForwardingUnit4Stage
from common.branch import BranchPredictor
from common.perf import PerformanceTracker


@dataclass
class Latch_IF_OF:
    pc: int = 0
    instruction: int = 0
    decoded: Instruction = field(default_factory=Instruction)
    predicted_taken: bool = False
    predicted_target: int = 0
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.decoded = Instruction()
        self.predicted_taken = False
        self.predicted_target = 0
        self.is_bubble = True
        self.disassembly = "nop"

    def copy_from(self, other: Latch_IF_OF) -> None:
        self.pc = other.pc
        self.instruction = other.instruction
        self.decoded = other.decoded
        self.predicted_taken = other.predicted_taken
        self.predicted_target = other.predicted_target
        self.is_bubble = other.is_bubble
        self.disassembly = other.disassembly


@dataclass
class Latch_OF_EX:
    pc: int = 0
    instruction: int = 0
    decoded: Instruction = field(default_factory=Instruction)
    op1: int = 0
    op2: int = 0
    immx: int = 0
    branch_target: int = 0
    predicted_taken: bool = False
    predicted_target: int = 0
    is_immediate: bool = False
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.decoded = Instruction()
        self.op1 = 0
        self.op2 = 0
        self.immx = 0
        self.branch_target = 0
        self.predicted_taken = False
        self.predicted_target = 0
        self.is_immediate = False
        self.is_bubble = True
        self.disassembly = "nop"

    def copy_from(self, other: Latch_OF_EX) -> None:
        self.pc = other.pc
        self.instruction = other.instruction
        self.decoded = other.decoded
        self.op1 = other.op1
        self.op2 = other.op2
        self.immx = other.immx
        self.branch_target = other.branch_target
        self.predicted_taken = other.predicted_taken
        self.predicted_target = other.predicted_target
        self.is_immediate = other.is_immediate
        self.is_bubble = other.is_bubble
        self.disassembly = other.disassembly


@dataclass
class Latch_EX_MARW:
    pc: int = 0
    instruction: int = 0
    decoded: Instruction = field(default_factory=Instruction)
    alu_result: int = 0
    op2: int = 0
    is_bubble: bool = True
    disassembly: str = "nop"

    def reset(self) -> None:
        self.pc = 0
        self.instruction = 0
        self.decoded = Instruction()
        self.alu_result = 0
        self.op2 = 0
        self.is_bubble = True
        self.disassembly = "nop"

    def copy_from(self, other: Latch_EX_MARW) -> None:
        self.pc = other.pc
        self.instruction = other.instruction
        self.decoded = other.decoded
        self.alu_result = other.alu_result
        self.op2 = other.op2
        self.is_bubble = other.is_bubble
        self.disassembly = other.disassembly


class Pipeline4Stage:
    """
    4-Stage Pipelined Processor: IF -> OF -> EX -> MA_RW
    Cycle-accurate with advanced hazards, forwarding, branch prediction, and perf accounting.
    """
    def __init__(
        self,
        memory_hierarchy: Optional[MemoryHierarchy] = None,
        forwarding_mode: str = "full",
        branch_policy: str = "flush",
        predictor_size: int = 64,
        history_bits: int = 6,
        btb_entries: int = 64,
        ras_entries: int = 16,
        delay_slots: int = 0,
        unified_mem: bool = False,
        regfile_write_ports: int = 1,
        clock_mhz: float = 3200.0,
        check_forwarding: bool = False,
        fwd_path_enables: Optional[Dict[str, bool]] = None,
    ):
        self.mem = memory_hierarchy if memory_hierarchy is not None else MemoryHierarchy()
        self.reg_file = RegisterFile()
        self.alu = ALU()
        self.stack_bounds = StackBounds()

        self.hazard_unit = HazardUnit4Stage(
            forwarding_mode=forwarding_mode,
            unified_mem=unified_mem,
            regfile_write_ports=regfile_write_ports
        )
        self.forwarding_unit = ForwardingUnit4Stage(mode=forwarding_mode)
        if fwd_path_enables:
            self.forwarding_unit.path_enables.update(fwd_path_enables)
        self.branch_predictor = BranchPredictor(
            policy=branch_policy,
            table_size=predictor_size,
            history_bits=history_bits,
            btb_entries=btb_entries,
            ras_entries=ras_entries,
            delay_slots=delay_slots
        )
        self.perf = PerformanceTracker(pipeline_stages=4, clock_mhz=clock_mhz)
        self.check_forwarding = check_forwarding

        self.pc: int = 0
        self.program_size: int = 0
        self.halted: bool = False
        self.current_exception = CpuException()
        self.asid: int = 0

        # Pipeline Latches
        self.l_if_of = Latch_IF_OF()
        self.l_of_ex = Latch_OF_EX()
        self.l_ex_marw = Latch_EX_MARW()

        self.next_if_of = Latch_IF_OF()
        self.next_of_ex = Latch_OF_EX()
        self.next_ex_marw = Latch_EX_MARW()

        # Multi-cycle pending stalls
        self.if_pending_stall = 0
        self.mem_pending_stall = 0
        self.if_latched_inst = 0
        self.if_latched_pc = 0

        # Delay slot tracking
        self._delay_slot_redirect: Optional[int] = None
        self._delay_slot_remaining: int = 0

        # Cycle Trace Buffer for ASCII Diagrams
        self.cycle_log: List[Dict[str, Any]] = []

        # Forwarding check tracker: { (producer_pc, reg): expected_val }
        self._pending_fwd_checks: Dict[Tuple[int, int], int] = {}

    @property
    def cycles(self) -> int:
        return self.perf.total_cycles

    @cycles.setter
    def cycles(self, val: int) -> None:
        self.perf.total_cycles = val

    @property
    def retired_instructions(self) -> int:
        return self.perf.retired_instructions

    @retired_instructions.setter
    def retired_instructions(self, val: int) -> None:
        self.perf.retired_instructions = val

    @property
    def raw_stall_cycles(self) -> int:
        return self.perf.stall_cycles.get("RAW_LOAD_USE", 0) + self.perf.stall_cycles.get("RAW_NOFWD", 0)

    @property
    def bubble_cycles(self) -> int:
        return self.perf.stall_cycles.get("BRANCH_MISPREDICT", 0) + self.perf.stall_cycles.get("BRANCH_STALL", 0)

    @property
    def cache_stall_cycles(self) -> int:
        return self.perf.stall_cycles.get("ICACHE_MISS", 0) + self.perf.stall_cycles.get("DCACHE_MISS", 0)

    @property
    def translation_stall_cycles(self) -> int:
        return self.perf.stall_cycles.get("TLB_MISS", 0) + self.perf.stall_cycles.get("PAGE_WALK", 0)

    @property
    def branch_count(self) -> int:
        return self.perf.total_branches

    @property
    def branch_taken_count(self) -> int:
        return self.perf.branches_taken

    @property
    def forwarding_enabled(self) -> bool:
        return self.forwarding_unit.mode != "none"

    @forwarding_enabled.setter
    def forwarding_enabled(self, val: bool) -> None:
        mode = "full" if val else "none"
        self.forwarding_unit.mode = mode
        self.hazard_unit.forwarding_mode = mode

    def reset(self) -> None:
        self.reg_file.reset()
        self.hazard_unit.reset_telemetry()
        self.forwarding_unit.store_buffer.reset()
        self.branch_predictor.reset()
        self.perf.reset()

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

        self.if_pending_stall = 0
        self.mem_pending_stall = 0
        self.if_latched_inst = 0
        self.if_latched_pc = 0
        self._delay_slot_redirect = None
        self._delay_slot_remaining = 0
        self.cycle_log.clear()
        self._pending_fwd_checks.clear()

    def is_drained(self) -> bool:
        return (self.program_size > 0 and self.pc >= self.program_size) and \
               self.l_if_of.is_bubble and self.l_of_ex.is_bubble and self.l_ex_marw.is_bubble

    def step_cycle(self) -> None:
        if self.halted:
            return
        self.perf.total_cycles += 1

        stall_if = False
        stall_of = False
        bubble_ex = False
        redirect_pc: Optional[int] = None
        fwd_events: List[str] = []
        cycle_stall_reason: Optional[str] = None

        # Handle pending multi-cycle memory stalls
        if self.mem_pending_stall > 0:
            self.mem_pending_stall -= 1
            self.perf.record_stall("DCACHE_MISS", 1)
            self._record_trace(fwd_events, "DCACHE_MISS")
            return

        if self.if_pending_stall > 0:
            self.if_pending_stall -= 1
            self.perf.record_stall("ICACHE_MISS", 1)
            stall_if = True
            if self.if_pending_stall == 0:
                self.next_if_of.pc = self.if_latched_pc
                self.next_if_of.instruction = self.if_latched_inst
                try:
                    self.next_if_of.decoded = decode(self.if_latched_inst, self.if_latched_pc)
                except IllegalInstructionError as err:
                    self.next_if_of.decoded = Instruction(
                        raw_word=self.if_latched_inst,
                        has_trap=True,
                        trap_type=ExceptionType.ILLEGAL_OPCODE,
                        trap_details=str(err),
                        is_bubble=False,
                    )
                self.next_if_of.is_bubble = False
                self.next_if_of.disassembly = disassemble_instruction(self.if_latched_inst, self.if_latched_pc)
                self.pc += 4
            else:
                self.next_if_of.reset()

        # =====================================================================
        # STAGE 4: MA_RW (Memory Access and Register Writeback Merged)
        # =====================================================================
        mem_stage_busy = False
        if not self.l_ex_marw.is_bubble:
            d = self.l_ex_marw.decoded
            wb_data = self.l_ex_marw.alu_result

            if d.is_load():
                mem_stage_busy = True
                addr = self.l_ex_marw.alu_result

                # Check StoreBuffer for Store-to-Load forwarding in Full+ mode
                forwarded_from_sb, sb_val = False, 0
                if self.forwarding_unit.mode == "full+":
                    forwarded_from_sb, sb_val = self.forwarding_unit.store_buffer.forward_load(addr)

                if forwarded_from_sb:
                    wb_data = sb_val
                    self.l_ex_marw.alu_result = sb_val
                    fwd_events.append(f"SB->LD 0x{addr:X}")
                    self.perf.record_forward("STORE_TO_LOAD")
                else:
                    try:
                        word_val, total_lat = self.mem.read_word(
                            vaddr=addr, is_fetch=False, pc=self.l_ex_marw.pc,
                            current_cycle=self.perf.total_cycles, asid=self.asid
                        )
                        wb_data = word_val
                        self.l_ex_marw.alu_result = word_val
                        if total_lat > 1:
                            self.mem_pending_stall = total_lat - 1
                    except PageFaultException as pfe:
                        self.current_exception.type = ExceptionType.STACK_CORRUPTION if pfe.fault_type == "guard_page" else ExceptionType.OUT_OF_BOUNDS_MEMORY
                        self.current_exception.faulting_pc = self.l_ex_marw.pc
                        self.current_exception.faulting_address = pfe.vaddr
                        self.current_exception.details = pfe.details
                        self.perf.record_stall("PAGE_FAULT", 1)
                        self.halted = True
                        return

            elif d.is_store():
                mem_stage_busy = True
                addr = self.l_ex_marw.alu_result
                store_val = self.l_ex_marw.op2

                # Verify stack boundary safety
                stack_ex = self.stack_bounds.verify_write_address(addr)
                if stack_ex != ExceptionType.NONE:
                    self.current_exception.type = stack_ex
                    self.current_exception.faulting_pc = self.l_ex_marw.pc
                    self.current_exception.faulting_address = addr
                    self.current_exception.details = f"Stack guard violation at 0x{addr:08X}"
                    self.halted = True
                    return

                # Record store in StoreBuffer for Full+ Store-to-Load forwarding
                if self.forwarding_unit.mode == "full+":
                    self.forwarding_unit.store_buffer.add_store(addr, store_val, self.perf.total_cycles)

                try:
                    total_lat = self.mem.write_word(
                        vaddr=addr, val=store_val, pc=self.l_ex_marw.pc,
                        current_cycle=self.perf.total_cycles, asid=self.asid
                    )
                    if total_lat > 1:
                        self.mem_pending_stall = total_lat - 1
                except PageFaultException as pfe:
                    self.current_exception.type = ExceptionType.STACK_CORRUPTION
                    self.current_exception.faulting_pc = self.l_ex_marw.pc
                    self.current_exception.faulting_address = pfe.vaddr
                    self.perf.record_stall("PAGE_FAULT", 1)
                    self.halted = True
                    return

            elif d.is_call():
                wb_data = (self.l_ex_marw.pc + 4) & 0xFFFFFFFF

            if d.writes_register() and not self.current_exception.has_occurred():
                dest_reg = d.get_dest_reg()
                self.reg_file.write_port(dest_reg, wb_data, True)

            self.perf.record_instruction_retired(d.opcode)

        # Structural Hazard Check: Unified Memory Bus
        if self.hazard_unit.unified_mem and mem_stage_busy:
            stall_if = True
            self.perf.record_stall("STRUCTURAL_MEM", 1)
            cycle_stall_reason = "STRUCTURAL_MEM"

        # =====================================================================
        # STAGE 3: EX (Execute Stage)
        # =====================================================================
        if not self.l_of_ex.is_bubble:
            d = self.l_of_ex.decoded
            if d.has_trap:
                self.current_exception.type = ExceptionType(d.trap_type)
                self.current_exception.faulting_pc = self.l_of_ex.pc
                self.current_exception.details = d.trap_details
                self.perf.record_stall("EXCEPTION_FLUSH", 1)
                self.halted = True
                return

            # Resolve forwarding for Operand A, Operand B, and Store Data
            marw_val = self.l_ex_marw.alu_result
            if self.l_ex_marw.decoded.is_call():
                marw_val = (self.l_ex_marw.pc + 4) & 0xFFFFFFFF

            opA, opB, sel_a, sel_b, fwd_path = self.forwarding_unit.resolve(
                consumer=d,
                consumer_op1=self.l_of_ex.op1,
                consumer_op2=self.l_of_ex.immx if self.l_of_ex.is_immediate else self.l_of_ex.op2,
                producer_marw=self.l_ex_marw.decoded,
                marw_val=marw_val,
            )

            store_data = self.l_of_ex.op2
            if d.is_store():
                store_data, st_path = self.forwarding_unit.resolve_store_data_4stage(
                    consumer=d,
                    current_store_data=store_data,
                    producer_marw=self.l_ex_marw.decoded,
                    marw_val=marw_val,
                )
                if st_path:
                    fwd_events.append(st_path)
                    self.perf.record_forward("STORE_DATA_FWD")
                    if self.check_forwarding:
                        assert store_data == self.reg_file.read_port2(d.rd), (
                            f"Forwarding check assertion failure at PC 0x{self.l_of_ex.pc:08X}: "
                            f"Store data forwarded {store_data} != Retired {self.reg_file.read_port2(d.rd)} for r{d.rd}"
                        )

            if fwd_path is not None:
                fwd_events.append(fwd_path)
                self.perf.record_forward(fwd_path)
                if self.check_forwarding:
                    if sel_a == 1:
                        assert opA == self.reg_file.read_port1(d.rs1), (
                            f"Forwarding check assertion failure at PC 0x{self.l_of_ex.pc:08X}: "
                            f"Forwarded opA {opA} != Retired {self.reg_file.read_port1(d.rs1)} for r{d.rs1}"
                        )
                    if sel_b == 1:
                        assert opB == self.reg_file.read_port2(d.rs2), (
                            f"Forwarding check assertion failure at PC 0x{self.l_of_ex.pc:08X}: "
                            f"Forwarded opB {opB} != Retired {self.reg_file.read_port2(d.rs2)} for r{d.rs2}"
                        )

            # ALU computation
            alu_out = self.alu.execute(d.opcode, opA, opB, self.reg_file.flags)
            if d.opcode == OP_CMP:
                self.reg_file.flags = alu_out.flags
            if alu_out.exception.has_occurred():
                self.current_exception = alu_out.exception
                self.current_exception.faulting_pc = self.l_of_ex.pc
                self.perf.record_stall("EXCEPTION_FLUSH", 1)
                self.halted = True
                return

            if d.is_store() and self.forwarding_unit.mode == "full+":
                self.forwarding_unit.store_buffer.add_store(alu_out.result, store_data, self.perf.total_cycles)

            # Branch condition evaluation
            if d.is_branch():
                self.perf.total_branches += 1
                flg = self.reg_file.flags
                actual_taken = False
                actual_target = self.l_of_ex.branch_target

                if d.opcode == OP_B:
                    actual_taken = True
                elif d.opcode == OP_BEQ:
                    actual_taken = bool(flg.E)
                elif d.opcode == OP_BGT:
                    actual_taken = bool(flg.GT)
                elif d.opcode == OP_CALL:
                    actual_taken = True
                    self.branch_predictor.ras.push((self.l_of_ex.pc + 4) & 0xFFFFFFFF)
                elif d.opcode == OP_RET:
                    actual_taken = True
                    actual_target = opA
                elif d.opcode == OP_RETZ:
                    actual_taken = True
                    actual_target = self.reg_file.old_pc
                    self.reg_file.cpl = 1

                if actual_taken:
                    self.perf.branches_taken += 1

                # Update Branch Predictor state
                self.branch_predictor.update(self.l_of_ex.pc, d, actual_taken, actual_target)

                # Check for misprediction
                pred_taken = self.l_of_ex.predicted_taken
                pred_target = self.l_of_ex.predicted_target
                mispredict = (actual_taken != pred_taken) or (actual_taken and (actual_target != pred_target))

                if mispredict:
                    self.perf.branches_mispredicted += 1
                    redirect_pc = actual_target if actual_taken else (self.l_of_ex.pc + 4) & 0xFFFFFFFF
                    # Mispredict flush penalty (accounted based on delay slots)
                    flushed_cycles = max(0, 2 - self.branch_predictor.delay_slots)
                    if flushed_cycles > 0:
                        self.perf.record_stall("BRANCH_MISPREDICT", flushed_cycles)
                        cycle_stall_reason = "BRANCH_MISPREDICT"

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
            d = self.l_if_of.decoded

            # Evaluate hazards using HazardUnit
            action = self.hazard_unit.evaluate(
                consumer_of=d,
                in_ex=self.l_of_ex.decoded if not self.l_of_ex.is_bubble else Instruction(is_bubble=True),
                in_marw=self.l_ex_marw.decoded if not self.l_ex_marw.is_bubble else Instruction(is_bubble=True),
                mem_busy=mem_stage_busy,
            )

            if action.stall_id_of:
                stall_if = True
                stall_of = True
                bubble_ex = True
                if action.stall_reason:
                    self.perf.record_stall(action.stall_reason, 1)
                    cycle_stall_reason = action.stall_reason
            else:
                op1 = self.reg_file.read_port1(REG_RA if d.is_ret() else d.rs1)
                op2 = self.reg_file.read_port2(d.rd if d.is_store() else d.rs2)

                # Early RF writeback bypass (WB -> RF)
                if not self.l_ex_marw.is_bubble and self.l_ex_marw.decoded.writes_register():
                    wb_dest = self.l_ex_marw.decoded.get_dest_reg()
                    if wb_dest is not None:
                        wb_val = self.l_ex_marw.alu_result
                        if self.l_ex_marw.decoded.is_call():
                            wb_val = (self.l_ex_marw.pc + 4) & 0xFFFFFFFF
                        if d.reads_rs1() and d.rs1 == wb_dest:
                            op1 = wb_val
                            fwd_events.append("WB->OF_RS1")
                        if not d.is_immediate and d.reads_rs2() and d.rs2 == wb_dest:
                            op2 = wb_val
                            fwd_events.append("WB->OF_RS2")
                        if d.is_store() and d.rd == wb_dest:
                            op2 = wb_val
                            fwd_events.append("WB->OF_STORE_DATA")

                self.next_of_ex.pc = self.l_if_of.pc
                self.next_of_ex.instruction = self.l_if_of.instruction
                self.next_of_ex.decoded = d
                self.next_of_ex.op1 = op1
                self.next_of_ex.op2 = op2
                self.next_of_ex.immx = d.immx
                self.next_of_ex.branch_target = d.branch_target
                self.next_of_ex.predicted_taken = self.l_if_of.predicted_taken
                self.next_of_ex.predicted_target = self.l_if_of.predicted_target
                self.next_of_ex.is_immediate = d.is_immediate
                self.next_of_ex.is_bubble = False
                self.next_of_ex.disassembly = self.l_if_of.disassembly
        else:
            self.next_of_ex.reset()

        # =====================================================================
        # STAGE 1: IF (Instruction Fetch)
        # =====================================================================
        if redirect_pc is not None:
            # Branch misprediction redirect
            self.pc = redirect_pc
            if self.branch_predictor.delay_slots == 0:
                self.next_if_of.reset()
                self.next_of_ex.reset()
            elif self.branch_predictor.delay_slots == 1:
                # 1 delay slot: allow next_if_of to enter OF, flush only IF
                self.next_if_of.reset()

        elif self.program_size > 0 and self.pc >= self.program_size:
            self.next_if_of.reset()

        elif not stall_if:
            # Unified memory structural hazard check
            if self.hazard_unit.unified_mem and mem_stage_busy:
                self.perf.record_stall("STRUCTURAL_MEM", 1)
                cycle_stall_reason = "STRUCTURAL_MEM"
                self.next_if_of.reset()
            else:
                try:
                    inst_word, fetch_lat = self.mem.read_word(
                        vaddr=self.pc, is_fetch=True, pc=self.pc,
                        current_cycle=self.perf.total_cycles, asid=self.asid
                    )
                    if fetch_lat > 1:
                        self.if_pending_stall = fetch_lat - 1
                        self.if_latched_inst = inst_word
                        self.if_latched_pc = self.pc
                        self.next_if_of.reset()
                    else:
                        try:
                            dec = decode(inst_word, self.pc)
                        except IllegalInstructionError as err:
                            dec = Instruction(
                                raw_word=inst_word,
                                has_trap=True,
                                trap_type=ExceptionType.ILLEGAL_OPCODE,
                                trap_details=str(err),
                                is_bubble=False,
                            )

                        pred_taken = False
                        pred_target = (self.pc + 4) & 0xFFFFFFFF

                        # Consult branch predictor if instruction is branch
                        if dec.is_branch():
                            if self.branch_predictor.policy == "stall":
                                self.perf.record_stall("BRANCH_STALL", 1)
                                stall_if = True
                            else:
                                pred_taken, pred_target = self.branch_predictor.predict(
                                    pc=self.pc,
                                    inst=dec
                                )

                        self.next_if_of.pc = self.pc
                        self.next_if_of.instruction = inst_word
                        self.next_if_of.decoded = dec
                        self.next_if_of.predicted_taken = pred_taken
                        self.next_if_of.predicted_target = pred_target
                        self.next_if_of.is_bubble = False
                        self.next_if_of.disassembly = disassemble_instruction(inst_word, self.pc)

                        if pred_taken:
                            if self.branch_predictor.delay_slots == 0:
                                self.pc = pred_target
                            else:
                                self._delay_slot_redirect = pred_target
                                self._delay_slot_remaining = self.branch_predictor.delay_slots
                                self.pc = (self.pc + 4) & 0xFFFFFFFF
                        elif self._delay_slot_remaining > 0:
                            self._delay_slot_remaining -= 1
                            if self._delay_slot_remaining == 0:
                                self.pc = self._delay_slot_redirect
                            else:
                                self.pc = (self.pc + 4) & 0xFFFFFFFF
                        else:
                            self.pc = (self.pc + 4) & 0xFFFFFFFF

                except PageFaultException as pfe:
                    self.current_exception.type = ExceptionType.OUT_OF_BOUNDS_MEMORY
                    self.current_exception.faulting_pc = self.pc
                    self.current_exception.faulting_address = pfe.vaddr
                    self.current_exception.details = pfe.details
                    self.perf.record_stall("PAGE_FAULT", 1)
                    self.halted = True
                    return

        # Synchronous Latch Updates
        if bubble_ex:
            self.l_of_ex.reset()
        elif not stall_of:
            self.l_of_ex.copy_from(self.next_of_ex)

        if not stall_of:
            if self.if_pending_stall > 0:
                self.l_if_of.reset()
            elif not self.next_if_of.is_bubble or not stall_if:
                self.l_if_of.copy_from(self.next_if_of)

        self.l_ex_marw.copy_from(self.next_ex_marw)

        self._record_trace(fwd_events, cycle_stall_reason)

    def _record_trace(self, fwd_events: List[str], stall_reason: Optional[str]) -> None:
        if len(self.cycle_log) >= 200:
            self.cycle_log.pop(0)
        self.cycle_log.append({
            "cycle": self.perf.total_cycles,
            "IF": f"0x{self.l_if_of.pc:X}:{self.l_if_of.disassembly}" if not self.l_if_of.is_bubble else "---",
            "OF": f"0x{self.l_of_ex.pc:X}:{self.l_of_ex.disassembly}" if not self.l_of_ex.is_bubble else "---",
            "EX": f"0x{self.l_ex_marw.pc:X}:{self.l_ex_marw.disassembly}" if not self.l_ex_marw.is_bubble else "---",
            "fwd": list(fwd_events),
            "stall": stall_reason,
        })

    def run(self, max_cycles: int = 100000) -> None:
        while not self.halted and self.perf.total_cycles < max_cycles:
            self.step_cycle()
            if self.is_drained():
                break

    def render_timing_diagram(self, last_n_cycles: int = 20) -> str:
        """Renders an ASCII pipeline execution timing diagram with forwarding annotations."""
        logs = self.cycle_log[-last_n_cycles:]
        if not logs:
            return "No pipeline cycles logged."
        lines = [
            f"=== 4-Stage Pipeline Timing Diagram (Last {len(logs)} Cycles) ===",
            f"{'Cyc':<5} | {'IF Stage':<22} | {'OF Stage':<22} | {'EX / MARW Stage':<22} | {'Forwarding & Stalls'}"
        ]
        lines.append("-" * 95)
        for entry in logs:
            c = entry["cycle"]
            if_st = entry["IF"][:20]
            of_st = entry["OF"][:20]
            ex_st = entry["EX"][:20]
            annot = []
            if entry["stall"]:
                annot.append(f"[STALL:{entry['stall']}]")
            if entry["fwd"]:
                annot.append(f"FWD({','.join(entry['fwd'])})")
            annot_str = " ".join(annot)
            lines.append(f"{c:<5} | {if_st:<22} | {of_st:<22} | {ex_st:<22} | {annot_str}")
        return "\n".join(lines)

    def print_stats(self) -> str:
        return self.perf.generate_report(format_type="table")
