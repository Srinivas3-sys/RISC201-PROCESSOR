#ifndef RISC201_MEMORY_VIRTUAL_MEMORY_HPP
#define RISC201_MEMORY_VIRTUAL_MEMORY_HPP

#include "Config.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>
#include <stdexcept>
#include <iostream>

namespace risc201 {
namespace memory {

struct PageTableEntry {
    uint32_t pfn{0};           // Physical Frame Number
    bool valid{false};
    bool dirty{false};
    bool referenced{false};
    bool readable{true};
    bool writable{true};
    bool executable{true};
    uint32_t asid{0};
    bool inSwap{false};
    uint64_t swapOffset{0};
};

struct TLBEntry {
    uint64_t vpn{0};
    uint32_t pfn{0};
    bool valid{false};
    bool dirty{false};
    bool writable{true};
    bool executable{true};
    uint32_t asid{0};
    uint64_t lruAge{0};
};

class GenericTLB {
public:
    std::string name{"TLB"};
    size_t numEntries{64};
    size_t associativity{4};
    size_t numSets{16};
    uint32_t hitLatencyCycles{1};
    std::vector<std::vector<TLBEntry>> sets;
    uint64_t accessSequence{0};

    uint64_t lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};

    GenericTLB(const std::string& n, size_t entries, size_t assoc, uint32_t latency)
        : name(n), numEntries(entries), associativity(assoc), hitLatencyCycles(latency) {
        numSets = numEntries / associativity;
        if (numSets == 0) numSets = 1;
        sets.resize(numSets, std::vector<TLBEntry>(associativity));
    }

    void reset() {
        for (auto& s : sets) {
            for (auto& e : s) {
                e.valid = false;
                e.vpn = 0;
                e.pfn = 0;
                e.lruAge = 0;
            }
        }
        lookups = 0;
        hits = 0;
        misses = 0;
        accessSequence = 0;
    }

    bool lookup(uint64_t vpn, uint32_t asid, TLBEntry& outEntry) {
        lookups++;
        accessSequence++;
        size_t setIdx = (vpn ^ asid) % numSets;
        auto& set = sets[setIdx];

        for (auto& e : set) {
            if (e.valid && e.vpn == vpn && (e.asid == asid || e.asid == 0)) {
                hits++;
                e.lruAge = accessSequence;
                outEntry = e;
                return true;
            }
        }

        misses++;
        return false;
    }

    void insert(uint64_t vpn, uint32_t pfn, uint32_t asid, bool writable, bool exec, bool dirty = false) {
        accessSequence++;
        size_t setIdx = (vpn ^ asid) % numSets;
        auto& set = sets[setIdx];

        // 1. Check for invalid or matching entry
        for (auto& e : set) {
            if (!e.valid || (e.vpn == vpn && e.asid == asid)) {
                e.valid = true;
                e.vpn = vpn;
                e.pfn = pfn;
                e.asid = asid;
                e.writable = writable;
                e.executable = exec;
                e.dirty = dirty;
                e.lruAge = accessSequence;
                return;
            }
        }

        // 2. Evict LRU
        size_t lruWay = 0;
        uint64_t minAge = set[0].lruAge;
        for (size_t w = 1; w < associativity; ++w) {
            if (set[w].lruAge < minAge) {
                minAge = set[w].lruAge;
                lruWay = w;
            }
        }

        auto& victim = set[lruWay];
        victim.valid = true;
        victim.vpn = vpn;
        victim.pfn = pfn;
        victim.asid = asid;
        victim.writable = writable;
        victim.executable = exec;
        victim.dirty = dirty;
        victim.lruAge = accessSequence;
    }

    void invalidate(uint64_t vpn, uint32_t asid) {
        size_t setIdx = (vpn ^ asid) % numSets;
        for (auto& e : sets[setIdx]) {
            if (e.valid && e.vpn == vpn && (e.asid == asid || e.asid == 0)) {
                e.valid = false;
            }
        }
    }
};

struct TranslationResult {
    bool success{false};
    uint64_t paddr{0};
    uint32_t latencyCycles{0};
    bool itlbHit{false};
    bool dtlbHit{false};
    bool stlbHit{false};
    bool pwcHit{false};
    bool pageWalkOccurred{false};
    bool pageFaultOccurred{false};
    std::string faultType{""}; // "minor", "major", "permission"
};

class PageFaultException : public std::runtime_error {
public:
    uint64_t vaddr;
    std::string type;
    PageFaultException(uint64_t addr, const std::string& t)
        : std::runtime_error("Page Fault at 0x" + std::to_string(addr) + " (" + t + ")"),
          vaddr(addr), type(t) {}
};

class VirtualMemorySubsystem {
public:
    VMConfig cfg;
    double cpuFreqGhz{3.2};

