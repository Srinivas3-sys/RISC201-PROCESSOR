#ifndef RISC201_MEMORY_CONFIG_HPP
#define RISC201_MEMORY_CONFIG_HPP

#include <cstdint>
#include <string>
#include <stdexcept>
#include <algorithm>

namespace risc201 {
namespace memory {

struct CacheConfig {
    std::string name{"L1D"};
    size_t sizeBytes{32 * 1024};           // 32 KB default
    size_t lineSizeBytes{64};              // 64 B default
    size_t associativity{8};               // 8-way default
    uint32_t hitLatencyCycles{4};          // 4-cycle hit default
    std::string replacementPolicy{"lru"};  // lru, fifo, random, plru, srrip
    std::string insertionPolicy{"mru"};    // mru, lru, bip
    std::string writePolicy{"wb_wa"};      // wb_wa (write-back + write-allocate) or wt_nwa (write-through)
    std::string indexingMode{"vipt"};      // pipt, vipt, vivt
    size_t mshrEntries{8};                 // Number of MSHRs
    size_t writeBufferEntries{16};
    size_t victimCacheEntries{0};          // 0 = disabled
    uint32_t srripBits{2};                 // For SRRIP (2 bits = 0..3)
    double bipEpsilon{0.03125};            // 1/32 probability to insert at MRU for BIP
};

struct DRAMTimingConfig {
    double tCK_ns{0.625};                  // DDR4-3200 has 1600 MHz clock => 0.625 ns
    uint32_t tRCD{22};                     // Row to Column Delay
    uint32_t tRP{22};                      // Row Precharge
    uint32_t tCL{22};                      // CAS Latency
    uint32_t tRAS{52};                     // Row Active Time (tRC = tRAS + tRP = 74)
    uint32_t tRC{74};
    uint32_t tWR{24};                      // Write Recovery
    uint32_t tWTR_S{4};                    // Write to Read (short, diff bank group)
    uint32_t tWTR_L{12};                   // Write to Read (long, same bank group)
    uint32_t tRTP{12};                     // Read to Precharge
    uint32_t tFAW{34};                     // Four Activate Window
    uint32_t tRRD_S{6};                    // Activate to Activate (diff bank group)
    uint32_t tRRD_L{8};                    // Activate to Activate (same bank group)
    uint32_t tCCD_S{4};                    // CAS to CAS (diff bank group)
    uint32_t tCCD_L{8};                    // CAS to CAS (same bank group)
    uint32_t tBURST{4};                    // Burst transfer duration in DRAM cycles
    uint32_t burstLength{8};               // 8 words per burst (64 bytes on 64-bit bus)
    double tRFC_ns{350.0};                 // Refresh cycle time
    double tREFI_ns{7800.0};               // Refresh interval (7.8 us)
};

struct DRAMConfig {
    std::string dramType{"DDR4-3200"};
    size_t channels{1};
    size_t subchannelsPerChannel{1};       // DDR5 uses 2 subchannels
    size_t ranksPerChannel{1};
    size_t bankGroupsPerRank{4};
    size_t banksPerGroup{4};
    size_t rowsPerBank{65536};
    size_t colsPerRow{1024};
    size_t busWidthBytes{8};               // 64-bit channel bus
    std::string pagePolicy{"open"};        // "open" or "closed"
    std::string schedulingPolicy{"fr_fcfs"}; // "fcfs" or "fr_fcfs"
    bool xorHashMapping{false};            // XOR address hash to reduce bank conflicts
    DRAMTimingConfig timings{};
};

struct VMConfig {
    bool enabled{true};
    size_t vaBits{32};                     // Virtual address width (32-bit SimpleRisc)
    size_t paBits{32};                     // Physical address width
    size_t pageSizeBytes{4096};            // 4 KB standard page size
    size_t ptLevels{2};                    // 2-level (10+10+12) or 4-level (9+9+9+9+12)

    // TLBs
    size_t itlbEntries{64};
    size_t dtlbEntries{64};
    size_t itlbAssoc{4};
    size_t dtlbAssoc{4};
    size_t stlbEntries{1024};              // Unified L2 TLB (STLB)
    size_t stlbAssoc{8};
    size_t pwcEntries{16};                 // Page Walk Cache entries

    // Latencies
    uint32_t tlbHitLatencyCycles{1};
    uint32_t stlbHitLatencyCycles{7};
    uint32_t pwcHitLatencyCycles{3};
    uint32_t minorFaultLatencyCycles{1200}; // Zero-fill demand page
    uint32_t majorFaultLatencyCycles{300000}; // Swap-in from SSD/Disk (~100us at 3GHz)

    std::string replacementPolicy{"lru"};  // lru, fifo, clock, optimal
    size_t physMemSizeBytes{64 * 1024 * 1024}; // 64 MB physical memory pool
};

struct PrefetchConfig {
    bool enabled{true};
    std::string prefetcherType{"stream"};  // none, next_line, stride, stream
    size_t lookaheadLines{2};
    size_t streamBuffers{4};
    size_t rptEntries{16};                 // Reference Prediction Table entries for stride
    size_t prefetchDegree{2};
    bool crossPageBoundary{false};        // Modern CPUs never prefetch across page boundary
};

struct HierarchyConfig {
    bool l2Enabled{true};
    bool l3Enabled{true};
    std::string inclusionPolicy{"inclusive"}; // inclusive, exclusive, nine
    bool criticalWordFirst{true};
    bool earlyRestart{true};
};

struct MemorySystemConfig {
    std::string presetName{"modern"};
    double cpuFreqGhz{3.2};

    CacheConfig l1i{};
    CacheConfig l1d{};
    CacheConfig l2{};
    CacheConfig l3{};
    DRAMConfig dram{};
    VMConfig vm{};
    PrefetchConfig prefetcher{};
    HierarchyConfig hierarchy{};

