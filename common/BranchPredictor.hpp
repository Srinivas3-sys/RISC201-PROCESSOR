#ifndef RISC201_COMMON_BRANCH_PREDICTOR_HPP
#define RISC201_COMMON_BRANCH_PREDICTOR_HPP

#include "ISA.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace risc201 {

enum class BranchPolicy {
    STALL,           // 1. Stall until branch resolves in EX
    FLUSH,           // 2. Predict not taken; flush if taken
    PREDICT_TAKEN,   // 3. Always predict taken
    STATIC_BTFN,     // 4. Backward taken, forward not-taken
    BIMODAL_1BIT,    // 5. 1-bit counter table
    BIMODAL_2BIT,    // 6. Smith 2-bit saturating counter table (00, 01, 10, 11)
    GSHARE,          // 7. Global history register XOR PC
    PERFECT          // 8. Oracle / perfect predictor
};

inline BranchPolicy parseBranchPolicy(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "stall") return BranchPolicy::STALL;
    if (s == "flush" || s == "predict-not-taken" || s == "not-taken" || s == "not_taken" || s == "always-not-taken" || s == "nottaken") return BranchPolicy::FLUSH;
    if (s == "predict-taken" || s == "always-taken") return BranchPolicy::PREDICT_TAKEN;
    if (s == "static-btfn" || s == "btfn") return BranchPolicy::STATIC_BTFN;
    if (s == "1bit" || s == "bimodal-1bit") return BranchPolicy::BIMODAL_1BIT;
    if (s == "2bit" || s == "bimodal-2bit" || s == "smith") return BranchPolicy::BIMODAL_2BIT;
    if (s == "gshare") return BranchPolicy::GSHARE;
    if (s == "perfect" || s == "oracle") return BranchPolicy::PERFECT;
    return BranchPolicy::BIMODAL_2BIT; // Default
}

inline std::string branchPolicyToString(BranchPolicy p) {
    switch (p) {
        case BranchPolicy::STALL: return "stall";
        case BranchPolicy::FLUSH: return "flush";
        case BranchPolicy::PREDICT_TAKEN: return "predict-taken";
        case BranchPolicy::STATIC_BTFN: return "static-btfn";
        case BranchPolicy::BIMODAL_1BIT: return "1bit";
        case BranchPolicy::BIMODAL_2BIT: return "2bit";
        case BranchPolicy::GSHARE: return "gshare";
        case BranchPolicy::PERFECT: return "perfect";
    }
    return "2bit";
}

struct BTBEntry {
    uint32_t tag{0};
    uint32_t target{0};
    bool valid{false};
};

class BranchTargetBuffer {
public:
    size_t numEntries{64};
    std::vector<BTBEntry> table;
    uint64_t lookups{0};
    uint64_t hits{0};

    explicit BranchTargetBuffer(size_t entries = 64) : numEntries(entries), table(entries) {}

    void reset() {
        for (auto& e : table) {
            e.valid = false;
            e.tag = 0;
            e.target = 0;
        }
        lookups = 0;
        hits = 0;
    }

    bool lookup(uint32_t pc, uint32_t& outTarget) {
        lookups++;
        size_t idx = (pc >> 2) % numEntries;
        uint32_t tag = pc >> 2;
        if (table[idx].valid && table[idx].tag == tag) {
            hits++;
            outTarget = table[idx].target;
            return true;
        }
        return false;
    }

    void update(uint32_t pc, uint32_t target) {
        size_t idx = (pc >> 2) % numEntries;
        table[idx].valid = true;
        table[idx].tag = pc >> 2;
        table[idx].target = target;
    }
};

class ReturnAddressStack {
public:
    size_t capacity{16};
    std::vector<uint32_t> stack;
    uint64_t pushes{0};
    uint64_t pops{0};
    uint64_t hits{0};

    explicit ReturnAddressStack(size_t cap = 16) : capacity(cap) {}

    void reset() {
        stack.clear();
        pushes = 0;
        pops = 0;
        hits = 0;
    }

    void push(uint32_t returnAddr) {
        pushes++;
        if (stack.size() >= capacity) {
            stack.erase(stack.begin());
        }
        stack.push_back(returnAddr);
    }

    bool pop(uint32_t& outAddr) {
        pops++;
        if (stack.empty()) return false;
        outAddr = stack.back();
        stack.pop_back();
        hits++;
        return true;
    }
};

struct BranchPrediction {
    bool predictedTaken{false};
    uint32_t predictedTarget{0};
    bool usedBtb{false};
    bool usedRas{false};
    bool shouldStall{false};
};

class BranchPredictor {
public:
    BranchPolicy policy{BranchPolicy::BIMODAL_2BIT};
    BranchTargetBuffer btb;
    ReturnAddressStack ras;

    // Bimodal tables
    std::vector<uint8_t> bimodal1Bit;
    std::vector<uint8_t> bimodal2Bit; // 00, 01, 10, 11 (2 or 3 = taken)

    // GShare
    uint32_t ghr{0}; // Global history register (10 bits)
    std::vector<uint8_t> gshareTable;

    // Performance telemetry
    uint64_t totalBranches{0};
    uint64_t conditionalBranches{0};
    uint64_t callCount{0};
    uint64_t retCount{0};
    uint64_t correctPredictions{0};
    uint64_t mispredictions{0};

    explicit BranchPredictor(BranchPolicy p = BranchPolicy::BIMODAL_2BIT, size_t tableSize = 256)
        : policy(p), btb(64), ras(16),
          bimodal1Bit(tableSize, 0),
          bimodal2Bit(tableSize, 2), // Default weakly taken (10)
          ghr(0),
          gshareTable(tableSize, 2) {}

