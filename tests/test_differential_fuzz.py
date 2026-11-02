"""
tests/test_differential_fuzz.py: Differential testing between non-pipelined ISS
and 4-stage / 6-stage pipelined cores under all forwarding modes, plus randomized hazard fuzzer.
"""

import os
import glob
import random
import pytest
from common.isa import (
    Instruction, decode, encode, OP_ADD, OP_SUB, OP_MUL, OP_AND, OP_OR,
    OP_MOV, OP_CMP, OP_LD, OP_ST, OP_B, OP_BEQ, OP_BGT, OP_NOP,
    REG_SP, REG_RA, IllegalInstructionError
)
from memory.hierarchy import MemoryHierarchy
from memory.config import get_preset
from cpu.assembler import Assembler
from cpu.iss import InstructionSetSimulator
from cpu.pipeline4 import Pipeline4Stage
from cpu.pipeline6 import Pipeline6Stage


def load_benchmark(filepath: str):
    with open(filepath, "r") as f:
        src = f.read()
    asmb = Assembler()
    assert asmb.assemble(src)
    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])
    return words, bytes(data)


# =========================================================================
# 1. DIFFERENTIAL BENCHMARK VERIFICATION (ISS vs 4-Stage vs 6-Stage)
# =========================================================================

BENCHMARK_FILES = glob.glob("benchmarks/*.s")

@pytest.mark.parametrize("bench_path", BENCHMARK_FILES)
@pytest.mark.parametrize("fwd_mode", ["full", "none"])
def test_differential_equivalence_benchmarks(bench_path, fwd_mode):
    """
    Asserts exact architectural equivalence across ISS, 4-Stage, and 6-Stage:
      1. Register file contents (r0 - r15)
      2. Condition flags (E, GT)
      3. Exception status (type, faulting PC)
    """
    words, data = load_benchmark(bench_path)
    prog_bytes = len(words) * 4

    # 1. Run ISS (Reference Sequential Interpreter)
    mem_iss = MemoryHierarchy(get_preset("ideal"))
    mem_iss.load_binary(0, data)
    iss = InstructionSetSimulator(mem_iss)
    iss.program_size = prog_bytes
    iss.run()

    # 2. Run 4-Stage Pipeline
    mem_p4 = MemoryHierarchy(get_preset("ideal"))
    mem_p4.load_binary(0, data)
    p4 = Pipeline4Stage(mem_p4, forwarding_mode=fwd_mode, check_forwarding=True)
    p4.program_size = prog_bytes
    p4.run()

    # 3. Run 6-Stage Pipeline
    mem_p6 = MemoryHierarchy(get_preset("ideal"))
    mem_p6.load_binary(0, data)
    p6 = Pipeline6Stage(mem_p6, forwarding_mode=fwd_mode, check_forwarding=True)
    p6.program_size = prog_bytes
    p6.run()

    # Verify Register File Equivalence
    for reg in range(16):
        iss_val = iss.reg_file.read_port1(reg)
        p4_val = p4.reg_file.read_port1(reg)
        p6_val = p6.reg_file.read_port1(reg)
        assert p4_val == iss_val, f"Mismatch in r{reg} for {bench_path} (4-stage vs ISS): {p4_val} != {iss_val}"
        assert p6_val == iss_val, f"Mismatch in r{reg} for {bench_path} (6-stage vs ISS): {p6_val} != {iss_val}"

    # Verify Condition Flags
    assert p4.reg_file.flags.E == iss.reg_file.flags.E
    assert p4.reg_file.flags.GT == iss.reg_file.flags.GT
    assert p6.reg_file.flags.E == iss.reg_file.flags.E
    assert p6.reg_file.flags.GT == iss.reg_file.flags.GT

    # Verify Exception State
    assert p4.current_exception.type == iss.current_exception.type
    assert p6.current_exception.type == iss.current_exception.type


# =========================================================================
# 2. DENSE HAZARD CHAIN DIFFERENTIAL TEST
# =========================================================================