    MemorySystemConfig() {
        // Defaults for modern DDR4 system
        l1i.name = "L1I";
        l1i.sizeBytes = 32 * 1024;
        l1i.associativity = 8;
        l1i.lineSizeBytes = 64;
        l1i.hitLatencyCycles = 1;
        l1i.indexingMode = "vipt";

        l1d.name = "L1D";
        l1d.sizeBytes = 32 * 1024;
        l1d.associativity = 8;
        l1d.lineSizeBytes = 64;
        l1d.hitLatencyCycles = 1;
        l1d.indexingMode = "vipt";

        prefetcher.enabled = true;
        prefetcher.prefetcherType = "next_line";
        prefetcher.lookaheadLines = 2;

        l2.name = "L2";
        l2.sizeBytes = 512 * 1024; // 512 KB
        l2.associativity = 8;
        l2.lineSizeBytes = 64;
        l2.hitLatencyCycles = 14;
        l2.indexingMode = "pipt";

        l3.name = "L3";
        l3.sizeBytes = 8 * 1024 * 1024; // 8 MB
        l3.associativity = 16;
        l3.lineSizeBytes = 64;
        l3.hitLatencyCycles = 40;
        l3.indexingMode = "pipt";
    }
};

inline MemorySystemConfig getPreset(const std::string& name) {
    std::string preset = name;
    std::transform(preset.begin(), preset.end(), preset.begin(), ::tolower);

    if (preset == "ideal") {
        MemorySystemConfig cfg;
        cfg.presetName = "ideal";
        cfg.cpuFreqGhz = 3.0;
        cfg.l1i.hitLatencyCycles = 0;
        cfg.l1d.hitLatencyCycles = 0;
        cfg.l2.hitLatencyCycles = 0;
        cfg.l3.hitLatencyCycles = 0;
        cfg.hierarchy.l2Enabled = false;
        cfg.hierarchy.l3Enabled = false;
        cfg.prefetcher.enabled = false;
        cfg.vm.enabled = false;
        cfg.dram.timings.tRCD = 0;
        cfg.dram.timings.tRP = 0;
        cfg.dram.timings.tCL = 0;
        cfg.dram.timings.tBURST = 0;
        return cfg;
    } else if (preset == "simple") {
        MemorySystemConfig cfg;
        cfg.presetName = "simple";
        cfg.cpuFreqGhz = 2.0;
        cfg.l1i.sizeBytes = 32 * 1024;
        cfg.l1d.sizeBytes = 32 * 1024;
        cfg.l1i.hitLatencyCycles = 1;
        cfg.l1d.hitLatencyCycles = 1;
        cfg.hierarchy.l2Enabled = false;
        cfg.hierarchy.l3Enabled = false;
        cfg.prefetcher.enabled = false;
        cfg.vm.enabled = false;
        cfg.dram.dramType = "SimpleDRAM";
        cfg.dram.pagePolicy = "closed";
        cfg.dram.timings.tCK_ns = 1.0;
        cfg.dram.timings.tRCD = 15;
        cfg.dram.timings.tRP = 15;
        cfg.dram.timings.tCL = 15;
        cfg.dram.timings.tBURST = 4;
        return cfg;
    } else if (preset == "modern") {
        MemorySystemConfig cfg;
        cfg.presetName = "modern";
        cfg.cpuFreqGhz = 3.2;
        return cfg;
    } else if (preset == "modern-ddr5") {
        MemorySystemConfig cfg;
        cfg.presetName = "modern-ddr5";
        cfg.cpuFreqGhz = 4.0;
        cfg.dram.dramType = "DDR5-4800";
        cfg.dram.channels = 1;
        cfg.dram.subchannelsPerChannel = 2; // 2 independent 32-bit subchannels
        cfg.dram.busWidthBytes = 4;        // 32-bit per subchannel
        cfg.dram.bankGroupsPerRank = 8;    // DDR5 doubles bank groups
        cfg.dram.banksPerGroup = 4;
        cfg.dram.timings.tCK_ns = 0.416;
        cfg.dram.timings.tRCD = 34;
        cfg.dram.timings.tRP = 34;
        cfg.dram.timings.tCL = 40;
        cfg.dram.timings.tRAS = 77;
        cfg.dram.timings.tRC = 111;
        cfg.dram.timings.burstLength = 16;
        cfg.dram.timings.tBURST = 8;
        cfg.dram.timings.tRRD_S = 8;
        cfg.dram.timings.tRRD_L = 12;
        cfg.dram.timings.tCCD_S = 8;
        cfg.dram.timings.tCCD_L = 16;
        return cfg;
    } else if (preset == "lecture") {
        MemorySystemConfig cfg;
        cfg.presetName = "lecture";
        cfg.cpuFreqGhz = 3.3;
        cfg.l1i.hitLatencyCycles = 1;
        cfg.l1d.hitLatencyCycles = 1;
        cfg.l2.hitLatencyCycles = 11;
        cfg.l3.hitLatencyCycles = 42;
        cfg.dram.dramType = "LectureDRAM";
        cfg.dram.timings.tCK_ns = 1.0;
        cfg.dram.timings.tRCD = 20;
        cfg.dram.timings.tRP = 20;
        cfg.dram.timings.tCL = 20;
        cfg.dram.timings.tBURST = 4;
        cfg.vm.vaBits = 32;
        cfg.vm.ptLevels = 2;
        cfg.vm.minorFaultLatencyCycles = 1500;
        cfg.vm.majorFaultLatencyCycles = 330000;
        return cfg;
    }

    throw std::invalid_argument("Unknown memory preset: '" + name + "'. Choose: ideal, simple, modern, modern-ddr5, lecture.");
}

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_CONFIG_HPP