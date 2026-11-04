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
        d.opcode = static_cast<Opcode>((word >> 27) & 0x1F);

        // Branch format check: branch opcodes have MSB bit = 1 or opcode == NOP
        // (Sarangi Section 3.3.14, Table 3.10)
        // b (18), beq (16), bgt (17), call (19), ret (20), nop (13)
        if (d.opcode == OP_B || d.opcode == OP_BEQ || d.opcode == OP_BGT ||
            d.opcode == OP_CALL || d.opcode == OP_RET || d.opcode == OP_NOP ||
            d.opcode == OP_RETZ) {
            d.format = InstFormat::BRANCH;
            if (d.opcode == OP_RET) {
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

        switch (d.opcode) {
            case OP_NOP: return "nop";
            case OP_RET: return "ret";
            case OP_RETZ: return "retz";

            case OP_B:
            case OP_BEQ:
            case OP_BGT:
            case OP_CALL: {
                std::string opName;
                if (d.opcode == OP_B) opName = "b";
                else if (d.opcode == OP_BEQ) opName = "beq";
                else if (d.opcode == OP_BGT) opName = "bgt";
                else opName = "call";

                oss << opName << " ";
                if (d.branchOffset >= 0) oss << "+" << (d.branchOffset * 4);
                else oss << (d.branchOffset * 4);
                oss << " [target: " << toHex(d.branchTarget) << "]";
                return oss.str();
            }

            case OP_LD:
                oss << "ld " << getReg(d.rd) << ", " << d.imm16 << "[" << getReg(d.rs1) << "]";
                return oss.str();

            case OP_ST:
                oss << "st " << getReg(d.rd) << ", " << d.imm16 << "[" << getReg(d.rs1) << "]";
                return oss.str();

            case OP_MOV:
                oss << "mov" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
                if (d.isImmediate) oss << d.imm16;
                else oss << getReg(d.rs2);
                return oss.str();

            case OP_NOT:
                oss << "not" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
                if (d.isImmediate) oss << d.imm16;
                else oss << getReg(d.rs2);
                return oss.str();

            case OP_ABS:
                oss << "abs" << getImmSuffix(d.modifier) << " " << getReg(d.rd) << ", ";
                if (d.isImmediate) oss << d.imm16;
                else oss << getReg(d.rs2);
                return oss.str();

            case OP_CMP:
                oss << "cmp" << getImmSuffix(d.modifier) << " " << getReg(d.rs1) << ", ";
                if (d.isImmediate) oss << d.imm16;
                else oss << getReg(d.rs2);
                return oss.str();

            default: {
                std::string opName;
                switch (d.opcode) {
                    case OP_ADD: opName = "add"; break;
                    case OP_SUB: opName = "sub"; break;
                    case OP_MUL: opName = "mul"; break;
                    case OP_DIV: opName = "div"; break;
                    case OP_MOD: opName = "mod"; break;
                    case OP_AND: opName = "and"; break;
                    case OP_OR:  opName = "or"; break;
                    case OP_XOR: opName = "xor"; break;
                    case OP_LSL: opName = "lsl"; break;
                    case OP_LSR: opName = "lsr"; break;
                    case OP_ASR: opName = "asr"; break;
                    default: return "unknown (" + toHex(word) + ")";
                }
                oss << opName << getImmSuffix(d.modifier) << " "
                    << getReg(d.rd) << ", " << getReg(d.rs1) << ", ";
                if (d.isImmediate) oss << d.imm16;
                else oss << getReg(d.rs2);
                return oss.str();
            }
        }
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
