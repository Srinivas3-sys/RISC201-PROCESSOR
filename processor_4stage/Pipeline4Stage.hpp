#ifndef RISC201_PIPELINE_4STAGE_HPP
#define RISC201_PIPELINE_4STAGE_HPP

#include "StageRegisters4.hpp"
#include "../common/Types.hpp"
#include "../common/RegisterFile.hpp"
#include "../common/Memory.hpp"
#include "../common/Exception.hpp"
#include "../alu/ALU.hpp"
#include "../assembler/Disassembler.hpp"
#include "../visualizer/PipelineVisualizer.hpp"
#include "../memory/MemoryHierarchy.hpp"
#include "../common/ForwardingUnit.hpp"
#include "../common/HazardUnit.hpp"
#include "../common/BranchPredictor.hpp"
#include "../common/PerformanceTracker.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace risc201 {
namespace stage4 {
class Pipeline4Stage {
public:
    RegisterFile regFile;
    Memory memory;
    alu::ALU aluUnit;

    Word pc{0};
    Word programSize{0};
    bool halted{false};
    CpuException currentException{};

    // Pipeline Latches (3 latches for 4 stages) 
    Latch_IF_OF l_IF_OF{};
    Latch_OF_EX l_OF_EX{};
    Latch_EX_MARW l_EX_MARW{};

    // Next cycle latch buffers
    Latch_IF_OF next_IF_OF{};
    Latch_OF_EX next_OF_EX{};
    Latch_EX_MARW next_EX_MARW{};

    // countings 
    uint64_t cycles{0};
    uint64_t retiredInstructions{0};
    uint64_t stallCycles{0};
    uint64_t bubbleCycles{0};
    uint64_t branchCount{0};
    uint64_t branchTakenCount{0};

    // Cycle history tracking for pipeline trace & timing diagram
    uint64_t nextInstId{1};
    std::vector<visualizer::CycleSnapshot> cycleHistory;

    // we are starting with forwarding enabled 
    bool forwardingEnabled{true};

    memory::MemoryHierarchy memHierarchy;
    bool useMemoryHierarchy{false};
    ForwardingUnit fwdUnit{ForwardingMode::FULL};
    HazardUnit hazardUnit{4, ForwardingMode::FULL};
    BranchPredictor branchPredictor{BranchPolicy::BIMODAL_2BIT};
    PerformanceTracker perfTracker{4};

public:
    explicit Pipeline4Stage(size_t memSize = 1024 * 1024)
        : memory(memSize), memHierarchy(memory::getPreset("ideal")) {
        reset();
    }

// this is reset function which makes all the values to the initial value 
    void reset() {
        regFile.reset();
        memory.reset();
        if (useMemoryHierarchy) memHierarchy.reset();
        fwdUnit.reset();
        hazardUnit.reset();
        branchPredictor.reset();
        perfTracker.reset();
        pc = 0;
        programSize = 0;
        halted = false;
        currentException.clear();

        l_IF_OF.reset();
        l_OF_EX.reset();
        l_EX_MARW.reset();
        next_IF_OF.reset();
        next_OF_EX.reset();
        next_EX_MARW.reset();

        cycles = 0;
        retiredInstructions = 0;
        stallCycles = 0;
        bubbleCycles = 0;
        branchCount = 0;
        branchTakenCount = 0;
        nextInstId = 1;
        cycleHistory.clear();
    }

    void setProgramSize(Word sz) { programSize = sz; }
//enable or disable forwarding function runs here and prints what ever there 
    void setForwarding(bool enable) {
        forwardingEnabled = enable;
        fwdUnit.mode = enable ? ForwardingMode::FULL : ForwardingMode::NONE;
        hazardUnit.forwardingMode = fwdUnit.mode;
    }
    bool isForwardingEnabled() const { return forwardingEnabled; }

    void setMemPreset(const std::string& preset) {
        auto cfg = memory::getPreset(preset);
        cfg.cpuFreqGhz = 0.5; // 500 MHz for 4-Stage Pipeline
        memHierarchy = memory::MemoryHierarchy(cfg);
        useMemoryHierarchy = (preset != "ideal");
    }

    void setBranchPolicy(const std::string& pol) {
        branchPredictor.policy = parseBranchPolicy(pol);
    }

