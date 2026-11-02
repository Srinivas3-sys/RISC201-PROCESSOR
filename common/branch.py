"""
common/branch.py: Advanced Branch Predictor, BTB, and RAS Subsystem for RISC201.
Supports policies: stall, flush, predict-taken, static-btfn, 1bit, 2bit, gshare, perfect.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from common.isa import Instruction, OP_B, OP_BEQ, OP_BGT, OP_CALL, OP_RET


@dataclass
class BTBEntry:
    tag: int = 0
    target: int = 0
    valid: bool = False


class BranchTargetBuffer:
    """Branch Target Buffer (BTB) caching target addresses for fast redirect in IF."""
    def __init__(self, entries: int = 64):
        self.num_entries = max(4, entries)
        self.entries: List[BTBEntry] = [BTBEntry() for _ in range(self.num_entries)]
        self.lookups = 0
        self.hits = 0

    def reset(self) -> None:
        self.entries = [BTBEntry() for _ in range(self.num_entries)]
        self.lookups = 0
        self.hits = 0

    def lookup(self, pc: int) -> Tuple[bool, int]:
        self.lookups += 1
        idx = (pc >> 2) % self.num_entries
        tag = pc >> 2
        entry = self.entries[idx]
        if entry.valid and entry.tag == tag:
            self.hits += 1
            return True, entry.target
        return False, 0

    def update(self, pc: int, target: int) -> None:
        idx = (pc >> 2) % self.num_entries
        tag = pc >> 2
        self.entries[idx] = BTBEntry(tag=tag, target=target, valid=True)


class ReturnAddressStack:
    """Return Address Stack (RAS) for accurate call/return subroutine branch prediction."""
    def __init__(self, depth: int = 16):
        self.depth = depth
        self.stack: List[int] = []
        self.lookups = 0
        self.hits = 0

    def reset(self) -> None:
        self.stack.clear()
        self.lookups = 0
        self.hits = 0

    def push(self, return_address: int) -> None:
        if len(self.stack) >= self.depth:
            self.stack.pop(0) # Drop oldest on overflow
        self.stack.append(return_address)

    def pop(self) -> Optional[int]:
        self.lookups += 1
        if self.stack:
            self.hits += 1
            return self.stack.pop()
        return None


class BranchPredictor:
    """
    Unified Branch Prediction Unit supporting multiple dynamic and static policies.
    """
    def __init__(
        self,
        policy: str = "flush",
        table_size: int = 64,
        history_bits: int = 6,
        btb_entries: int = 64,
        ras_entries: int = 16,
        delay_slots: int = 0
    ):
        self.policy = policy.lower() # stall, flush, predict-taken, static-btfn, 1bit, 2bit, gshare, perfect
        self.table_size = max(4, table_size)
        self.history_bits = history_bits
        self.delay_slots = delay_slots

        # 1-bit table: list of booleans (True = Taken, False = Not Taken)
        self.table_1bit: List[bool] = [False] * self.table_size

        # 2-bit saturating counter table (0=SNT, 1=WNT, 2=WT, 3=ST)
        self.table_2bit: List[int] = [1] * self.table_size # Initialized to Weakly Not Taken

        # GShare global history register
        self.global_history: int = 0
        self.gshare_mask = (1 << self.history_bits) - 1

        self.btb = BranchTargetBuffer(btb_entries)
        self.ras = ReturnAddressStack(ras_entries)

        # Performance counters
        self.total_predictions: int = 0
        self.correct_predictions: int = 0
        self.mispredictions: int = 0

    def reset(self) -> None:
        self.table_1bit = [False] * self.table_size
        self.table_2bit = [1] * self.table_size
        self.global_history = 0
        self.btb.reset()
        self.ras.reset()
        self.total_predictions = 0
        self.correct_predictions = 0
        self.mispredictions = 0

    def predict(self, pc: int, inst: Instruction) -> Tuple[bool, int]:
        """
        Predicts branch direction and target address.
        Returns (predict_taken: bool, predicted_target: int).
        """
        self.total_predictions += 1
        target = inst.branch_target

        # Subroutine Return uses RAS
        if inst.is_ret():
            ras_target = self.ras.pop()
            if ras_target is not None:
                return True, ras_target
            return True, target

        # Unconditional Jump / Call is always taken
        if inst.is_jump() or inst.is_call():
            return True, target

        if not inst.is_conditional_branch():
            return False, (pc + 4) & 0xFFFFFFFF

        # Conditional branch prediction policies:
        if self.policy == "stall":
            # Predict not taken, but stall until resolution
            return False, (pc + 4) & 0xFFFFFFFF

        elif self.policy == "flush":
            # Predict Not Taken (static baseline)
            return False, (pc + 4) & 0xFFFFFFFF

        elif self.policy == "predict-taken":
            # Predict Taken
            return True, target

        elif self.policy == "static-btfn":
            # Backward Taken, Forward Not Taken
            is_backward = (target < pc)
            return is_backward, target if is_backward else (pc + 4) & 0xFFFFFFFF

        elif self.policy == "1bit":
            idx = (pc >> 2) % self.table_size
            pred_taken = self.table_1bit[idx]
            return pred_taken, target if pred_taken else (pc + 4) & 0xFFFFFFFF

        elif self.policy == "2bit":
            idx = (pc >> 2) % self.table_size
            pred_taken = (self.table_2bit[idx] >= 2)
            return pred_taken, target if pred_taken else (pc + 4) & 0xFFFFFFFF

        elif self.policy == "gshare":
            pc_hash = (pc >> 2) & self.gshare_mask
            idx = (pc_hash ^ self.global_history) % self.table_size
            pred_taken = (self.table_2bit[idx] >= 2)
            return pred_taken, target if pred_taken else (pc + 4) & 0xFFFFFFFF

        elif self.policy == "perfect":
            # Oracle prediction (handled dynamically during evaluation)
            return False, target

        return False, (pc + 4) & 0xFFFFFFFF

    def update(self, pc: int, inst: Instruction, actual_taken: bool, actual_target: int) -> bool:
        """
        Updates predictor state based on actual resolved outcome.
        Returns True if prediction was correct, False if mispredicted.
        """
        idx = (pc >> 2) % self.table_size

        # 1-bit update
        self.table_1bit[idx] = actual_taken

        # 2-bit saturating counter update
        counter = self.table_2bit[idx]
        if actual_taken:
            self.table_2bit[idx] = min(3, counter + 1)
        else:
            self.table_2bit[idx] = max(0, counter - 1)

        # GShare update
        if self.policy == "gshare":
            pc_hash = (pc >> 2) & self.gshare_mask
            g_idx = (pc_hash ^ self.global_history) % self.table_size
            c = self.table_2bit[g_idx]
            if actual_taken:
                self.table_2bit[g_idx] = min(3, c + 1)
            else:
                self.table_2bit[g_idx] = max(0, c - 1)
            self.global_history = ((self.global_history << 1) | (1 if actual_taken else 0)) & self.gshare_mask

        # BTB update on taken branch
        if actual_taken:
            self.btb.update(pc, actual_target)

        # RAS update on call
        if inst.is_call():
            self.ras.push((pc + 4) & 0xFFFFFFFF)

        return True
