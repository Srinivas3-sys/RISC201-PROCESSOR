#ifndef RISC201_REGISTER_FILE_HPP
#define RISC201_REGISTER_FILE_HPP

#include "Types.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace risc201 {

/**
 * RegisterFile implements the 16 architectural 32-bit registers (r0-r15)
 * along with the internal flags register (E, GT) and privileged registers
 * (oldPC, oldSP, oldFlags, CPL) described in Sarangi Chapter 9 & 10.
 *
 * Characteristics:
 * - 2 Read Ports: readPort1, readPort2
 * - 1 Write Port: writePort(regId, data, enable)
 * - r14 is stack pointer (sp)
 * - r15 is return address (ra)
 */
class RegisterFile {
private:
    std::array<Word, NUM_REGISTERS> regs{};
    Flags flags{};

    // Privileged registers (Section 10.8)
    Word oldPC{0};
    Word oldSP{0};
    Flags oldFlags{};
    uint8_t cpl{0}; // 0 = Kernel / Privileged, 1 = User Mode

public:
    RegisterFile() {
        reset();
    }

    void reset(Word initialSp = 0x000FFFFC) {
        regs.fill(0);
        regs[REG_SP] = initialSp;
        flags.clear();
        oldPC = 0;
        oldSP = initialSp;
        oldFlags.clear();
        cpl = 1; // Default to user mode unless in handler
    }

    // Read port 1
    Word readPort1(RegId id) const {
        if (id >= NUM_REGISTERS) return 0;
        return regs[id];
    }

    // Read port 2
    Word readPort2(RegId id) const {
        if (id >= NUM_REGISTERS) return 0;
        return regs[id];
    }

    // Write port
    void writePort(RegId id, Word value, bool enable) {
        if (!enable) return;
        if (id < NUM_REGISTERS) {
            regs[id] = value;
        }
    }

    // Direct register access (for debugger/initialization)
    Word get(RegId id) const {
        if (id < NUM_REGISTERS) return regs[id];
        return 0;
    }

    Word read(RegId id) const { return get(id); }

    void set(RegId id, Word value) {
        if (id < NUM_REGISTERS) {
            regs[id] = value;
        }
    }

    void write(RegId id, Word value) { set(id, value); }

    // Stack pointer helper
    Word getSP() const { return regs[REG_SP]; }
    void setSP(Word sp) { regs[REG_SP] = sp; }

    // Return address helper
    Word getRA() const { return regs[REG_RA]; }
    void setRA(Word ra) { regs[REG_RA] = ra; }

    // Flags access
    Flags getFlags() const { return flags; }
    void setFlags(const Flags& f) { flags = f; }
    void setFlags(bool e, bool gt) { flags.E = e; flags.GT = gt; }

    // Privileged registers
    Word getOldPC() const { return oldPC; }
    void setOldPC(Word pc) { oldPC = pc; }

    Word getOldSP() const { return oldSP; }
    void setOldSP(Word sp) { oldSP = sp; }

    Flags getOldFlags() const { return oldFlags; }
    void setOldFlags(const Flags& f) { oldFlags = f; }

    uint8_t getCPL() const { return cpl; }
    void setCPL(uint8_t level) { cpl = level; }

    // Register string name helper
    static std::string getRegName(RegId id) {
        if (id == REG_SP) return "sp";
        if (id == REG_RA) return "ra";
        return "r" + std::to_string(id);
    }

    // Format register file state for CLI display
    std::string dumpState() const {
        std::ostringstream oss;
        oss << "=== Register File (r0 - r15) ===\n";
        for (int i = 0; i < 16; i += 4) {
            for (int j = 0; j < 4; ++j) {
                int r = i + j;
                std::string name = getRegName(r);
                oss << std::setw(4) << std::left << name << ": "
                    << toHex(regs[r]) << "  ";
            }
            oss << "\n";
        }
        oss << "Flags: [E=" << (flags.E ? "1" : "0")
            << ", GT=" << (flags.GT ? "1" : "0") << "]"
            << " | CPL=" << static_cast<int>(cpl)
            << " | oldPC=" << toHex(oldPC)
            << " | oldSP=" << toHex(oldSP) << "\n";
        return oss.str();
    }
};

} // namespace risc201

#endif // RISC201_REGISTER_FILE_HPP
