#ifndef RISC201_DISASSEMBLER_HPP
#define RISC201_DISASSEMBLER_HPP

#include "../common/Types.hpp"
#include "MacroPreprocessor.hpp"
#include <string>
#include <sstream>
#include <iomanip>

namespace risc201 {
namespace assembler {

/**
 * Disassembler for the RISC201 / SimpleRisc ISA.
 * Parses 32-bit binary words and reconstructs textual assembly instructions
 * (Assignment Requirement 2b).
 */
class Disassembler {
public:
    static DecodedInst decode(Word word, Word pc = 0) {
        DecodedInst d;
        d.rawWord = word;
        d.opcode = isa::wordToOpcode(word);
        const auto& tbl = isa::getInstructionTable();
        auto it = tbl.find(d.opcode);
        if (it != tbl.end()) {
            d.info = &it->second;
        }

        // Branch format check: branch opcodes have MSB bit = 1 or opcode == NOP
        // (Sarangi Section 3.3.14, Table 3.10)
        // b (18), beq (16), bgt (17), call (19), ret (20), nop (13)
        if (d.opcode == isa::OP_B || d.opcode == isa::OP_BEQ || d.opcode == isa::OP_BGT ||
            d.opcode == isa::OP_CALL || d.opcode == isa::OP_RET || d.opcode == isa::OP_NOP ||
            d.opcode == isa::OP_RETZ) {
            d.format = InstFormat::BRANCH;
            if (d.opcode == isa::OP_RET) {
                d.rs1 = REG_RA;
            }
            int32_t offset27 = static_cast<int32_t>(word & 0x07FFFFFF);
            // Sign-extend 27-bit signed word offset
            if (offset27 & 0x04000000) offset27 |= 0xF8000000;
            d.branchOffset = offset27;
            d.branchTarget = pc + static_cast<Word>(offset27 * 4);
            return d;
        }

        // Otherwise: Register or Immediate format
        d.isImmediate = ((word >> 26) & 1) != 0;
        d.format = d.isImmediate ? InstFormat::IMMEDIATE : InstFormat::REGISTER;
        d.rd = static_cast<RegId>((word >> 22) & 0xF);
        d.rs1 = static_cast<RegId>((word >> 18) & 0xF);

        if (d.isImmediate) {
            // Immediate format: modifier in bits 17..16, constant in 15..0
            uint8_t mod = static_cast<uint8_t>((word >> 16) & 0x3);
            d.modifier = static_cast<ImmModifier>(mod);
            d.imm16 = static_cast<int16_t>(word & 0xFFFF);

            if (d.modifier == ImmModifier::UNSIGNED) {
                d.immx = static_cast<Word>(static_cast<uint16_t>(d.imm16));
            } else if (d.modifier == ImmModifier::HIGH) {
                d.immx = static_cast<Word>(static_cast<uint16_t>(d.imm16)) << 16;
            } else {
                d.immx = static_cast<Word>(static_cast<int32_t>(d.imm16));
            }
        } else {
            // Register format: rs2 in bits 17..14
            d.rs2 = static_cast<RegId>((word >> 14) & 0xF);
        }

        return d;
    }

    static std::string disassemble(Word word, Word pc = 0) {
        DecodedInst d = decode(word, pc);
        std::ostringstream oss;

        auto getReg = [](RegId id) -> std::string {
            if (id == REG_SP) return "sp";
            if (id == REG_RA) return "ra";
            return "r" + std::to_string(id);
        };

        auto getImmSuffix = [](ImmModifier mod) -> std::string {
            if (mod == ImmModifier::UNSIGNED) return "u";
            if (mod == ImmModifier::HIGH) return "h";
            return "";
        };

        if (d.opcode == isa::OP_NOP) return "nop";
        if (d.opcode == isa::OP_RET) return "ret";
        if (d.opcode == isa::OP_RETZ) return "retz";

        if (d.opcode == isa::OP_B || d.opcode == isa::OP_BEQ || d.opcode == isa::OP_BGT || d.opcode == isa::OP_CALL) {
            std::string opName;
            if (d.opcode == isa::OP_B) opName = "b";
            else if (d.opcode == isa::OP_BEQ) opName = "beq";
            else if (d.opcode == isa::OP_BGT) opName = "bgt";
            else opName = "call";

            oss << opName << " ";
            if (d.branchOffset >= 0) oss << "+" << (d.branchOffset * 4);
            else oss << (d.branchOffset * 4);
            oss << " [target: " << toHex(d.branchTarget) << "]";
            return oss.str();
        }

        if (d.opcode == isa::OP_LD) {
            oss << "ld " << getReg(d.rd) << ", " << d.imm16 << "[" << getReg(d.rs1) << "]";
            return oss.str();
        }

        if (d.opcode == isa::OP_ST) {
            oss << "st " << getReg(d.rd) << ", " << d.imm16 << "[" << getReg(d.rs1) << "]";
            return oss.str();
        }

        if (d.opcode == isa::OP_MOV) {
            oss << "mov" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
            if (d.isImmediate) oss << d.imm16;
            else oss << getReg(d.rs2);
            return oss.str();
        }

        if (d.opcode == isa::OP_NOT) {
            oss << "not" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
            if (d.isImmediate) oss << d.imm16;
            else oss << getReg(d.rs2);
            return oss.str();
        }

        if (d.opcode == isa::OP_ABS) {
            oss << "abs" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
            if (d.isImmediate) oss << d.imm16;
            else oss << getReg(d.rs2);
            return oss.str();
        }

        if (d.opcode == isa::OP_CMP) {
            oss << "cmp" << getImmSuffix(d.modifier) << " " << getReg(d.rs1) << ", ";
            if (d.isImmediate) oss << d.imm16;
            else oss << getReg(d.rs2);
            return oss.str();
        }

        std::string opName;
        if (d.opcode == isa::OP_ADD) opName = "add";
        else if (d.opcode == isa::OP_SUB) opName = "sub";
        else if (d.opcode == isa::OP_MUL) opName = "mul";
        else if (d.opcode == isa::OP_DIV) opName = "div";
        else if (d.opcode == isa::OP_MOD) opName = "mod";
        else if (d.opcode == isa::OP_AND) opName = "and";
        else if (d.opcode == isa::OP_OR)  opName = "or";
        else if (d.opcode == isa::OP_XOR) opName = "xor";
        else if (d.opcode == isa::OP_LSL) opName = "lsl";
        else if (d.opcode == isa::OP_LSR) opName = "lsr";
        else if (d.opcode == isa::OP_ASR) opName = "asr";
        else if (d.opcode == isa::OP_MIN) opName = "min";
        else if (d.opcode == isa::OP_MAX) opName = "max";
        else if (d.opcode == isa::OP_ROTS) opName = "rots";
        else return "unknown (" + toHex(word) + ")";

        oss << opName << getImmSuffix(d.modifier) << " "
            << getReg(d.rd) << ", " << getReg(d.rs1) << ", ";
        if (d.isImmediate) oss << d.imm16;
        else oss << getReg(d.rs2);
        return oss.str();
    }

    static Word parseHexString(const std::string& hex) {
        std::string s = MacroPreprocessor::trim(hex);
        if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) s = s.substr(2);
        return static_cast<Word>(std::stoul(s, nullptr, 16));
    }
};

} // namespace assembler
} // namespace risc201

#endif // RISC201_DISASSEMBLER_HPP
