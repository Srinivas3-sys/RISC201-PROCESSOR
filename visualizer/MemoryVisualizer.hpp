#ifndef RISC201_VISUALIZER_MEMORY_VISUALIZER_HPP
#define RISC201_VISUALIZER_MEMORY_VISUALIZER_HPP

#include "../memory/MemoryHierarchy.hpp"
#include "../common/PerformanceTracker.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace risc201 {
namespace visualizer {

class MemoryVisualizer {
public:
    static std::string renderCache(const memory::Cache& cache, size_t maxSets = 8) {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << "       CACHE STATE: " << cache.cfg.name
            << " (" << (cache.cfg.sizeBytes / 1024) << " KB, "
            << cache.cfg.associativity << "-Way, "
            << cache.cfg.lineSizeBytes << " B Lines)\n";
        oss << "========================================================================\n";
        oss << "Hit Rate: " << std::fixed << std::setprecision(2) << (cache.getHitRate() * 100.0) << "% "
            << "(Hits: " << cache.hits << ", Misses: " << cache.misses << ", Writebacks: " << cache.writebacks << ")\n";
        oss << "3C Misses: Compulsory: " << cache.compulsoryMisses
            << ", Capacity: " << cache.capacityMisses
            << ", Conflict: " << cache.conflictMisses << "\n";
        oss << "------------------------------------------------------------------------\n";
        oss << std::left << std::setw(8) << "Set"
            << std::setw(12) << "Way 0"
            << std::setw(12) << "Way 1"
            << std::setw(12) << "Way 2"
            << std::setw(12) << "Way 3" << "...\n";
        oss << "------------------------------------------------------------------------\n";

        size_t setsToShow = std::min(maxSets, cache.numSets);
        for (size_t s = 0; s < setsToShow; ++s) {
            oss << "[" << std::setw(4) << s << "]  ";
            const auto& set = cache.sets[s];
            size_t waysToShow = std::min(size_t(4), set.lines.size());
            for (size_t w = 0; w < waysToShow; ++w) {
                const auto& l = set.lines[w];
                if (!l.valid) {
                    oss << std::setw(12) << "[ INV ]";
                } else {
                    std::ostringstream tagStr;
                    tagStr << (l.dirty ? "D:0x" : "V:0x") << std::hex << std::setw(4) << std::setfill('0') << (l.tag & 0xFFFF);
                    oss << std::setw(12) << tagStr.str();
                }
            }
            oss << "\n";
        }
        if (cache.numSets > setsToShow) {
            oss << "  ... (" << (cache.numSets - setsToShow) << " more sets omitted)\n";
        }
        oss << "========================================================================\n";
        return oss.str();
    }

    static std::string renderDRAM(const memory::DRAMController& dram) {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << "       DRAM CONTROLLER STATE: " << dram.cfg.dramType
            << " (" << dram.cfg.channels << " Ch, "
            << dram.cfg.bankGroupsPerRank << " BG, "
            << dram.cfg.banksPerGroup << " Banks/BG, "
            << dram.cfg.pagePolicy << " Page)\n";
        oss << "========================================================================\n";
        oss << "Row Hit Rate: " << std::fixed << std::setprecision(2) << (dram.getRowHitRate() * 100.0) << "% "
            << "(Hits: " << dram.rowHits << ", Misses: " << dram.rowMisses << ", Conflicts: " << dram.rowConflicts << ")\n";
        oss << "Refreshes: " << dram.refreshesExecuted
            << ", Total CPU Stall Cycles: " << dram.totalCpuStallCycles << "\n";
        oss << "------------------------------------------------------------------------\n";
        oss << std::left << std::setw(10) << "Bank ID"
            << std::setw(16) << "State"
            << std::setw(16) << "Active Row"
            << "Row Buffer\n";
        oss << "------------------------------------------------------------------------\n";

        size_t banksToShow = std::min(size_t(8), dram.banks.size());
        for (size_t i = 0; i < banksToShow; ++i) {
            const auto& b = dram.banks[i];
            std::string st = "IDLE";
            if (b.state == memory::BankState::ACTIVE) st = "ACTIVE";
            else if (b.state == memory::BankState::ACTIVATING) st = "ACTIVATING";
            else if (b.state == memory::BankState::PRECHARGING) st = "PRECHARGING";
            else if (b.state == memory::BankState::REFRESHING) st = "REFRESHING";

            oss << "Bank " << std::setw(5) << i
                << std::setw(16) << st;
            if (b.rowOpen) {
                oss << "0x" << std::hex << std::setw(12) << std::setfill('0') << b.activeRow << std::dec << std::setfill(' ')
                    << "OPEN (Cached)\n";
            } else {
                oss << std::setw(16) << "---"
                    << "CLOSED\n";
            }
        }
        if (dram.banks.size() > banksToShow) {
            oss << "  ... (" << (dram.banks.size() - banksToShow) << " more banks omitted)\n";
        }
        oss << "========================================================================\n";
        return oss.str();
    }

