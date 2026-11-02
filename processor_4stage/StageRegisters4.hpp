#ifndef RISC201_STAGE_REGISTERS_4_HPP
#define RISC201_STAGE_REGISTERS_4_HPP

#include "../common/Types.hpp"
#include <string>

namespace risc201 {
namespace stage4 {

/**
 * Pipeline Registers (Latches) for the 4-Stage Processor:
 * Stages: IF -> OF -> EX -> MA_RW
 * Latches:
 *   1. IF_OF: Between Instruction Fetch and Operand Fetch
 *   2. OF_EX: Between Operand Fetch and Execute
 *   3. EX_MARW: Between Execute and Memory-Access/Writeback
 */

// Latch 1: IF -> OF
struct Latch_IF_OF {
    Word pc{0};
    Word instruction{0};
    bool isBubble{true};
    std::string disassembly{"nop"};
    uint64_t instId{0};

    void reset() {
        pc = 0;
        instruction = 0;
        isBubble = true;
        disassembly = "nop";
        instId = 0;
    }
};

// Latch 2: OF -> EX
struct Latch_OF_EX {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word op1{0};           // First register operand (rs1 or ra)
    Word op2{0};           // Second register operand (rs2 or rd for store)
    Word immx{0};          // Sign-extended/modified immediate
    Word branchTarget{0};  // Precomputed branch target PC + (offset << 2)
    bool isImmediate{false};
    bool isBubble{true};
    std::string disassembly{"nop"};
    uint64_t instId{0};

    void reset() {
        pc = 0;
        instruction = 0;
        decoded = DecodedInst{};
        op1 = op2 = immx = branchTarget = 0;
        isImmediate = false;
        isBubble = true;
        disassembly = "nop";
        instId = 0;
    }
};

// Latch 3: EX -> MA_RW
struct Latch_EX_MARW {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word aluResult{0};     // ALU result or computed memory address
    Word op2{0};           // Value to store into memory (for store instruction)
    bool isBubble{true};
    std::string disassembly{"nop"};
    uint64_t instId{0};

    void reset() {
        pc = 0;
        instruction = 0;
        decoded = DecodedInst{};
        aluResult = op2 = 0;
        isBubble = true;
        disassembly = "nop";
        instId = 0;
    }
};

} // namespace stage4
} // namespace risc201

#endif // RISC201_STAGE_REGISTERS_4_HPP
