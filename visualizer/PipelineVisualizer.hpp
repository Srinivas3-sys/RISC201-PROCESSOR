#ifndef RISC201_PIPELINE_VISUALIZER_HPP
#define RISC201_PIPELINE_VISUALIZER_HPP

#include "../common/Types.hpp"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace risc201 {
namespace visualizer {

struct PipelineStageInfo {
    std::string stageName;
    std::string instructionText;
    Word pc{0};
    bool isBubble{false};
    bool isStalled{false};
    uint64_t instId{0};
    std::string stallReason{""};
    std::string forwardingInfo{""};
};

struct CycleSnapshot {
    uint64_t cycle{0};
    std::vector<PipelineStageInfo> stages;
};

/**
 * Pipeline Visualizer rendering cycle-by-cycle pipeline diagrams
 * and stage latch state in ASCII art format (Sarangi Section 10.4.1).
 */
class PipelineVisualizer {
private:
    static std::string truncateStr(const std::string& str, size_t maxLen) {
        if (str.size() <= maxLen) return str;
        return str.substr(0, maxLen - 2) + "..";
    }

public:
    static std::string renderStages(const std::vector<PipelineStageInfo>& stages, uint64_t cycle) {
        std::ostringstream oss;
        oss << "\n+--------------------------------------------------------------------------+\n";
        oss << "|               PIPELINE EXECUTION STATE (Clock Cycle: " << std::setw(6) << cycle << ")             |\n";
        oss << "+--------------------------------------------------------------------------+\n";

        for (const auto& st : stages) {
            oss << "  [" << std::setw(6) << std::left << st.stageName << "] : ";
            if (st.isBubble) {
                if (!st.stallReason.empty()) {
                    oss << "[BUBBLE: " << st.stallReason << "]";
                } else {
                    oss << "-- BUBBLE / NOP --";
                }
            } else if (st.isStalled) {
                if (!st.stallReason.empty()) {
                    oss << "[STALL: " << st.stallReason << "] " << st.instructionText << " (PC: " << toHex(st.pc) << ")";
                } else {
                    oss << "[STALLED] " << st.instructionText << " (PC: " << toHex(st.pc) << ")";
                }
            } else if (!st.instructionText.empty()) {
                oss << st.instructionText << " (PC: " << toHex(st.pc) << ")";
            } else {
                oss << "(empty)";
            }
            if (!st.forwardingInfo.empty()) {
                oss << "\n             ↳ " << st.forwardingInfo;
            }
            oss << "\n";
        }
        oss << "+--------------------------------------------------------------------------+\n";
        return oss.str();
    }