    void reset() {
        btb.reset();
        ras.reset();
        std::fill(bimodal1Bit.begin(), bimodal1Bit.end(), 0);
        std::fill(bimodal2Bit.begin(), bimodal2Bit.end(), 2);
        std::fill(gshareTable.begin(), gshareTable.end(), 2);
        ghr = 0;
        totalBranches = 0;
        conditionalBranches = 0;
        callCount = 0;
        retCount = 0;
        correctPredictions = 0;
        mispredictions = 0;
    }

    BranchPrediction predict(uint32_t pc, const Instruction& inst) {
        BranchPrediction pred;
        pred.predictedTarget = pc + 4;

        if (!inst.isBranch()) {
            return pred;
        }

        totalBranches++;

        // Subroutine Call (call): Push return address PC+4 to RAS, branch is unconditional
        if (inst.isCall()) {
            callCount++;
            ras.push(pc + 4);
            pred.predictedTaken = true;
            pred.predictedTarget = inst.branchTarget;
            return pred;
        }

        // Subroutine Return (ret): Predict target using RAS
        if (inst.isRet()) {
            retCount++;
            uint32_t rasTarget = 0;
            if (ras.pop(rasTarget)) {
                pred.predictedTaken = true;
                pred.predictedTarget = rasTarget;
                pred.usedRas = true;
                return pred;
            }
        }

        // Unconditional branch (b)
        if (inst.opcode == isa::OP_B) {
            pred.predictedTaken = true;
            pred.predictedTarget = inst.branchTarget;
            return pred;
        }

        // Conditional branch (beq, bgt)
        conditionalBranches++;

        if (policy == BranchPolicy::STALL) {
            pred.shouldStall = true;
            pred.predictedTaken = false;
            return pred;
        } else if (policy == BranchPolicy::FLUSH) {
            pred.predictedTaken = false;
            return pred;
        } else if (policy == BranchPolicy::PREDICT_TAKEN) {
            pred.predictedTaken = true;
            pred.predictedTarget = inst.branchTarget;
            return pred;
        } else if (policy == BranchPolicy::STATIC_BTFN) {
            // Backward taken, forward not taken
            if (inst.branchTarget < pc) {
                pred.predictedTaken = true;
                pred.predictedTarget = inst.branchTarget;
            } else {
                pred.predictedTaken = false;
            }
            return pred;
        } else if (policy == BranchPolicy::BIMODAL_1BIT) {
            size_t idx = (pc >> 2) % bimodal1Bit.size();
            pred.predictedTaken = (bimodal1Bit[idx] == 1);
            if (pred.predictedTaken) pred.predictedTarget = inst.branchTarget;
            return pred;
        } else if (policy == BranchPolicy::BIMODAL_2BIT) {
            size_t idx = (pc >> 2) % bimodal2Bit.size();
            pred.predictedTaken = (bimodal2Bit[idx] >= 2);
            if (pred.predictedTaken) pred.predictedTarget = inst.branchTarget;
            return pred;
        } else if (policy == BranchPolicy::GSHARE) {
            size_t idx = ((pc >> 2) ^ ghr) % gshareTable.size();
            pred.predictedTaken = (gshareTable[idx] >= 2);
            if (pred.predictedTaken) pred.predictedTarget = inst.branchTarget;
            return pred;
        } else if (policy == BranchPolicy::PERFECT) {
            // Handled during update/resolution
            pred.predictedTaken = true;
            pred.predictedTarget = inst.branchTarget;
            return pred;
        }

        return pred;
    }

    void update(uint32_t pc, const Instruction& inst, bool actuallyTaken, uint32_t actualTarget, bool wasPredictedTaken) {
        if (!inst.isBranch()) return;

        bool mispredicted = (actuallyTaken != wasPredictedTaken);
        if (mispredicted) mispredictions++;
        else correctPredictions++;

        // Update BTB on taken branches
        if (actuallyTaken) {
            btb.update(pc, actualTarget);
        }

        // Update pattern history tables for conditional branches
        if (inst.isConditionalBranch()) {
            if (policy == BranchPolicy::BIMODAL_1BIT) {
                size_t idx = (pc >> 2) % bimodal1Bit.size();
                bimodal1Bit[idx] = actuallyTaken ? 1 : 0;
            } else if (policy == BranchPolicy::BIMODAL_2BIT) {
                size_t idx = (pc >> 2) % bimodal2Bit.size();
                uint8_t c = bimodal2Bit[idx];
                if (actuallyTaken) {
                    if (c < 3) bimodal2Bit[idx] = c + 1;
                } else {
                    if (c > 0) bimodal2Bit[idx] = c - 1;
                }
            } else if (policy == BranchPolicy::GSHARE) {
                size_t idx = ((pc >> 2) ^ ghr) % gshareTable.size();
                uint8_t c = gshareTable[idx];
                if (actuallyTaken) {
                    if (c < 3) gshareTable[idx] = c + 1;
                } else {
                    if (c > 0) gshareTable[idx] = c - 1;
                }
                // Shift GHR
                ghr = ((ghr << 1) | (actuallyTaken ? 1 : 0)) & 0x3FF;
            }
        }
    }

    double getAccuracy() const {
        uint64_t evaluated = correctPredictions + mispredictions;
        if (evaluated == 0) return 1.0;
        return static_cast<double>(correctPredictions) / evaluated;
    }
};

} // namespace risc201

#endif // RISC201_COMMON_BRANCH_PREDICTOR_HPP