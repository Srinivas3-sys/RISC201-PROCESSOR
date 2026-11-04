#ifndef RISC201_ALU_HPP
#define RISC201_ALU_HPP

#include "LogicGates.hpp"
#include "Adders.hpp"
#include "Multipliers.hpp"
#include "Dividers.hpp"
#include "Shifter.hpp"
#include "../common/Types.hpp"
#include "../common/ISA.hpp"
#include "../common/Exception.hpp"

namespace risc201 {
namespace alu {

// Supported hardware algorithms for evaluation (Requirement 5)
enum class AdderAlgorithm {
    RIPPLE_CARRY,
    CARRY_SELECT,
    CARRY_LOOKAHEAD
};

enum class MultiplierAlgorithm {
    ITERATIVE,
    BOOTH,
    WALLACE_TREE
};

struct AluConfig {
    AdderAlgorithm adderAlgo{AdderAlgorithm::CARRY_LOOKAHEAD};
    MultiplierAlgorithm mulAlgo{MultiplierAlgorithm::BOOTH};
    DividerAlgorithm divAlgo{DividerAlgorithm::NON_RESTORING};
};

struct AluOutput {
    Word result{0};
    Flags flags{};
    bool overflow{false};
    bool carryOut{false};
    CpuException exception{};
};

/**
 * RISC201 Optimized ALU Class.
 * Coordinates all arithmetic, shift, and logical operations.
 * Implements configurable underlying algorithms for performance and hardware comparison.
 */
class ALU {
private:
    AluConfig config;

public:
    explicit ALU(AluConfig cfg = {}) : config(cfg) {}

    void setConfig(const AluConfig& cfg) { config = cfg; }
    const AluConfig& getConfig() const { return config; }

    /**
     * Execute an ALU operation given inputs A, B, and canonical 5-bit opcode string.
     * Note: flags are only updated if opcode == isa::OP_CMP.
     */
    AluOutput execute(const std::string& opcode, Word a, Word b, const Flags& currentFlags = {}) {
        AluOutput out;
        out.flags = currentFlags; // Default: maintain flags unless cmp

        if (opcode == isa::OP_ADD || opcode == isa::OP_LD || opcode == isa::OP_ST) {
            // Load/Store compute effective address via adder (rs1 + imm)
            uint8_t cout = 0;
            bool ovf = false;
            if (config.adderAlgo == AdderAlgorithm::RIPPLE_CARRY) {
                std::tie(out.result, cout, ovf) = rippleCarryAdd(a, b, 0);
            } else if (config.adderAlgo == AdderAlgorithm::CARRY_SELECT) {
                std::tie(out.result, cout, ovf) = carrySelectAdd(a, b, 0);
            } else {
                std::tie(out.result, cout, ovf) = carryLookaheadAdd(a, b, 0);
            }
            out.carryOut = (cout != 0);
            out.overflow = ovf;
        } else if (opcode == isa::OP_SUB) {
            uint8_t cout = 0;
            bool ovf = false;
            std::tie(out.result, cout, ovf) = subtract32(a, b);
            out.carryOut = (cout != 0);
            out.overflow = ovf;
        } else if (opcode == isa::OP_CMP) {
            // cmp performs subtraction (A - B) and sets flags.E and flags.GT (Sarangi Section 3.3.4 & 9.2.4)
            auto subRes = subtract32(a, b);
            Word diff = std::get<0>(subRes);
            out.result = diff;
            out.flags.E = (diff == 0);
            // Signed comparison: A > B if (diff > 0) with respect to 2's complement
            int32_t sa = static_cast<int32_t>(a);
            int32_t sb = static_cast<int32_t>(b);
            out.flags.GT = (sa > sb);
        } else if (opcode == isa::OP_MUL) {
            uint64_t prod = 0;
            if (config.mulAlgo == MultiplierAlgorithm::ITERATIVE) {
                prod = iterativeMultiply(a, b);
            } else if (config.mulAlgo == MultiplierAlgorithm::WALLACE_TREE) {
                prod = wallaceTreeMultiply(a, b);
            } else {
                prod = boothMultiply(a, b);
            }
            out.result = static_cast<Word>(prod & 0xFFFFFFFF);
        } else if (opcode == isa::OP_DIV) {
            if (b == 0) {
                out.exception.type = ExceptionType::DIVISION_BY_ZERO;
                out.exception.details = "Division by zero in ALU";
                out.result = 0;
            } else {
                out.result = std::get<0>(divideSigned(a, b, config.divAlgo));
            }
        } else if (opcode == isa::OP_MOD) {
            if (b == 0) {
                out.exception.type = ExceptionType::DIVISION_BY_ZERO;
                out.exception.details = "Modulo by zero in ALU";
                out.result = 0;
            } else {
                out.result = std::get<1>(divideSigned(a, b, config.divAlgo));
            }
        } else if (opcode == isa::OP_AND) {
            out.result = gates::bitwise_and(a, b);
        } else if (opcode == isa::OP_OR) {
            out.result = gates::bitwise_or(a, b);
        } else if (opcode == isa::OP_XOR) {
            out.result = gates::bitwise_xor(a, b);
        } else if (opcode == isa::OP_NOT) {
            // not takes single operand (rs2 or imm, provided as b)
            out.result = gates::bitwise_not(b);
        } else if (opcode == isa::OP_ABS) {
            // abs takes single operand (rs2 or imm, provided as b)
            int32_t val = static_cast<int32_t>(b);
            out.result = static_cast<Word>(val < 0 ? -val : val);
        } else if (opcode == isa::OP_MOV) {
            // mov takes operand in b (rs2 or imm)
            out.result = b;
        } else if (opcode == isa::OP_LSL) {
            out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::LSL);
        } else if (opcode == isa::OP_LSR) {
            out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::LSR);
        } else if (opcode == isa::OP_ASR) {
            out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::ASR);
        } else if (opcode == isa::OP_MIN) {
            out.result = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? a : b;
        } else if (opcode == isa::OP_MAX) {
            out.result = (static_cast<int32_t>(a) > static_cast<int32_t>(b)) ? a : b;
        } else if (opcode == isa::OP_ROTS) {
            uint8_t sh = static_cast<uint8_t>(b & 0x1F);
            out.result = (sh == 0) ? a : ((a >> sh) | (a << (32 - sh)));
        } else {
            out.result = b;
        }

        return out;
    }
};

} // namespace alu
} // namespace risc201

#endif // RISC201_ALU_HPP
