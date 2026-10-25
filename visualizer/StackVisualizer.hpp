#ifndef RISC201_STACK_VISUALIZER_HPP
#define RISC201_STACK_VISUALIZER_HPP

#include "../common/Types.hpp"
#include "../common/Memory.hpp"
#include "../common/RegisterFile.hpp"
#include <string>
#include <sstream>
#include <iomanip>

namespace risc201 {
namespace visualizer {

/**
 * Active Stack Memory Visualizer (Requirement 2g).
 * Implements an ASCII stack memory frame renderer displaying:
 *   - Stack memory from stackBase down to current sp
 *   - Current SP pointer indicator (<-- SP / Top of Stack)
 *   - Stack boundary verification alerts (Guard band, Limit, Overflow/Underflow)
 *   - Interpretation of words (Hex, Decimal, and likely role: RA, Saved Reg, Local Var)
 */
class StackVisualizer {
public:
    static std::string render(const Memory& memory, const RegisterFile& rf, Word maxFrames = 32) {
        std::ostringstream oss;
        Word sp = rf.getSP();
        const auto& bounds = memory.getStackBounds();

        oss << "\n+--------------------------------------------------------------------------+\n";
        oss << "|                   ACTIVE STACK MEMORY FRAME RENDERER                     |\n";
        oss << "+--------------------------------------------------------------------------+\n";
        oss << "  Stack Base : " << toHex(bounds.stackBase)
            << " | Current SP : " << toHex(sp)
            << " | Stack Limit : " << toHex(bounds.stackLimit) << "\n";

        // Verify boundary safety
        ExceptionType safety = bounds.verifySp(sp);
        if (safety == ExceptionType::STACK_OVERFLOW) {
            oss << "  *** CRITICAL ALERT: STACK OVERFLOW! sp < stackLimit ***\n";
        } else if (safety == ExceptionType::STACK_UNDERFLOW) {
            oss << "  *** WARNING: STACK UNDERFLOW! sp > stackBase ***\n";
        } else {
            oss << "  Stack Boundary Status: SECURE & HEALTHY\n";
        }
        oss << "+--------------+--------------+------------------+-------------------------+\n";
        oss << "|   Address    |  Value (Hex) |  Value (Signed)  | Annotation / Frame Role |\n";
        oss << "+--------------+--------------+------------------+-------------------------+\n";

        // Render stack downwards from stackBase to sp (or around sp)
        Word topAddr = bounds.stackBase;
        Word botAddr = (sp < bounds.stackLimit) ? bounds.stackLimit : (sp & ~0x3);

        // Ensure we don't display an excessively huge empty stack if SP hasn't moved much
        if (topAddr > sp + (maxFrames * 4)) {
            topAddr = (sp + (maxFrames * 4)) & ~0x3;
            if (topAddr > bounds.stackBase) topAddr = bounds.stackBase;
        }

        if (sp > topAddr) {
            topAddr = sp;
        }

        // Include 2 words below SP for guard band inspection
        Word displayBottom = (botAddr >= 8) ? botAddr - 8 : 0;

        for (Word addr = topAddr; addr >= displayBottom && addr <= topAddr; addr -= 4) {
            CpuException ex;
            Word val = memory.readWord(addr, &ex);

            std::string annotation;
            if (addr == sp) {
                annotation = "<-- [SP] (Top of Stack)";
            } else if (addr == bounds.stackBase) {
                annotation = "[Stack Origin / Base]";
            } else if (addr < sp) {
                if (addr >= bounds.stackLimit - bounds.guardBandSize && addr < bounds.stackLimit) {
                    annotation = "[GUARD BAND REGION]";
                } else {
                    annotation = "[Unallocated Space]";
                }
            } else {
                // Heuristic detection of return address or saved frame
                if (val > 0 && val < 0x00010000 && (val % 4 == 0)) {
                    annotation = "Possible Saved Return Address (ra)";
                } else {
                    annotation = "Local Variable / Spill";
                }
            }

            int32_t sval = static_cast<int32_t>(val);

            oss << "|  " << toHex(addr) << "  |  "
                << toHex(val) << "  |  "
                << std::setw(14) << std::right << sval << "  |  "
                << std::setw(23) << std::left << annotation << "|\n";

            if (addr == 0) break; // prevent unsigned underflow of addr -= 4
        }

        oss << "+--------------+--------------+------------------+-------------------------+\n";
        return oss.str();
    }
};

} // namespace visualizer
} // namespace risc201

#endif // RISC201_STACK_VISUALIZER_HPP