    /**
     * Render the complete cycle-by-cycle execution trace and space-time timing diagram
     * for all executed cycles from start to drain.
     */
    static std::string renderTrace(const std::vector<CycleSnapshot>& history, const std::string& pipelineType = "4-Stage") {
        if (history.empty()) {
            return "\nNo pipeline cycles have been executed yet. Run the program (e.g. 'r') to generate a pipeline trace.\n";
        }

        std::ostringstream oss;
        oss << "\n========================================================================================\n"
            << "                         RISC201 COMPLETE PIPELINE TRACE\n"
            << "========================================================================================\n"
            << "Total Clock Cycles : " << history.size() << "\n"
            << "Pipeline Depth     : " << pipelineType << "\n\n";

        bool is6Stage = (pipelineType == "6-Stage" || pipelineType == "6");

        // ---------------------------------------------------------------------
        // PART 1: Cycle-by-Cycle Stage State Table
        // ---------------------------------------------------------------------
        oss << "--- Cycle-by-Cycle Stage State Table ---\n";
        if (is6Stage) {
            oss << "+-------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+\n"
                << "| Cycle | IF                | ID                | RF                | EX                | MA                | RW                |\n"
                << "+-------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+\n";

            for (const auto& snap : history) {
                oss << "| " << std::setw(5) << std::right << snap.cycle << " | ";
                for (size_t s = 0; s < 6; ++s) {
                    std::string cell = "[bubble]";
                    if (s < snap.stages.size()) {
                        const auto& st = snap.stages[s];
                        if (st.isStalled) {
                            cell = st.stallReason.empty() ? ("[STALL] " + st.instructionText) : ("[STALL:" + st.stallReason + "]");
                        } else if (st.isBubble && !st.stallReason.empty()) {
                            cell = "[" + st.stallReason + "]";
                        } else if (!st.isBubble && !st.instructionText.empty()) {
                            cell = st.instructionText;
                        }
                    }
                    oss << std::setw(17) << std::left << truncateStr(cell, 17) << " | ";
                }
                oss << "\n";
            }
            oss << "+-------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+\n\n";
        } else {
            oss << "+-------+-------------------------+-------------------------+-------------------------+-------------------------+\n"
                << "| Cycle | IF                      | OF                      | EX                      | MA_RW                   |\n"
                << "+-------+-------------------------+-------------------------+-------------------------+-------------------------+\n";

            for (const auto& snap : history) {
                oss << "| " << std::setw(5) << std::right << snap.cycle << " | ";
                for (size_t s = 0; s < 4; ++s) {
                    std::string cell = "[bubble]";
                    if (s < snap.stages.size()) {
                        const auto& st = snap.stages[s];
                        if (st.isStalled) {
                            cell = st.stallReason.empty() ? ("[STALL] " + st.instructionText) : ("[STALL:" + st.stallReason + "]");
                        } else if (st.isBubble && !st.stallReason.empty()) {
                            cell = "[" + st.stallReason + "]";
                        } else if (!st.isBubble && !st.instructionText.empty()) {
                            cell = st.instructionText;
                        }
                    }
                    oss << std::setw(23) << std::left << truncateStr(cell, 23) << " | ";
                }
                oss << "\n";
            }
            oss << "+-------+-------------------------+-------------------------+-------------------------+-------------------------+\n\n";
        }

        // ---------------------------------------------------------------------
        // PART 2: Pipeline Space-Time Timing Diagram (Instruction vs Cycles)
        // ---------------------------------------------------------------------
        struct InstTrace {
            uint64_t id{0};
            Word pc{0};
            std::string text;
            std::map<uint64_t, std::string> stageMap; // cycle -> short stage name
        };

        std::map<uint64_t, InstTrace> instMap;
        std::vector<uint64_t> instOrder;

        for (const auto& snap : history) {
            for (const auto& st : snap.stages) {
                if (st.instId > 0 && !st.isBubble && !st.instructionText.empty()) {
                    if (instMap.find(st.instId) == instMap.end()) {
                        InstTrace it;
                        it.id = st.instId;
                        it.pc = st.pc;
                        it.text = st.instructionText;
                        instMap[st.instId] = it;
                        instOrder.push_back(st.instId);
                    }
                    std::string shortName = st.stageName;
                    if (shortName == "MA_RW") shortName = "RW";
                    if (st.isStalled) shortName += "*";
                    instMap[st.instId].stageMap[snap.cycle] = shortName;
                }
            }
        }

        if (!instOrder.empty()) {
            oss << "--- Pipeline Space-Time Timing Diagram ---\n";
            uint64_t minCycle = history.front().cycle;
            uint64_t maxCycle = history.back().cycle;
            const uint64_t CHUNK_SIZE = 15;

            for (uint64_t chunkStart = minCycle; chunkStart <= maxCycle; chunkStart += CHUNK_SIZE) {
                uint64_t chunkEnd = std::min(chunkStart + CHUNK_SIZE - 1, maxCycle);

                std::string headerLabel = "Instruction";
                oss << std::setw(32) << std::left << headerLabel << " | ";
                for (uint64_t c = chunkStart; c <= chunkEnd; ++c) {
                    std::string cLabel = "C" + std::to_string(c);
                    oss << std::setw(4) << std::left << cLabel << "| ";
                }
                oss << "\n";

                std::string sep(33, '-');
                oss << sep << "+-";
                for (uint64_t c = chunkStart; c <= chunkEnd; ++c) {
                    oss << "-----+";
                }
                oss << "\n";

                for (uint64_t iId : instOrder) {
                    const auto& it = instMap[iId];
                    // Check if this instruction was active during this chunk
                    bool activeInChunk = false;
                    for (uint64_t c = chunkStart; c <= chunkEnd; ++c) {
                        if (it.stageMap.find(c) != it.stageMap.end()) {
                            activeInChunk = true;
                            break;
                        }
                    }
                    if (!activeInChunk) continue;

                    std::string fullLabel = toHex(it.pc) + ": " + it.text;
                    oss << std::setw(32) << std::left << truncateStr(fullLabel, 32) << " | ";
                    for (uint64_t c = chunkStart; c <= chunkEnd; ++c) {
                        auto fit = it.stageMap.find(c);
                        std::string val = (fit != it.stageMap.end()) ? fit->second : "  ";
                        oss << std::setw(4) << std::left << val << "| ";
                    }
                    oss << "\n";
                }
                oss << "\n";
            }
        }

        oss << "========================================================================================\n\n";
        return oss.str();
    }
};

} // namespace visualizer
} // namespace risc201

#endif // RISC201_PIPELINE_VISUALIZER_HPP
