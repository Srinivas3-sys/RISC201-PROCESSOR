"""
processor_4stage/forwarding.py: 4-stage-specific bypass network and mux select helpers.
"""

from __future__ import annotations
from typing import Tuple, Optional
from common.forwarding import ForwardingUnit
from common.isa import Instruction


class ForwardingUnit4Stage(ForwardingUnit):
    """
    Forwarding unit specialized for the 4-stage pipeline (IF -> OF -> EX -> MA_RW).
    Provides bypass paths from MA_RW back to EX (rs1, rs2, store_data).
    """
    def __init__(self, mode: str = "full"):
        super().__init__(mode=mode, pipeline_stages=4)

    def resolve(
        self,
        consumer: Instruction,
        consumer_op1: int,
        consumer_op2: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, int, str, str, Optional[str]]:
        return self.resolve_4stage_ex(
            consumer=consumer,
            consumer_op1=consumer_op1,
            consumer_op2=consumer_op2,
            producer_marw=producer_marw,
            marw_val=marw_val,
        )