    GenericTLB itlb;
    GenericTLB dtlb;
    GenericTLB stlb;

    // Page Walk Cache: VPN prefix -> partial walk base
    std::unordered_map<uint64_t, uint64_t> pwc;

    // Radix page table: [asid][vpn] -> PTE
    std::unordered_map<uint64_t, PageTableEntry> pageTable;

    // Physical Frame Management
    size_t totalFrames{16384}; // 64 MB / 4 KB = 16384 frames
    uint32_t nextFreePfn{1};   // Frame 0 reserved
    std::vector<uint64_t> allocatedVpns; // For Clock/FIFO replacement
    size_t clockHand{0};

    // Statistics
    uint64_t totalTranslations{0};
    uint64_t itlbHits{0};
    uint64_t dtlbHits{0};
    uint64_t stlbHits{0};
    uint64_t pwcHits{0};
    uint64_t pageWalks{0};
    uint64_t minorFaults{0};
    uint64_t majorFaults{0};
    uint64_t totalTranslationCycles{0};

    explicit VirtualMemorySubsystem(const VMConfig& vcfg = VMConfig(), double cpuFreq = 3.2)
        : cfg(vcfg), cpuFreqGhz(cpuFreq),
          itlb("ITLB", vcfg.itlbEntries, vcfg.itlbAssoc, vcfg.tlbHitLatencyCycles),
          dtlb("DTLB", vcfg.dtlbEntries, vcfg.dtlbAssoc, vcfg.tlbHitLatencyCycles),
          stlb("STLB", vcfg.stlbEntries, vcfg.stlbAssoc, vcfg.stlbHitLatencyCycles) {
        totalFrames = cfg.physMemSizeBytes / cfg.pageSizeBytes;
        reset();
    }

    void reset() {
        itlb.reset();
        dtlb.reset();
        stlb.reset();
        pwc.clear();
        pageTable.clear();
        allocatedVpns.clear();
        nextFreePfn = 1;
        clockHand = 0;

        totalTranslations = 0;
        itlbHits = 0;
        dtlbHits = 0;
        stlbHits = 0;
        pwcHits = 0;
        pageWalks = 0;
        minorFaults = 0;
        majorFaults = 0;
        totalTranslationCycles = 0;
    }

    uint64_t makeKey(uint64_t vpn, uint32_t asid) const {
        return (static_cast<uint64_t>(asid) << 48) | (vpn & 0xFFFFFFFFFFFFULL);
    }

    PageTableEntry& mapPage(uint64_t vaddr, bool read = true, bool write = true, bool exec = true, uint32_t asid = 0) {
        uint64_t vpn = vaddr / cfg.pageSizeBytes;
        uint64_t key = makeKey(vpn, asid);

        auto it = pageTable.find(key);
        if (it != pageTable.end()) {
            return it->second;
        }

        // Allocate physical frame
        uint32_t pfn = allocateFrame(vpn, asid);

        PageTableEntry pte;
        pte.pfn = pfn;
        pte.valid = true;
        pte.readable = read;
        pte.writable = write;
        pte.executable = exec;
        pte.asid = asid;
        pte.dirty = false;
        pte.referenced = true;
        pte.inSwap = false;

        pageTable[key] = pte;
        return pageTable[key];
    }

    uint32_t allocateFrame(uint64_t vpn, uint32_t asid) {
        if (nextFreePfn < totalFrames) {
            uint32_t pfn = nextFreePfn++;
            allocatedVpns.push_back(makeKey(vpn, asid));
            return pfn;
        }

        // Out of physical frames: execute Clock page replacement algorithm
        if (allocatedVpns.empty()) return 1;

        while (true) {
            uint64_t candKey = allocatedVpns[clockHand];
            clockHand = (clockHand + 1) % allocatedVpns.size();

            auto it = pageTable.find(candKey);
            if (it != pageTable.end() && it->second.valid) {
                if (it->second.referenced) {
                    it->second.referenced = false; // Give second chance
                } else {
                    // Evict candidate
                    it->second.valid = false;
                    it->second.inSwap = true;
                    it->second.swapOffset = it->second.pfn * cfg.pageSizeBytes;

                    // Invalidate in TLBs
                    uint64_t candVpn = candKey & 0xFFFFFFFFFFFFULL;
                    uint32_t candAsid = static_cast<uint32_t>(candKey >> 48);
                    itlb.invalidate(candVpn, candAsid);
                    dtlb.invalidate(candVpn, candAsid);
                    stlb.invalidate(candVpn, candAsid);

                    uint32_t recycledPfn = it->second.pfn;
                    return recycledPfn;
                }
            }
        }
    }

