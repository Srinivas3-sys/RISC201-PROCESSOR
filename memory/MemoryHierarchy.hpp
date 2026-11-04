#ifndef RISC201_MEMORY_HIERARCHY_HPP
#define RISC201_MEMORY_HIERARCHY_HPP

#include "Config.hpp"
#include "AddressMapping.hpp"
#include "Prefetcher.hpp"
#include "DRAM.hpp"
#include "Cache.hpp"
#include "VirtualMemory.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace risc201 {
namespace memory {

struct MemoryAccessRecord {
    uint64_t vaddr{0};
    uint64_t paddr{0};
    bool isWrite{false};
    bool isFetch{false};
    uint32_t sizeBytes{4};
    uint64_t cycleStart{0};
    uint64_t cycleEnd{0};
    uint32_t totalLatency{0};
    uint32_t criticalWordLatency{0};
    uint32_t tlbLatency{0};
    uint32_t cacheLatency{0};
    std::string servedBy{"L1D"}; // "L1I", "L1D", "L2", "L3", "DRAM", "SWAP"
};

struct StoreBufferEntry {
    uint32_t address{0};
    uint32_t data{0};
    uint64_t cycleIssued{0};
    bool valid{false};
};

class MemoryHierarchy {
public:
    MemorySystemConfig cfg;
    double cpuFreqGhz{3.2};

    VirtualMemorySubsystem vm;
    Cache l1i;
    Cache l1d;
    std::unique_ptr<Cache> l2;
    std::unique_ptr<Cache> l3;
    DRAMController dram;
    AddressMapper mapper;
    Prefetcher prefetcher;
    std::vector<StoreBufferEntry> storeBuffer;

    std::vector<uint8_t> rawMemory;
    std::vector<MemoryAccessRecord> accessHistory;
    uint64_t currentCpuCycle{0};

    explicit MemoryHierarchy(const MemorySystemConfig& config = getPreset("modern"))
        : cfg(config), cpuFreqGhz(config.cpuFreqGhz),
          vm(config.vm, config.cpuFreqGhz),
          l1i(config.l1i, config.vm.pageSizeBytes),
          l1d(config.l1d, config.vm.pageSizeBytes),
          dram(config.dram, config.cpuFreqGhz),
          mapper(config.dram, config.l1d.lineSizeBytes),
          prefetcher(config.prefetcher, config.l1d.lineSizeBytes, config.vm.pageSizeBytes) {

        if (cfg.hierarchy.l2Enabled) {
            l2 = std::make_unique<Cache>(cfg.l2, cfg.vm.pageSizeBytes);
        }
        if (cfg.hierarchy.l3Enabled) {
            l3 = std::make_unique<Cache>(cfg.l3, cfg.vm.pageSizeBytes);
        }

        rawMemory.resize(cfg.vm.physMemSizeBytes, 0);
        reset();
    }

    void reset() {
        vm.reset();
        l1i.reset();
        l1d.reset();
        if (l2) l2->reset();
        if (l3) l3->reset();
        dram.reset();
        prefetcher.reset();
        storeBuffer.clear();
        std::fill(rawMemory.begin(), rawMemory.end(), 0);
        accessHistory.clear();
        currentCpuCycle = 0;
    }