    static std::string renderTLB(const memory::GenericTLB& tlb, size_t maxEntries = 8) {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << "       " << tlb.name << " STATE (" << tlb.numEntries << " Entries, " << tlb.associativity << "-Way)\n";
        oss << "========================================================================\n";
        oss << "Lookups: " << tlb.lookups << ", Hits: " << tlb.hits << ", Misses: " << tlb.misses << "\n";
        oss << "------------------------------------------------------------------------\n";
        oss << std::left << std::setw(8) << "Set"
            << std::setw(16) << "VPN"
            << std::setw(12) << "PFN"
            << std::setw(8) << "ASID"
            << "Status\n";
        oss << "------------------------------------------------------------------------\n";

        size_t count = 0;
        for (size_t s = 0; s < tlb.sets.size() && count < maxEntries; ++s) {
            for (const auto& e : tlb.sets[s]) {
                if (e.valid && count < maxEntries) {
                    oss << "[" << std::setw(4) << s << "]  "
                        << "0x" << std::hex << std::setw(12) << std::setfill('0') << e.vpn << std::dec << std::setfill(' ')
                        << std::setw(12) << e.pfn
                        << std::setw(8) << e.asid
                        << (e.dirty ? "VALID (DIRTY)" : "VALID") << "\n";
                    count++;
                }
            }
        }
        if (count == 0) {
            oss << "(No valid entries in TLB)\n";
        }
        oss << "========================================================================\n";
        return oss.str();
    }

    static std::string renderCPIStack(const PerformanceTracker& perf, bool steadyState = false, uint64_t warmCycles = 0, uint64_t warmRetired = 0) {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << (steadyState ? "             CPI STACK BREAKDOWN VISUALIZER (STEADY-STATE)              \n"
                            : "                      CPI STACK BREAKDOWN VISUALIZER                    \n");
        oss << "========================================================================\n";
        oss << "Base CPI:  [####################] 1.000\n";

        uint64_t ret = perf.retiredInstructions;
        uint64_t cyc = perf.totalCycles;
        if (steadyState && warmRetired > 0 && ret > warmRetired) {
            ret -= warmRetired;
            cyc = cyc > warmCycles ? cyc - warmCycles : 0;
        }

        uint64_t totalStalls = perf.getTotalStallCycles();
        for (const auto& r : CANONICAL_STALL_REASONS) {
            uint64_t c = perf.stallBreakdown.at(r);
            if (c > 0) {
                double deltaCpi = (ret > 0) ? (static_cast<double>(c) / ret) : 0.0;
                size_t barLen = std::min(size_t(30), static_cast<size_t>(deltaCpi * 20.0));
                if (barLen == 0) barLen = 1;
                std::string bar(barLen, '#');

                oss << std::left << std::setw(20) << r << " [" << std::setw(20) << bar << "] +"
                    << std::fixed << std::setprecision(3) << deltaCpi
                    << " (" << c << " cyc, " << (100.0 * c / (totalStalls > 0 ? totalStalls : 1)) << "%)\n";
            }
        }
        oss << "------------------------------------------------------------------------\n";
        double measuredCpi = (ret > 0) ? (static_cast<double>(cyc) / ret) : perf.getCPI();
        oss << "Total Measured CPI: " << std::fixed << std::setprecision(3) << measuredCpi
            << " | Mode: " << (steadyState ? "STEADY-STATE" : "COLD-START")
            << " | Invariant Check: " << (perf.verifyInvariant() ? "PASSED" : "WARN") << "\n";
        oss << "========================================================================\n";
        return oss.str();
    }

