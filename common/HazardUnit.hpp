#ifndef RISC201_COMMON_HAZARD_UNIT_HPP
#define RISC201_COMMON_HAZARD_UNIT_HPP

#include "ISA.hpp"
#include "ForwardingUnit.hpp"
#include <string>
#include <vector>

namespace risc201 {

struct PipelineAction {
    bool stallIF{false};
    bool stallID_OF{false};
    bool stallRF{false};
    bool stallEX{false};
    bool bubbleOF_EX{false};
    bool bubbleRF_EX{false};
    bool flushPipeline{false};
    std::string stallReason{""};
};

class HazardUnit {
public:
    size_t pipelineStages{4};
    ForwardingMode forwardingMode{ForwardingMode::FULL};
    bool unifiedMemory{false};

    // Telemetry
    uint64_t rawHazardsDetected{0};
    uint64_t loadUseStalls{0};
    uint64_t rawNoFwdStalls{0};
    uint64_t structuralMemStalls{0};
    uint64_t branchFlushCount{0};

    explicit HazardUnit(size_t stages = 4, ForwardingMode fwd = ForwardingMode::FULL, bool unifiedMem = false)
        : pipelineStages(stages), forwardingMode(fwd), unifiedMemory(unifiedMem) {}

    void reset() {
        rawHazardsDetected = 0;
        loadUseStalls = 0;
        rawNoFwdStalls = 0;
        structuralMemStalls = 0;
        branchFlushCount = 0;
    }

    // 4-Stage Hazard Detection
    // Consumer in OF_EX (or IF_OF waiting to enter OF)
    PipelineAction check4Stage(
        const Instruction& ofInst,
        bool ofValid,
        const Instruction& exMarwInst,
        bool exMarwValid,
        bool branchTakenOrMispredict
    ) {
        PipelineAction act;

        // 1. Branch Misprediction / Redirect Flush
        if (branchTakenOrMispredict) {
            act.flushPipeline = true;
            branchFlushCount++;
            return act;
        }

        if (!ofValid || ofInst.isNop()) {
            return act;
        }

        // 2. Data Hazards against producer in EX_MARW
        if (exMarwValid && exMarwInst.writesRegister()) {
            uint8_t prodRd = exMarwInst.getDestReg();
            bool readsReg = (ofInst.readsRs1() && ofInst.rs1 == prodRd) ||
                            (ofInst.readsRs2() && ofInst.rs2 == prodRd) ||
                            (ofInst.isStore() && ofInst.rd == prodRd);

            if (readsReg) {
                // If forwarding is disabled: RAW hazard causes stall until instruction finishes MARW
                if (forwardingMode == ForwardingMode::NONE) {
                    act.stallIF = true;
                    act.stallID_OF = true;
                    act.bubbleOF_EX = true;
                    act.stallReason = "RAW_NOFWD";
                    rawNoFwdStalls++;
                    return act;
                }

                // If producer is a Load instruction: Load-Use Hazard!
                // Load data only arrives after memory access in MARW.
                // In 4-stage pipeline where EX and MARW execute, a load followed immediately
                // by a consumer needs 1 bubble if data is needed in EX!
                if (exMarwInst.isLoad()) {
                    act.stallIF = true;
                    act.stallID_OF = true;
                    act.bubbleOF_EX = true;
                    act.stallReason = "RAW_LOAD_USE";
                    loadUseStalls++;
                    return act;
                }
            }
        }

        return act;
    }

    // 6-Stage Hazard Detection
    // Consumer in ID_RF or RF_EX
    PipelineAction check6Stage(
        const Instruction& rfInst,
        bool rfValid,
        const Instruction& exMaInst,
        bool exMaValid,
        const Instruction& maRwInst,
        bool maRwValid,
        bool branchTakenOrMispredict
    ) {
        PipelineAction act;

        if (branchTakenOrMispredict) {
            act.flushPipeline = true;
            branchFlushCount++;
            return act;
        }

        if (!rfValid || rfInst.isNop()) {
            return act;
        }

        // Load-Use Hazard check against EX_MA
        if (exMaValid && exMaInst.isLoad()) {
            uint8_t loadRd = exMaInst.getDestReg();
            bool readsLoadReg = (rfInst.readsRs1() && rfInst.rs1 == loadRd) ||
                                (rfInst.readsRs2() && rfInst.rs2 == loadRd);

            if (readsLoadReg) {
                // Load-Use hazard! Requires 1 bubble cycle
                act.stallIF = true;
                act.stallID_OF = true;
                act.stallRF = true;
                act.bubbleRF_EX = true;
                act.stallReason = "RAW_LOAD_USE";
                loadUseStalls++;
                return act;
            }
        }

        // If forwarding is disabled: check dependencies against EX_MA and MA_RW
        if (forwardingMode == ForwardingMode::NONE) {
            bool dep = false;
            if (exMaValid && exMaInst.writesRegister()) {
                uint8_t rd = exMaInst.getDestReg();
                if ((rfInst.readsRs1() && rfInst.rs1 == rd) || (rfInst.readsRs2() && rfInst.rs2 == rd)) {
                    dep = true;
                }
            }
            if (maRwValid && maRwInst.writesRegister()) {
                uint8_t rd = maRwInst.getDestReg();
                if ((rfInst.readsRs1() && rfInst.rs1 == rd) || (rfInst.readsRs2() && rfInst.rs2 == rd)) {
                    dep = true;
                }
            }

            if (dep) {
                act.stallIF = true;
                act.stallID_OF = true;
                act.stallRF = true;
                act.bubbleRF_EX = true;
                act.stallReason = "RAW_NOFWD";
                rawNoFwdStalls++;
                return act;
            }
        }

        return act;
    }
};

} // namespace risc201

#endif // RISC201_COMMON_HAZARD_UNIT_HPP