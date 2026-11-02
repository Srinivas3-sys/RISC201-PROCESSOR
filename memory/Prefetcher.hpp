#ifndef RISC201_MEMORY_PREFETCHER_HPP
#define RISC201_MEMORY_PREFETCHER_HPP

#include "Config.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <deque>

namespace risc201 {
namespace memory {

enum class RPTState {
    INIT,
    TRANSIENT,
    STEADY,
    NO_PRED
};

struct RPTEntry {
    uint32_t pc{0};
    uint64_t lastAddress{0};
    int64_t stride{0};
    RPTState state{RPTState::INIT};
};

struct StreamBuffer {
    bool valid{false};
    uint64_t startLine{0};
    int direction{1}; // +1 or -1
    std::deque<uint64_t> prefetchedLines{};
    uint64_t lastAccessCycle{0};
};

class Prefetcher {
public:
    PrefetchConfig cfg;
    size_t lineSizeBytes{64};
    size_t pageSizeBytes{4096};

    // Stride RPT table
    std::unordered_map<uint32_t, RPTEntry> rptTable;

    // Stream buffers
    std::vector<StreamBuffer> streamBuffers;

    // Statistics
    uint64_t prefetchRequests{0};
    uint64_t prefetchHits{0};
    uint64_t pageBoundaryCrossSuppressed{0};

    explicit Prefetcher(const PrefetchConfig& pcfg = PrefetchConfig(),
                        size_t lineSize = 64,
                        size_t pageSize = 4096)
        : cfg(pcfg), lineSizeBytes(lineSize), pageSizeBytes(pageSize) {
        streamBuffers.resize(cfg.streamBuffers);
    }

    void reset() {
        rptTable.clear();
        for (auto& sb : streamBuffers) {
            sb.valid = false;
            sb.prefetchedLines.clear();
        }
        prefetchRequests = 0;
        prefetchHits = 0;
        pageBoundaryCrossSuppressed = 0;
    }

    bool isSamePage(uint64_t addr1, uint64_t addr2) const {
        return (addr1 / pageSizeBytes) == (addr2 / pageSizeBytes);
    }

    std::vector<uint64_t> onAccess(uint64_t paddr, uint32_t pc = 0, bool isMiss = false) {
        std::vector<uint64_t> linesToFetch;
        if (!cfg.enabled || cfg.prefetcherType == "none") {
            return linesToFetch;
        }

        uint64_t currentLine = paddr / lineSizeBytes;

        if (cfg.prefetcherType == "next_line") {
            for (size_t i = 1; i <= cfg.lookaheadLines; ++i) {
                uint64_t nextLine = currentLine + i;
                uint64_t targetAddr = nextLine * lineSizeBytes;
                if (!cfg.crossPageBoundary && !isSamePage(paddr, targetAddr)) {
                    pageBoundaryCrossSuppressed++;
                    break;
                }
                linesToFetch.push_back(targetAddr);
                prefetchRequests++;
            }
        } else if (cfg.prefetcherType == "stride") {
            auto it = rptTable.find(pc);
            if (it == rptTable.end()) {
                RPTEntry entry;
                entry.pc = pc;
                entry.lastAddress = paddr;
                entry.stride = 0;
                entry.state = RPTState::INIT;
                rptTable[pc] = entry;
            } else {
                RPTEntry& entry = it->second;
                int64_t newStride = static_cast<int64_t>(paddr) - static_cast<int64_t>(entry.lastAddress);

                switch (entry.state) {
                    case RPTState::INIT:
                        entry.stride = newStride;
                        entry.state = RPTState::TRANSIENT;
                        break;
                    case RPTState::TRANSIENT:
                        if (newStride == entry.stride && newStride != 0) {
                            entry.state = RPTState::STEADY;
                        } else {
                            entry.stride = newStride;
                        }
                        break;
                    case RPTState::STEADY:
                        if (newStride != entry.stride) {
                            entry.state = RPTState::TRANSIENT;
                        }
                        break;
                    case RPTState::NO_PRED:
                        entry.stride = newStride;
                        entry.state = RPTState::TRANSIENT;
                        break;
                }

                entry.lastAddress = paddr;

                if (entry.state == RPTState::STEADY && entry.stride != 0) {
                    for (size_t d = 1; d <= cfg.prefetchDegree; ++d) {
                        int64_t target = static_cast<int64_t>(paddr) + (entry.stride * d);
                        if (target >= 0) {
                            uint64_t targetAddr = static_cast<uint64_t>(target);
                            if (!cfg.crossPageBoundary && !isSamePage(paddr, targetAddr)) {
                                pageBoundaryCrossSuppressed++;
                                break;
                            }
                            linesToFetch.push_back(targetAddr);
                            prefetchRequests++;
                        }
                    }
                }
            }
        } else if (cfg.prefetcherType == "stream") {
            // Check stream buffers for hit
            bool hit = false;
            for (auto& sb : streamBuffers) {
                if (sb.valid) {
                    for (auto itLine = sb.prefetchedLines.begin(); itLine != sb.prefetchedLines.end(); ++itLine) {
                        if (*itLine == currentLine) {
                            hit = true;
                            prefetchHits++;
                            sb.prefetchedLines.erase(sb.prefetchedLines.begin(), itLine + 1);

                            // Refill buffer forward
                            uint64_t lastLine = sb.prefetchedLines.empty() ? currentLine : sb.prefetchedLines.back();
                            while (sb.prefetchedLines.size() < cfg.lookaheadLines) {
                                lastLine += sb.direction;
                                uint64_t targetAddr = lastLine * lineSizeBytes;
                                if (!cfg.crossPageBoundary && !isSamePage(paddr, targetAddr)) {
                                    pageBoundaryCrossSuppressed++;
                                    break;
                                }
                                sb.prefetchedLines.push_back(lastLine);
                                linesToFetch.push_back(targetAddr);
                                prefetchRequests++;
                            }
                            break;
                        }
                    }
                }
                if (hit) break;
            }

            // If miss, allocate new stream buffer
            if (!hit && isMiss) {
                StreamBuffer* victim = &streamBuffers[0];
                for (auto& sb : streamBuffers) {
                    if (!sb.valid) {
                        victim = &sb;
                        break;
                    }
                }
                victim->valid = true;
                victim->startLine = currentLine;
                victim->direction = 1;
                victim->prefetchedLines.clear();

                for (size_t i = 1; i <= cfg.lookaheadLines; ++i) {
                    uint64_t nextLine = currentLine + i;
                    uint64_t targetAddr = nextLine * lineSizeBytes;
                    if (!cfg.crossPageBoundary && !isSamePage(paddr, targetAddr)) {
                        pageBoundaryCrossSuppressed++;
                        break;
                    }
                    victim->prefetchedLines.push_back(nextLine);
                    linesToFetch.push_back(targetAddr);
                    prefetchRequests++;
                }
            }
        }

        return linesToFetch;
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_PREFETCHER_HPP