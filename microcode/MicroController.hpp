#ifndef RISC201_MICRO_CONTROLLER_HPP
#define RISC201_MICRO_CONTROLLER_HPP

#include "MicroInstructions.hpp"
#include "HorizontalControl.hpp"
#include "VerticalControl.hpp"
#include "../common/Types.hpp"
#include "../common/RegisterFile.hpp"
#include "../common/Memory.hpp"
#include "../alu/ALU.hpp"
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace risc201 {
namespace microcode {

enum class MicroControlMode {
    HORIZONTAL,
    VERTICAL
};

/**
 * Microprogrammed Control Unit (MCU) and Shared Bus Execution Engine.
 * Fully implements Sarangi Chapter 9 (Sections 9.4 - 9.8).
 * Executes SimpleRisc using microcode stored in Control Memory.
 */
class MicroController {
public:
    // Microcode Control Store (up to 1024 microinstructions)
    std::vector<HorizontalControlWord> controlStoreHorizontal;
    std::vector<VerticalMicroInst> controlStoreVertical;
    std::map<Opcode, uint16_t> opcodeDispatchTable;

    // Execution state
    uint16_t upc{0};
    MicroControlMode mode{MicroControlMode::HORIZONTAL};
    bool instructionFinished{false};
    uint64_t totalMicroCycles{0};

    // Internal micro-registers (Sarangi Table 9.7)
    Word pc{0};
    Word ir{0};
    uint8_t i_bit{0};
    RegId rd{0};
    RegId rs1{0};
    RegId rs2{0};
    Word immx{0};
    Word branchTarget{0};
    RegId regSrc{0};
    Word regData{0};
    Word regVal{0};
    Word A{0};
    Word B{0};
    Flags flags{};
    Word aluResult{0};
    Word mar{0};
    Word mdr{0};
    Word ldResult{0};

    // Internal hardware blocks
    RegisterFile* regFile{nullptr};
    Memory* memory{nullptr};
    alu::ALU* aluUnit{nullptr};

public:
    MicroController(RegisterFile* rf, Memory* mem, alu::ALU* alu)
        : regFile(rf), memory(mem), aluUnit(alu) {
        buildControlStore();
        reset();
    }

    void reset() {
        upc = 0; // Point to .begin preamble
        instructionFinished = false;
        totalMicroCycles = 0;
        pc = 0;
        ir = 0;
        i_bit = 0;
        rd = rs1 = rs2 = 0;
        immx = branchTarget = 0;
        regSrc = 0;
        regData = regVal = 0;
        A = B = 0;
        flags.clear();
        aluResult = mar = mdr = ldResult = 0;
    }

    void setMode(MicroControlMode m) { mode = m; }
    MicroControlMode getMode() const { return mode; }
    uint16_t getMicroPC() const { return upc; }
    void setPC(Word newPC) { pc = newPC; }
    Word getPC() const { return pc; }

    /**
     * Build the microprogram memory table (Sarangi Section 9.6.3 - 9.6.8).
     * Populates both Vertical and Horizontal control stores.
     */
    void buildControlStore() {
        controlStoreHorizontal.clear();
        controlStoreVertical.clear();
        opcodeDispatchTable.clear();

        // Helper lambda to emit microinstructions
        auto emit = [this](MicroOp op, MicroReg d, MicroReg s, int16_t imm, uint16_t tgt, FunctionalArg arg, const std::string& dis) -> uint16_t {
            uint16_t addr = static_cast<uint16_t>(controlStoreVertical.size());
            VerticalMicroInst vi;
            vi.type = op;
            vi.dest = d;
            vi.src = s;
            vi.immediate = imm;
            vi.branchTarget = tgt;
            vi.args = arg;
            vi.disassembly = dis;

            HorizontalControlWord hw = MicroDecoder::decode(vi);

            controlStoreVertical.push_back(vi);
            controlStoreHorizontal.push_back(hw);
            return addr;
        };

        // --- PREAMBLE (Address 0..3) ---
        // .begin:
        // 0: mloadIR
        emit(MicroOp::MLOAD_IR, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mloadIR");
        // 1: mdecode
        emit(MicroOp::MDECODE, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mdecode");
        // 2: madd pc, 4
        emit(MicroOp::MADD, MicroReg::PC, MicroReg::PC, 4, 0, FunctionalArg::NONE, "madd pc, 4");
        // 3: mswitch
        emit(MicroOp::MSWITCH, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mswitch");

        // Helper for 3-address ALU instructions (add, sub, mul, div, mod, and, or, lsl, lsr, asr)
        auto emitAlu3Op = [&](Opcode op, FunctionalArg arg, [[maybe_unused]] const std::string& opName) {
            uint16_t startAddr = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[op] = startAddr;

            // mmov regSrc, rs1, <read>
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS1, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs1, <read>");
            // mmov A, regVal
            emit(MicroOp::MMOV, MicroReg::A, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov A, regVal");
            // mbeq I, 1, .imm
            uint16_t branchIdx = static_cast<uint16_t>(controlStoreVertical.size());
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::I_BIT, 1, branchIdx + 4, FunctionalArg::NONE, "mbeq I, 1, .imm");
            // mmov regSrc, rs2, <read>
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS2, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs2, <read>");
            // mmov B, regVal, <aluop>
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::REG_VAL, 0, 0, arg, "mmov B, regVal, " + functionalArgName(arg));
            // mb .rw
            uint16_t mbRwIdx = static_cast<uint16_t>(controlStoreVertical.size());
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, mbRwIdx + 2, FunctionalArg::NONE, "mb .rw");
            // .imm: mmov B, immx, <aluop>
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::IMMX, 0, 0, arg, "mmov B, immx, " + functionalArgName(arg));
            // .rw: mmov regSrc, rd
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RD, 0, 0, FunctionalArg::NONE, "mmov regSrc, rd");
            // mmov regData, aluResult, <write>
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::ALU_RESULT, 0, 0, FunctionalArg::REG_WRITE, "mmov regData, aluResult, <write>");
            // mb .begin
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        };

        emitAlu3Op(OP_ADD, FunctionalArg::ALU_ADD, "add");
        emitAlu3Op(OP_SUB, FunctionalArg::ALU_SUB, "sub");
        emitAlu3Op(OP_MUL, FunctionalArg::ALU_MUL, "mul");
        emitAlu3Op(OP_DIV, FunctionalArg::ALU_DIV, "div");
        emitAlu3Op(OP_MOD, FunctionalArg::ALU_MOD, "mod");
        emitAlu3Op(OP_AND, FunctionalArg::ALU_AND, "and");
        emitAlu3Op(OP_OR,  FunctionalArg::ALU_OR,  "or");
        emitAlu3Op(OP_LSL, FunctionalArg::ALU_LSL, "lsl");
        emitAlu3Op(OP_LSR, FunctionalArg::ALU_LSR, "lsr");
        emitAlu3Op(OP_ASR, FunctionalArg::ALU_ASR, "asr");

        // --- MOV (Opcode 9) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_MOV] = start;
            // mbeq I, 1, .imm
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::I_BIT, 1, start + 4, FunctionalArg::NONE, "mbeq I, 1, .imm");
            // mmov regSrc, rs2, <read>
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS2, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs2, <read>");
            // mmov regData, regVal
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov regData, regVal");
            // mb .rw
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, start + 5, FunctionalArg::NONE, "mb .rw");
            // .imm: mmov regData, immx
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::IMMX, 0, 0, FunctionalArg::NONE, "mmov regData, immx");
            // .rw: mmov regSrc, rd, <write>
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RD, 0, 0, FunctionalArg::REG_WRITE, "mmov regSrc, rd, <write>");
            // mb .begin
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- NOT (Opcode 8) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_NOT] = start;
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::I_BIT, 1, start + 4, FunctionalArg::NONE, "mbeq I, 1, .imm");
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS2, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs2, <read>");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::REG_VAL, 0, 0, FunctionalArg::ALU_NOT, "mmov B, regVal, <not>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, start + 5, FunctionalArg::NONE, "mb .rw");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::IMMX, 0, 0, FunctionalArg::ALU_NOT, "mmov B, immx, <not>");
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::ALU_RESULT, 0, 0, FunctionalArg::NONE, "mmov regData, aluResult");
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RD, 0, 0, FunctionalArg::REG_WRITE, "mmov regSrc, rd, <write>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- CMP (Opcode 5) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_CMP] = start;
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS1, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs1, <read>");
            emit(MicroOp::MMOV, MicroReg::A, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov A, regVal");
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::I_BIT, 1, start + 6, FunctionalArg::NONE, "mbeq I, 1, .imm");
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS2, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs2, <read>");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::REG_VAL, 0, 0, FunctionalArg::ALU_CMP, "mmov B, regVal, <cmp>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::IMMX, 0, 0, FunctionalArg::ALU_CMP, "mmov B, immx, <cmp>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- NOP (Opcode 13) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_NOP] = start;
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- LD (Opcode 14) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_LD] = start;
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS1, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs1, <read>");
            emit(MicroOp::MMOV, MicroReg::A, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov A, regVal");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::IMMX, 0, 0, FunctionalArg::ALU_ADD, "mmov B, immx, <add>");
            emit(MicroOp::MMOV, MicroReg::MAR, MicroReg::ALU_RESULT, 0, 0, FunctionalArg::MEM_LOAD, "mmov mar, aluResult, <load>");
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::LD_RESULT, 0, 0, FunctionalArg::NONE, "mmov regData, ldResult");
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RD, 0, 0, FunctionalArg::REG_WRITE, "mmov regSrc, rd, <write>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- ST (Opcode 15) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_ST] = start;
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RS1, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rs1, <read>");
            emit(MicroOp::MMOV, MicroReg::A, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov A, regVal");
            emit(MicroOp::MMOV, MicroReg::B, MicroReg::IMMX, 0, 0, FunctionalArg::ALU_ADD, "mmov B, immx, <add>");
            emit(MicroOp::MMOV, MicroReg::MAR, MicroReg::ALU_RESULT, 0, 0, FunctionalArg::NONE, "mmov mar, aluResult");
            emit(MicroOp::MMOV, MicroReg::REG_SRC, MicroReg::RD, 0, 0, FunctionalArg::REG_READ, "mmov regSrc, rd, <read>");
            emit(MicroOp::MMOV, MicroReg::MDR, MicroReg::REG_VAL, 0, 0, FunctionalArg::MEM_STORE, "mmov mdr, regVal, <store>");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- B (Opcode 18) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_B] = start;
            emit(MicroOp::MMOV, MicroReg::PC, MicroReg::BRANCH_TARGET, 0, 0, FunctionalArg::NONE, "mmov pc, branchTarget");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- BEQ (Opcode 16) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_BEQ] = start;
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::FLAGS_E, 1, start + 2, FunctionalArg::NONE, "mbeq flags.E, 1, .branch");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
            emit(MicroOp::MMOV, MicroReg::PC, MicroReg::BRANCH_TARGET, 0, 0, FunctionalArg::NONE, "mmov pc, branchTarget");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- BGT (Opcode 17) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_BGT] = start;
            emit(MicroOp::MBEQ, MicroReg::NONE, MicroReg::FLAGS_GT, 1, start + 2, FunctionalArg::NONE, "mbeq flags.GT, 1, .branch");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
            emit(MicroOp::MMOV, MicroReg::PC, MicroReg::BRANCH_TARGET, 0, 0, FunctionalArg::NONE, "mmov pc, branchTarget");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- CALL (Opcode 19) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_CALL] = start;
            emit(MicroOp::MMOV, MicroReg::REG_DATA, MicroReg::PC, 0, 0, FunctionalArg::NONE, "mmov regData, pc");
            emit(MicroOp::MMOVI, MicroReg::REG_SRC, MicroReg::NONE, 15, 0, FunctionalArg::REG_WRITE, "mmovi regSrc, 15, <write>");
            emit(MicroOp::MMOV, MicroReg::PC, MicroReg::BRANCH_TARGET, 0, 0, FunctionalArg::NONE, "mmov pc, branchTarget");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }

        // --- RET (Opcode 20) ---
        {
            uint16_t start = static_cast<uint16_t>(controlStoreVertical.size());
            opcodeDispatchTable[OP_RET] = start;
            emit(MicroOp::MMOVI, MicroReg::REG_SRC, MicroReg::NONE, 15, 0, FunctionalArg::REG_READ, "mmovi regSrc, 15, <read>");
            emit(MicroOp::MMOV, MicroReg::PC, MicroReg::REG_VAL, 0, 0, FunctionalArg::NONE, "mmov pc, regVal");
            emit(MicroOp::MB, MicroReg::NONE, MicroReg::NONE, 0, 0, FunctionalArg::NONE, "mb .begin");
        }
    }

    /**
     * Step single microinstruction cycle.
     * Returns true if a program instruction has just completed (transition back to upc=0).
     */
    bool stepMicroCycle() {
        if (upc >= controlStoreHorizontal.size()) {
            upc = 0;
        }

        totalMicroCycles++;
        HorizontalControlWord hw;

        if (mode == MicroControlMode::VERTICAL) {
            // In vertical mode, decode microinstruction first
            const auto& vi = controlStoreVertical[upc];
            hw = MicroDecoder::decode(vi);
        } else {
            // In horizontal mode, read control word directly from control memory
            hw = controlStoreHorizontal[upc];
        }

        // 1. Preamble special actions
        if (upc == 0) { // mloadIR: ir <- [pc]
            CpuException ex;
            ir = memory->readWord(pc, &ex);
        } else if (upc == 1) { // mdecode: populate decode registers
            decodeInstruction(ir);
        }

        // 2. Read selected source onto write-bus
        Word writeBusValue = 0;
        if (hw.getSignal(HorizontalControlWord::CB_PC_OUT)) writeBusValue = pc;
        else if (hw.getSignal(HorizontalControlWord::CB_IR_OUT)) writeBusValue = ir;
        else if (hw.getSignal(HorizontalControlWord::CB_I_OUT)) writeBusValue = i_bit;
        else if (hw.getSignal(HorizontalControlWord::CB_RD_OUT)) writeBusValue = rd;
        else if (hw.getSignal(HorizontalControlWord::CB_RS1_OUT)) writeBusValue = rs1;
        else if (hw.getSignal(HorizontalControlWord::CB_RS2_OUT)) writeBusValue = rs2;
        else if (hw.getSignal(HorizontalControlWord::CB_IMMX_OUT)) writeBusValue = immx;
        else if (hw.getSignal(HorizontalControlWord::CB_BRANCH_TGT_OUT)) writeBusValue = branchTarget;
        else if (hw.getSignal(HorizontalControlWord::CB_REG_SRC_OUT)) writeBusValue = regSrc;
        else if (hw.getSignal(HorizontalControlWord::CB_REG_DATA_OUT)) writeBusValue = regData;
        else if (hw.getSignal(HorizontalControlWord::CB_REG_VAL_OUT)) writeBusValue = regVal;
        else if (hw.getSignal(HorizontalControlWord::CB_A_OUT)) writeBusValue = A;
        else if (hw.getSignal(HorizontalControlWord::CB_B_OUT)) writeBusValue = B;
        else if (hw.getSignal(HorizontalControlWord::CB_FLAGS_E_OUT)) writeBusValue = flags.E ? 1 : 0;
        else if (hw.getSignal(HorizontalControlWord::CB_FLAGS_GT_OUT)) writeBusValue = flags.GT ? 1 : 0;
        else if (hw.getSignal(HorizontalControlWord::CB_ALU_RES_OUT)) writeBusValue = aluResult;
        else if (hw.getSignal(HorizontalControlWord::CB_MAR_OUT)) writeBusValue = mar;
        else if (hw.getSignal(HorizontalControlWord::CB_MDR_OUT)) writeBusValue = mdr;
        else if (hw.getSignal(HorizontalControlWord::CB_LD_RES_OUT)) writeBusValue = ldResult;

        // 3. Transfer multiplexer selects data for read-bus
        Word readBusValue = 0;
        uint8_t xferMux = hw.getXferMux();
        if (xferMux == 0) {
            readBusValue = writeBusValue;
        } else if (xferMux == 1) {
            readBusValue = static_cast<Word>(hw.uimm);
        } else if (xferMux == 2) {
            // uadder: adds writeBusValue + uimm
            readBusValue = writeBusValue + static_cast<Word>(hw.uimm);
        }

        // 4. Latch data into enabled destination register
        if (hw.getSignal(HorizontalControlWord::CB_PC_IN)) pc = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_REG_SRC_IN)) regSrc = static_cast<RegId>(readBusValue & 0xF);
        if (hw.getSignal(HorizontalControlWord::CB_REG_DATA_IN)) regData = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_REG_VAL_IN)) regVal = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_A_IN)) A = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_B_IN)) B = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_ALU_RES_IN)) aluResult = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_MAR_IN)) mar = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_MDR_IN)) mdr = readBusValue;
        if (hw.getSignal(HorizontalControlWord::CB_LD_RES_IN)) ldResult = readBusValue;

        // 5. Execute functional unit commands if args specified
        if (hw.args != FunctionalArg::NONE) {
            executeFunctionalArg(hw.args);
        }

        // 6. MicroPC update (µfetch multiplexer)
        uint8_t ufetchMux = hw.getUfetchMux();
        uint16_t nextUpc = upc + 1;

        if (ufetchMux == 0) {
            nextUpc = upc + 1;
        } else if (ufetchMux == 1) {
            nextUpc = hw.ubranchTarget;
        } else if (ufetchMux == 2) {
            // mswitch: jump to start of opcode routine
            Opcode op = static_cast<Opcode>((ir >> 27) & 0x1F);
            auto it = opcodeDispatchTable.find(op);
            if (it != opcodeDispatchTable.end()) {
                nextUpc = it->second;
            } else {
                nextUpc = 0; // Return to begin on invalid
            }
        } else if (ufetchMux == 3) {
            // mbeq condition: compare writeBusValue with uimm
            bool isMBranch = (writeBusValue == static_cast<Word>(hw.uimm));
            if (isMBranch) {
                nextUpc = hw.ubranchTarget;
            } else {
                nextUpc = upc + 1;
            }
        }

        bool finished = (nextUpc == 0);
        upc = nextUpc;
        return finished;
    }

    // Step an entire program instruction via microcode
    void stepInstruction() {
        while (!stepMicroCycle()) {
            // Run until upc wraps back to 0
        }
    }

