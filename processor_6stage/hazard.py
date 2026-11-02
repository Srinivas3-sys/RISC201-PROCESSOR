"""
processor_6stage/hazard.py: 6-stage-specific hazard detection logic and convenience methods.
"""

from __future__ import annotations
from common.hazard import HazardUnit, HazardAction
from common.isa import Instruction


class HazardUnit6Stage(HazardUnit):
    """
    Hazard unit specialized for the 6-stage deep pipeline (IF -> ID -> RF -> EX -> MA -> RW).
    """
    def __init__(
        self,
        forwarding_mode: str = "full",
        unified_mem: bool = False,
        regfile_write_ports: int = 1,
    ):
        super().__init__(
            pipeline_stages=6,
            forwarding_mode=forwarding_mode,
            unified_mem=unified_mem,
            regfile_write_ports=regfile_write_ports,
        )

    def evaluate(
        self,
        consumer_rf: Instruction,
        in_ex: Instruction,
        in_ma: Instruction,
        in_rw: Instruction,
        mem_busy: bool = False,
        wb_requests: int = 1,
    ) -> HazardAction:
        return self.evaluate_6stage(
            consumer_rf=consumer_rf,
            in_ex=in_ex,
            in_ma=in_ma,
            in_rw=in_rw,
            mem_busy=mem_busy,
            wb_requests=wb_requests,
        )