    void loadProgram(uint64_t baseVaddr, const std::vector<uint32_t>& words, uint32_t asid = 0) {
        size_t totalBytes = words.size() * 4;
        size_t pageSize = cfg.vm.pageSizeBytes;
        size_t numPages = (totalBytes + pageSize - 1) / pageSize;

        for (size_t p = 0; p < numPages; ++p) {
            uint64_t vpage = baseVaddr + (p * pageSize);
            uint64_t pbase = vpage;
            if (cfg.vm.enabled) {
                auto& pte = vm.mapPage(vpage, true, true, true, asid);
                pbase = static_cast<uint64_t>(pte.pfn) * pageSize;
            }

            size_t startByte = p * pageSize;
            size_t endByte = std::min(totalBytes, (p + 1) * pageSize);

            for (size_t b = startByte; b < endByte; b += 4) {
                size_t wordIdx = b / 4;
                if (wordIdx < words.size()) {
                    uint32_t w = words[wordIdx];
                    uint64_t targetPaddr = pbase + (b - startByte);
                    if (targetPaddr + 4 <= rawMemory.size()) {
                        rawMemory[targetPaddr + 0] = static_cast<uint8_t>(w & 0xFF);
                        rawMemory[targetPaddr + 1] = static_cast<uint8_t>((w >> 8) & 0xFF);
                        rawMemory[targetPaddr + 2] = static_cast<uint8_t>((w >> 16) & 0xFF);
                        rawMemory[targetPaddr + 3] = static_cast<uint8_t>((w >> 24) & 0xFF);
                    }
                }
            }
        }

        // Eagerly map initial stack frame and common benchmark data pages
        if (cfg.vm.enabled) {
            vm.mapPage(0x000FF000, true, true, false, asid);
            vm.mapPage(0x000FE000, true, true, false, asid);
            vm.mapPage(0x00001000, true, true, false, asid);
            vm.mapPage(0x00002000, true, true, false, asid);
            vm.mapPage(0x00003000, true, true, false, asid);
            vm.mapPage(0x00004000, true, true, false, asid);
        }
    }

