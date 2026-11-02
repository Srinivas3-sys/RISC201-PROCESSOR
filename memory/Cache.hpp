#ifndef RISC201_MEMORY_CACHE_HPP
#define RISC201_MEMORY_CACHE_HPP

#include "Config.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <iostream>

namespace risc201 {
namespace memory {

struct CacheLine {
    uint64_t tag{0};
    bool valid{false};
    bool dirty{false};
    uint64_t lruAge{0};
    uint64_t fifoSeq{0};
    uint32_t rrpv{3}; // 2-bit SRRIP: 0..3 (3 is distant re-reference)
    std::vector<uint8_t> data;

    explicit CacheLine(size_t lineSize = 64) : data(lineSize, 0) {}
};

struct CacheSet {
    std::vector<CacheLine> lines;
    uint32_t plruBits{0}; // Bit tree for up to 16-way associative PLRU

    explicit CacheSet(size_t assoc = 8, size_t lineSize = 64) {
        lines.reserve(assoc);
        for (size_t i = 0; i < assoc; ++i) {
            lines.emplace_back(lineSize);
        }
    }
};

struct MSHREntry {
    uint64_t lineAddress{0};
    uint64_t targetCycle{0};
    bool valid{false};
    std::vector<uint32_t> pendingRequesters;
};

struct CacheAccessResult {
    bool hit{false};
    uint32_t latencyCycles{0};
    uint32_t criticalWordLatency{0};
    bool evictedDirty{false};
    uint64_t evictedLineAddress{0};
    std::string missType{""}; // "compulsory", "capacity", "conflict"
    bool mshrHit{false};
};

class Cache {
public:
    CacheConfig cfg;
    size_t pageSizeBytes{4096};

    size_t numSets{64};
    uint32_t offsetBits{6};
    uint32_t indexBits{6};
    uint32_t tagBits{20};
    bool hasAliasingRisk{false};

    std::vector<CacheSet> sets;
    std::vector<MSHREntry> mshrs;

    // 3C miss categorization tracking
    std::unordered_set<uint64_t> globalSeenLines;
    // Fully-associative shadow tag directory for capacity vs conflict separation
    std::unordered_set<uint64_t> fullyAssocTags;
    size_t totalLines{512};

    // Age / sequence counters
    uint64_t accessSequence{0};
    std::mt19937 rng{42};

