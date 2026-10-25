#ifndef RISC201_VERTICAL_CONTROL_HPP
#define RISC201_VERTICAL_CONTROL_HPP

#include "MicroInstructions.hpp"
#include "HorizontalControl.hpp"
#include <cstdint>

namespace risc201 {
namespace microcode {

/**
 * 45-bit Vertical Microinstruction (Sarangi Section 9.8.1, Figure 9.24).
 * Encoded format:
 *   [44..42]: 3 bits  - type (MicroOp)
 *   [41..37]: 5 bits  - src register (MicroReg)
 *   [36..32]: 5 bits  - dest register (MicroReg)
 *   [31..20]: 12 bits - immediate (signed)
 *   [19..10]: 10 bits - branchTarget (microPC)
 *   [9..0]:   10 bits - args (FunctionalArg)
 */
struct VerticalMicroInst {
    MicroOp type{MicroOp::MLOAD_IR};
    MicroReg src{MicroReg::NONE};
    MicroReg dest{MicroReg::NONE};
    int16_t immediate{0};
    uint16_t branchTarget{0};
    FunctionalArg args{FunctionalArg::NONE};
    std::string disassembly;

    // Pack into a 64-bit container (using 45 bits)
    uint64_t pack() const {
        uint64_t word = 0;
        word |= (static_cast<uint64_t>(type) & 0x7) << 42;
        word |= (static_cast<uint64_t>(src) & 0x1F) << 37;
        word |= (static_cast<uint64_t>(dest) & 0x1F) << 32;
        word |= (static_cast<uint64_t>(immediate) & 0xFFF) << 20;
        word |= (static_cast<uint64_t>(branchTarget) & 0x3FF) << 10;
        word |= (static_cast<uint64_t>(args) & 0x3FF);
        return word;
    }