    std::pair<uint32_t, uint32_t> readWord(uint64_t vaddr, bool isFetch = false, uint32_t pc = 0, uint64_t cycle = 0, uint32_t asid = 0) {
        currentCpuCycle = cycle;

        // 1. Address Translation
        TranslationResult trans = vm.translate(vaddr, false, isFetch, asid);
        uint64_t paddr = trans.paddr;
        uint32_t tlbLatency = trans.latencyCycles;

        // 0. Store Buffer Bypass (Store-to-Load Forwarding)
        if (!isFetch) {
            for (int i = static_cast<int>(storeBuffer.size()) - 1; i >= 0; --i) {
                if (storeBuffer[i].valid && storeBuffer[i].address == static_cast<uint32_t>(paddr)) {
                    return {storeBuffer[i].data, 1}; // 1-cycle bypass hit
                }
            }
        }

        // 2. Cache Hierarchy Access
        uint32_t cacheLatency = 0;
        uint32_t critLatency = 0;
        std::string servedBy = "L1D";

        if (isFetch) {
            servedBy = "L1I";
            CacheAccessResult res1 = l1i.access(paddr, false, pc);
            cacheLatency += res1.latencyCycles;
            critLatency += res1.criticalWordLatency;

            if (!res1.hit) {
                // Miss in L1I -> check L2
                if (l2) {
                    servedBy = "L2";
                    CacheAccessResult res2 = l2->access(paddr, false, pc);
                    cacheLatency += res2.latencyCycles;
                    critLatency += res2.criticalWordLatency;

                    if (!res2.hit) {
                        // Miss in L2 -> check L3
                        if (l3) {
                            servedBy = "L3";
                            CacheAccessResult res3 = l3->access(paddr, false, pc);
                            cacheLatency += res3.latencyCycles;
                            critLatency += res3.criticalWordLatency;

                            if (!res3.hit) {
                                servedBy = "DRAM";
                                uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                                cacheLatency += static_cast<uint32_t>(dramLat);
                                critLatency += static_cast<uint32_t>(dramLat);
                            }
                        } else {
                            servedBy = "DRAM";
                            uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                            cacheLatency += static_cast<uint32_t>(dramLat);
                            critLatency += static_cast<uint32_t>(dramLat);
                        }
                    }
                } else {
                    servedBy = "DRAM";
                    uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                    cacheLatency += static_cast<uint32_t>(dramLat);
                    critLatency += static_cast<uint32_t>(dramLat);
                }

                // Trigger instruction prefetcher
                auto prefLines = prefetcher.onAccess(paddr, pc, true);
                for (uint64_t prefAddr : prefLines) {
                    l1i.access(prefAddr, false, pc);
                }
            } else {
                prefetcher.onAccess(paddr, pc, false);
            }
        } else {
            // Data read: L1D
            servedBy = "L1D";
            CacheAccessResult res1 = l1d.access(paddr, false, pc);
            cacheLatency += res1.latencyCycles;
            critLatency += res1.criticalWordLatency;

            if (!res1.hit) {
                // Check L2
                if (l2) {
                    servedBy = "L2";
                    CacheAccessResult res2 = l2->access(paddr, false, pc);
                    cacheLatency += res2.latencyCycles;
                    critLatency += res2.criticalWordLatency;

                    if (!res2.hit) {
                        // Check L3
                        if (l3) {
                            servedBy = "L3";
                            CacheAccessResult res3 = l3->access(paddr, false, pc);
                            cacheLatency += res3.latencyCycles;
                            critLatency += res3.criticalWordLatency;

                            if (!res3.hit) {
                                servedBy = "DRAM";
                                uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                                cacheLatency += static_cast<uint32_t>(dramLat);
                                critLatency += static_cast<uint32_t>(dramLat);
                            }
                        } else {
                            servedBy = "DRAM";
                            uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                            cacheLatency += static_cast<uint32_t>(dramLat);
                            critLatency += static_cast<uint32_t>(dramLat);
                        }
                    }
                } else {
                    servedBy = "DRAM";
                    uint64_t dramLat = dram.access(paddr, false, currentCpuCycle + cacheLatency);
                    cacheLatency += static_cast<uint32_t>(dramLat);
                    critLatency += static_cast<uint32_t>(dramLat);
                }

                // Trigger prefetcher
                auto prefLines = prefetcher.onAccess(paddr, pc, true);
                for (uint64_t prefAddr : prefLines) {
                    l1d.access(prefAddr, false, pc);
                }
            } else {
                prefetcher.onAccess(paddr, pc, false);
            }
        }

        bool isL1Hit = (servedBy == "L1I" || servedBy == "L1D");
        uint32_t totalLatency = isL1Hit ? 1 : (tlbLatency + cacheLatency);
        uint32_t effectiveCritLatency = isL1Hit ? 1 : (tlbLatency + critLatency);

        // 3. Read 32-bit word from raw physical memory
        uint32_t wordVal = 0;
        if (paddr + 4 <= rawMemory.size()) {
            wordVal = static_cast<uint32_t>(rawMemory[paddr]) |
                     (static_cast<uint32_t>(rawMemory[paddr + 1]) << 8) |
                     (static_cast<uint32_t>(rawMemory[paddr + 2]) << 16) |
                     (static_cast<uint32_t>(rawMemory[paddr + 3]) << 24);
        }

        // Record trace
        if (accessHistory.size() < 1000) {
            MemoryAccessRecord rec;
            rec.vaddr = vaddr;
            rec.paddr = paddr;
            rec.isWrite = false;
            rec.isFetch = isFetch;
            rec.sizeBytes = 4;
            rec.cycleStart = cycle;
            rec.totalLatency = totalLatency;
            rec.criticalWordLatency = effectiveCritLatency;
            rec.tlbLatency = tlbLatency;
            rec.cacheLatency = cacheLatency;
            rec.servedBy = servedBy;
            accessHistory.push_back(rec);
        }

        uint32_t retLatency = (cfg.hierarchy.criticalWordFirst && !isL1Hit) ? effectiveCritLatency : totalLatency;
        return {wordVal, std::max(uint32_t(1), retLatency)};
    }