    TranslationResult translate(uint64_t vaddr, bool isWrite = false, bool isFetch = false, uint32_t asid = 0) {
        totalTranslations++;
        TranslationResult res;

        if (!cfg.enabled) {
            res.success = true;
            res.paddr = vaddr;
            res.latencyCycles = 0;
            return res;
        }

        uint64_t vpn = vaddr / cfg.pageSizeBytes;
        uint32_t offset = static_cast<uint32_t>(vaddr % cfg.pageSizeBytes);
        uint64_t key = makeKey(vpn, asid);

        TLBEntry tlbEntry;

        // 1. Check L1 TLB (ITLB for fetch, DTLB for data)
        GenericTLB& l1tlb = isFetch ? itlb : dtlb;
        if (l1tlb.lookup(vpn, asid, tlbEntry)) {
            if (isFetch) itlbHits++;
            else dtlbHits++;
            res.itlbHit = isFetch;
            res.dtlbHit = !isFetch;
            res.latencyCycles = cfg.tlbHitLatencyCycles;
            res.paddr = (static_cast<uint64_t>(tlbEntry.pfn) * cfg.pageSizeBytes) + offset;
            res.success = true;
            totalTranslationCycles += res.latencyCycles;
            return res;
        }

        // 2. Check unified L2 TLB (STLB)
        if (stlb.lookup(vpn, asid, tlbEntry)) {
            stlbHits++;
            res.stlbHit = true;
            res.latencyCycles = cfg.tlbHitLatencyCycles + cfg.stlbHitLatencyCycles;
            res.paddr = (static_cast<uint64_t>(tlbEntry.pfn) * cfg.pageSizeBytes) + offset;
            res.success = true;

            // Fill L1 TLB
            l1tlb.insert(vpn, tlbEntry.pfn, asid, tlbEntry.writable, tlbEntry.executable, tlbEntry.dirty);
            totalTranslationCycles += res.latencyCycles;
            return res;
        }

        // 3. Hardware Page-Table Walk
        pageWalks++;
        res.pageWalkOccurred = true;
        uint32_t walkLatency = cfg.tlbHitLatencyCycles + cfg.stlbHitLatencyCycles;

        // Check Page-Walk Cache (PWC)
        uint64_t pwcKey = vpn >> 10;
        if (pwc.find(pwcKey) != pwc.end()) {
            pwcHits++;
            res.pwcHit = true;
            walkLatency += cfg.pwcHitLatencyCycles;
        } else {
            // Full multi-level walk (e.g. 2 memory references for 2-level radix tree)
            walkLatency += (static_cast<uint32_t>(cfg.ptLevels) * 20); // ~20 cycles per level access
            if (pwc.size() < cfg.pwcEntries) {
                pwc[pwcKey] = 1;
            }
        }

        // Lookup PTE in page table
        auto it = pageTable.find(key);
        if (it == pageTable.end() || !it->second.valid) {
            // Page Fault!
            res.pageFaultOccurred = true;

            if (it != pageTable.end() && it->second.inSwap) {
                // Major Page Fault: page is in swap storage
                majorFaults++;
                res.faultType = "major";
                walkLatency += cfg.majorFaultLatencyCycles;

                uint32_t pfn = allocateFrame(vpn, asid);
                it->second.pfn = pfn;
                it->second.valid = true;
                it->second.inSwap = false;
                it->second.referenced = true;
            } else {
                // Minor Page Fault: allocate new zero-filled demand page
                minorFaults++;
                res.faultType = "minor";
                walkLatency += cfg.minorFaultLatencyCycles;
                mapPage(vaddr, true, true, isFetch, asid);
            }
        }

        PageTableEntry& pte = pageTable[key];
        pte.referenced = true;
        if (isWrite) pte.dirty = true;

        // Fill STLB and L1 TLB
        stlb.insert(vpn, pte.pfn, asid, pte.writable, pte.executable, pte.dirty);
        l1tlb.insert(vpn, pte.pfn, asid, pte.writable, pte.executable, pte.dirty);

        res.latencyCycles = walkLatency;
        res.paddr = (static_cast<uint64_t>(pte.pfn) * cfg.pageSizeBytes) + offset;
        res.success = true;
        totalTranslationCycles += res.latencyCycles;

        return res;
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_VIRTUAL_MEMORY_HPP