#ifndef RISC201_COMMON_FORWARDING_UNIT_HPP
#define RISC201_COMMON_FORWARDING_UNIT_HPP

#include "ISA.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>

namespace risc201 {

enum class ForwardingMode {
    NONE,       // Interlocks only; no bypass network
    BASIC,      // EX-to-EX only
    FULL,       // Full forwarding: EX-to-EX, MEM-to-EX, WB-to-EX
    FULL_PLUS   // Full forwarding + Store-to-Load buffer bypass
};

inline ForwardingMode parseForwardingMode(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "none" || s == "off" || s == "false") return ForwardingMode::NONE;
    if (s == "basic" || s == "ex-ex") return ForwardingMode::BASIC;
    if (s == "full" || s == "on" || s == "true") return ForwardingMode::FULL;
    if (s == "full+" || s == "full_plus" || s == "store-buffer") return ForwardingMode::FULL_PLUS;
    return ForwardingMode::FULL; // Default
}

inline std::string forwardingModeToString(ForwardingMode m) {
    switch (m) {
        case ForwardingMode::NONE: return "none";
        case ForwardingMode::BASIC: return "basic";
        case ForwardingMode::FULL: return "full";
        case ForwardingMode::FULL_PLUS: return "full+";
    }
    return "full";
}

enum ForwardSource : uint8_t {
    FWD_FROM_REG = 0,
    FWD_FROM_EX  = 1,
    FWD_FROM_MEM = 2,
    FWD_FROM_WB  = 3,
    FWD_FROM_STORE_BUF = 4
};

struct StoreBufferEntry {
    uint32_t address{0};
    uint32_t data{0};
    uint64_t cycleIssued{0};
    bool valid{false};
};

class StoreBuffer {
public:
    size_t capacity{4};
    std::vector<StoreBufferEntry> entries;
    uint64_t forwardHits{0};

    explicit StoreBuffer(size_t cap = 4) : capacity(cap) {}

    void reset() {
        entries.clear();
        forwardHits = 0;
    }

    void addStore(uint32_t addr, uint32_t data, uint64_t cycle) {
        if (entries.size() >= capacity) {
            entries.erase(entries.begin());
        }
        StoreBufferEntry e;
        e.address = addr;
        e.data = data;
        e.cycleIssued = cycle;
        e.valid = true;
        entries.push_back(e);
    }

    bool forwardLoad(uint32_t loadAddr, uint32_t& outData) {
        // Search newest to oldest for youngest store to matching address
        for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
            if (entries[i].valid && entries[i].address == loadAddr) {
                forwardHits++;
                outData = entries[i].data;
                return true;
            }
        }
        return false;
    }
};

struct ForwardDecision {
    ForwardSource srcOp1{FWD_FROM_REG};
    ForwardSource srcOp2{FWD_FROM_REG};
    ForwardSource srcStoreData{FWD_FROM_REG};
    uint32_t valOp1{0};
    uint32_t valOp2{0};
    uint32_t valStoreData{0};
    bool forwardedOp1{false};
    bool forwardedOp2{false};
    bool forwardedStoreData{false};
};

class ForwardingUnit {
public:
    ForwardingMode mode{ForwardingMode::FULL};
    StoreBuffer storeBuffer;

    // Telemetry
    uint64_t exToExForwards{0};
    uint64_t memToExForwards{0};
    uint64_t wbToExForwards{0};
    uint64_t storeDataForwards{0};
    uint64_t storeBufferForwards{0};

    explicit ForwardingUnit(ForwardingMode m = ForwardingMode::FULL)
        : mode(m), storeBuffer(4) {}

    void reset() {
        storeBuffer.reset();
        exToExForwards = 0;
        memToExForwards = 0;
        wbToExForwards = 0;
        storeDataForwards = 0;
        storeBufferForwards = 0;
    }