    void unpack(uint64_t word) {
        type = static_cast<MicroOp>((word >> 42) & 0x7);
        src = static_cast<MicroReg>((word >> 37) & 0x1F);
        dest = static_cast<MicroReg>((word >> 32) & 0x1F);
        int16_t imm = static_cast<int16_t>((word >> 20) & 0xFFF);
        if (imm & 0x800) imm |= 0xF000; // Sign extend 12-bit
        immediate = imm;
        branchTarget = static_cast<uint16_t>((word >> 10) & 0x3FF);
        args = static_cast<FunctionalArg>(word & 0x3FF);
    }
};

/**
 * MicroDecoder translates vertically-encoded microinstructions into
 * equivalent horizontal control signal vectors (Sarangi Section 9.8.1).
 */
class MicroDecoder {
public:
    static HorizontalControlWord decode(const VerticalMicroInst& vinst) {
        HorizontalControlWord hword;
        hword.uimm = vinst.immediate;
        hword.ubranchTarget = vinst.branchTarget;
        hword.args = vinst.args;
        hword.disassembly = vinst.disassembly;

        // Map source register to write-bus enable
        MicroReg srcReg = vinst.src;
        MicroReg destReg = vinst.dest;
        if (vinst.type == MicroOp::MBEQ) {
            if (srcReg == MicroReg::NONE) {
                srcReg = destReg;
            }
            destReg = MicroReg::NONE; // Conditional microbranch does not write back to destination register
        }

        switch (srcReg) {
            case MicroReg::PC: hword.setSignal(HorizontalControlWord::CB_PC_OUT); break;
            case MicroReg::IR: hword.setSignal(HorizontalControlWord::CB_IR_OUT); break;
            case MicroReg::I_BIT: hword.setSignal(HorizontalControlWord::CB_I_OUT); break;
            case MicroReg::RD: hword.setSignal(HorizontalControlWord::CB_RD_OUT); break;
            case MicroReg::RS1: hword.setSignal(HorizontalControlWord::CB_RS1_OUT); break;
            case MicroReg::RS2: hword.setSignal(HorizontalControlWord::CB_RS2_OUT); break;
            case MicroReg::IMMX: hword.setSignal(HorizontalControlWord::CB_IMMX_OUT); break;
            case MicroReg::BRANCH_TARGET: hword.setSignal(HorizontalControlWord::CB_BRANCH_TGT_OUT); break;
            case MicroReg::REG_SRC: hword.setSignal(HorizontalControlWord::CB_REG_SRC_OUT); break;
            case MicroReg::REG_DATA: hword.setSignal(HorizontalControlWord::CB_REG_DATA_OUT); break;
            case MicroReg::REG_VAL: hword.setSignal(HorizontalControlWord::CB_REG_VAL_OUT); break;
            case MicroReg::A: hword.setSignal(HorizontalControlWord::CB_A_OUT); break;
            case MicroReg::B: hword.setSignal(HorizontalControlWord::CB_B_OUT); break;
            case MicroReg::FLAGS_E: hword.setSignal(HorizontalControlWord::CB_FLAGS_E_OUT); break;
            case MicroReg::FLAGS_GT: hword.setSignal(HorizontalControlWord::CB_FLAGS_GT_OUT); break;
            case MicroReg::ALU_RESULT: hword.setSignal(HorizontalControlWord::CB_ALU_RES_OUT); break;
            case MicroReg::MAR: hword.setSignal(HorizontalControlWord::CB_MAR_OUT); break;
            case MicroReg::MDR: hword.setSignal(HorizontalControlWord::CB_MDR_OUT); break;
            case MicroReg::LD_RESULT: hword.setSignal(HorizontalControlWord::CB_LD_RES_OUT); break;
            default: break;
        }

        // Map dest register to read-bus enable
        switch (destReg) {
            case MicroReg::PC: hword.setSignal(HorizontalControlWord::CB_PC_IN); break;
            case MicroReg::REG_SRC: hword.setSignal(HorizontalControlWord::CB_REG_SRC_IN); break;
            case MicroReg::REG_DATA: hword.setSignal(HorizontalControlWord::CB_REG_DATA_IN); break;
            case MicroReg::REG_VAL: hword.setSignal(HorizontalControlWord::CB_REG_VAL_IN); break;
            case MicroReg::A: hword.setSignal(HorizontalControlWord::CB_A_IN); break;
            case MicroReg::B: hword.setSignal(HorizontalControlWord::CB_B_IN); break;
            case MicroReg::ALU_RESULT: hword.setSignal(HorizontalControlWord::CB_ALU_RES_IN); break;
            case MicroReg::MAR: hword.setSignal(HorizontalControlWord::CB_MAR_IN); break;
            case MicroReg::MDR: hword.setSignal(HorizontalControlWord::CB_MDR_IN); break;
            case MicroReg::LD_RESULT: hword.setSignal(HorizontalControlWord::CB_LD_RES_IN); break;
            default: break;
        }

        // Transfer multiplexer & ufetch multiplexer configuration based on micro-opcode
        switch (vinst.type) {
            case MicroOp::MLOAD_IR:
            case MicroOp::MDECODE:
                hword.setUfetchMux(0); // next upc
                break;

            case MicroOp::MSWITCH:
                hword.setUfetchMux(2); // switch unit based on instruction opcode
                break;

            case MicroOp::MMOV:
                hword.setXferMux(0);   // writeBus
                hword.setUfetchMux(0); // next upc
                break;

            case MicroOp::MMOVI:
                hword.setXferMux(1);   // uimm
                hword.setUfetchMux(0); // next upc
                break;

            case MicroOp::MADD:
                hword.setXferMux(2);   // uadder output
                hword.setUfetchMux(0); // next upc
                break;

            case MicroOp::MB:
                hword.setUfetchMux(1); // unconditional micro-branch target
                break;

            case MicroOp::MBEQ:
                hword.setUfetchMux(3); // conditional M1 mux (branchTarget if isMBranch==1 else next upc)
                break;
        }

        return hword;
    }
};

} // namespace microcode
} // namespace risc201

#endif // RISC201_VERTICAL_CONTROL_HPP
