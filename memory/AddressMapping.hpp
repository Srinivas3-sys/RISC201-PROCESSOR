#ifndef RISC201_MEMORY_ADDRESS_MAPPING_HPP
#define RISC201_MEMORY_ADDRESS_MAPPING_HPP

#include "Config.hpp"
#include <cstdint>
#include <cmath>
#include <stdexcept>

namespace risc201 {
namespace memory {

struct DRAMCoordinates {
    uint32_t channel{0};
    uint32_t rank{0};
    uint32_t bankGroup{0};
    uint32_t bank{0};
    uint32_t row{0};
    uint32_t col{0};
    uint32_t byteOffset{0};
    uint64_t rawAddress{0};
};

inline uint32_t log2_exact(size_t val) {
    if (val == 0) return 0;
    uint32_t bits = 0;
    size_t tmp = val;
    while ((tmp & 1) == 0) {
        bits++;
        tmp >>= 1;
    }
    return bits;
}

class AddressMapper {
public:
    DRAMConfig cfg;
    size_t lineSizeBytes;

    uint32_t byteOffsetBits{6};
    uint32_t colBits{10};
    uint32_t bankBits{2};
    uint32_t bankGroupBits{2};
    uint32_t rankBits{0};
    uint32_t channelBits{0};
    uint32_t rowBits{16};

    explicit AddressMapper(const DRAMConfig& dcfg = DRAMConfig(), size_t lineSize = 64)
        : cfg(dcfg), lineSizeBytes(lineSize) {
        byteOffsetBits = log2_exact(lineSizeBytes);
        colBits = log2_exact(cfg.colsPerRow);
        bankBits = log2_exact(cfg.banksPerGroup);
        bankGroupBits = log2_exact(cfg.bankGroupsPerRank);
        rankBits = log2_exact(cfg.ranksPerChannel);
        channelBits = log2_exact(cfg.channels);
        rowBits = log2_exact(cfg.rowsPerBank);
    }

    DRAMCoordinates map(uint64_t paddr) const {
        DRAMCoordinates coords;
        coords.rawAddress = paddr;
        uint64_t addr = paddr;

        // 1. Byte offset within line
        coords.byteOffset = static_cast<uint32_t>(addr & ((1ULL << byteOffsetBits) - 1));
        addr >>= byteOffsetBits;

        // 2. Channel
        if (channelBits > 0) {
            coords.channel = static_cast<uint32_t>(addr & ((1ULL << channelBits) - 1));
            addr >>= channelBits;
        }

        // 3. Bank Group
        if (bankGroupBits > 0) {
            coords.bankGroup = static_cast<uint32_t>(addr & ((1ULL << bankGroupBits) - 1));
            addr >>= bankGroupBits;
        }

        // 4. Bank
        if (bankBits > 0) {
            coords.bank = static_cast<uint32_t>(addr & ((1ULL << bankBits) - 1));
            addr >>= bankBits;
        }

        // 5. Rank
        if (rankBits > 0) {
            coords.rank = static_cast<uint32_t>(addr & ((1ULL << rankBits) - 1));
            addr >>= rankBits;
        }

        // 6. Column
        uint32_t remainingColBits = (colBits > byteOffsetBits) ? (colBits - byteOffsetBits) : 0;
        if (remainingColBits > 0) {
            coords.col = static_cast<uint32_t>(addr & ((1ULL << remainingColBits) - 1));
            addr >>= remainingColBits;
        }

        // 7. Row
        coords.row = static_cast<uint32_t>(addr & ((1ULL << rowBits) - 1));

        // XOR Hash Interleaving (if enabled)
        if (cfg.xorHashMapping) {
            uint32_t hash = (coords.row ^ (coords.row >> 4)) & ((1U << (bankBits + bankGroupBits)) - 1);
            coords.bank = (coords.bank ^ (hash & ((1U << bankBits) - 1))) % cfg.banksPerGroup;
            if (cfg.bankGroupsPerRank > 0) {
                coords.bankGroup = (coords.bankGroup ^ (hash >> bankBits)) % cfg.bankGroupsPerRank;
            }
        }

        return coords;
    }
};

} // namespace memory
} // namespace risc201

#endif // RISC201_MEMORY_ADDRESS_MAPPING_HPP