private:
    void decodeInstruction(Word instWord) {
        i_bit = (instWord >> 26) & 1;
        rd = (instWord >> 22) & 0xF;
        rs1 = (instWord >> 18) & 0xF;
        rs2 = (instWord >> 14) & 0xF;

        // Immediate extraction with modifier
        uint8_t mod = (instWord >> 16) & 0x3;
        int16_t imm = static_cast<int16_t>(instWord & 0xFFFF);
        if (mod == 0b01) { // unsigned 'u'
            immx = static_cast<Word>(static_cast<uint16_t>(imm));
        } else if (mod == 0b10) { // high 'h'
            immx = static_cast<Word>(static_cast<uint16_t>(imm)) << 16;
        } else { // default sign-extended
            immx = static_cast<Word>(static_cast<int32_t>(imm));
        }

        // Branch target: PC + (offset << 2)
        int32_t offset27 = static_cast<int32_t>(instWord & 0x07FFFFFF);
        if (offset27 & 0x04000000) offset27 |= 0xF8000000; // Sign extend 27-bit
        branchTarget = pc + (offset27 << 2);
    }

    void executeFunctionalArg(FunctionalArg arg) {
        if (arg == FunctionalArg::REG_READ) {
            regVal = regFile->readPort1(regSrc);
        } else if (arg == FunctionalArg::REG_WRITE) {
            regFile->writePort(regSrc, regData, true);
        } else if (arg == FunctionalArg::MEM_LOAD) {
            CpuException ex;
            ldResult = memory->readWord(mar, &ex);
        } else if (arg == FunctionalArg::MEM_STORE) {
            CpuException ex;
            memory->writeWord(mar, mdr, &ex);
        } else {
            // ALU operation
            Opcode op = static_cast<Opcode>(static_cast<uint16_t>(arg) & 0x1F);
            auto out = aluUnit->execute(op, A, B, flags);
            aluResult = out.result;
            if (op == OP_CMP) {
                flags = out.flags;
                regFile->setFlags(flags);
            }
        }
    }
};

} // namespace microcode
} // namespace risc201

#endif // RISC201_MICRO_CONTROLLER_HPP
