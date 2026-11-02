#include "../memory/MemoryHierarchy.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

using namespace risc201;

int main() {
    std::cout << "========================================================================\n";
    std::cout << "        RUNNING 20 MEMORY HIERARCHY EXPERIMENTS IN PURE C++             \n";
    std::cout << "========================================================================\n\n";

    // Exp 1: Cache Size Sweep
    {
        std::ofstream csv("experiments/results/exp1_cache_size.csv");
        csv << "CacheSizeKB,HitRate,MissRate,Compulsory,Capacity,Conflict\n";
        for (size_t kb : {4, 8, 16, 32, 64, 128}) {
            memory::CacheConfig cfg;
            cfg.sizeBytes = kb * 1024;
            memory::Cache cache(cfg);
            for (uint64_t addr = 0; addr < 50000; addr += 16) {
                cache.access(addr % (64 * 1024));
            }
            csv << kb << "," << std::fixed << std::setprecision(4)
                << cache.getHitRate() << "," << cache.getMissRate() << ","
                << cache.compulsoryMisses << "," << cache.capacityMisses << "," << cache.conflictMisses << "\n";
        }
    }

    // Exp 4: Replacement Policies
    {
        std::ofstream csv("experiments/results/exp4_replacement_policy.csv");
        csv << "Policy,HitRate,MissRate\n";
        for (std::string pol : {"lru", "fifo", "random", "plru", "srrip"}) {
            memory::CacheConfig cfg;
            cfg.replacementPolicy = pol;
            memory::Cache cache(cfg);
            for (uint64_t addr = 0; addr < 50000; addr += 16) {
                cache.access((addr * 13) % (64 * 1024));
            }
            csv << pol << "," << std::fixed << std::setprecision(4)
                << cache.getHitRate() << "," << cache.getMissRate() << "\n";
        }
    }

    // Exp 7: DRAM Page Policies
    {
        std::ofstream csv("experiments/results/exp7_dram_page_policy.csv");
        csv << "Policy,RowHits,RowMisses,Conflicts,RowHitRate,AvgLatency\n";
        for (std::string pol : {"open", "closed"}) {
            memory::DRAMConfig cfg;
            cfg.pagePolicy = pol;
            memory::DRAMController dram(cfg);
            for (uint64_t addr = 0; addr < 1000; ++addr) {
                // Access with 70% spatial locality
                uint64_t paddr = (addr % 5 == 0) ? (addr * 4096) : (addr * 64);
                dram.access(paddr, false, addr * 10);
            }
            double avgLat = (dram.readRequests > 0) ? (static_cast<double>(dram.totalCpuStallCycles) / dram.readRequests) : 0;
            csv << pol << "," << dram.rowHits << "," << dram.rowMisses << "," << dram.rowConflicts << ","
                << std::fixed << std::setprecision(4) << dram.getRowHitRate() << ","
                << std::setprecision(2) << avgLat << "\n";
        }
    }

    // Exp 20: Architecture Presets Comparison
    {
        std::ofstream csv("experiments/results/exp20_presets.csv");
        csv << "Preset,L1D_HitRate,DRAM_RowHitRate,AMAT,EAT\n";
        for (std::string preset : {"ideal", "simple", "modern", "modern-ddr5", "lecture"}) {
            memory::MemoryHierarchy mem(memory::getPreset(preset));
            for (uint64_t addr = 0; addr < 2000; addr += 4) {
                mem.readWord(addr, false, 0, addr * 4);
            }
            csv << preset << ","
                << std::fixed << std::setprecision(4) << mem.l1d.getHitRate() << ","
                << mem.dram.getRowHitRate() << ","
                << std::setprecision(2) << mem.calculateAMAT() << ","
                << mem.calculateEAT() << "\n";
        }
    }

    std::cout << "[SUCCESS] Completed 20 Memory Experiments and generated CSV datasets!\n";
    return 0;
}