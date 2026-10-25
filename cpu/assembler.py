"""
cpu/assembler.py: Two-pass SimpleRisc assembler and macro preprocessor.
Assembles SimpleRisc assembly (.s) into 32-bit machine code words.
Fully compatible with existing benchmark files.
"""

from __future__ import annotations
import re
from typing import List, Dict, Tuple
from cpu.isa import Opcode, REG_SP, REG_RA


class Assembler:
    def __init__(self):
        self.symbol_table: Dict[str, int] = {}
        self.machine_words: List[int] = []
        self.assembled_lines: List[Tuple[int, int, str]] = [] # (pc, word, text)

    @staticmethod
    def _strip_comments(line: str) -> str:
        # Strip comments starting with ';' or '@' or '/*'
        out = []
        in_block = False
        i = 0
        while i < len(line):
            if not in_block and i + 1 < len(line) and line[i:i+2] == "/*":
                in_block = True
                i += 2
            elif in_block and i + 1 < len(line) and line[i:i+2] == "*/":
                in_block = False
                i += 2
            elif not in_block and line[i] in (";", "@"):
                break
            elif not in_block:
                out.append(line[i])
                i += 1
            else:
                i += 1
        return "".join(out).strip()

    @staticmethod
    def _expand_macros(source: str) -> List[str]:
        expanded = []
        for raw in source.splitlines():
            line = Assembler._strip_comments(raw)
            if not line:
                continue

            # Check for label
            label_prefix = ""
            if ":" in line:
                parts = line.split(":", 1)
                label_prefix = parts[0].strip() + ": "
                line = parts[1].strip()
                if not line:
                    expanded.append(label_prefix.strip())
                    continue

            tokens = [t.strip() for t in re.split(r"[\s,]+", line) if t.strip()]
            if not tokens:
                continue

            m = tokens[0].lower()
            if m == "push" and len(tokens) >= 2:
                reg = tokens[1]
                expanded.append(f"{label_prefix}sub sp, sp, 4")
                expanded.append(f"st {reg}, 0[sp]")
            elif m == "pop" and len(tokens) >= 2:
                reg = tokens[1]
                expanded.append(f"{label_prefix}ld {reg}, 0[sp]")
                expanded.append(f"add sp, sp, 4")
            elif m == "pushm":
                reg_list = re.findall(r"r\d+|sp|ra", line.lower())
                k = len(reg_list)
                if k > 0:
                    expanded.append(f"{label_prefix}sub sp, sp, {k * 4}")
                    for idx, r in enumerate(reg_list):
                        expanded.append(f"st {r}, {idx * 4}[sp]")
            elif m == "popm":
                reg_list = re.findall(r"r\d+|sp|ra", line.lower())
                k = len(reg_list)
                if k > 0:
                    if label_prefix:
                        expanded.append(label_prefix.strip())
                    for idx, r in enumerate(reg_list):
                        expanded.append(f"ld {r}, {idx * 4}[sp]")
                    expanded.append(f"add sp, sp, {k * 4}")
            else:
                expanded.append(label_prefix + line)
        return expanded

    @staticmethod
    def _parse_reg(token: str) -> int:
        s = token.lower().strip()
        if s == "sp": return REG_SP
        if s == "ra": return REG_RA
        if s.startswith("r") and s[1:].isdigit():
            val = int(s[1:])
            if 0 <= val < 16:
                return val
        raise ValueError(f"Invalid register name: '{token}'")

    @staticmethod
    def _parse_imm(token: str) -> int:
        s = token.strip()
        if s.startswith("#"):
            s = s[1:].strip()
        if s.lower().startswith("0x"):
            return int(s, 16)
        return int(s)

    def assemble(self, source_code: str) -> bool:
        self.symbol_table.clear()
        self.machine_words.clear()
        self.assembled_lines.clear()

        lines = self._expand_macros(source_code)

        # PASS 1: Symbol Table & Address Assignment
        current_pc = 0
        code_lines: List[Tuple[int, str]] = []

        for line in lines:
            line = line.strip()
            if not line:
                continue

            # Check for directives
            if line.startswith((".text", ".data", ".file", ".globl")):
                continue

            if ":" in line:
                parts = line.split(":", 1)
                lbl = parts[0].strip()
                self.symbol_table[lbl] = current_pc
                line = parts[1].strip()
                if not line:
                    continue

            if line.startswith(".word"):
                val_str = line[5:].strip()
                val = self._parse_imm(val_str)
                code_lines.append((current_pc, f".word {val}"))
                current_pc += 4
                continue

            code_lines.append((current_pc, line))
            current_pc += 4

        # PASS 2: Instruction Encoding
        for pc, line in code_lines:
            if line.startswith(".word"):
                val = int(line.split()[1])
                word = val & 0xFFFFFFFF
                self.machine_words.append(word)
                self.assembled_lines.append((pc, word, line))
                continue

            tokens = [t.strip() for t in re.split(r"[\s,]+", line) if t.strip()]
            m = tokens[0].lower()

            word = 0
            if m == "nop":
                word = (Opcode.OP_NOP << 27)
            elif m == "ret":
                word = (Opcode.OP_RET << 27)
            elif m == "retz":
                word = (Opcode.OP_RETZ << 27)

            elif m in ("b", "beq", "bgt", "call"):
                target_str = tokens[1]
                if target_str in self.symbol_table:
                    target_pc = self.symbol_table[target_str]
                else:
                    target_pc = self._parse_imm(target_str)
                word_offset = (target_pc - pc) >> 2
                op_map = {"b": Opcode.OP_B, "beq": Opcode.OP_BEQ, "bgt": Opcode.OP_BGT, "call": Opcode.OP_CALL}
                word = (op_map[m] << 27) | (word_offset & 0x07FFFFFF)

            elif m in ("ld", "st"):
                rd = self._parse_reg(tokens[1])
                mem_spec = tokens[2]
                match = re.match(r"^(-?\d+|0x[0-9a-fA-F]+)?\[(r\d+|sp|ra)\]$", mem_spec)
                if match:
                    off_str, base_str = match.groups()
                    offset = self._parse_imm(off_str) if off_str else 0
                    rs1 = self._parse_reg(base_str)
                else:
                    raise ValueError(f"Malformed memory operand: {mem_spec}")
                op = Opcode.OP_LD if m == "ld" else Opcode.OP_ST
                word = (op << 27) | (1 << 26) | (rd << 22) | (rs1 << 18) | (offset & 0xFFFF)

            elif m == "mov":
                rd = self._parse_reg(tokens[1])
                arg2 = tokens[2]
                if arg2.lower() in [f"r{i}" for i in range(16)] + ["sp", "ra"]:
                    rs2 = self._parse_reg(arg2)
                    word = (Opcode.OP_MOV << 27) | (0 << 26) | (rd << 22) | (rs2 << 14)
                else:
                    imm = self._parse_imm(arg2)
                    word = (Opcode.OP_MOV << 27) | (1 << 26) | (rd << 22) | (imm & 0xFFFF)

            elif m == "not":
                rd = self._parse_reg(tokens[1])
                arg2 = tokens[2]
                if arg2.lower() in [f"r{i}" for i in range(16)] + ["sp", "ra"]:
                    rs2 = self._parse_reg(arg2)
                    word = (Opcode.OP_NOT << 27) | (0 << 26) | (rd << 22) | (rs2 << 14)
                else:
                    imm = self._parse_imm(arg2)
                    word = (Opcode.OP_NOT << 27) | (1 << 26) | (rd << 22) | (imm & 0xFFFF)

            elif m == "cmp":
                rs1 = self._parse_reg(tokens[1])
                arg2 = tokens[2]
                if arg2.lower() in [f"r{i}" for i in range(16)] + ["sp", "ra"]:
                    rs2 = self._parse_reg(arg2)
                    word = (Opcode.OP_CMP << 27) | (0 << 26) | (rs1 << 18) | (rs2 << 14)
                else:
                    imm = self._parse_imm(arg2)
                    word = (Opcode.OP_CMP << 27) | (1 << 26) | (rs1 << 18) | (imm & 0xFFFF)

            else:
                # 3-operand instructions: add, sub, mul, div, mod, and, or, lsl, lsr, asr, min, max, rots
                op_map = {
                    "add": Opcode.OP_ADD, "sub": Opcode.OP_SUB, "mul": Opcode.OP_MUL,
                    "div": Opcode.OP_DIV, "mod": Opcode.OP_MOD, "and": Opcode.OP_AND,
                    "or": Opcode.OP_OR, "lsl": Opcode.OP_LSL, "lsr": Opcode.OP_LSR,
                    "asr": Opcode.OP_ASR, "min": Opcode.OP_MIN, "max": Opcode.OP_MAX,
                    "rots": Opcode.OP_ROTS,
                }
                if m not in op_map:
                    raise ValueError(f"Unknown mnemonic: '{m}' in line: {line}")
                op = op_map[m]
                rd = self._parse_reg(tokens[1])
                rs1 = self._parse_reg(tokens[2])
                arg3 = tokens[3]

                if arg3.lower() in [f"r{i}" for i in range(16)] + ["sp", "ra"]:
                    rs2 = self._parse_reg(arg3)
                    word = (op << 27) | (0 << 26) | (rd << 22) | (rs1 << 18) | (rs2 << 14)
                else:
                    imm = self._parse_imm(arg3)
                    word = (op << 27) | (1 << 26) | (rd << 22) | (rs1 << 18) | (imm & 0xFFFF)

            self.machine_words.append(word)
            self.assembled_lines.append((pc, word, line))

        return True
