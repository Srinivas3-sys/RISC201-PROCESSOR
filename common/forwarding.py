"""
common/forwarding.py: Configurable Data Forwarding (Bypass Network) for RISC201.
Supports modes: none, basic, full, full+.
Features young-producer priority resolution, store-data bypass, branch forwarding,
and store-to-load forwarding via a dedicated Store Buffer.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Any
from common.isa import (
    Instruction, REG_ZERO, FWD_SEL_REG, FWD_SEL_EXMEM, FWD_SEL_MEMWB, FWD_SEL_WBID
)


@dataclass
class StoreBufferEntry:
    address: int
    data: int
    cycle_issued: int
    valid: bool = True


class StoreBuffer:
    """Small associative store buffer for Full+ Store-to-Load forwarding."""
    def __init__(self, capacity: int = 4):
        self.capacity = capacity
        self.entries: List[StoreBufferEntry] = []
        self.forward_hits: int = 0

    def reset(self) -> None:
        self.entries.clear()
        self.forward_hits = 0

    def add_store(self, address: int, data: int, current_cycle: int) -> None:
        if len(self.entries) >= self.capacity:
            self.entries.pop(0)
        self.entries.append(StoreBufferEntry(address=address, data=data, cycle_issued=current_cycle))

    def forward_load(self, load_address: int) -> Tuple[bool, int]:
        # Search youngest to oldest
        for entry in reversed(self.entries):
            if entry.valid and entry.address == load_address:
                self.forward_hits += 1
                return True, entry.data
        return False, 0


class ForwardingUnit:
    """
    Dedicated Forwarding Unit parameterized for both 4-Stage and 6-Stage pipelines.
    """
    def __init__(self, mode: str = "full", pipeline_stages: int = 4):
        self.mode = mode.lower() # none, basic, full, full+
        self.pipeline_stages = pipeline_stages
        self.store_buffer = StoreBuffer(4)

        # Per-path enable toggles
        self.path_enables: Dict[str, bool] = {
            "EX_TO_EX_RS1": True,
            "EX_TO_EX_RS2": True,
            "MEM_TO_EX_RS1": True,
            "MEM_TO_EX_RS2": True,
            "STORE_DATA_FWD": True,
            "BRANCH_FWD": True,
            "WB_TO_RF": True,
            "STORE_TO_LOAD": (self.mode == "full+"),
        }

        # Apply forwarding mode constraints
        if self.mode == "none":
            for k in self.path_enables:
                self.path_enables[k] = False
        elif self.mode == "basic":
            for k in self.path_enables:
                self.path_enables[k] = False
            self.path_enables["EX_TO_EX_RS1"] = True
            self.path_enables["EX_TO_EX_RS2"] = True
            self.path_enables["MEM_TO_EX_RS1"] = True
            self.path_enables["MEM_TO_EX_RS2"] = True
            self.path_enables["STORE_DATA_FWD"] = True
            self.path_enables["WB_TO_RF"] = True

    def set_path_enable(self, path_name: str, enabled: bool) -> None:
        if path_name in self.path_enables:
            self.path_enables[path_name] = enabled

    # =====================================================================
    # 4-STAGE PIPELINE FORWARDING LOGIC
    # Stages: IF -> OF -> EX -> MA_RW
    # =====================================================================
    def resolve_4stage_ex(
        self,
        consumer: Instruction,
        consumer_op1: int,
        consumer_op2: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, int, str, str, Optional[str]]:
        """
        Resolves forwardings to EX stage inputs (rs1, rs2, store_data) for 4-Stage pipeline.
        Returns: (final_op1, final_op2, mux_sel_rs1, mux_sel_rs2, path_taken)
        """
        val_a = consumer_op1
        val_b = consumer_op2
        sel_a = FWD_SEL_REG
        sel_b = FWD_SEL_REG
        path_taken = None

        if self.mode == "none":
            return val_a, val_b, sel_a, sel_b, None

        if not producer_marw.is_bubble and producer_marw.writes_register():
            dest = producer_marw.get_dest_reg()
            if dest is not None:
                # Forward to Operand A (rs1)
                if consumer.reads_rs1() and consumer.rs1 == dest and self.path_enables["EX_TO_EX_RS1"]:
                    val_a = marw_val
                    sel_a = FWD_SEL_EXMEM
                    path_taken = "MA_RW_TO_EX_RS1"

                # Forward to Operand B (rs2)
                if not consumer.is_immediate and consumer.reads_rs2() and consumer.rs2 == dest and self.path_enables["EX_TO_EX_RS2"]:
                    val_b = marw_val
                    sel_b = FWD_SEL_EXMEM
                    path_taken = "MA_RW_TO_EX_RS2"

        return val_a, val_b, sel_a, sel_b, path_taken

    def resolve_store_data_4stage(
        self,
        consumer: Instruction,
        current_store_data: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, Optional[str]]:
        if not self.path_enables["STORE_DATA_FWD"] or self.mode == "none":
            return current_store_data, None
        if consumer.is_store() and not producer_marw.is_bubble and producer_marw.writes_register():
            dest = producer_marw.get_dest_reg()
            if dest is not None and consumer.rd == dest:
                return marw_val, "MA_RW_TO_EX_STORE_DATA"
        return current_store_data, None

    # =====================================================================
    # 6-STAGE PIPELINE FORWARDING LOGIC
    # Stages: IF -> ID -> RF -> EX -> MA -> RW
    # Producers: EX_MA (younger), MA_RW (older)
    # =====================================================================
    def resolve_6stage_ex(
        self,
        consumer: Instruction,
        rf_op1: int,
        rf_op2: int,
        producer_exma: Instruction,
        exma_val: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, int, str, str, Optional[str]]:
        """
        Resolves forwardings to EX stage inputs for 6-Stage pipeline.
        Applies strict youngest-older-producer priority:
          EX_MA (younger) takes priority over MA_RW (older).
        Returns: (final_op1, final_op2, mux_sel_rs1, mux_sel_rs2, path_taken)
        """
        val_a = rf_op1
        val_b = rf_op2
        sel_a = FWD_SEL_REG
        sel_b = FWD_SEL_REG
        path_taken = None

        if self.mode == "none":
            return val_a, val_b, sel_a, sel_b, None

        dest_exma = producer_exma.get_dest_reg() if (not producer_exma.is_bubble and producer_exma.writes_register()) else None
        dest_marw = producer_marw.get_dest_reg() if (not producer_marw.is_bubble and producer_marw.writes_register()) else None

        # 1. Forwarding for rs1 (Operand A)
        if consumer.reads_rs1():
            # Younger priority: EX_MA
            if dest_exma is not None and consumer.rs1 == dest_exma and self.path_enables["EX_TO_EX_RS1"] and not producer_exma.is_load():
                val_a = exma_val
                sel_a = FWD_SEL_EXMEM
                path_taken = "EX_MA_TO_EX_RS1"
            # Older priority: MA_RW
            elif dest_marw is not None and consumer.rs1 == dest_marw and self.path_enables["MEM_TO_EX_RS1"]:
                val_a = marw_val
                sel_a = FWD_SEL_MEMWB
                path_taken = "MA_RW_TO_EX_RS1"

        # 2. Forwarding for rs2 (Operand B)
        if not consumer.is_immediate and consumer.reads_rs2():
            # Younger priority: EX_MA
            if dest_exma is not None and consumer.rs2 == dest_exma and self.path_enables["EX_TO_EX_RS2"] and not producer_exma.is_load():
                val_b = exma_val
                sel_b = FWD_SEL_EXMEM
                path_taken = "EX_MA_TO_EX_RS2"
            # Older priority: MA_RW
            elif dest_marw is not None and consumer.rs2 == dest_marw and self.path_enables["MEM_TO_EX_RS2"]:
                val_b = marw_val
                sel_b = FWD_SEL_MEMWB
                path_taken = "MA_RW_TO_EX_RS2"

        return val_a, val_b, sel_a, sel_b, path_taken

    def resolve_store_data_6stage(
        self,
        consumer: Instruction,
        current_store_data: int,
        producer_exma: Instruction,
        exma_val: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, Optional[str]]:
        if not self.path_enables["STORE_DATA_FWD"] or self.mode == "none":
            return current_store_data, None
        if consumer.is_store():
            dest_exma = producer_exma.get_dest_reg() if (not producer_exma.is_bubble and producer_exma.writes_register()) else None
            dest_marw = producer_marw.get_dest_reg() if (not producer_marw.is_bubble and producer_marw.writes_register()) else None
            # Younger priority: EX_MA (only if producer is not a load)
            if dest_exma is not None and consumer.rd == dest_exma and not producer_exma.is_load():
                return exma_val, "EX_MA_TO_EX_STORE_DATA"
            elif dest_marw is not None and consumer.rd == dest_marw:
                return marw_val, "MA_RW_TO_EX_STORE_DATA"
        return current_store_data, None

    def resolve_store_data_in_ma(
        self,
        consumer: Instruction,
        current_data: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, Optional[str]]:
        """
        Late store data forwarding in MA stage (allows load -> store with 0 stalls).
        """
        if not self.path_enables["STORE_DATA_FWD"] or self.mode == "none":
            return current_data, None

        if not producer_marw.is_bubble and producer_marw.writes_register():
            dest = producer_marw.get_dest_reg()
            if dest is not None and consumer.rd == dest:
                return marw_val, "MA_RW_TO_MA_STORE_DATA"
        return current_data, None

    @staticmethod
    def generate_minimum_stall_table(pipeline_stages: int = 4) -> str:
        """
        Generates the theoretical and model-verified minimum stall cycle table:
        Producer Class x Consumer Class x Forwarding Mode.
        """
        header = f"=== Minimum Stall Bubbles Table ({pipeline_stages}-Stage Pipeline) ==="
        cols = f"{'Producer':<14} {'Consumer':<16} {'None':<8} {'Basic':<8} {'Full':<8} {'Full+':<8}"
        sep = "-" * len(cols)
        lines = [header, cols, sep]

        producers = [("ALU", "EX"), ("LOAD", "MEM"), ("MUL/DIV", "EX_LATE")]
        consumers = [
            ("ALU (rs1/rs2)", "EX"),
            ("STORE (addr)", "EX"),
            ("STORE (data)", "MEM" if pipeline_stages == 6 else "EX"),
            ("BRANCH (cond)", "EX"),
            ("LOAD (addr)", "EX")
        ]

        for prod_name, prod_stage in producers:
            for cons_name, cons_stage in consumers:
                if pipeline_stages == 4:
                    # 4-Stage (IF -> OF -> EX -> MA_RW)
                    # Produce in EX (stage 3) or MA_RW (stage 4). Consume in OF (2) or EX (3).
                    if prod_name == "ALU":
                        s_none = 2
                        s_basic = 0
                        s_full = 0
                        s_full_plus = 0
                    elif prod_name == "LOAD":
                        s_none = 2
                        s_basic = 1
                        s_full = 1 if cons_name != "STORE (data)" else 0
                        s_full_plus = 0
                    else: # MUL/DIV
                        s_none = 3
                        s_basic = 1
                        s_full = 1
                        s_full_plus = 0
                else:
                    # 6-Stage (IF -> ID -> RF -> EX -> MA -> RW)
                    if prod_name == "ALU":
                        s_none = 3
                        s_basic = 0
                        s_full = 0
                        s_full_plus = 0
                    elif prod_name == "LOAD":
                        s_none = 3
                        s_basic = 2
                        s_full = 1 if cons_name != "STORE (data)" else 0
                        s_full_plus = 0
                    else:
                        s_none = 4
                        s_basic = 2
                        s_full = 1
                        s_full_plus = 0

                lines.append(f"{prod_name:<14} {cons_name:<16} {s_none:<8} {s_basic:<8} {s_full:<8} {s_full_plus:<8}")

        return "\n".join(lines)


class ForwardingUnit4Stage(ForwardingUnit):
    """Forwarding unit specialized for 4-Stage pipeline (IF -> OF -> EX -> MA_RW)."""
    def __init__(self, mode: str = "full"):
        super().__init__(mode=mode, pipeline_stages=4)

    def resolve(
        self, consumer: Instruction, consumer_op1: int, consumer_op2: int, producer_marw: Instruction, marw_val: int
    ) -> Tuple[int, int, str, str, Optional[str]]:
        return self.resolve_4stage_ex(consumer, consumer_op1, consumer_op2, producer_marw, marw_val)


class ForwardingUnit6Stage(ForwardingUnit):
    """Forwarding unit specialized for 6-Stage pipeline (IF -> ID -> RF -> EX -> MA -> RW)."""
    def __init__(self, mode: str = "full"):
        super().__init__(mode=mode, pipeline_stages=6)

    def resolve(
        self, consumer: Instruction, rf_op1: int, rf_op2: int,
        producer_exma: Instruction, exma_val: int, producer_marw: Instruction, marw_val: int
    ) -> Tuple[int, int, str, str, Optional[str]]:
        return self.resolve_6stage_ex(consumer, rf_op1, rf_op2, producer_exma, exma_val, producer_marw, marw_val)

