#ifndef RISC201_TYPES_HPP
#define RISC201_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>

namespace risc201 {

// Fundamental 32-bit architectural types
using Word = uint32_t;
using SignedWord = int32_t;
using HalfWord = uint16_t;
using Byte = uint8_t;
using RegId = uint8_t; // 0 to 15

// Register Constants
constexpr RegId REG_ZERO = 0;   // Often used as scratch or zero in idioms
constexpr RegId REG_SP   = 14;  // r14 is stack pointer (sp)
constexpr RegId REG_RA   = 15;  // r15 is return address (ra)
constexpr size_t NUM_REGISTERS = 16;

// Privileged Registers (Section 10.8)
enum class PrivRegId : uint8_t {
    R0       = 0,  // regular r0
    OLD_PC   = 1,  // oldPC
    OLD_SP   = 2,  // oldSP
    FLAGS    = 3,  // flags
    OLD_FLAGS= 4,  // oldFlags
    SP       = 5   // r14 / sp
};

// SimpleRisc Opcode Definitions (5 bits, 0-31)
// As specified in Sarangi Chapter 3 & 9, Table 3.10 / 9.2
enum Opcode : uint8_t {
    OP_ADD  = 0b00000, // 0:  add  rd, rs1, (rs2/imm)
    OP_SUB  = 0b00001, // 1:  sub  rd, rs1, (rs2/imm)
    OP_MUL  = 0b00010, // 2:  mul  rd, rs1, (rs2/imm)
    OP_DIV  = 0b00011, // 3:  div  rd, rs1, (rs2/imm)
    OP_MOD  = 0b00100, // 4:  mod  rd, rs1, (rs2/imm)
    OP_CMP  = 0b00101, // 5:  cmp  rs1, (rs2/imm)
    OP_AND  = 0b00110, // 6:  and  rd, rs1, (rs2/imm)
    OP_OR   = 0b00111, // 7:  or   rd, rs1, (rs2/imm)
    OP_NOT  = 0b01000, // 8:  not  rd, (rs2/imm)
    OP_MOV  = 0b01001, // 9:  mov  rd, (rs2/imm)
    OP_LSL  = 0b01010, // 10: lsl  rd, rs1, (rs2/imm)
    OP_LSR  = 0b01011, // 11: lsr  rd, rs1, (rs2/imm)
    OP_ASR  = 0b01100, // 12: asr  rd, rs1, (rs2/imm)
    OP_NOP  = 0b01101, // 13: nop
    OP_LD   = 0b01110, // 14: ld   rd, imm[rs1]
    OP_ST   = 0b01111, // 15: st   rd, imm[rs1] (stores rd into [rs1+imm])
    OP_BEQ  = 0b10000, // 16: beq  offset (branch if flags.E == 1)
    OP_BGT  = 0b10001, // 17: bgt  offset (branch if flags.GT == 1)
    OP_B    = 0b10010, // 18: b    offset (unconditional branch)
    OP_CALL = 0b10011, // 19: call offset (ra <- PC + 4; PC <- target)
    OP_RET  = 0b10100, // 20: ret  (PC <- ra)
    // Privileged extension opcodes
    OP_MOVZ = 0b10101, // 21: movz (transfer privileged <-> general registers)
    OP_RETZ = 0b10110, // 22: retz (PC <- oldPC, CPL <- 1)
    OP_MIN  = 0b10111, // 23: min  rd, rs1, (rs2/imm)
    OP_MAX  = 0b11000, // 24: max  rd, rs1, (rs2/imm)
    OP_ROTS = 0b11001, // 25: rots rd, rs1, (rs2/imm)
    OP_XOR  = 0b11010, // 26: xor  rd, rs1, (rs2/imm)
    OP_ABS  = 0b11011, // 27: abs  rd, (rs2/imm)
    OP_INVALID = 0b11111
};

// Immediate Modifier (2 bits, bits 17..16 of immediate field)
// Sarangi Section 3.3.13:
// 00: default sign-extended 16-bit
// 01: 'u' unsigned 16-bit (upper 16 bits zeroed)
// 10: 'h' high 16-bit (shifted left by 16 bits)
enum class ImmModifier : uint8_t {
    DEFAULT = 0b00,
    UNSIGNED = 0b01,
    HIGH     = 0b10,
    RESERVED = 0b11
};

// Instruction Formats (Sarangi Section 3.3.14, Table 3.11)
enum class InstFormat {
    REGISTER,  // 3-address register operand (I=0)
    IMMEDIATE, // 3-address with immediate operand (I=1)
    BRANCH     // 1-address or 0-address branch format (b, beq, bgt, call, ret, nop)
};

// Decoded Instruction Structure
struct DecodedInst {
    Word rawWord{0};
    Opcode opcode{OP_NOP};
    InstFormat format{InstFormat::BRANCH};
    bool isImmediate{false};
    RegId rd{0};
    RegId rs1{0};
    RegId rs2{0};
    ImmModifier modifier{ImmModifier::DEFAULT};
    int16_t imm16{0};
    Word immx{0};           // Expanded 32-bit immediate
    int32_t branchOffset{0}; // Signed 27-bit word offset
    Word branchTarget{0};   // Computed branch target = PC + (offset << 2)

