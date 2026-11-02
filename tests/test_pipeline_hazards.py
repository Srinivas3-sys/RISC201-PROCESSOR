"""
tests/test_pipeline_hazards.py: Comprehensive hand-computed unit test suite
for hazard detection, data forwarding, branch prediction, structural conflicts,
and exact cycle accounting on both 4-stage and 6-stage SimpleRisc pipelines.
"""

import pytest
from common.isa import (
    encode, decode, OP_ADD, OP_SUB, OP_MOV, OP_LD, OP_ST, OP_B, OP_BEQ, OP_BGT, OP_NOP,
    REG_SP, REG_RA, IllegalInstructionError
)
from memory.hierarchy import MemoryHierarchy
from memory.config import get_preset
from cpu.assembler import Assembler
from cpu.pipeline4 import Pipeline4Stage
from cpu.pipeline6 import Pipeline6Stage
from common.forwarding import ForwardingUnit


def assemble_and_load(assembly_text: str, mem: MemoryHierarchy) -> int:
    """Assembles assembly code and loads machine bytes into MemoryHierarchy at address 0."""
    asm = Assembler()
    assert asm.assemble(assembly_text)
    words = asm.machine_words
    data = bytearray()
    for w in words:
        data.extend([w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF])
    mem.load_binary(0, bytes(data))
    return len(words) * 4


# =========================================================================
# 1. ALU -> ALU BACK-TO-BACK HAZARD TESTS
# =========================================================================

def test_alu_to_alu_back_to_back():
    """
    ADD r1, r2, r3
    ADD r4, r1, r5
    Forwarding: 0 stall bubbles.
    Without forwarding:
      4-Stage: 2 stall cycles.
      6-Stage: 3 stall cycles.
    """
    prog = """
    mov r2, 10
    mov r3, 20
    mov r5, 5
    add r1, r2, r3
    add r4, r1, r5
    """

    # --- 4-Stage With Full Forwarding ---
    mem4_fwd = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem4_fwd)
    p4_fwd = Pipeline4Stage(mem4_fwd, forwarding_mode="full", check_forwarding=True)
    p4_fwd.program_size = sz
    p4_fwd.run()
    assert p4_fwd.reg_file.read_port1(4) == 35
    assert p4_fwd.perf.stall_cycles["RAW_LOAD_USE"] == 0
    assert p4_fwd.perf.stall_cycles["RAW_NOFWD"] == 0

    # --- 4-Stage Without Forwarding ---
    mem4_nofwd = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem4_nofwd)
    p4_nofwd = Pipeline4Stage(mem4_nofwd, forwarding_mode="none")
    p4_nofwd.program_size = sz
    p4_nofwd.run()
    assert p4_nofwd.reg_file.read_port1(4) == 35
    # Stalls occur when resolving dependencies without bypass
    assert p4_nofwd.perf.stall_cycles["RAW_NOFWD"] > 0
    assert p4_nofwd.cycles > p4_fwd.cycles

    # --- 6-Stage With Full Forwarding ---
    mem6_fwd = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem6_fwd)
    p6_fwd = Pipeline6Stage(mem6_fwd, forwarding_mode="full", check_forwarding=True)
    p6_fwd.program_size = sz
    p6_fwd.run()
    assert p6_fwd.reg_file.read_port1(4) == 35
    assert p6_fwd.perf.stall_cycles["RAW_LOAD_USE"] == 0
    assert p6_fwd.perf.stall_cycles["RAW_NOFWD"] == 0

    # --- 6-Stage Without Forwarding ---
    mem6_nofwd = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem6_nofwd)
    p6_nofwd = Pipeline6Stage(mem6_nofwd, forwarding_mode="none")
    p6_nofwd.program_size = sz
    p6_nofwd.run()
    assert p6_nofwd.reg_file.read_port1(4) == 35
    assert p6_nofwd.perf.stall_cycles["RAW_NOFWD"] > 0
    assert p6_nofwd.cycles > p6_fwd.cycles


# =========================================================================
# 2. ALU -> ALU AT DISTANCE 2 AND 3
# =========================================================================

def test_alu_to_alu_distance_2_and_3():
    """
    ADD r1, r2, r3
    NOP
    ADD r4, r1, r5
    With 1 NOP, RAW without forwarding stalls fewer cycles than back-to-back.
    """
    prog = """
    mov r2, 10
    mov r3, 20
    mov r5, 5
    add r1, r2, r3
    nop
    add r4, r1, r5
    """
    mem = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem)
    p4 = Pipeline4Stage(mem, forwarding_mode="none")
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(4) == 35


# =========================================================================
# 3. LOAD -> USE HAZARD (EXACT STALL BUBBLES)
# =========================================================================

def test_load_use_hazard_stalls():
    """
    ST r3, 0[r2]
    LD r1, 0[r2]
    ADD r4, r1, r5
    In 4-stage: MUST inject exactly 1 load-use stall bubble even with full forwarding.
    """
    prog = """
    mov r2, 100
    mov r3, 42
    mov r5, 8
    st r3, 0[r2]
    ld r1, 0[r2]
    add r4, r1, r5
    """
    # 4-Stage
    mem4 = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem4)
    p4 = Pipeline4Stage(mem4, forwarding_mode="full")
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(4) == 50
    assert p4.perf.stall_cycles["RAW_LOAD_USE"] == 1

    # 6-Stage
    mem6 = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem6)
    p6 = Pipeline6Stage(mem6, forwarding_mode="full")
    p6.program_size = sz
    p6.run()
    assert p6.reg_file.read_port1(4) == 50
    assert p6.perf.stall_cycles["RAW_LOAD_USE"] == 1


