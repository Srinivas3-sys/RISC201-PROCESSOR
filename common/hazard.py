"""
common/hazard.py: Hazard Detection and Stall Resolution Subsystem for RISC201.
Driven directly by instruction-table metadata (reads_rs1, reads_rs2, writes_rd, is_load, is_store).
Detects RAW, WAR, WAW hazards, structural memory and regfile port conflicts,
and emits canonical stall reason codes for 4-Stage and 6-Stage pipelines.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Any
from common.isa import Instruction, REG_ZERO, OP_NOP


@dataclass
class HazardAction:
    """Action decisions computed by the HazardUnit for the current cycle."""
    stall_if: bool = False
    stall_id_of: bool = False     # OF in 4-stage, ID in 6-stage
    stall_rf: bool = False        # RF in 6-stage
    stall_ex: bool = False        # EX stage
    stall_mem: bool = False       # MA / MA_RW stage
    bubble_id_of: bool = False    # Inject bubble into ID/OF
    bubble_rf: bool = False       # Inject bubble into RF (6-stage)
    bubble_ex: bool = False       # Inject bubble into EX
    bubble_mem: bool = False      # Inject bubble into MA
    stall_reason: Optional[str] = None
    flush_pipeline: bool = False
    flushed_stages: List[str] = field(default_factory=list)


class HazardUnit:
    """
    Dedicated Hazard Detection and Resolution Unit.
    Parameterized by pipeline depth (4 or 6) and configuration options.
    """
    def __init__(
        self,
        pipeline_stages: int = 4,
        forwarding_mode: str = "full",
        unified_mem: bool = False,
        regfile_write_ports: int = 1,
    ):
        self.pipeline_stages = pipeline_stages
        self.forwarding_mode = forwarding_mode.lower()
        self.unified_mem = unified_mem
        self.regfile_write_ports = regfile_write_ports

        # Telemetry counters
        self.raw_count = 0
        self.war_count = 0
        self.waw_count = 0
        self.load_use_count = 0
        self.structural_mem_count = 0
        self.structural_rf_count = 0
        self.branch_hazard_count = 0

    def reset_telemetry(self) -> None:
        self.raw_count = 0
        self.war_count = 0
        self.waw_count = 0
        self.load_use_count = 0
        self.structural_mem_count = 0
        self.structural_rf_count = 0
        self.branch_hazard_count = 0

    # =========================================================================
    # 4-STAGE PIPELINE HAZARD RESOLUTION
    # Stages: IF -> OF -> EX -> MA_RW
    # =========================================================================
    def evaluate_4stage(
        self,
        consumer_of: Instruction,
        in_ex: Instruction,
        in_marw: Instruction,
        mem_busy: bool = False,
        wb_requests: int = 1,
    ) -> HazardAction:
        """
        Evaluates hazards for the 4-stage pipeline at the OF stage.
        """
        action = HazardAction()

        # 1. Structural Hazard: Unified Memory Bus Conflict
        # If MA_RW is accessing memory (ld or st), IF cannot fetch on a unified bus
        if self.unified_mem and mem_busy:
            action.stall_if = True
            action.bubble_id_of = True
            action.stall_reason = "STRUCTURAL_MEM"
            self.structural_mem_count += 1
            return action

        # 2. Structural Hazard: Register File Write Ports
        if wb_requests > self.regfile_write_ports:
            action.stall_ex = True
            action.stall_reason = "STRUCTURAL_RF"
            self.structural_rf_count += 1
            return action

        # If consumer in OF is a bubble or NOP, no data hazard possible
        if consumer_of.is_bubble or consumer_of.is_nop():
            return action

        # Detect RAW hazards against instructions in-flight (EX and MA_RW)
        raw_rs1_ex = (
            consumer_of.reads_rs1() and
            not in_ex.is_bubble and
            in_ex.writes_register() and
            consumer_of.rs1 == in_ex.get_dest_reg()
        )
        raw_rs2_ex = (
            not consumer_of.is_immediate and
            consumer_of.reads_rs2() and
            not in_ex.is_bubble and
            in_ex.writes_register() and
            consumer_of.rs2 == in_ex.get_dest_reg()
        )
        # Store data hazard: consumer is store, in_ex writes store source register rd
        raw_st_ex = (
            consumer_of.is_store() and
            not in_ex.is_bubble and
            in_ex.writes_register() and
            consumer_of.rd == in_ex.get_dest_reg()
        )

        raw_rs1_marw = (
            consumer_of.reads_rs1() and
            not in_marw.is_bubble and
            in_marw.writes_register() and
            consumer_of.rs1 == in_marw.get_dest_reg()
        )
        raw_rs2_marw = (
            not consumer_of.is_immediate and
            consumer_of.reads_rs2() and
            not in_marw.is_bubble and
            in_marw.writes_register() and
            consumer_of.rs2 == in_marw.get_dest_reg()
        )
        raw_st_marw = (
            consumer_of.is_store() and
            not in_marw.is_bubble and
            in_marw.writes_register() and
            consumer_of.rd == in_marw.get_dest_reg()
        )

        if raw_rs1_ex or raw_rs2_ex or raw_st_ex or raw_rs1_marw or raw_rs2_marw or raw_st_marw:
            self.raw_count += 1

        # Check WAR and WAW in-flight (for verification and telemetry)
        if consumer_of.writes_register():
            dest = consumer_of.get_dest_reg()
            # WAW: older instruction also writes the same register
            if (not in_ex.is_bubble and in_ex.get_dest_reg() == dest) or \
               (not in_marw.is_bubble and in_marw.get_dest_reg() == dest):
                self.waw_count += 1

        # Determine stall behavior based on forwarding mode
        if self.forwarding_mode == "none":
            # Without forwarding, any RAW against EX or MA_RW requires stall until writeback
            if raw_rs1_ex or raw_rs2_ex or raw_st_ex or raw_rs1_marw or raw_rs2_marw or raw_st_marw:
                action.stall_if = True
                action.stall_id_of = True
                action.bubble_ex = True
                action.stall_reason = "RAW_NOFWD"
                return action

        elif self.forwarding_mode in ("basic", "full", "full+"):
            # With forwarding:
            # 1. Load-Use Hazard:
            # If instruction in EX is a LOAD, its data is not available until MA_RW finishes memory read.
            if in_ex.is_load() and not in_ex.is_bubble:
                load_dest = in_ex.get_dest_reg()
                if load_dest is not None:
                    # If consumer needs load result for ALU operand (rs1 or rs2)
                    needs_for_alu = (consumer_of.reads_rs1() and consumer_of.rs1 == load_dest) or \
                                    (not consumer_of.is_immediate and consumer_of.reads_rs2() and consumer_of.rs2 == load_dest)
                    # If consumer is a store and needs load result for address (rs1)
                    needs_for_addr = consumer_of.is_store() and consumer_of.rs1 == load_dest

                    if needs_for_alu or needs_for_addr:
                        action.stall_if = True
                        action.stall_id_of = True
                        action.bubble_ex = True
                        action.stall_reason = "RAW_LOAD_USE"
                        self.load_use_count += 1
                        return action

                    # Store-data dependency (consumer is store, rd == load_dest):
                    # In basic mode (no store-data forwarding), must stall 1 cycle.
                    # In full / full+ mode, store-data forwarding allows 0 stalls!
                    if self.forwarding_mode == "basic" and consumer_of.is_store() and consumer_of.rd == load_dest:
                        action.stall_if = True
                        action.stall_id_of = True
                        action.bubble_ex = True
                        action.stall_reason = "RAW_LOAD_USE"
                        self.load_use_count += 1
                        return action

        return action

    # =========================================================================
    # 6-STAGE PIPELINE HAZARD RESOLUTION
    # Stages: IF -> ID -> RF -> EX -> MA -> RW
    # =========================================================================
    def evaluate_6stage(
        self,
        consumer_rf: Instruction,
        in_ex: Instruction,
        in_ma: Instruction,
        in_rw: Instruction,
        mem_busy: bool = False,
        wb_requests: int = 1,
    ) -> HazardAction:
        """
        Evaluates hazards for the 6-stage pipeline at the RF stage.
        """
        action = HazardAction()

        # 1. Structural Hazard: Unified Memory Bus Conflict
        if self.unified_mem and mem_busy:
            action.stall_if = True
            action.bubble_id_of = True
            action.stall_reason = "STRUCTURAL_MEM"
            self.structural_mem_count += 1
            return action

        # 2. Structural Hazard: Register File Write Ports
        if wb_requests > self.regfile_write_ports:
            action.stall_ex = True
            action.stall_reason = "STRUCTURAL_RF"
            self.structural_rf_count += 1
            return action

        if consumer_rf.is_bubble or consumer_rf.is_nop():
            return action

        # In-flight destination checking
        dest_ex = in_ex.get_dest_reg() if (not in_ex.is_bubble and in_ex.writes_register()) else None
        dest_ma = in_ma.get_dest_reg() if (not in_ma.is_bubble and in_ma.writes_register()) else None
        dest_rw = in_rw.get_dest_reg() if (not in_rw.is_bubble and in_rw.writes_register()) else None

        raw_rs1 = (
            consumer_rf.reads_rs1() and (
                (dest_ex is not None and consumer_rf.rs1 == dest_ex) or
                (dest_ma is not None and consumer_rf.rs1 == dest_ma) or
                (dest_rw is not None and consumer_rf.rs1 == dest_rw)
            )
        )
        raw_rs2 = (
            not consumer_rf.is_immediate and consumer_rf.reads_rs2() and (
                (dest_ex is not None and consumer_rf.rs2 == dest_ex) or
                (dest_ma is not None and consumer_rf.rs2 == dest_ma) or
                (dest_rw is not None and consumer_rf.rs2 == dest_rw)
            )
        )
        raw_st = (
            consumer_rf.is_store() and (
                (dest_ex is not None and consumer_rf.rd == dest_ex) or
                (dest_ma is not None and consumer_rf.rd == dest_ma) or
                (dest_rw is not None and consumer_rf.rd == dest_rw)
            )
        )

        if raw_rs1 or raw_rs2 or raw_st:
            self.raw_count += 1

        # WAW tracking
        if consumer_rf.writes_register():
            dest = consumer_rf.get_dest_reg()
            if dest in (dest_ex, dest_ma, dest_rw):
                self.waw_count += 1

        if self.forwarding_mode == "none":
            # Without forwarding, must stall until producer has retired past RW
            if raw_rs1 or raw_rs2 or raw_st:
                action.stall_if = True
                action.stall_id_of = True
                action.stall_rf = True
                action.bubble_ex = True
                action.stall_reason = "RAW_NOFWD"
                return action

        elif self.forwarding_mode in ("basic", "full", "full+"):
            # Load-use hazard detection in 6-stage:
            # Case 1: Producer in EX is a LOAD.
            # When producer is in EX, it is computing address. Load data arrives at end of MA.
            # If consumer in RF moves to EX next cycle, it needs operand in EX, which is 1 cycle too early!
            # Therefore, consumer in RF MUST stall 1 cycle.
            if in_ex.is_load() and not in_ex.is_bubble:
                load_dest = in_ex.get_dest_reg()
                if load_dest is not None:
                    needs_alu = (consumer_rf.reads_rs1() and consumer_rf.rs1 == load_dest) or \
                                (not consumer_rf.is_immediate and consumer_rf.reads_rs2() and consumer_rf.rs2 == load_dest)
                    needs_addr = consumer_rf.is_store() and consumer_rf.rs1 == load_dest

                    if needs_alu or needs_addr:
                        action.stall_if = True
                        action.stall_id_of = True
                        action.stall_rf = True
                        action.bubble_ex = True
                        action.stall_reason = "RAW_LOAD_USE"
                        self.load_use_count += 1
                        return action

                    # Store data forward: in full mode, store data is consumed in MA.
                    # Load in EX enters MA in cycle +1; Store in RF enters EX in cycle +1.
                    # In cycle +2: Load enters RW; Store enters MA.
                    # Load data can be forwarded directly from RW to MA! 0 stalls required.
                    if self.forwarding_mode == "basic" and consumer_rf.is_store() and consumer_rf.rd == load_dest:
                        action.stall_if = True
                        action.stall_id_of = True
                        action.stall_rf = True
                        action.bubble_ex = True
                        action.stall_reason = "RAW_LOAD_USE"
                        self.load_use_count += 1
                        return action

        return action


class HazardUnit4Stage(HazardUnit):
    """Hazard unit specialized for 4-Stage pipeline (IF -> OF -> EX -> MA_RW)."""
    def __init__(self, forwarding_mode: str = "full", unified_mem: bool = False, regfile_write_ports: int = 1):
        super().__init__(4, forwarding_mode, unified_mem, regfile_write_ports)

    def evaluate(self, consumer_of: Instruction, in_ex: Instruction, in_marw: Instruction, mem_busy: bool = False, wb_requests: int = 1) -> HazardAction:
        return self.evaluate_4stage(consumer_of, in_ex, in_marw, mem_busy, wb_requests)


class HazardUnit6Stage(HazardUnit):
    """Hazard unit specialized for 6-Stage pipeline (IF -> ID -> RF -> EX -> MA -> RW)."""
    def __init__(self, forwarding_mode: str = "full", unified_mem: bool = False, regfile_write_ports: int = 1):
        super().__init__(6, forwarding_mode, unified_mem, regfile_write_ports)

    def evaluate(self, consumer_rf: Instruction, in_ex: Instruction, in_ma: Instruction, in_rw: Instruction, mem_busy: bool = False, wb_requests: int = 1) -> HazardAction:
        return self.evaluate_6stage(consumer_rf, in_ex, in_ma, in_rw, mem_busy, wb_requests)