    void setForwardingMode(const std::string& mode) {
        fwdUnit.mode = parseForwardingMode(mode);
        forwardingEnabled = (fwdUnit.mode != ForwardingMode::NONE);
        hazardUnit.forwardingMode = fwdUnit.mode;
    }

    void loadProgram(Word base, const std::vector<Word>& words) {
        memory.loadProgram(base, words);
        if (useMemoryHierarchy) {
            memHierarchy.loadProgram(base, words);
        }
        setProgramSize(static_cast<Word>(words.size() * 4));
    }

    // Check if pipeline has completely drained or not 
    bool isDrained() const {
        return (programSize > 0 && pc >= programSize) &&
               l_IF_OF.isBubble && l_OF_EX.isBubble && l_EX_MARW.isBubble;
    }

    // one clock cycle 
    void stepCycle() {
        if (halted) return;
        cycles++;

        // Control Signals for current cycle
        bool stall_IF = false;
        bool stall_OF = false;
        bool bubble_EX = false;
        bool isBranchTaken = false;
        Word branchPC = 0;
        bool isBranchMispredict = false;
        Word redirectPC = 0;
        std::string fwdInfo = "";
        std::string stallReasonLabel = "";

        if (!l_EX_MARW.isBubble) {
            const auto& d = l_EX_MARW.decoded;
            Word writebackData = l_EX_MARW.aluResult;

            if (d.isLoad()) {
                if (useMemoryHierarchy) {
                    auto res = memHierarchy.readWord(l_EX_MARW.aluResult, false, l_EX_MARW.pc, cycles);
                    Word data = std::get<0>(res);
                    uint32_t lat = std::get<1>(res);
                    writebackData = data;
                    if (lat > 1) {
                        perfTracker.recordStall("DCACHE_MISS", lat - 1);
                        stallCycles += (lat - 1);
                        cycles += (lat - 1);
                    }
                } else {
                    // Read word from memory
                    writebackData = memory.readWord(l_EX_MARW.aluResult, &currentException);
                }
            } else if (d.isStore()) {
                if (useMemoryHierarchy) {
                    uint32_t lat = memHierarchy.writeWord(l_EX_MARW.aluResult, l_EX_MARW.op2, l_EX_MARW.pc, cycles);
                    if (lat > 1) {
                        perfTracker.recordStall("DCACHE_MISS", lat - 1);
                        stallCycles += (lat - 1);
                        cycles += (lat - 1);
                    }
                } else {
                    // Store op2 into memory at aluResult
                    memory.writeWord(l_EX_MARW.aluResult, l_EX_MARW.op2, &currentException);
                }
            } else if (d.isCall()) {
                // Call writes return address (PC + 4)
                writebackData = l_EX_MARW.pc + 4;
            }

            // Writeback to register file if instruction produces a register value
            if (d.writesRegister() && !currentException.hasOccurred()) {
                regFile.writePort(d.getDestReg(), writebackData, true);
            }

            retiredInstructions++;
        }
        // 3.EX
     
        if (!l_OF_EX.isBubble) {
            const auto& d = l_OF_EX.decoded;

            // Determine operand A with Forwarding
            Word opA = l_OF_EX.op1;
            if (forwardingEnabled && !l_EX_MARW.isBubble && l_EX_MARW.decoded.writesRegister()) {
                RegId destPrev = l_EX_MARW.decoded.getDestReg();
                if (d.readsRs1() && d.rs1 == destPrev) {
                    hazardUnit.rawHazardsDetected++;
                    fwdUnit.exToExForwards++;
                    fwdInfo = "r" + std::to_string(d.rs1) + " forwarded from EX_MARW -> EX_IN1";
                    // Forwarded value from previous instruction (ALU result or Load result)
                    if (l_EX_MARW.decoded.isLoad()) {
                        opA = memory.readWord(l_EX_MARW.aluResult, nullptr);
                    } else if (l_EX_MARW.decoded.isCall()) {
                        opA = l_EX_MARW.pc + 4;
                    } else {
                        opA = l_EX_MARW.aluResult;
                    }
                }
            }

            // Determine operand B with Forwarding
            Word opB = l_OF_EX.isImmediate ? l_OF_EX.immx : l_OF_EX.op2;
            if (!l_OF_EX.isImmediate && forwardingEnabled && !l_EX_MARW.isBubble && l_EX_MARW.decoded.writesRegister()) {
                RegId destPrev = l_EX_MARW.decoded.getDestReg();
                if (d.readsRs2() && d.rs2 == destPrev) {
                    hazardUnit.rawHazardsDetected++;
                    fwdUnit.exToExForwards++;
                    if (!fwdInfo.empty()) fwdInfo += " | ";
                    fwdInfo += "r" + std::to_string(d.rs2) + " forwarded from EX_MARW -> EX_IN2";
                    if (l_EX_MARW.decoded.isLoad()) {
                        opB = memory.readWord(l_EX_MARW.aluResult, nullptr);
                    } else if (l_EX_MARW.decoded.isCall()) {
                        opB = l_EX_MARW.pc + 4;
                    } else {
                        opB = l_EX_MARW.aluResult;
                    }
                }
            }

            // Store instruction data forwarding (st rd, imm[rs1] stores rd)
            Word storeData = l_OF_EX.op2;
            if (d.isStore() && forwardingEnabled && !l_EX_MARW.isBubble && l_EX_MARW.decoded.writesRegister()) {
                if (d.rd == l_EX_MARW.decoded.getDestReg()) {
                    hazardUnit.rawHazardsDetected++;
                    fwdUnit.storeDataForwards++;
                    if (!fwdInfo.empty()) fwdInfo += " | ";
                    fwdInfo += "r" + std::to_string(d.rd) + " forwarded from EX_MARW -> STORE_DATA";
                    if (l_EX_MARW.decoded.isLoad()) {
                        storeData = memory.readWord(l_EX_MARW.aluResult, nullptr);
                    } else {
                        storeData = l_EX_MARW.aluResult;
                    }
                }
            }

            // Execute in ALU
            auto aluOut = aluUnit.execute(d.opcode, opA, opB, regFile.getFlags());
            if (d.opcode == isa::OP_CMP) {
                regFile.setFlags(aluOut.flags);
            }
            if (aluOut.exception.hasOccurred()) {
                currentException = aluOut.exception;
                currentException.faultingPC = l_OF_EX.pc;
            }

            // Branch Condition Evaluation
            if (d.isBranch()) {
                branchCount++;
                Flags flg = regFile.getFlags();

                if (d.opcode == isa::OP_B) {
                    isBranchTaken = true;
                    branchPC = l_OF_EX.branchTarget;
                } else if (d.opcode == isa::OP_BEQ) {
                    isBranchTaken = flg.E;
                    branchPC = l_OF_EX.branchTarget;
                } else if (d.opcode == isa::OP_BGT) {
                    isBranchTaken = flg.GT;
                    branchPC = l_OF_EX.branchTarget;
                } else if (d.opcode == isa::OP_CALL) {
                    isBranchTaken = true;
                    branchPC = l_OF_EX.branchTarget;
                } else if (d.opcode == isa::OP_RET) {
                    isBranchTaken = true;
                    branchPC = opA; // ra
                } else if (d.opcode == isa::OP_RETZ) {
                    isBranchTaken = true;
                    branchPC = regFile.getOldPC();
                    regFile.setCPL(1);
                }

                if (isBranchTaken) {
                    branchTakenCount++;
                }

                bool wasPredTaken = l_OF_EX.predictedTaken;
                Word wasPredTgt = l_OF_EX.predictedTarget;
                branchPredictor.update(l_OF_EX.pc, d, isBranchTaken, branchPC, wasPredTaken);

                // Branch misprediction detection
                bool mispredicted = (isBranchTaken != wasPredTaken) || (isBranchTaken && branchPC != wasPredTgt);
                if (mispredicted) {
                    isBranchMispredict = true;
                    hazardUnit.branchFlushCount++;
                    redirectPC = isBranchTaken ? branchPC : (l_OF_EX.pc + 4);
                }
            }

            // Prepare next EX_MARW latch
            next_EX_MARW.pc = l_OF_EX.pc;
            next_EX_MARW.instruction = l_OF_EX.instruction;
            next_EX_MARW.decoded = d;
            next_EX_MARW.aluResult = aluOut.result;
            next_EX_MARW.op2 = storeData;
            next_EX_MARW.isBubble = false;
            next_EX_MARW.disassembly = l_OF_EX.disassembly;
            next_EX_MARW.instId = l_OF_EX.instId;
        } else {
            next_EX_MARW.reset();
        }
        
        // STAGE 2: OF
        if (!l_IF_OF.isBubble) {
            DecodedInst d = assembler::Disassembler::decode(l_IF_OF.instruction, l_IF_OF.pc);

            // Hazard Detection Unit:
            // Check for Load-Use Hazard (Instruction in EX is LD and OF needs loaded register)
            bool loadUseHazard = false;
            if (!l_OF_EX.isBubble && l_OF_EX.decoded.isLoad()) {
                RegId loadDest = l_OF_EX.decoded.rd;
                if ((d.readsRs1() && d.rs1 == loadDest) ||
                    (d.readsRs2() && d.rs2 == loadDest)) {
                    loadUseHazard = true;
                }
            }

            // If forwarding is disabled, stall for ANY RAW dependency until previous instruction clears EX
            bool rawHazardNoForward = false;
            if (!forwardingEnabled) {
                if (!l_OF_EX.isBubble && l_OF_EX.decoded.writesRegister()) {
                    RegId dest = l_OF_EX.decoded.getDestReg();
                    if ((d.readsRs1() && d.rs1 == dest) ||
                        (d.readsRs2() && d.rs2 == dest) ||
                        (d.isStore() && d.rd == dest)) {
                        rawHazardNoForward = true;
                    }
                }
            }

            if (loadUseHazard || rawHazardNoForward) {
                // Interlock: Stall IF and OF, insert bubble into EX
                stall_IF = true;
                stall_OF = true;
                bubble_EX = true;
                stallCycles++;
                if (loadUseHazard) hazardUnit.loadUseStalls++;
                if (rawHazardNoForward) {
                    hazardUnit.rawHazardsDetected++;
                    hazardUnit.rawNoFwdStalls++;
                }
                stallReasonLabel = loadUseHazard ? "RAW_LOAD_USE" : "RAW_NOFWD";
                perfTracker.recordStall(stallReasonLabel);
            } else {
                // Read register operands
                Word op1 = regFile.readPort1(d.isRet() ? REG_RA : d.rs1);
                Word op2 = regFile.readPort2(d.isStore() ? d.rd : d.rs2);

                // Forwarding from MA_RW writeback port into OF stage if write & read same cycle (RW->OF)
                if (forwardingEnabled && !l_EX_MARW.isBubble && l_EX_MARW.decoded.writesRegister()) {
                    RegId wbReg = l_EX_MARW.decoded.getDestReg();
                    Word wbData = l_EX_MARW.aluResult;
                    if (l_EX_MARW.decoded.isLoad()) {
                        wbData = memory.readWord(l_EX_MARW.aluResult, nullptr);
                    } else if (l_EX_MARW.decoded.isCall()) {
                        wbData = l_EX_MARW.pc + 4;
                    }
                    if (d.readsRs1() && d.rs1 == wbReg) {
                        op1 = wbData;
                        hazardUnit.rawHazardsDetected++;
                        fwdUnit.wbToExForwards++;
                    }
                    if (d.readsRs2() && d.rs2 == wbReg) {
                        op2 = wbData;
                        hazardUnit.rawHazardsDetected++;
                        fwdUnit.wbToExForwards++;
                    }
                    if (d.isStore() && d.rd == wbReg) {
                        op2 = wbData;
                        hazardUnit.rawHazardsDetected++;
                        fwdUnit.wbToExForwards++;
                    }
                }

                next_OF_EX.pc = l_IF_OF.pc;
                next_OF_EX.instruction = l_IF_OF.instruction;
                next_OF_EX.decoded = d;
                next_OF_EX.op1 = op1;
                next_OF_EX.op2 = op2;
                next_OF_EX.immx = d.immx;
                next_OF_EX.branchTarget = d.branchTarget;
                next_OF_EX.isImmediate = d.isImmediate;
                next_OF_EX.isBubble = false;
                next_OF_EX.disassembly = l_IF_OF.disassembly;
                next_OF_EX.instId = l_IF_OF.instId;
                next_OF_EX.predictedTaken = l_IF_OF.predictedTaken;
                next_OF_EX.predictedTarget = l_IF_OF.predictedTarget;
            }
        } else {
            next_OF_EX.reset();
        }

        // STAGE 1: IF 
        if (isBranchMispredict) {
            // Branch penalty / flush: Mispredicted branch in EX
            // Younger instruction in OF is flushed (1 cycle branch penalty for 4-stage)
            stallReasonLabel = "BRANCH_MISPREDICT";
            pc = redirectPC;
            next_OF_EX.reset(); // Flush wrong-path instruction in OF
            bubbleCycles += 1;
            perfTracker.recordBranchBubble(1);
            perfTracker.recordStall("BRANCH_MISPREDICT", 1);

            Word instWord = 0;
            if (useMemoryHierarchy) {
                auto res = memHierarchy.readWord(pc, true, pc, cycles);
                Word w = std::get<0>(res);
                uint32_t lat = std::get<1>(res);
                instWord = w;
                if (lat > 1) {
                    perfTracker.recordStall("ICACHE_MISS", lat - 1);
                    stallCycles += (lat - 1);
                    cycles += (lat - 1);
                }
            } else {
                instWord = memory.readWord(pc, &currentException);
                if (currentException.hasOccurred()) {
                    currentException.faultingPC = pc;
                    halted = true;
                    return;
                }
            }

            // Check dynamic branch predictor at fetch time
            uint32_t predTgt = 0;
            bool predTaken = branchPredictor.predictAtFetch(pc, predTgt);

            next_IF_OF.pc = pc;
            next_IF_OF.instruction = instWord;
            next_IF_OF.isBubble = false;
            next_IF_OF.disassembly = assembler::Disassembler::disassemble(instWord, pc);
            next_IF_OF.instId = nextInstId++;
            next_IF_OF.predictedTaken = predTaken;
            next_IF_OF.predictedTarget = predTgt;

            if (predTaken) {
                pc = predTgt;
            } else {
                pc += 4;
            }
        } else if (programSize > 0 && pc >= programSize) {
            next_IF_OF.reset(); // Program completed, drain pipeline with bubbles
        } else if (!stall_IF) {
            Word instWord = 0;
            if (useMemoryHierarchy) {
                auto res = memHierarchy.readWord(pc, true, pc, cycles);
                Word w = std::get<0>(res);
                uint32_t lat = std::get<1>(res);
                instWord = w;
                if (lat > 1) {
                    perfTracker.recordStall("ICACHE_MISS", lat - 1);
                    stallCycles += (lat - 1);
                    cycles += (lat - 1);
                }
            } else {
                instWord = memory.readWord(pc, &currentException);
                if (currentException.hasOccurred()) {
                    currentException.faultingPC = pc;
                    halted = true;
                    return;
                }
            }

            // Check dynamic branch predictor at fetch time
            uint32_t predTgt = 0;
            bool predTaken = branchPredictor.predictAtFetch(pc, predTgt);

            next_IF_OF.pc = pc;
            next_IF_OF.instruction = instWord;
            next_IF_OF.isBubble = false;
            next_IF_OF.disassembly = assembler::Disassembler::disassemble(instWord, pc);
            next_IF_OF.instId = nextInstId++;
            next_IF_OF.predictedTaken = predTaken;
            next_IF_OF.predictedTarget = predTgt;

            if (predTaken) {
                pc = predTgt;
            } else {
                pc += 4;
            }
        }

        // Record cycle snapshot for trace visualization
        visualizer::CycleSnapshot snap;
        snap.cycle = cycles;

        // Stage 1: IF
        visualizer::PipelineStageInfo ifStage;
        ifStage.stageName = "IF";
        if (stall_IF) {
            ifStage.instructionText = next_IF_OF.disassembly;
            ifStage.pc = next_IF_OF.pc;
            ifStage.isBubble = next_IF_OF.isBubble;
            ifStage.isStalled = true;
            ifStage.instId = next_IF_OF.instId;
            ifStage.stallReason = stallReasonLabel;
        } else {
            ifStage.instructionText = next_IF_OF.isBubble ? "" : next_IF_OF.disassembly;
            ifStage.pc = next_IF_OF.pc;
            ifStage.isBubble = next_IF_OF.isBubble;
            ifStage.isStalled = false;
            ifStage.instId = next_IF_OF.instId;
            if (isBranchMispredict) ifStage.stallReason = "BRANCH_MISPREDICT";
        }
        snap.stages.push_back(ifStage);

        // Stage 2: OF
        visualizer::PipelineStageInfo ofStage;
        ofStage.stageName = "OF";
        ofStage.instructionText = l_IF_OF.isBubble ? "" : l_IF_OF.disassembly;
        ofStage.pc = l_IF_OF.pc;
        ofStage.isBubble = l_IF_OF.isBubble;
        ofStage.isStalled = stall_OF;
        ofStage.instId = l_IF_OF.instId;
        if (stall_OF) ofStage.stallReason = stallReasonLabel;
        snap.stages.push_back(ofStage);

        // Stage 3: EX
        visualizer::PipelineStageInfo exStage;
        exStage.stageName = "EX";
        exStage.instructionText = l_OF_EX.isBubble ? "" : l_OF_EX.disassembly;
        exStage.pc = l_OF_EX.pc;
        exStage.isBubble = l_OF_EX.isBubble;
        exStage.isStalled = bubble_EX;
        exStage.instId = l_OF_EX.instId;
        if (bubble_EX) {
            exStage.isBubble = true;
            exStage.stallReason = stallReasonLabel;
        }
        exStage.forwardingInfo = fwdInfo;
        snap.stages.push_back(exStage);

        // Stage 4: MA_RW
        visualizer::PipelineStageInfo marwStage;
        marwStage.stageName = "MA_RW";
        marwStage.instructionText = l_EX_MARW.isBubble ? "" : l_EX_MARW.disassembly;
        marwStage.pc = l_EX_MARW.pc;
        marwStage.isBubble = l_EX_MARW.isBubble;
        marwStage.isStalled = false;
        marwStage.instId = l_EX_MARW.instId;
        snap.stages.push_back(marwStage);

        cycleHistory.push_back(snap);

        if (bubble_EX) {
            l_OF_EX.reset();
        } else if (!stall_OF) {
            l_OF_EX = next_OF_EX;
        }

        if (!stall_IF) {
            l_IF_OF = next_IF_OF;
        }
        l_EX_MARW = next_EX_MARW;

        perfTracker.totalCycles = cycles;
        perfTracker.retiredInstructions = retiredInstructions;
    }