    // Helper query functions
    bool isBranch() const {
        return opcode == OP_B || opcode == OP_BEQ || opcode == OP_BGT ||
               opcode == OP_CALL || opcode == OP_RET || opcode == OP_RETZ;
    }

    bool isConditionalBranch() const {
        return opcode == OP_BEQ || opcode == OP_BGT;
    }

    bool isCall() const {
        return opcode == OP_CALL;
    }

    bool isRet() const {
        return opcode == OP_RET || opcode == OP_RETZ;
    }

    bool isLoad() const {
        return opcode == OP_LD;
    }

    bool isStore() const {
        return opcode == OP_ST;
    }

    bool isNop() const {
        return opcode == OP_NOP;
    }

    // Does this instruction write back to the register file?
    // Sarangi Section 9.3: isWb is true for ALU, mov, ld, and call (writes ra)
    bool writesRegister() const {
        if (opcode == OP_NOP || opcode == OP_ST || opcode == OP_CMP ||
            opcode == OP_B || opcode == OP_BEQ || opcode == OP_BGT || opcode == OP_RET || opcode == OP_RETZ) {
            return false;
        }
        return true;
    }

    // Destination register id (ra for CALL, rd for others)
    RegId getDestReg() const {
        if (opcode == OP_CALL) return REG_RA;
        return rd;
    }

    // Does instruction read rs1? (Not used for NOT, MOV, or branch formats except RET)
    bool readsRs1() const {
        if (opcode == OP_NOP || opcode == OP_B || opcode == OP_BEQ ||
            opcode == OP_BGT || opcode == OP_CALL || opcode == OP_NOT || opcode == OP_MOV || opcode == OP_ABS) {
            return false;
        }
        // RET reads ra (REG_RA = 15)
        return true;
    }

    // Does instruction read rs2? (Only when not immediate, and not store which reads rd as source data)
    bool readsRs2() const {
        if (isImmediate) return false;
        if (opcode == OP_NOP || opcode == OP_B || opcode == OP_BEQ ||
            opcode == OP_BGT || opcode == OP_CALL || opcode == OP_RET || opcode == OP_RETZ ||
            opcode == OP_LD) {
            return false;
        }
        return true; // add, sub, mul, div, mod, cmp, and, or, lsl, lsr, asr, not, mov
    }

    // ST reads rd as data to store!
    bool readsRd() const {
        return opcode == OP_ST;
    }
};

// Flags Register Structure (Sarangi Section 3.3.2 & 9.2.4)
struct Flags {
    bool E{false};  // Equal flag
    bool GT{false}; // Greater-Than flag

    void clear() { E = false; GT = false; }
    bool isEqual() const { return E; }
    bool isGreater() const { return GT; }

    Word toWord() const {
        return (E ? 1u : 0u) | (GT ? 2u : 0u);
    }

    void fromWord(Word w) {
        E = (w & 1u) != 0;
        GT = (w & 2u) != 0;
    }
};

// Convert Word to hex string "0x00000000"
inline std::string toHex(Word val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << val;
    return oss.str();
}

} // namespace risc201

#endif // RISC201_TYPES_HPP
