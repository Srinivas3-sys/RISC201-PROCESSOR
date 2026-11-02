#ifndef RISC201_STAGE_REGISTERS_6_HPP
#define RISC201_STAGE_REGISTERS_6_HPP

#include "../common/Types.hpp"
#include <string>

namespace risc201 {
namespace stage6 {

/**
 * Pipeline Registers (Latches) for the 6-Stage Processor:
 * Stages: IF -> ID -> RF -> EX -> MA -> RW
 *
 * Latches:
 *   1. Latch_IF_ID : Between Instruction Fetch and Instruction Decode
 *   2. Latch_ID_RF : Between Instruction Decode and Register Fetch
 *   3. Latch_RF_EX : Between Register Fetch and Execute
 *   4. Latch_EX_MA : Between Execute and Memory Access
 *   5. Latch_MA_RW : Between Memory Access and Register Writeback
 */

// Latch 1: IF -> ID
struct Latch_IF_ID {
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

// Latch 2: ID -> RF
struct Latch_ID_RF {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word immx{0};          // 32-bit expanded immediate
    Word branchTarget{0};  // PC + (offset << 2)
    bool isImmediate{false};
    bool isBubble{true};
    std::string disassembly{"nop"};
    uint64_t instId{0};

    void reset() {
        pc = 0;
        instruction = 0;
        decoded = DecodedInst{};
        immx = branchTarget = 0;
        isImmediate = false;
        isBubble = true;
        disassembly = "nop";
        instId = 0;
    }
};

// Latch 3: RF -> EX
struct Latch_RF_EX {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word op1{0};           // First operand read from regFile
    Word op2{0};           // Second operand read from regFile
    Word immx{0};
    Word branchTarget{0};
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

// Latch 4: EX -> MA
struct Latch_EX_MA {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word aluResult{0};     // Computed result or memory address
    Word op2{0};           // Data to write into memory for stores
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

// Latch 5: MA -> RW
struct Latch_MA_RW {
    Word pc{0};
    Word instruction{0};
    DecodedInst decoded{};
    Word aluResult{0};
    Word ldResult{0};      // Data loaded from memory
    bool isBubble{true};
    std::string disassembly{"nop"};
    uint64_t instId{0};

    void reset() {
        pc = 0;
        instruction = 0;
        decoded = DecodedInst{};
        aluResult = ldResult = 0;
        isBubble = true;
        disassembly = "nop";
        instId = 0;
    }
};

} // namespace stage6
} // namespace risc201

#endif // RISC201_STAGE_REGISTERS_6_HPP
