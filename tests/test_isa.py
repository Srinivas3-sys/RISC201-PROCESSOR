"""
tests/test_isa.py: Comprehensive test suite for Part A - Binary-String Opcodes.
Tests bit-string utilities, property round-trips, exhaustive table coverage,
golden file regressions, assembler/disassembler round-trips, and opcode linting.
"""

import os
import re
import glob
import pytest
from common.isa import (
    WORD_BITS, OPCODE_BITS, REG_BITS, MOD_BITS, IMM16_BITS,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_CMP, OP_AND, OP_OR,
    OP_NOT, OP_MOV, OP_LSL, OP_LSR, OP_ASR, OP_NOP, OP_LD, OP_ST,
    OP_BEQ, OP_BGT, OP_B, OP_CALL, OP_RET, OP_MOVZ, OP_RETZ,
    OP_MIN, OP_MAX, OP_ROTS,
    int_to_bits, bits_to_int, word_to_bits, bits_to_word, slice_field,
    IllegalInstructionError, INSTRUCTION_TABLE, MNEMONIC_TO_OPCODE,
    mnemonic_of, opcode_of, decode, encode, print_isa_table
)
from cpu.assembler import Assembler


def test_bit_string_utilities_properties():
    # 1. Round-trip for widths 1 to 32
    for width in [1, 2, 4, 5, 8, 16, 27, 32]:
        for val in [0, 1, (1 << (width - 1)) - 1, (1 << width) - 1]:
            b = int_to_bits(val, width)
            assert len(b) == width
            assert bits_to_int(b, signed=False) == val

    # 2. Signed round-trip
    for width in [4, 5, 8, 16, 27]:
        for val in [-1, -2, -(1 << (width - 1)), (1 << (width - 1)) - 1]:
            b = int_to_bits(val, width)
            assert len(b) == width
            assert bits_to_int(b, signed=True) == val

    # 3. Reject out-of-range values
    with pytest.raises(ValueError):
        int_to_bits(32, 5) # 5 bits max unsigned is 31
    with pytest.raises(ValueError):
        int_to_bits(-17, 5) # 5 bits min signed is -16
    with pytest.raises(ValueError):
        int_to_bits(5, 0) # width <= 0

    # 4. Reject invalid characters
    with pytest.raises(ValueError):
        bits_to_int("010201")
    with pytest.raises(ValueError):
        bits_to_int("")
    with pytest.raises(ValueError):
        bits_to_int("abc")


def test_slice_field():
    # 32-bit word test pattern: 0xA5A500FF
    word_val = 0xA5A500FF
    bitstr = word_to_bits(word_val, 32)
    assert len(bitstr) == 32

    # Extract [31:28] -> 0xA -> "1010"
    assert slice_field(bitstr, 31, 28) == "1010"
    # Extract [7:0] -> 0xFF -> "11111111"
    assert slice_field(bitstr, 7, 0) == "11111111"
    # Single bit
    assert slice_field(bitstr, 0, 0) == "1"

    with pytest.raises(ValueError):
        slice_field(bitstr, 10, 20) # hi < lo
    with pytest.raises(ValueError):
        slice_field(bitstr, 35, 0) # hi >= 32


def test_exhaustive_instruction_table():
    assert len(INSTRUCTION_TABLE) == 26

    seen_opcodes = set()
    for opcode_bits, info in INSTRUCTION_TABLE.items():
        # Every opcode must be exactly 5 bits of '0'/'1'
        assert len(opcode_bits) == OPCODE_BITS
        assert all(c in ('0', '1') for c in opcode_bits)
        assert info.opcode == opcode_bits
        assert opcode_bits not in seen_opcodes, f"Duplicate opcode: {opcode_bits}"
        seen_opcodes.add(opcode_bits)

        # Mnemonic lookup must round-trip
        assert mnemonic_of(opcode_bits) == info.mnemonic
        assert opcode_of(info.mnemonic) == opcode_bits

        # Test decode(encode(x)) round-trip
        if info.is_branch and info.mnemonic in ("b", "beq", "bgt", "call"):
            w = encode(info.mnemonic, branch_offset=10)
            d = decode(w, pc=0)
            assert d.opcode == opcode_bits
            assert d.branch_offset == 10
        elif info.mnemonic in ("nop", "ret", "retz"):
            w = encode(info.mnemonic)
            d = decode(w, pc=0)
            assert d.opcode == opcode_bits
        elif info.format == "I":
            w = encode(info.mnemonic, rd=3, rs1=5, imm=100)
            d = decode(w, pc=0)
            assert d.opcode == opcode_bits
            assert d.rd == 3
            assert d.rs1 == 5
            assert d.imm16 == 100
        else:
            w = encode(info.mnemonic, rd=2, rs1=4, rs2=6)
            d = decode(w, pc=0)
            assert d.opcode == opcode_bits
            assert d.rd == 2
            assert d.rs1 == 4
            assert d.rs2 == 6


def test_unknown_opcode_raises_illegal_instruction():
    # Opcode "11110" is not in the table
    raw_word = bits_to_word("11110" + "0" * 27)
    with pytest.raises(IllegalInstructionError) as exc_info:
        decode(raw_word)
    assert exc_info.value.opcode_bits == "11110"
    assert "0x" in str(exc_info.value)


def test_golden_file_assembler_regression():
    # Verify that assembling all sample benchmarks produces byte-identical output to tests/golden/
    for s_path in sorted(glob.glob("benchmarks/*.s")):
        name = os.path.basename(s_path)[:-2]
        with open(s_path) as f:
            src = f.read()
        a = Assembler()
        assert a.assemble(src)

        hex_lines = [f"0x{w:08X}\n" for w in a.machine_words]
        bin_lines = [f"{w:032b}\n" for w in a.machine_words]

        with open(f"tests/golden/{name}.hex") as f:
            gold_hex = f.readlines()
        with open(f"tests/golden/{name}.bin") as f:
            gold_bin = f.readlines()

        assert hex_lines == gold_hex, f"Hex regression detected in {name}.hex"
        assert bin_lines == gold_bin, f"Bin regression detected in {name}.bin"


def test_opcode_lint_check():
    """
    Fails if Python source files outside common/isa.py contain bare numeric opcode comparisons
    or raw numeric literals used as opcodes.
    """
    # Regex for bad patterns like 'opcode == 0' or 'op == 14' or 'Opcode.OP_ADD == 0'
    bad_cmp = re.compile(r'\b(opcode|op)\s*==\s*\d+\b')
    hex_opcode_cmp = re.compile(r'\b(opcode|op)\s*==\s*0x[0-9a-fA-F]+\b')

    source_files = glob.glob("cpu/*.py") + glob.glob("common/*.py")

    for path in source_files:
        if path.endswith("common/isa.py"):
            continue
        with open(path) as f:
            for line_idx, line in enumerate(f, 1):
                assert not bad_cmp.search(line), f"Lint error at {path}:{line_idx}: numeric opcode comparison: '{line.strip()}'"
                assert not hex_opcode_cmp.search(line), f"Lint error at {path}:{line_idx}: hex opcode comparison: '{line.strip()}'"


def test_print_isa_table():
    table_str = print_isa_table()
    assert "add" in table_str
    assert "00000" in table_str
    assert "ld" in table_str
    assert "01110" in table_str
