#ifndef RISC201_COMMON_ISS_HPP
#define RISC201_COMMON_ISS_HPP

#include "ISA.hpp"
#include "RegisterFile.hpp"
#include "../alu/ALU.hpp"
#include "../memory/MemoryHierarchy.hpp"
#include <cstdint>
#include <vector>
#include <iostream>

namespace risc201 {

class InstructionSetSimulator {
public:
    RegisterFile regFile;
    alu::ALU alu;
    memory::MemoryHierarchy mem;

    uint32_t pc{0};
    uint32_t programSize{0};
    bool halted{false};
    uint64_t retiredInstructions{0};
    uint64_t cycles{0};

    explicit InstructionSetSimulator(const memory::MemorySystemConfig& memCfg = memory::getPreset("ideal"))
        : mem(memCfg) {
        reset();
    }

    void reset() {
        regFile.reset();
        mem.reset();
        pc = 0;
        programSize = 0;
        halted = false;
        retiredInstructions = 0;
        cycles = 0;
    }

    void setProgramSize(uint32_t sz) { programSize = sz; }

    bool step() {
        if (halted || (programSize > 0 && pc >= programSize)) {
            halted = true;
            return false;
        }

        // Fetch instruction
        auto fetchRes = mem.readWord(pc, true, pc, cycles);
        uint32_t word = std::get<0>(fetchRes);
        uint32_t fetchLat = std::get<1>(fetchRes);
        cycles += std::max(1U, fetchLat);

        Instruction inst;
        try {
            inst = decode(word, pc);
        } catch (...) {
            halted = true;
            return false;
        }

        uint32_t nextPc = pc + 4;
        bool branchTaken = false;

        // Operand preparation
        uint32_t op1 = inst.readsRs1() ? regFile.read(inst.rs1) : 0;
        uint32_t op2 = inst.isImmediate ? inst.immx : (inst.readsRs2() ? regFile.read(inst.rs2) : 0);

        // Branch operations
        if (inst.opcode == isa::OP_B) {
            nextPc = inst.branchTarget;
            branchTaken = true;
        } else if (inst.opcode == isa::OP_BEQ) {
            if (regFile.getFlags().isEqual()) {
                nextPc = inst.branchTarget;
                branchTaken = true;
            }
        } else if (inst.opcode == isa::OP_BGT) {
            if (regFile.getFlags().isGreater()) {
                nextPc = inst.branchTarget;
                branchTaken = true;
            }
        } else if (inst.opcode == isa::OP_CALL) {
            regFile.write(REG_ID_RA, pc + 4);
            nextPc = inst.branchTarget;
            branchTaken = true;
        } else if (inst.opcode == isa::OP_RET) {
            nextPc = regFile.read(REG_ID_RA);
            branchTaken = true;
        } else if (inst.opcode == isa::OP_LD) {
            uint32_t effAddr = op1 + inst.immx;
            auto ldRes = mem.readWord(effAddr, false, pc, cycles);
            uint32_t loadedVal = std::get<0>(ldRes);
            uint32_t memLat = std::get<1>(ldRes);
            cycles += std::max(1U, memLat);
            regFile.write(inst.rd, loadedVal);
        } else if (inst.opcode == isa::OP_ST) {
            uint32_t effAddr = op1 + inst.immx;
            uint32_t storeData = regFile.read(inst.rd);
            uint32_t memLat = mem.writeWord(effAddr, storeData, pc, cycles);
            cycles += std::max(1U, memLat);
        } else if (inst.opcode == isa::OP_NOP) {
            // Nop
        } else {
            // ALU instruction
            auto out = alu.execute(inst.opcode, op1, op2);
            if (inst.opcode == isa::OP_CMP) {
                regFile.setFlags(out.flags);
            } else if (inst.writesRegister()) {
                regFile.write(inst.rd, out.result);
            }
        }

        (void)branchTaken;
        pc = nextPc;
        retiredInstructions++;
        return true;
    }

    void run(uint64_t maxInstructions = 1000000) {
        while (!halted && retiredInstructions < maxInstructions) {
            if (!step()) break;
        }
    }
};

} // namespace risc201

#endif // RISC201_COMMON_ISS_HPP