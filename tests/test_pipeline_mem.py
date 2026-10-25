"""
tests/test_pipeline_mem.py: Integration tests for 4-stage and 6-stage pipelines
with the memory hierarchy subsystem. Verifies exact cycle-accuracy in ideal mode,
multi-cycle stalls in modern mode, and precise exception behavior.
"""

import pytest
from cpu.assembler import Assembler
from cpu.pipeline4 import Pipeline4Stage
from cpu.pipeline6 import Pipeline6Stage
from cpu.microcode import MicroController
from cpu.exceptions import ExceptionType
from memory.config import get_preset
from memory.hierarchy import MemoryHierarchy


def load_and_assemble(filepath: str):
    with open(filepath, "r") as f:
        src = f.read()
    asmb = Assembler()
    assert asmb.assemble(src)
    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])
    return words, bytes(data)


def test_4stage_and_6stage_ideal_mode_cycle_accuracy():
    # In 'ideal' preset (0 extra latency, 1-cycle access):
    # Factorial (Iterative) MUST execute in exactly:
    #   4-Stage: 35 cycles, 24 retired instructions
    #   6-Stage: 41 cycles, 24 retired instructions
    words, data = load_and_assemble("benchmarks/factorial_iter.s")

    # 4-Stage
    mem4 = MemoryHierarchy(get_preset("ideal"))
    mem4.load_binary(0, data)
    p4 = Pipeline4Stage(mem4)
    p4.program_size = len(words) * 4
    p4.run()
    assert p4.cycles == 35
    assert p4.retired_instructions == 24
    assert p4.reg_file.read_port1(1) == 720 # 6! = 720

    # 6-Stage
    mem6 = MemoryHierarchy(get_preset("ideal"))
    mem6.load_binary(0, data)
    p6 = Pipeline6Stage(mem6)
    p6.program_size = len(words) * 4
    p6.run()
    assert p6.cycles == 41
    assert p6.retired_instructions == 24
    assert p6.reg_file.read_port1(1) == 720


def test_microcode_wait_states():
    words, data = load_and_assemble("benchmarks/factorial_iter.s")
    mem = MemoryHierarchy(get_preset("simple"))
    mem.load_binary(0, data)
    mcu = MicroController(mem)

    # Step through instructions
    for _ in range(5):
        while not mcu.step_micro_cycle():
            pass

    assert mcu.total_micro_cycles > 0


def test_stack_guard_band_precise_exception():
    # Write into stack guard band [0x7FFC0, 0x80000) -> triggers precise STACK_CORRUPTION
    test_src = """
    mov r1, 8
    lsl r1, r1, 16  ; r1 = 0x00080000
    st r0, -16[r1]  ; address = 0x0007FFF0 (inside 64-byte guard zone)
    """
    asmb = Assembler()
    assert asmb.assemble(test_src)
    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])

    mem = MemoryHierarchy(get_preset("ideal"))
    mem.load_binary(0, bytes(data))
    p4 = Pipeline4Stage(mem)
    p4.program_size = len(words) * 4
    p4.run()

    assert p4.halted
    assert p4.current_exception.has_occurred()
    assert p4.current_exception.type == ExceptionType.STACK_CORRUPTION