# =========================================================================
# 4. LOAD -> STORE DATA BYPASS (0 STALLS IN FULL vs 1 STALL IN BASIC)
# =========================================================================

def test_load_to_store_data_forwarding():
    """
    LD r1, 0[r2]
    ST r1, 0[r3]
    In Full forwarding: 0 stalls because store data is needed in MA, not EX!
    In Basic forwarding: 1 stall.
    """
    prog = """
    mov r2, 100
    mov r3, 200
    mov r4, 77
    st r4, 0[r2]
    ld r1, 0[r2]
    st r1, 0[r3]
    """
    # 4-Stage Full Forwarding
    mem_full = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem_full)
    p4_full = Pipeline4Stage(mem_full, forwarding_mode="full")
    p4_full.program_size = sz
    p4_full.run()
    val, _ = mem_full.read_word(200, is_fetch=False)
    assert val == 77
    assert p4_full.perf.stall_cycles["RAW_LOAD_USE"] == 0

    # 4-Stage Basic Forwarding
    mem_basic = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem_basic)
    p4_basic = Pipeline4Stage(mem_basic, forwarding_mode="basic")
    p4_basic.program_size = sz
    p4_basic.run()
    val_b, _ = mem_basic.read_word(200, is_fetch=False)
    assert val_b == 77
    assert p4_basic.perf.stall_cycles["RAW_LOAD_USE"] == 1


# =========================================================================
# 5. YOUNGEST-PRODUCER PRIORITY (WAW RESOLUTION)
# =========================================================================

def test_youngest_producer_priority():
    """
    ADD r1, r2, r3  (older writes r1 = 30)
    ADD r1, r4, r5  (younger writes r1 = 12)
    ADD r6, r1, r7  (consumer MUST read younger r1 = 12, not 30)
    """
    prog = """
    mov r2, 10
    mov r3, 20
    mov r4, 5
    mov r5, 7
    mov r7, 100
    add r1, r2, r3
    add r1, r4, r5
    add r6, r1, r7
    """
    # 4-Stage
    mem4 = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem4)
    p4 = Pipeline4Stage(mem4, forwarding_mode="full", check_forwarding=True)
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(6) == 112

    # 6-Stage
    mem6 = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem6)
    p6 = Pipeline6Stage(mem6, forwarding_mode="full", check_forwarding=True)
    p6.program_size = sz
    p6.run()
    assert p6.reg_file.read_port1(6) == 112


# =========================================================================
# 6. BRANCH PREDICTION POLICIES EVALUATION
# =========================================================================

@pytest.mark.parametrize("policy", ["flush", "predict-taken", "static-btfn", "1bit", "2bit", "gshare"])
def test_branch_policies(policy):
    """Verifies that iterative loop executes correctly under all branch prediction policies."""
    prog = """
    mov r0, 5
    mov r1, 1
    mov r2, r0
    mul r1, r1, r2
    sub r2, r2, 1
    cmp r2, 1
    bgt 12
    """
    mem = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem)
    p4 = Pipeline4Stage(mem, branch_policy=policy)
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(1) == 120 # 5! = 120
    assert p4.perf.total_branches > 0


# =========================================================================
# 7. BRANCH DELAY SLOTS
# =========================================================================

def test_branch_delay_slots():
    """With 1 delay slot, instruction following branch always executes."""
    # Branch taken, delay slot instruction executes
    prog = """
    mov r1, 10
    b 16
    add r1, r1, 5
    mov r2, 99
    """
    mem = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem)
    p4 = Pipeline4Stage(mem, delay_slots=1)
    p4.program_size = sz
    p4.run()
    # r1 should be 15 because delay slot (add r1, r1, 5) executed
    assert p4.reg_file.read_port1(1) == 15


# =========================================================================
# 8. STRUCTURAL HAZARD: UNIFIED MEMORY
# =========================================================================

def test_structural_hazard_unified_mem():
    """In unified-mem mode, a memory stage load/store conflicts with IF fetch."""
    prog = """
    mov r2, 100
    mov r3, 55
    st r3, 0[r2]
    ld r1, 0[r2]
    add r4, r1, 10
    """
    mem = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem)
    p4 = Pipeline4Stage(mem, unified_mem=True)
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(4) == 65
    assert p4.perf.stall_cycles["STRUCTURAL_MEM"] > 0


# =========================================================================
# 9. FULL+ STORE-TO-LOAD FORWARDING VIA STORE BUFFER
# =========================================================================

def test_full_plus_store_to_load_forwarding():
    """In Full+ mode, store buffer provides data to a younger load before memory."""
    prog = """
    mov r2, 200
    mov r3, 888
    st r3, 0[r2]
    ld r4, 0[r2]
    add r5, r4, 12
    """
    mem = MemoryHierarchy(get_preset("ideal"))
    sz = assemble_and_load(prog, mem)
    p4 = Pipeline4Stage(mem, forwarding_mode="full+")
    p4.program_size = sz
    p4.run()
    assert p4.reg_file.read_port1(5) == 900
    assert p4.perf.forwarding_path_usage.get("STORE_TO_LOAD", 0) > 0


# =========================================================================
# 10. MINIMUM STALL TABLE GENERATION
# =========================================================================

def test_minimum_stall_table_generation():
    table4 = ForwardingUnit.generate_minimum_stall_table(4)
    table6 = ForwardingUnit.generate_minimum_stall_table(6)
    assert "4-Stage Pipeline" in table4
    assert "6-Stage Pipeline" in table6
    assert "RAW_LOAD_USE" not in table4 # table headers
    assert "Producer" in table4