    static std::string renderMemorySummary(const memory::MemoryHierarchy& mem) {
        std::ostringstream oss;
        oss << "========================================================================\n";
        oss << "                    MEMORY SUBSYSTEM LIVE MONITOR                       \n";
        oss << "========================================================================\n";
        oss << "L1I Cache      : Hit Rate: " << std::fixed << std::setprecision(2) << mem.l1i.getHitRate() * 100.0 << "%"
            << " (Hits: " << mem.l1i.hits << ", Misses: " << mem.l1i.misses << ")\n";
        oss << "L1D Cache      : Hit Rate: " << std::fixed << std::setprecision(2) << mem.l1d.getHitRate() * 100.0 << "%"
            << " (Hits: " << mem.l1d.hits << ", Misses: " << mem.l1d.misses << ")\n";
        if (mem.l2) {
            oss << "L2 Cache       : Hit Rate: " << std::fixed << std::setprecision(2) << mem.l2->getHitRate() * 100.0 << "%"
                << " (Hits: " << mem.l2->hits << ", Misses: " << mem.l2->misses << ")\n";
        }
        if (mem.l3) {
            oss << "L3 Cache       : Hit Rate: " << std::fixed << std::setprecision(2) << mem.l3->getHitRate() * 100.0 << "%"
                << " (Hits: " << mem.l3->hits << ", Misses: " << mem.l3->misses << ")\n";
        }
        oss << "ITLB           : Hits: " << mem.vm.itlb.hits << ", Misses: " << mem.vm.itlb.misses
            << " (Hit Rate: " << std::fixed << std::setprecision(2)
            << (mem.vm.itlb.lookups > 0 ? (100.0 * mem.vm.itlb.hits / mem.vm.itlb.lookups) : 100.0) << "%)\n";
        oss << "DTLB           : Hits: " << mem.vm.dtlb.hits << ", Misses: " << mem.vm.dtlb.misses
            << " (Hit Rate: " << std::fixed << std::setprecision(2)
            << (mem.vm.dtlb.lookups > 0 ? (100.0 * mem.vm.dtlb.hits / mem.vm.dtlb.lookups) : 100.0) << "%)\n";

        size_t activeMshrI = 0, activeMshrD = 0;
        for (const auto& m : mem.l1i.mshrs) if (m.valid) activeMshrI++;
        for (const auto& m : mem.l1d.mshrs) if (m.valid) activeMshrD++;
        oss << "MSHR Occupancy : L1I: " << activeMshrI << "/" << mem.l1i.cfg.mshrEntries
            << " | L1D: " << activeMshrD << "/" << mem.l1d.cfg.mshrEntries << " active\n";

        size_t activeSb = 0;
        for (const auto& sb : mem.storeBuffer) if (sb.valid) activeSb++;
        oss << "Store Buffer   : " << activeSb << "/" << mem.l1d.cfg.writeBufferEntries << " entries queued\n";

        oss << "------------------------------------------------------------------------\n";
        oss << "LATEST MEMORY ACCESS TRACE:\n";
        if (!mem.accessHistory.empty()) {
            const auto& last = mem.accessHistory.back();
            oss << "[Cycle " << std::setw(6) << last.cycleStart << "] "
                << (last.isWrite ? "WRITE" : "READ ") << " "
                << (last.isFetch ? "(INST) " : "(DATA) ")
                << "VAddr: " << toHex(static_cast<Word>(last.vaddr))
                << " -> PAddr: " << toHex(static_cast<Word>(last.paddr))
                << " | Served by: " << std::setw(5) << last.servedBy
                << " | Latency: " << last.totalLatency << " cycles\n";
        } else {
            oss << "(No memory accesses recorded yet)\n";
        }
        oss << "========================================================================\n";
        return oss.str();
    }
};

} // namespace visualizer
} // namespace risc201

#endif // RISC201_VISUALIZER_MEMORY_VISUALIZER_HPP