    // Run until drained or breakpoint or max cycles
    void run(uint64_t maxCycles = 100000) {
        while (!halted && cycles < maxCycles) {
            stepCycle();
            if (isDrained()) {
                break;
            }
        }
        perfTracker.totalCycles = cycles;
        perfTracker.retiredInstructions = retiredInstructions;
    }

    std::vector<visualizer::PipelineStageInfo> getStageInfo() const {
        if (!cycleHistory.empty()) {
            return cycleHistory.back().stages;
        }
        std::vector<visualizer::PipelineStageInfo> info;
        info.push_back({"IF", l_IF_OF.isBubble ? "" : l_IF_OF.disassembly, l_IF_OF.pc, l_IF_OF.isBubble, false, l_IF_OF.instId});
        info.push_back({"OF", l_OF_EX.isBubble ? "" : l_OF_EX.disassembly, l_OF_EX.pc, l_OF_EX.isBubble, false, l_OF_EX.instId});
        info.push_back({"EX", l_EX_MARW.isBubble ? "" : l_EX_MARW.disassembly, l_EX_MARW.pc, l_EX_MARW.isBubble, false, l_EX_MARW.instId});
        info.push_back({"MA_RW", l_EX_MARW.isBubble ? "" : l_EX_MARW.disassembly, l_EX_MARW.pc, l_EX_MARW.isBubble, false, l_EX_MARW.instId});
        return info;
    }
    std::string printStats() const {
        std::ostringstream oss;
        double cpi = (retiredInstructions > 0) ? (static_cast<double>(cycles) / retiredInstructions) : 0.0;
        double ipc = (cycles > 0) ? (static_cast<double>(retiredInstructions) / cycles) : 0.0;
        oss << "=== 4-Stage Pipeline Statistics ===\n"
            << "Total Clock Cycles      : " << cycles << "\n"
            << "Retired Instructions    : " << retiredInstructions << "\n"
            << "Cycles Per Inst (CPI)   : " << std::fixed << std::setprecision(3) << cpi << "\n"
            << "Inst Per Cycle (IPC)    : " << std::fixed << std::setprecision(3) << ipc << "\n"
            << "Stall Cycles (RAW/Load) : " << stallCycles << "\n"
            << "Branch Bubble Cycles    : " << bubbleCycles << "\n"
            << "Total Branches          : " << branchCount << "\n"
            << "Taken Branches          : " << branchTakenCount << "\n"
            << "Forwarding Enabled      : " << (forwardingEnabled ? "YES" : "NO (Interlocks only)") << "\n";
        return oss.str();
    }
};
}
} 
#endif 
