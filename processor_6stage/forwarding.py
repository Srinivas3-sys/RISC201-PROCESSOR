"""
processor_6stage/forwarding.py: 6-stage-specific bypass network and mux select helpers.
"""

from __future__ import annotations
from typing import Tuple, Optional
from common.forwarding import ForwardingUnit
from common.isa import Instruction


class ForwardingUnit6Stage(ForwardingUnit):
    """
    Forwarding unit specialized for the 6-stage pipeline (IF -> ID -> RF -> EX -> MA -> RW).
    Provides bypass paths from EX_MA (younger) and MA_RW (older) to EX and RF inputs.
    """
    def __init__(self, mode: str = "full"):
        super().__init__(mode=mode, pipeline_stages=6)

    def resolve(
        self,
        consumer: Instruction,
        rf_op1: int,
        rf_op2: int,
        producer_exma: Instruction,
        exma_val: int,
        producer_marw: Instruction,
        marw_val: int,
    ) -> Tuple[int, int, str, str, Optional[str]]:
        return self.resolve_6stage_ex(
            consumer=consumer,
            rf_op1=rf_op1,
            rf_op2=rf_op2,
            producer_exma=producer_exma,
            exma_val=exma_val,
            producer_marw=producer_marw,
            marw_val=marw_val,
        )
