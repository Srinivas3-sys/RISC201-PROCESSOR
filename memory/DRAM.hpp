#ifndef RISC201_MEMORY_DRAM_HPP
#define RISC201_MEMORY_DRAM_HPP

#include "Config.hpp"
#include "AddressMapping.hpp"
#include <cstdint>
#include <vector>
#include <deque>
#include <algorithm>
#include <iostream>

namespace risc201 {
namespace memory {

enum class BankState {
    IDLE,
    ACTIVE,
    ACTIVATING,
    PRECHARGING,
    REFRESHING
};

struct Bank {
    BankState state{BankState::IDLE};
    uint32_t activeRow{0};
    bool rowOpen{false};
    uint64_t lastActivateDramCycle{0};
    uint64_t lastPrechargeDramCycle{0};
    uint64_t lastAccessDramCycle{0};
};

struct DRAMRequest {
    uint64_t paddr{0};
    bool isWrite{false};
    DRAMCoordinates coords{};
    uint64_t arrivalCycleCpu{0};
    uint64_t arrivalCycleDram{0};
    uint64_t completionCycleDram{0};
    uint64_t latencyCyclesCpu{0};
    bool isRowHit{false};
};

class DRAMController {
public:
    DRAMConfig cfg;
    double cpuFreqGhz{3.2};
    AddressMapper mapper;

    // Banking hierarchy: [channel][rank][bankGroup][bank]
    // Flattened: totalBanks = channels * ranks * bankGroups * banks
    std::vector<Bank> banks;

    // Command queues
    std::deque<DRAMRequest> requestQueue;

    // Timing tracking
    uint64_t currentDramCycle{0};
    uint64_t lastRefreshDramCycle{0};
    std::deque<uint64_t> recentActivates; // For tFAW tracking

    // Statistics
    uint64_t readRequests{0};
    uint64_t writeRequests{0};
    uint64_t rowHits{0};
    uint64_t rowMisses{0};
    uint64_t rowConflicts{0};
    uint64_t refreshesExecuted{0};
    uint64_t totalDramCycles{0};
    uint64_t totalCpuStallCycles{0};

    explicit DRAMController(const DRAMConfig& dcfg = DRAMConfig(), double cpuFreq = 3.2)
        : cfg(dcfg), cpuFreqGhz(cpuFreq), mapper(dcfg, 64) {
        size_t total = cfg.channels * cfg.ranksPerChannel * cfg.bankGroupsPerRank * cfg.banksPerGroup;
        banks.resize(total);
        reset();
    }

    void reset() {
        for (auto& b : banks) {
            b.state = BankState::IDLE;
            b.activeRow = 0;
            b.rowOpen = false;
            b.lastActivateDramCycle = 0;
            b.lastPrechargeDramCycle = 0;
            b.lastAccessDramCycle = 0;
        }
        requestQueue.clear();
        currentDramCycle = 0;
        lastRefreshDramCycle = 0;
        recentActivates.clear();

        readRequests = 0;
        writeRequests = 0;
        rowHits = 0;
        rowMisses = 0;
        rowConflicts = 0;
        refreshesExecuted = 0;
        totalDramCycles = 0;
        totalCpuStallCycles = 0;
    }

    size_t getBankIndex(const DRAMCoordinates& c) const {
        size_t idx = c.channel;
        idx = idx * cfg.ranksPerChannel + c.rank;
        idx = idx * cfg.bankGroupsPerRank + c.bankGroup;
        idx = idx * cfg.banksPerGroup + c.bank;
        return idx % banks.size();
    }

    uint64_t dramCyclesToCpuCycles(uint64_t dramCycles) const {
        // CPU cycles = dramCycles * (tCK_ns * cpuFreqGhz)
        double ratio = cfg.timings.tCK_ns * cpuFreqGhz;
        return static_cast<uint64_t>(std::ceil(dramCycles * ratio));
    }

    uint64_t cpuCyclesToDramCycles(uint64_t cpuCycles) const {
        double ratio = cfg.timings.tCK_ns * cpuFreqGhz;
        if (ratio <= 0.0) return cpuCycles;
        return static_cast<uint64_t>(std::ceil(cpuCycles / ratio));
    }

    void checkRefresh(uint64_t dramCycle) {
        double refiCycles = cfg.timings.tREFI_ns / cfg.timings.tCK_ns;
        if (refiCycles > 0 && (dramCycle - lastRefreshDramCycle) >= refiCycles) {
            refreshesExecuted++;
            lastRefreshDramCycle = dramCycle;
            uint64_t rfcCycles = static_cast<uint64_t>(std::ceil(cfg.timings.tRFC_ns / cfg.timings.tCK_ns));
            // All banks stall during refresh
            for (auto& b : banks) {
                b.state = BankState::IDLE;
                b.rowOpen = false;
                b.lastPrechargeDramCycle = dramCycle + rfcCycles;
            }
            currentDramCycle += rfcCycles;
        }
    }

    uint64_t access(uint64_t paddr, bool isWrite = false, uint64_t currentCpuCycle = 0) {
        if (isWrite) writeRequests++;
        else readRequests++;

        uint64_t dramNow = std::max(currentDramCycle, cpuCyclesToDramCycles(currentCpuCycle));
        checkRefresh(dramNow);

        DRAMCoordinates coords = mapper.map(paddr);
        size_t bankIdx = getBankIndex(coords);
        Bank& bank = banks[bankIdx];

        uint64_t latencyDram = 0;
        bool isHit = false;
        (void)isHit;

        const auto& t = cfg.timings;

        if (cfg.pagePolicy == "open") {
            if (bank.rowOpen && bank.activeRow == coords.row) {
                // Row Buffer Hit
                isHit = true;
                rowHits++;
                latencyDram = t.tCL + t.tBURST;
            } else if (bank.rowOpen) {
                // Row Conflict: Precharge active row, then activate new row
                rowConflicts++;
                latencyDram = t.tRP + t.tRCD + t.tCL + t.tBURST;
                bank.activeRow = coords.row;
                bank.rowOpen = true;
            } else {
                // Row Miss / Empty: Activate row, then read/write
                rowMisses++;
                latencyDram = t.tRCD + t.tCL + t.tBURST;
                bank.activeRow = coords.row;
                bank.rowOpen = true;
            }
        } else {
            // Closed page policy: Activate, access, auto-precharge
            rowMisses++;
            latencyDram = t.tRCD + t.tCL + t.tBURST + t.tRP;
            bank.rowOpen = false;
        }

        // Enforce tFAW window
        while (!recentActivates.empty() && dramNow - recentActivates.front() > t.tFAW) {
            recentActivates.pop_front();
        }
        if (recentActivates.size() >= 4) {
            uint64_t earliestAllowed = recentActivates.front() + t.tFAW;
            if (earliestAllowed > dramNow) {
                latencyDram += (earliestAllowed - dramNow);
            }
        }
        recentActivates.push_back(dramNow + latencyDram);

        currentDramCycle = dramNow + latencyDram;
        uint64_t cpuLatency = dramCyclesToCpuCycles(latencyDram);
        totalCpuStallCycles += cpuLatency;

        return cpuLatency;
    }

    double getRowHitRate() const {
        uint64_t total = rowHits + rowMisses + rowConflicts;
        if (total == 0) return 0.0;
        return static_cast<double>(rowHits) / total;
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_DRAM_HPP