#ifndef RISC201_COMMON_PERFORMANCE_TRACKER_HPP
#define RISC201_COMMON_PERFORMANCE_TRACKER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace risc201 {

inline const std::vector<std::string> CANONICAL_STALL_REASONS = {
    "RAW_LOAD_USE",
    "RAW_NOFWD",
    "WAR_HAZARD",
    "WAW_HAZARD",
    "BRANCH_MISPREDICT",
    "BRANCH_STALL",
    "STRUCTURAL_MEM",
    "STRUCTURAL_RF",
    "MULTICYCLE_EX",
    "ICACHE_MISS",
    "DCACHE_MISS",
    "TLB_MISS",
    "PAGE_WALK",
    "PAGE_FAULT",
    "DRAM_REFRESH",
    "EXCEPTION_FLUSH",
    "OTHER_STALL"
};

class PerformanceTracker {
public:
    size_t pipelineStages{4};
    double clockMhz{3200.0};

    uint64_t totalCycles{0};
    uint64_t retiredInstructions{0};
    uint64_t pipelineFillCycles{0};
    uint64_t branchBubbles{0};

    std::unordered_map<std::string, uint64_t> stallBreakdown;

    explicit PerformanceTracker(size_t stages = 4, double clk = 3200.0)
        : pipelineStages(stages), clockMhz(clk) {
        pipelineFillCycles = stages > 0 ? (stages - 1) : 0;
        reset();
    }

    void reset() {
        totalCycles = 0;
        retiredInstructions = 0;
        branchBubbles = 0;
        stallBreakdown.clear();
        for (const auto& r : CANONICAL_STALL_REASONS) {
            stallBreakdown[r] = 0;
        }
    }

    void recordStall(const std::string& reason, uint64_t count = 1) {
        std::string r = reason;
        if (stallBreakdown.find(r) == stallBreakdown.end()) {
            r = "OTHER_STALL";
        }
        stallBreakdown[r] += count;
    }

    void recordBranchBubble(uint64_t count = 1) {
        branchBubbles += count;
    }

    uint64_t getTotalStallCycles() const {
        uint64_t sum = 0;
        for (const auto& kv : stallBreakdown) {
            sum += kv.second;
        }
        return sum;
    }

    double getCPI() const {
        if (retiredInstructions == 0) return 0.0;
        return static_cast<double>(totalCycles) / retiredInstructions;
    }

    double getIPC() const {
        if (totalCycles == 0) return 0.0;
        return static_cast<double>(retiredInstructions) / totalCycles;
    }

    bool verifyInvariant(int tolerance = 5) const {
        uint64_t accounted = retiredInstructions + getTotalStallCycles() + branchBubbles + pipelineFillCycles;
        int64_t diff = static_cast<int64_t>(totalCycles) - static_cast<int64_t>(accounted);
        return std::abs(diff) <= tolerance;
    }

    std::string generateReport(const std::string& title = "RISC201 PERFORMANCE REPORT") const {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << "                  " << title << "\n";
        oss << "========================================================================\n";
        oss << std::left << std::setw(28) << "Total Cycles:" << totalCycles << "\n";
        oss << std::left << std::setw(28) << "Retired Instructions:" << retiredInstructions << "\n";
        oss << std::left << std::setw(28) << "Pipeline Stages:" << pipelineStages << "\n";
        oss << std::left << std::setw(28) << "CPI:" << std::fixed << std::setprecision(3) << getCPI() << "\n";
        oss << std::left << std::setw(28) << "IPC:" << std::fixed << std::setprecision(3) << getIPC() << "\n";
        oss << std::left << std::setw(28) << "Branch Bubbles / Flushes:" << branchBubbles << "\n";
        oss << std::left << std::setw(28) << "Pipeline Fill Cycles:" << pipelineFillCycles << "\n";
        oss << "------------------------------------------------------------------------\n";
        oss << "CPI STACK BREAKDOWN BY CANONICAL STALL REASON:\n";
        oss << "------------------------------------------------------------------------\n";
        oss << std::left << std::setw(24) << "Reason" << std::setw(12) << "Cycles" << std::setw(12) << "Delta CPI" << "Share\n";

        uint64_t totalStalls = getTotalStallCycles();
        for (const auto& reason : CANONICAL_STALL_REASONS) {
            uint64_t c = stallBreakdown.at(reason);
            if (c > 0) {
                double deltaCpi = (retiredInstructions > 0) ? (static_cast<double>(c) / retiredInstructions) : 0.0;
                double share = (totalStalls > 0) ? (100.0 * c / totalStalls) : 0.0;
                oss << std::left << std::setw(24) << reason
                    << std::setw(12) << c
                    << std::setw(12) << std::fixed << std::setprecision(3) << deltaCpi
                    << std::setprecision(1) << share << "%\n";
            }
        }

        oss << "------------------------------------------------------------------------\n";
        oss << "Invariant Verification: " << (verifyInvariant() ? "PASSED (Cycles == Retired + Stalls + Bubbles)" : "WARNING: MISMATCH") << "\n";
        oss << "========================================================================\n";
        return oss.str();
    }
};

} // namespace risc201

#endif // RISC201_COMMON_PERFORMANCE_TRACKER_HPP