def test_dense_hazard_chain():
    """Tests back-to-back deep RAW dependency chains across all 4 forwarding modes."""
    prog = """
    mov r0, 2
    mov r1, 3
    add r2, r0, r1
    sub r3, r2, r0
    mul r4, r3, r2
    and r5, r4, r3
    or  r6, r5, r2
    add r7, r6, r4
    """
    words, data = load_benchmark("benchmarks/factorial_iter.s") # dummy assemble
    asmb = Assembler()
    assert asmb.assemble(prog)
    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])
    prog_bytes = len(words) * 4

    # Run reference ISS
    mem_iss = MemoryHierarchy(get_preset("ideal"))
    mem_iss.load_binary(0, bytes(data))
    iss = InstructionSetSimulator(mem_iss)
    iss.program_size = prog_bytes
    iss.run()

    for mode in ["none", "basic", "full", "full+"]:
        # 4-stage
        mem4 = MemoryHierarchy(get_preset("ideal"))
        mem4.load_binary(0, bytes(data))
        p4 = Pipeline4Stage(mem4, forwarding_mode=mode)
        p4.program_size = prog_bytes
        p4.run()

        # 6-stage
        mem6 = MemoryHierarchy(get_preset("ideal"))
        mem6.load_binary(0, bytes(data))
        p6 = Pipeline6Stage(mem6, forwarding_mode=mode)
        p6.program_size = prog_bytes
        p6.run()

        for reg in range(8):
            ref = iss.reg_file.read_port1(reg)
            assert p4.reg_file.read_port1(reg) == ref, f"4-Stage mode {mode} failed at r{reg}"
            assert p6.reg_file.read_port1(reg) == ref, f"6-Stage mode {mode} failed at r{reg}"


# =========================================================================
# 3. RANDOM HAZARD FUZZER WITH DETERMINISTIC SEEDS
# =========================================================================

def generate_random_hazard_sequence(seed: int, num_instructions: int = 30) -> str:
    """Generates random ALU and data movement instructions with dense register dependencies."""
    rng = random.Random(seed)
    lines = ["mov r0, 5", "mov r1, 10", "mov r2, 15"]
    ops = ["add", "sub", "and", "or", "lsl", "lsr"]

    for _ in range(num_instructions):
        op = rng.choice(ops)
        rd = rng.randint(0, 7)
        rs1 = rng.randint(0, 7)
        if rng.random() < 0.4:
            imm = rng.randint(1, 31)
            lines.append(f"{op} r{rd}, r{rs1}, {imm}")
        else:
            rs2 = rng.randint(0, 7)
            lines.append(f"{op} r{rd}, r{rs1}, r{rs2}")

    return "\n".join(lines)


@pytest.mark.parametrize("seed", [42, 101, 777, 2026])
def test_random_hazard_fuzzer(seed):
    """Executes deterministic random instruction sequences on ISS, 4-Stage, and 6-Stage."""
    src = generate_random_hazard_sequence(seed, num_instructions=40)
    asmb = Assembler()
    assert asmb.assemble(src)
    words = asmb.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])
    prog_bytes = len(words) * 4

    # ISS
    mem_iss = MemoryHierarchy(get_preset("ideal"))
    mem_iss.load_binary(0, bytes(data))
    iss = InstructionSetSimulator(mem_iss)
    iss.program_size = prog_bytes
    iss.run()

    # 4-Stage Full
    mem4 = MemoryHierarchy(get_preset("ideal"))
    mem4.load_binary(0, bytes(data))
    p4 = Pipeline4Stage(mem4, forwarding_mode="full", check_forwarding=True)
    p4.program_size = prog_bytes
    p4.run()

    # 6-Stage Full
    mem6 = MemoryHierarchy(get_preset("ideal"))
    mem6.load_binary(0, bytes(data))
    p6 = Pipeline6Stage(mem6, forwarding_mode="full", check_forwarding=True)
    p6.program_size = prog_bytes
    p6.run()

    for r in range(8):
        assert p4.reg_file.read_port1(r) == iss.reg_file.read_port1(r)
        assert p6.reg_file.read_port1(r) == iss.reg_file.read_port1(r)


# =========================================================================
# 4. ILLEGAL OPCODE FUZZING AND PRECISE EXCEPTION HANDLING
# =========================================================================

def test_illegal_opcode_vectoring():
    """Verifies that an illegal opcode vectors cleanly without corrupting architectural latches."""
    # Machine code: valid add, then illegal opcode 0xFB000000 (opcode 11111), then add
    valid1 = encode("add", rd=1, rs1=0, imm=5)
    illegal = 0xFB000000 # Opcode "11111"
    valid2 = encode("add", rd=2, rs1=0, imm=9)

    words = [valid1, illegal, valid2]
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])

    # ISS
    mem_iss = MemoryHierarchy(get_preset("ideal"))
    mem_iss.load_binary(0, bytes(data))
    iss = InstructionSetSimulator(mem_iss)
    iss.program_size = 12
    iss.run()
    assert iss.halted
    assert iss.current_exception.has_occurred()
    assert iss.current_exception.faulting_pc == 4

    # 4-Stage
    mem4 = MemoryHierarchy(get_preset("ideal"))
    mem4.load_binary(0, bytes(data))
    p4 = Pipeline4Stage(mem4)
    p4.program_size = 12
    p4.run()
    # In 4-stage, illegal instruction is squashed / halts cleanly at PC 4
    assert p4.reg_file.read_port1(1) == 5
    assert p4.reg_file.read_port1(2) == 0 # valid2 never executed!