    // Statistics
    uint64_t totalAccesses{0};
    uint64_t readAccesses{0};
    uint64_t writeAccesses{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t writebacks{0};
    uint64_t compulsoryMisses{0};
    uint64_t capacityMisses{0};
    uint64_t conflictMisses{0};
    uint64_t mshrSecondaryHits{0};

    explicit Cache(const CacheConfig& ccfg = CacheConfig(), size_t pageSize = 4096)
        : cfg(ccfg), pageSizeBytes(pageSize) {
        size_t lineBytes = cfg.lineSizeBytes;
        size_t assoc = cfg.associativity;
        numSets = cfg.sizeBytes / (assoc * lineBytes);
        if (numSets == 0) numSets = 1;

        offsetBits = static_cast<uint32_t>(std::round(std::log2(lineBytes)));
        indexBits = static_cast<uint32_t>(std::round(std::log2(numSets)));
        tagBits = 32 - offsetBits - indexBits;

        // VIPT Aliasing check: index bits + offset bits > page offset bits?
        uint32_t pageOffsetBits = static_cast<uint32_t>(std::round(std::log2(pageSizeBytes)));
        if (cfg.indexingMode == "vipt" && (offsetBits + indexBits) > pageOffsetBits) {
            hasAliasingRisk = true;
        }

        totalLines = numSets * assoc;
        sets.reserve(numSets);
        for (size_t s = 0; s < numSets; ++s) {
            sets.emplace_back(assoc, lineBytes);
        }

        mshrs.resize(cfg.mshrEntries);
        reset();
    }

    void reset() {
        for (auto& s : sets) {
            s.plruBits = 0;
            for (auto& line : s.lines) {
                line.valid = false;
                line.dirty = false;
                line.tag = 0;
                line.lruAge = 0;
                line.fifoSeq = 0;
                line.rrpv = (1U << cfg.srripBits) - 1;
            }
        }
        for (auto& m : mshrs) {
            m.valid = false;
            m.pendingRequesters.clear();
        }
        globalSeenLines.clear();
        fullyAssocTags.clear();
        accessSequence = 0;

        totalAccesses = 0;
        readAccesses = 0;
        writeAccesses = 0;
        hits = 0;
        misses = 0;
        writebacks = 0;
        compulsoryMisses = 0;
        capacityMisses = 0;
        conflictMisses = 0;
        mshrSecondaryHits = 0;
    }

    void splitAddress(uint64_t addr, uint64_t& tag, size_t& setIdx, uint32_t& offset) const {
        offset = static_cast<uint32_t>(addr & ((1ULL << offsetBits) - 1));
        uint64_t shifted = addr >> offsetBits;
        setIdx = static_cast<size_t>(shifted & ((1ULL << indexBits) - 1)) % numSets;
        tag = shifted >> indexBits;
    }

    size_t pickVictimWay(const CacheSet& s) {
        size_t assoc = cfg.associativity;

        // 1. Check for invalid lines first
        for (size_t w = 0; w < assoc; ++w) {
            if (!s.lines[w].valid) return w;
        }

        // 2. Policy-driven victim selection
        if (cfg.replacementPolicy == "fifo") {
            size_t minWay = 0;
            uint64_t minSeq = s.lines[0].fifoSeq;
            for (size_t w = 1; w < assoc; ++w) {
                if (s.lines[w].fifoSeq < minSeq) {
                    minSeq = s.lines[w].fifoSeq;
                    minWay = w;
                }
            }
            return minWay;
        } else if (cfg.replacementPolicy == "random") {
            return rng() % assoc;
        } else if (cfg.replacementPolicy == "plru") {
            // Tree-PLRU for power-of-2 associativity
            size_t node = 0;
            size_t way = 0;
            for (size_t level = 0; (1ULL << level) < assoc; ++level) {
                uint32_t bit = (s.plruBits >> node) & 1;
                way = (way << 1) | (bit ^ 1);
                node = (node << 1) + 1 + (bit ^ 1);
            }
            return way % assoc;
        } else if (cfg.replacementPolicy == "srrip") {
            uint32_t maxRrpv = (1U << cfg.srripBits) - 1;
            while (true) {
                for (size_t w = 0; w < assoc; ++w) {
                    if (s.lines[w].rrpv == maxRrpv) return w;
                }
                // Age all lines
                for (size_t w = 0; w < assoc; ++w) {
                    const_cast<CacheSet&>(s).lines[w].rrpv++;
                }
            }
        }

        // Default LRU: pick max lruAge
        size_t maxWay = 0;
        uint64_t maxAge = s.lines[0].lruAge;
        for (size_t w = 1; w < assoc; ++w) {
            if (s.lines[w].lruAge > maxAge) {
                maxAge = s.lines[w].lruAge;
                maxWay = w;
            }
        }
        return maxWay;
    }

    void updatePlru(CacheSet& s, size_t accessedWay) {
        size_t assoc = cfg.associativity;
        size_t node = 0;
        for (int level = static_cast<int>(std::round(std::log2(assoc))) - 1; level >= 0; --level) {
            uint32_t bit = (accessedWay >> level) & 1;
            if (bit) {
                s.plruBits |= (1U << node);
                node = (node << 1) + 2;
            } else {
                s.plruBits &= ~(1U << node);
                node = (node << 1) + 1;
            }
        }
    }

    CacheAccessResult access(uint64_t addr, bool isWrite = false, uint32_t /*pc*/ = 0) {
        totalAccesses++;
        if (isWrite) writeAccesses++;
        else readAccesses++;
        accessSequence++;

        uint64_t lineAddr = (addr / cfg.lineSizeBytes) * cfg.lineSizeBytes;
        uint64_t tag = 0;
        size_t setIdx = 0;
        uint32_t offset = 0;
        splitAddress(addr, tag, setIdx, offset);

        CacheSet& set = sets[setIdx];
        CacheAccessResult res;
        res.latencyCycles = cfg.hitLatencyCycles;
        res.criticalWordLatency = cfg.hitLatencyCycles;

        // Check for tag hit
        for (size_t w = 0; w < cfg.associativity; ++w) {
            CacheLine& line = set.lines[w];
            if (line.valid && line.tag == tag) {
                // HIT!
                hits++;
                res.hit = true;
                line.lruAge = 0;
                line.rrpv = 0; // Hit sets RRPV to 0
                if (cfg.replacementPolicy == "plru") updatePlru(set, w);

                if (isWrite) {
                    if (cfg.writePolicy == "wb_wa") {
                        line.dirty = true;
                    } else {
                        // Write-through
                        writebacks++;
                    }
                }
                return res;
            }
        }

        // MISS!
        misses++;
        res.hit = false;

        // Check MSHRs for secondary hit
        for (auto& m : mshrs) {
            if (m.valid && m.lineAddress == lineAddr) {
                mshrSecondaryHits++;
                res.mshrHit = true;
                break;
            }
        }

        // 3C Miss Categorization
        if (globalSeenLines.find(lineAddr) == globalSeenLines.end()) {
            compulsoryMisses++;
            res.missType = "compulsory";
            globalSeenLines.insert(lineAddr);
        } else {
            // Check fully-associative shadow directory
            if (fullyAssocTags.find(lineAddr) != fullyAssocTags.end()) {
                conflictMisses++;
                res.missType = "conflict";
            } else {
                capacityMisses++;
                res.missType = "capacity";
            }
        }
        if (fullyAssocTags.size() >= totalLines) {
            fullyAssocTags.erase(fullyAssocTags.begin());
        }
        fullyAssocTags.insert(lineAddr);

        // Update all ages in set
        for (auto& l : set.lines) {
            if (l.valid) l.lruAge++;
        }

        // If write-through + no-write-allocate, don't allocate on write miss
        if (isWrite && cfg.writePolicy == "wt_nwa") {
            writebacks++;
            return res;
        }

        // Evict victim line and allocate
        size_t victimWay = pickVictimWay(set);
        CacheLine& victim = set.lines[victimWay];

        if (victim.valid && victim.dirty) {
            res.evictedDirty = true;
            uint64_t evictedAddr = (victim.tag << (indexBits + offsetBits)) | (setIdx << offsetBits);
            res.evictedLineAddress = evictedAddr;
            writebacks++;
        }

        // Fill line
        victim.valid = true;
        victim.tag = tag;
        victim.dirty = (isWrite && cfg.writePolicy == "wb_wa");
        victim.fifoSeq = accessSequence;

        // Insertion policy
        if (cfg.insertionPolicy == "lru") {
            victim.lruAge = 1000000;
            victim.rrpv = (1U << cfg.srripBits) - 1;
        } else if (cfg.insertionPolicy == "bip") {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            if (dist(rng) < cfg.bipEpsilon) {
                victim.lruAge = 0;
                victim.rrpv = 0;
            } else {
                victim.lruAge = 1000000;
                victim.rrpv = (1U << cfg.srripBits) - 1;
            }
        } else {
            // MRU default
            victim.lruAge = 0;
            victim.rrpv = (1U << cfg.srripBits) - 2; // Long re-reference interval
        }

        if (cfg.replacementPolicy == "plru") updatePlru(set, victimWay);

        return res;
    }

    double getHitRate() const {
        if (totalAccesses == 0) return 1.0;
        return static_cast<double>(hits) / totalAccesses;
    }

    double getMissRate() const {
        if (totalAccesses == 0) return 0.0;
        return static_cast<double>(misses) / totalAccesses;
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_CACHE_HPP