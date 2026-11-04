#ifndef RISC201_MICRO_INSTRUCTIONS_HPP
#define RISC201_MICRO_INSTRUCTIONS_HPP

#include "../common/Types.hpp"
#include <string>

namespace risc201 {
namespace microcode {

// 19 Internal Micro-Registers visible to microassembly (Sarangi Table 9.7)
enum class MicroReg : uint8_t {
    NONE = 0,
    PC,
    IR,
    I_BIT,
    RD,
    RS1,
    RS2,
    IMMX,
    BRANCH_TARGET,
    REG_SRC,
    REG_DATA,
    REG_VAL,
    A,
    B,
    FLAGS_E,
    FLAGS_GT,
    ALU_RESULT,
    MAR,
    MDR,
    LD_RESULT
};

inline std::string microRegName(MicroReg r) {
    switch (r) {
        case MicroReg::PC: return "pc";
        case MicroReg::IR: return "ir";
        case MicroReg::I_BIT: return "I";
        case MicroReg::RD: return "rd";
        case MicroReg::RS1: return "rs1";
        case MicroReg::RS2: return "rs2";
        case MicroReg::IMMX: return "immx";
        case MicroReg::BRANCH_TARGET: return "branchTarget";
        case MicroReg::REG_SRC: return "regSrc";
        case MicroReg::REG_DATA: return "regData";
        case MicroReg::REG_VAL: return "regVal";
        case MicroReg::A: return "A";
        case MicroReg::B: return "B";
        case MicroReg::FLAGS_E: return "flags.E";
        case MicroReg::FLAGS_GT: return "flags.GT";
        case MicroReg::ALU_RESULT: return "aluResult";
        case MicroReg::MAR: return "mar";
        case MicroReg::MDR: return "mdr";
        case MicroReg::LD_RESULT: return "ldResult";
        default: return "none";
    }
}

// 8 Microinstruction Types (Sarangi Section 9.6.2, Table 9.8)
enum class MicroOp : uint8_t {
    MLOAD_IR = 0, // 000: ir <- [pc]
    MDECODE  = 1, // 001: populate decode registers
    MSWITCH  = 2, // 010: jump to microPC corresponding to opcode
    MMOV     = 3, // 011: reg1 <- reg2, optionally send args
    MMOVI    = 4, // 100: reg1 <- imm, optionally send args
    MADD     = 5, // 101: reg1 <- reg1 + imm, optionally send args
    MBEQ     = 6, // 110: if (reg1 == imm) microPC <- addr
    MB       = 7  // 111: microPC <- addr
};

// Functional Unit Arguments (args on the shared bus - Section 9.7.2)
// 10-bit args: 3-bit unit id, 7-bit unit-specific opcode
enum class UnitId : uint8_t {
    NONE     = 0,
    ALU      = 1,
    REG_FILE = 2,
    MEMORY   = 3
};

enum class FunctionalArg : uint16_t {
    NONE = 0,
    // Register file args
    REG_READ  = (2 << 7) | 1,
    REG_WRITE = (2 << 7) | 2,
    // Memory unit args
    MEM_LOAD  = (3 << 7) | 1,
    MEM_STORE = (3 << 7) | 2,
    // ALU args (decoupled bit tags preserving control word bit layout)
    ALU_ADD = (1 << 7) | 0,
    ALU_SUB = (1 << 7) | 1,
    ALU_MUL = (1 << 7) | 2,
    ALU_DIV = (1 << 7) | 3,
    ALU_MOD = (1 << 7) | 4,
    ALU_CMP = (1 << 7) | 5,
    ALU_AND = (1 << 7) | 6,
    ALU_OR  = (1 << 7) | 7,
    ALU_NOT = (1 << 7) | 8,
    ALU_MOV = (1 << 7) | 9,
    ALU_LSL = (1 << 7) | 10,
    ALU_LSR = (1 << 7) | 11,
    ALU_ASR = (1 << 7) | 12
};

inline std::string functionalArgName(FunctionalArg arg) {
    switch (arg) {
        case FunctionalArg::REG_READ: return "<read>";
        case FunctionalArg::REG_WRITE: return "<write>";
        case FunctionalArg::MEM_LOAD: return "<load>";
        case FunctionalArg::MEM_STORE: return "<store>";
        case FunctionalArg::ALU_ADD: return "<add>";
        case FunctionalArg::ALU_SUB: return "<sub>";
        case FunctionalArg::ALU_MUL: return "<mul>";
        case FunctionalArg::ALU_DIV: return "<div>";
        case FunctionalArg::ALU_MOD: return "<mod>";
        case FunctionalArg::ALU_CMP: return "<cmp>";
        case FunctionalArg::ALU_AND: return "<and>";
        case FunctionalArg::ALU_OR: return "<or>";
        case FunctionalArg::ALU_NOT: return "<not>";
        case FunctionalArg::ALU_MOV: return "<mov>";
        case FunctionalArg::ALU_LSL: return "<lsl>";
        case FunctionalArg::ALU_LSR: return "<lsr>";
        case FunctionalArg::ALU_ASR: return "<asr>";
        default: return "";
    }
}

} // namespace microcode
} // namespace risc201

#endif // RISC201_MICRO_INSTRUCTIONS_HPP