    // 4-Stage Forwarding Resolver
    // Producer: EX_MARW stage
    // Consumer: OF_EX stage
    ForwardDecision resolve4Stage(
        const Instruction& consumer,
        uint32_t regVal1,
        uint32_t regVal2,
        bool exMarwValid,
        const Instruction& exMarwInst,
        uint32_t exMarwResult
    ) {
        ForwardDecision d;
        d.valOp1 = regVal1;
        d.valOp2 = regVal2;
        d.valStoreData = regVal2;

        if (mode == ForwardingMode::NONE) {
            return d;
        }

        if (exMarwValid && exMarwInst.writesRegister()) {
            uint8_t prodRd = exMarwInst.getDestReg();

            // Forward to Op1 (rs1)
            if (consumer.readsRs1() && consumer.rs1 == prodRd) {
                d.srcOp1 = FWD_FROM_EX;
                d.valOp1 = exMarwResult;
                d.forwardedOp1 = true;
                exToExForwards++;
            }

            // Forward to Op2 (rs2 for ALU ops)
            if (consumer.readsRs2() && consumer.rs2 == prodRd) {
                d.srcOp2 = FWD_FROM_EX;
                d.valOp2 = exMarwResult;
                d.forwardedOp2 = true;
                exToExForwards++;
            }

            // Separate Store Data forwarding (rd value to store into memory)
            if (consumer.isStore() && consumer.rd == prodRd) {
                d.srcStoreData = FWD_FROM_EX;
                d.valStoreData = exMarwResult;
                d.forwardedStoreData = true;
                storeDataForwards++;
            }
        }

        return d;
    }

    // 6-Stage Forwarding Resolver
    // Producers: EX_MA (youngest), MA_RW (older), RW (oldest)
    // Consumer: RF_EX
    ForwardDecision resolve6Stage(
        const Instruction& consumer,
        uint32_t regVal1,
        uint32_t regVal2,
        bool exMaValid, const Instruction& exMaInst, uint32_t exMaResult,
        bool maRwValid, const Instruction& maRwInst, uint32_t maRwResult,
        bool rwValid,   const Instruction& rwInst,   uint32_t rwResult
    ) {
        ForwardDecision d;
        d.valOp1 = regVal1;
        d.valOp2 = regVal2;
        d.valStoreData = regVal2;

        if (mode == ForwardingMode::NONE) {
            return d;
        }

        // Youngest-producer priority for Op1 (rs1)
        if (consumer.readsRs1()) {
            uint8_t rs1 = consumer.rs1;
            if (exMaValid && exMaInst.writesRegister() && exMaInst.getDestReg() == rs1) {
                d.srcOp1 = FWD_FROM_EX;
                d.valOp1 = exMaResult;
                d.forwardedOp1 = true;
                exToExForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       maRwValid && maRwInst.writesRegister() && maRwInst.getDestReg() == rs1) {
                d.srcOp1 = FWD_FROM_MEM;
                d.valOp1 = maRwResult;
                d.forwardedOp1 = true;
                memToExForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       rwValid && rwInst.writesRegister() && rwInst.getDestReg() == rs1) {
                d.srcOp1 = FWD_FROM_WB;
                d.valOp1 = rwResult;
                d.forwardedOp1 = true;
                wbToExForwards++;
            }
        }

        // Youngest-producer priority for Op2 (rs2)
        if (consumer.readsRs2()) {
            uint8_t rs2 = consumer.rs2;
            if (exMaValid && exMaInst.writesRegister() && exMaInst.getDestReg() == rs2) {
                d.srcOp2 = FWD_FROM_EX;
                d.valOp2 = exMaResult;
                d.forwardedOp2 = true;
                exToExForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       maRwValid && maRwInst.writesRegister() && maRwInst.getDestReg() == rs2) {
                d.srcOp2 = FWD_FROM_MEM;
                d.valOp2 = maRwResult;
                d.forwardedOp2 = true;
                memToExForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       rwValid && rwInst.writesRegister() && rwInst.getDestReg() == rs2) {
                d.srcOp2 = FWD_FROM_WB;
                d.valOp2 = rwResult;
                d.forwardedOp2 = true;
                wbToExForwards++;
            }
        }

        // Store data bypass (rd value for stores)
        if (consumer.isStore()) {
            uint8_t rd = consumer.rd;
            if (exMaValid && exMaInst.writesRegister() && exMaInst.getDestReg() == rd) {
                d.srcStoreData = FWD_FROM_EX;
                d.valStoreData = exMaResult;
                d.forwardedStoreData = true;
                storeDataForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       maRwValid && maRwInst.writesRegister() && maRwInst.getDestReg() == rd) {
                d.srcStoreData = FWD_FROM_MEM;
                d.valStoreData = maRwResult;
                d.forwardedStoreData = true;
                storeDataForwards++;
            } else if ((mode == ForwardingMode::FULL || mode == ForwardingMode::FULL_PLUS) &&
                       rwValid && rwInst.writesRegister() && rwInst.getDestReg() == rd) {
                d.srcStoreData = FWD_FROM_WB;
                d.valStoreData = rwResult;
                d.forwardedStoreData = true;
                storeDataForwards++;
            }
        }

        return d;
    }
};

} // namespace risc201

#endif // RISC201_COMMON_FORWARDING_UNIT_HPP