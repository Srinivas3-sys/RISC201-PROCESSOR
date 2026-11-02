#ifndef RISC201_ALU_HPP
#define RISC201_ALU_HPP

#include "LogicGates.hpp"
#include "Adders.hpp"
#include "Multipliers.hpp"
#include "Dividers.hpp"
#include "Shifter.hpp"
#include "../common/Types.hpp"
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

enum class DividerAlgorithm {
    RESTORING,
    NON_RESTORING
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

    AluOutput execute(const std::string& opcodeBits, Word a, Word b, const Flags& currentFlags = {}) {
        uint32_t val = 0;
        for (char c : opcodeBits) {
            val = (val << 1) | (c == '1' ? 1 : 0);
        }
        return execute(static_cast<Opcode>(val), a, b, currentFlags);
    }

    /**
     * Execute an ALU operation given inputs A, B, and the instruction Opcode.
     * Note: flags are only updated if opcode == OP_CMP.
     */
    AluOutput execute(Opcode op, Word a, Word b, const Flags& currentFlags = {}) {
        AluOutput out;
        out.flags = currentFlags; // Default: maintain flags unless cmp

        switch (op) {
            case OP_ADD:
            case OP_LD:
            case OP_ST: { // Load/Store compute effective address via adder (rs1 + imm)
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
                break;
            }

            case OP_SUB: {
                uint8_t cout = 0;
                bool ovf = false;
                std::tie(out.result, cout, ovf) = subtract32(a, b);
                out.carryOut = (cout != 0);
                out.overflow = ovf;
                break;
            }

            case OP_CMP: {
                // cmp performs subtraction (A - B) and sets flags.E and flags.GT (Sarangi Section 3.3.4 & 9.2.4)
                auto [diff, cout, ovf] = subtract32(a, b);
                out.result = diff;
                out.flags.E = (diff == 0);
                // Signed comparison: A > B if (diff > 0) with respect to 2's complement
                int32_t sa = static_cast<int32_t>(a);
                int32_t sb = static_cast<int32_t>(b);
                out.flags.GT = (sa > sb);
                break;
            }

            case OP_MUL: {
                uint64_t prod = 0;
                if (config.mulAlgo == MultiplierAlgorithm::ITERATIVE) {
                    prod = iterativeMultiply(a, b);
                } else if (config.mulAlgo == MultiplierAlgorithm::WALLACE_TREE) {
                    prod = wallaceTreeMultiply(a, b);
                } else {
                    prod = boothMultiply(a, b);
                }
                out.result = static_cast<Word>(prod & 0xFFFFFFFF);
                break;
            }

            case OP_DIV: {
                if (b == 0) {
                    out.exception.type = ExceptionType::DIVISION_BY_ZERO;
                    out.exception.details = "Division by zero in ALU";
                    out.result = 0;
                } else {
                    bool useNonRestoring = (config.divAlgo == DividerAlgorithm::NON_RESTORING);
                    auto [quot, rem, divZero] = divideSigned(a, b, useNonRestoring);
                    out.result = quot;
                }
                break;
            }

            case OP_MOD: {
                if (b == 0) {
                    out.exception.type = ExceptionType::DIVISION_BY_ZERO;
                    out.exception.details = "Modulo by zero in ALU";
                    out.result = 0;
                } else {
                    bool useNonRestoring = (config.divAlgo == DividerAlgorithm::NON_RESTORING);
                    auto [quot, rem, divZero] = divideSigned(a, b, useNonRestoring);
                    out.result = rem;
                }
                break;
            }

            case OP_AND: {
                out.result = gates::bitwise_and(a, b);
                break;
            }

            case OP_OR: {
                out.result = gates::bitwise_or(a, b);
                break;
            }

            case OP_NOT: {
                // not takes single operand (rs2 or imm, provided as b)
                out.result = gates::bitwise_not(b);
                break;
            }

            case OP_MOV: {
                // mov takes operand in b (rs2 or imm)
                out.result = b;
                break;
            }

            case OP_LSL: {
                out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::LSL);
                break;
            }

            case OP_LSR: {
                out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::LSR);
                break;
            }

            case OP_ASR: {
                out.result = BarrelShifter::shift(a, b, BarrelShifter::ShiftType::ASR);
                break;
            }

            default:
                out.result = b;
                break;
        }

        return out;
    }
};

} // namespace alu
} // namespace risc201

#endif // RISC201_ALU_HPP
