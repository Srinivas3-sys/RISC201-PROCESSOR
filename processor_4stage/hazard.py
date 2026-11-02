"""
processor_4stage/hazard.py: 4-stage-specific hazard detection logic and convenience methods.
"""

from __future__ import annotations
from common.hazard import HazardUnit, HazardAction
from common.isa import Instruction


class HazardUnit4Stage(HazardUnit):
    """
    Hazard unit specialized for the 4-stage simple pipeline (IF -> OF -> EX -> MA_RW).
    """
    def __init__(
        self,
        forwarding_mode: str = "full",
        unified_mem: bool = False,
        regfile_write_ports: int = 1,
    ):
        super().__init__(
            pipeline_stages=4,
            forwarding_mode=forwarding_mode,
            unified_mem=unified_mem,
            regfile_write_ports=regfile_write_ports,
        )

    def evaluate(
        self,
        consumer_of: Instruction,
        in_ex: Instruction,
        in_marw: Instruction,
        mem_busy: bool = False,
        wb_requests: int = 1,
    ) -> HazardAction:
        return self.evaluate_4stage(
            consumer_of=consumer_of,
            in_ex=in_ex,
            in_marw=in_marw,
            mem_busy=mem_busy,
            wb_requests=wb_requests,
        )