    uint32_t writeWord(uint64_t vaddr, uint32_t data, uint32_t pc = 0, uint64_t cycle = 0, uint32_t asid = 0) {
        currentCpuCycle = cycle;

        // 1. Translation
        TranslationResult trans = vm.translate(vaddr, true, false, asid);
        uint64_t paddr = trans.paddr;
        uint32_t tlbLatency = trans.latencyCycles;

        // 2. Cache Access
        uint32_t cacheLatency = 0;
        std::string servedBy = "L1D";

        CacheAccessResult res1 = l1d.access(paddr, true, pc);

        if (res1.hit) {
            cacheLatency = 1;
        } else {
            // Store Buffer: buffer write to allow 1-cycle store retire without core stall
            if (storeBuffer.size() < l1d.cfg.writeBufferEntries) {
                StoreBufferEntry e;
                e.address = static_cast<uint32_t>(paddr);
                e.data = data;
                e.cycleIssued = cycle;
                e.valid = true;
                storeBuffer.push_back(e);
                cacheLatency = 1;
            } else {
                // Buffer is full, wait for service
                cacheLatency += res1.latencyCycles;
                if (l2) {
                    servedBy = "L2";
                    CacheAccessResult res2 = l2->access(paddr, true, pc);
                    cacheLatency += res2.latencyCycles;
                    if (!res2.hit) {
                        if (l3) {
                            servedBy = "L3";
                            CacheAccessResult res3 = l3->access(paddr, true, pc);
                            cacheLatency += res3.latencyCycles;
                            if (!res3.hit) {
                                servedBy = "DRAM";
                                uint64_t dramLat = dram.access(paddr, true, currentCpuCycle + cacheLatency);
                                cacheLatency += static_cast<uint32_t>(dramLat);
                            }
                        } else {
                            servedBy = "DRAM";
                            uint64_t dramLat = dram.access(paddr, true, currentCpuCycle + cacheLatency);
                            cacheLatency += static_cast<uint32_t>(dramLat);
                        }
                    }
                } else {
                    servedBy = "DRAM";
                    uint64_t dramLat = dram.access(paddr, true, currentCpuCycle + cacheLatency);
                    cacheLatency += static_cast<uint32_t>(dramLat);
                }
            }
        }

        uint32_t totalLatency = (res1.hit || storeBuffer.size() <= l1d.cfg.writeBufferEntries) ? 1 : (tlbLatency + cacheLatency);

        // 3. Update raw memory
        if (paddr + 4 <= rawMemory.size()) {
            rawMemory[paddr]     = static_cast<uint8_t>(data & 0xFF);
            rawMemory[paddr + 1] = static_cast<uint8_t>((data >> 8) & 0xFF);
            rawMemory[paddr + 2] = static_cast<uint8_t>((data >> 16) & 0xFF);
            rawMemory[paddr + 3] = static_cast<uint8_t>((data >> 24) & 0xFF);
        }

        if (accessHistory.size() < 1000) {
            MemoryAccessRecord rec;
            rec.vaddr = vaddr;
            rec.paddr = paddr;
            rec.isWrite = true;
            rec.isFetch = false;
            rec.sizeBytes = 4;
            rec.cycleStart = cycle;
            rec.totalLatency = totalLatency;
            rec.tlbLatency = tlbLatency;
            rec.cacheLatency = cacheLatency;
            rec.servedBy = servedBy;
            accessHistory.push_back(rec);
        }

        return totalLatency;
    }

    // AMAT (Average Memory Access Time) Calculation
    double calculateAMAT() const {
        // AMAT = L1_hit_time + L1_miss_rate * (L2_hit_time + L2_miss_rate * (L3_hit_time + L3_miss_rate * DRAM_latency))
        double dramAvgLatency = (dram.cfg.pagePolicy == "open") ? 60.0 : 80.0;
        double l3Amat = dramAvgLatency;
        if (l3) {
            l3Amat = l3->cfg.hitLatencyCycles + (l3->getMissRate() * dramAvgLatency);
        }
        double l2Amat = l3Amat;
        if (l2) {
            l2Amat = l2->cfg.hitLatencyCycles + (l2->getMissRate() * l3Amat);
        }
        double l1Amat = l1d.cfg.hitLatencyCycles + (l1d.getMissRate() * l2Amat);
        return l1Amat;
    }

    // EAT (Effective Access Time) Calculation
    double calculateEAT() const {
        // EAT = T_translation + AMAT
        double tlbHitTime = cfg.vm.tlbHitLatencyCycles;
        double tlbMissRate = 0.05; // ~5% typical
        double walkTime = (cfg.vm.ptLevels * 20.0);
        double pageFaultRate = 0.0001; // 0.01%
        double faultServiceTime = cfg.vm.majorFaultLatencyCycles;

        double tTrans = tlbHitTime + tlbMissRate * (walkTime + pageFaultRate * faultServiceTime);
        return tTrans + calculateAMAT();
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_HIERARCHY_HPP