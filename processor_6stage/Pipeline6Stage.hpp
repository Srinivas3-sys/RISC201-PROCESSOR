#ifndef RISC201_PIPELINE_6STAGE_HPP
#define RISC201_PIPELINE_6STAGE_HPP

#include "StageRegisters6.hpp"
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
namespace stage6 {

/**
 * 6-Stage Pipelined Processor: IF -> ID -> RF -> EX -> MA -> RW
 *
 * Stage Splitting Decision:
 *   Operand Fetch (OF) is split into:
 *     1. ID (Instruction Decode & Immediate Generation)
 *     2. RF (Register Fetch & Operand Routing)
 *   Rationale: In high-performance RISC pipelines (such as MIPS R4000, ARM Cortex-A8),
 *   decoding instructions and accessing multi-ported SRAM register files in a single
 *   cycle creates an overly long critical path. Splitting them balances front-end timing
 *   and allows higher processor clock frequency.
 */
class Pipeline6Stage {
public:
    RegisterFile regFile;
    Memory memory;
    alu::ALU aluUnit;

    Word pc{0};
    Word programSize{0};
    bool halted{false};
    CpuException currentException{};

    // Pipeline Latches
    Latch_IF_ID l_IF_ID{};
    Latch_ID_RF l_ID_RF{};
    Latch_RF_EX l_RF_EX{};
    Latch_EX_MA l_EX_MA{};
    Latch_MA_RW l_MA_RW{};

    // Next cycle buffers for edge-triggered synchronization
    Latch_IF_ID next_IF_ID{};
    Latch_ID_RF next_ID_RF{};
    Latch_RF_EX next_RF_EX{};
    Latch_EX_MA next_EX_MA{};
    Latch_MA_RW next_MA_RW{};

    // Performance Counters
    uint64_t cycles{0};
    uint64_t retiredInstructions{0};
    uint64_t stallCycles{0};
    uint64_t bubbleCycles{0};
    uint64_t branchCount{0};
    uint64_t branchTakenCount{0};

    bool forwardingEnabled{true};

    // Advanced Subsystem Extensions
    memory::MemoryHierarchy memHierarchy;
    bool useMemoryHierarchy{false};
    ForwardingUnit fwdUnit{ForwardingMode::FULL};
    HazardUnit hazardUnit{6, ForwardingMode::FULL};
    BranchPredictor branchPredictor{BranchPolicy::BIMODAL_2BIT};
    PerformanceTracker perfTracker{6};

public:
    explicit Pipeline6Stage(size_t memSize = 1024 * 1024)
        : memory(memSize), memHierarchy(memory::getPreset("ideal")) {
        reset();
    }

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

        l_IF_ID.reset();
        l_ID_RF.reset();
        l_RF_EX.reset();
        l_EX_MA.reset();
        l_MA_RW.reset();

        next_IF_ID.reset();
        next_ID_RF.reset();
        next_RF_EX.reset();
        next_EX_MA.reset();
        next_MA_RW.reset();

        cycles = 0;
        retiredInstructions = 0;
        stallCycles = 0;
        bubbleCycles = 0;
        branchCount = 0;
        branchTakenCount = 0;
    }

    void setProgramSize(Word sz) { programSize = sz; }
    void setForwarding(bool enable) {
        forwardingEnabled = enable;
        fwdUnit.mode = enable ? ForwardingMode::FULL : ForwardingMode::NONE;
        hazardUnit.forwardingMode = fwdUnit.mode;
    }
    bool isForwardingEnabled() const { return forwardingEnabled; }

    void setMemPreset(const std::string& preset) {
        memHierarchy = memory::MemoryHierarchy(memory::getPreset(preset));
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

    bool isDrained() const {
        return (programSize > 0 && pc >= programSize) &&
               l_IF_ID.isBubble && l_ID_RF.isBubble &&
               l_RF_EX.isBubble && l_EX_MA.isBubble && l_MA_RW.isBubble;
    }

    /**
     * Execute one clock cycle of the 6-stage pipeline.
     */
    void stepCycle() {
        if (halted) return;
        cycles++;

        bool stall_IF = false;
        bool stall_ID = false;
        bool stall_RF = false;
        bool bubble_EX = false;
        bool isBranchTaken = false;
        Word branchPC = 0;

        // =========================================================================
        // STAGE 6: RW (Register Writeback Stage)
        // =========================================================================
        if (!l_MA_RW.isBubble) {
            const auto& d = l_MA_RW.decoded;
            Word wbData = l_MA_RW.aluResult;
            if (d.isLoad()) {
                wbData = l_MA_RW.ldResult;
            } else if (d.isCall()) {
                wbData = l_MA_RW.pc + 4;
            }

            if (d.writesRegister() && !currentException.hasOccurred()) {
                regFile.writePort(d.getDestReg(), wbData, true);
            }
            retiredInstructions++;
        }

        // =========================================================================
        // STAGE 5: MA (Memory Access Stage)
        // =========================================================================
        if (!l_EX_MA.isBubble) {
            const auto& d = l_EX_MA.decoded;
            Word ldData = 0;

            // Store data forwarding from MA_RW to MA if store data was written by preceding instruction
            Word storeData = l_EX_MA.op2;
            if (d.isStore() && forwardingEnabled && !l_MA_RW.isBubble && l_MA_RW.decoded.writesRegister()) {
                if (d.rd == l_MA_RW.decoded.getDestReg()) {
                    storeData = l_MA_RW.decoded.isLoad() ? l_MA_RW.ldResult : l_MA_RW.aluResult;
                }
            }

            if (d.isLoad()) {
                if (useMemoryHierarchy) {
                    auto [data, lat] = memHierarchy.readWord(l_EX_MA.aluResult, false, l_EX_MA.pc, cycles);
                    ldData = data;
                    if (lat > 1) {
                        perfTracker.recordStall("DCACHE_MISS", lat - 1);
                        stallCycles += (lat - 1);
                        cycles += (lat - 1);
                    }
                } else {
                    ldData = memory.readWord(l_EX_MA.aluResult, &currentException);
                }
            } else if (d.isStore()) {
                if (useMemoryHierarchy) {
                    uint32_t lat = memHierarchy.writeWord(l_EX_MA.aluResult, storeData, l_EX_MA.pc, cycles);
                    if (lat > 1) {
                        perfTracker.recordStall("DCACHE_MISS", lat - 1);
                        stallCycles += (lat - 1);
                        cycles += (lat - 1);
                    }
                } else {
                    memory.writeWord(l_EX_MA.aluResult, storeData, &currentException);
                }
            }

            next_MA_RW.pc = l_EX_MA.pc;
            next_MA_RW.instruction = l_EX_MA.instruction;
            next_MA_RW.decoded = d;
            next_MA_RW.aluResult = l_EX_MA.aluResult;
            next_MA_RW.ldResult = ldData;
            next_MA_RW.isBubble = false;
            next_MA_RW.disassembly = l_EX_MA.disassembly;
        } else {
            next_MA_RW.reset();
        }

        // =========================================================================
        // STAGE 4: EX (Execute Stage)
        // =========================================================================
        if (!l_RF_EX.isBubble) {
            const auto& d = l_RF_EX.decoded;

            // Forwarding for Operand A:
            // Priority 1: EX_MA (distance 1)
            // Priority 2: MA_RW (distance 2)
            Word opA = l_RF_EX.op1;
            if (forwardingEnabled && d.readsRs1()) {
                RegId r1 = (d.isRet()) ? REG_RA : d.rs1;
                if (!l_EX_MA.isBubble && l_EX_MA.decoded.writesRegister() && l_EX_MA.decoded.getDestReg() == r1) {
                    opA = l_EX_MA.aluResult;
                } else if (!l_MA_RW.isBubble && l_MA_RW.decoded.writesRegister() && l_MA_RW.decoded.getDestReg() == r1) {
                    opA = l_MA_RW.decoded.isLoad() ? l_MA_RW.ldResult : l_MA_RW.aluResult;
                }
            }

            // Forwarding for Operand B:
            Word opB = l_RF_EX.isImmediate ? l_RF_EX.immx : l_RF_EX.op2;
            if (!l_RF_EX.isImmediate && forwardingEnabled && d.readsRs2()) {
                RegId r2 = d.rs2;
                if (!l_EX_MA.isBubble && l_EX_MA.decoded.writesRegister() && l_EX_MA.decoded.getDestReg() == r2) {
                    opB = l_EX_MA.aluResult;
                } else if (!l_MA_RW.isBubble && l_MA_RW.decoded.writesRegister() && l_MA_RW.decoded.getDestReg() == r2) {
                    opB = l_MA_RW.decoded.isLoad() ? l_MA_RW.ldResult : l_MA_RW.aluResult;
                }
            }

            // Forwarding for Store data (op2)
            Word storeVal = l_RF_EX.op2;
            if (d.isStore() && forwardingEnabled) {
                if (!l_EX_MA.isBubble && l_EX_MA.decoded.writesRegister() && l_EX_MA.decoded.getDestReg() == d.rd) {
                    storeVal = l_EX_MA.aluResult;
                } else if (!l_MA_RW.isBubble && l_MA_RW.decoded.writesRegister() && l_MA_RW.decoded.getDestReg() == d.rd) {
                    storeVal = l_MA_RW.decoded.isLoad() ? l_MA_RW.ldResult : l_MA_RW.aluResult;
                }
            }

            // Execute in ALU
            auto aluOut = aluUnit.execute(d.opcode, opA, opB, regFile.getFlags());
            if (d.opcode == OP_CMP) {
                regFile.setFlags(aluOut.flags);
            }
            if (aluOut.exception.hasOccurred()) {
                currentException = aluOut.exception;
                currentException.faultingPC = l_RF_EX.pc;
            }

            // Branch Condition Evaluation
            if (d.isBranch()) {
                branchCount++;
                Flags flg = regFile.getFlags();

                if (d.opcode == OP_B) {
                    isBranchTaken = true;
                    branchPC = l_RF_EX.branchTarget;
                } else if (d.opcode == OP_BEQ) {
                    isBranchTaken = flg.E;
                    branchPC = l_RF_EX.branchTarget;
                } else if (d.opcode == OP_BGT) {
                    isBranchTaken = flg.GT;
                    branchPC = l_RF_EX.branchTarget;
                } else if (d.opcode == OP_CALL) {
                    isBranchTaken = true;
                    branchPC = l_RF_EX.branchTarget;
                } else if (d.opcode == OP_RET) {
                    isBranchTaken = true;
                    branchPC = opA;
                } else if (d.opcode == OP_RETZ) {
                    isBranchTaken = true;
                    branchPC = regFile.getOldPC();
                    regFile.setCPL(1);
                }

                if (isBranchTaken) {
                    branchTakenCount++;
                }
            }

            next_EX_MA.pc = l_RF_EX.pc;
            next_EX_MA.instruction = l_RF_EX.instruction;
            next_EX_MA.decoded = d;
            next_EX_MA.aluResult = aluOut.result;
            next_EX_MA.op2 = storeVal;
            next_EX_MA.isBubble = false;
            next_EX_MA.disassembly = l_RF_EX.disassembly;
        } else {
            next_EX_MA.reset();
        }

        // =========================================================================
        // STAGE 3: RF (Register Fetch Stage)
        // =========================================================================
        if (!l_ID_RF.isBubble) {
            const auto& d = l_ID_RF.decoded;

            // Hazard Detection:
            // 1. Load-Use Hazard: Instruction in EX is Load and RF needs that register
            bool loadUse = false;
            if (!l_RF_EX.isBubble && l_RF_EX.decoded.isLoad()) {
                RegId loadDest = l_RF_EX.decoded.rd;
                if ((d.readsRs1() && d.rs1 == loadDest) ||
                    (d.readsRs2() && d.rs2 == loadDest) ||
                    (d.isStore() && d.rd == loadDest)) {
                    loadUse = true;
                }
            }

            // 2. RAW Hazards if forwarding is disabled
            bool rawNoForward = false;
            if (!forwardingEnabled) {
                auto checkConflict = [&](const DecodedInst& prod) {
                    if (!prod.writesRegister()) return false;
                    RegId dest = prod.getDestReg();
                    return (d.readsRs1() && d.rs1 == dest) ||
                           (d.readsRs2() && d.rs2 == dest) ||
                           (d.isStore() && d.rd == dest);
                };
                if (!l_RF_EX.isBubble && checkConflict(l_RF_EX.decoded)) rawNoForward = true;
                if (!l_EX_MA.isBubble && checkConflict(l_EX_MA.decoded)) rawNoForward = true;
            }

            if (loadUse || rawNoForward) {
                stall_IF = true;
                stall_ID = true;
                stall_RF = true;
                bubble_EX = true;
                stallCycles++;
                perfTracker.recordStall(loadUse ? "RAW_LOAD_USE" : "RAW_NOFWD");
            } else {
                // Fetch register operands
                Word op1 = regFile.readPort1(d.isRet() ? REG_RA : d.rs1);
                Word op2 = regFile.readPort2(d.isStore() ? d.rd : d.rs2);

                // RW -> RF Forwarding: If instruction currently in RW writes to the register being read in RF
                if (forwardingEnabled && !l_MA_RW.isBubble && l_MA_RW.decoded.writesRegister()) {
                    RegId wbReg = l_MA_RW.decoded.getDestReg();
                    Word wbData = l_MA_RW.decoded.isLoad() ? l_MA_RW.ldResult : l_MA_RW.aluResult;
                    if (l_MA_RW.decoded.isCall()) wbData = l_MA_RW.pc + 4;

                    if (d.readsRs1() && d.rs1 == wbReg) op1 = wbData;
                    if (d.readsRs2() && d.rs2 == wbReg) op2 = wbData;
                    if (d.isStore() && d.rd == wbReg) op2 = wbData;
                }

                next_RF_EX.pc = l_ID_RF.pc;
                next_RF_EX.instruction = l_ID_RF.instruction;
                next_RF_EX.decoded = d;
                next_RF_EX.op1 = op1;
                next_RF_EX.op2 = op2;
                next_RF_EX.immx = l_ID_RF.immx;
                next_RF_EX.branchTarget = l_ID_RF.branchTarget;
                next_RF_EX.isImmediate = l_ID_RF.isImmediate;
                next_RF_EX.isBubble = false;
                next_RF_EX.disassembly = l_ID_RF.disassembly;
            }
        } else {
            next_RF_EX.reset();
        }

        // =========================================================================
        // STAGE 2: ID (Instruction Decode Stage)
        // =========================================================================
        if (!l_IF_ID.isBubble) {
            if (!stall_ID) {
                DecodedInst d = assembler::Disassembler::decode(l_IF_ID.instruction, l_IF_ID.pc);
                next_ID_RF.pc = l_IF_ID.pc;
                next_ID_RF.instruction = l_IF_ID.instruction;
                next_ID_RF.decoded = d;
                next_ID_RF.immx = d.immx;
                next_ID_RF.branchTarget = d.branchTarget;
                next_ID_RF.isImmediate = d.isImmediate;
                next_ID_RF.isBubble = false;
                next_ID_RF.disassembly = l_IF_ID.disassembly;
            }
        } else {
            next_ID_RF.reset();
        }

        // =========================================================================
        // STAGE 1: IF (Instruction Fetch Stage)
        // =========================================================================
        if (isBranchTaken) {
            // Flush wrong path instructions in IF_ID, ID_RF, RF_EX (3 cycles branch penalty)
            pc = branchPC;
            next_IF_ID.reset();
            next_ID_RF.reset();
            next_RF_EX.reset();
            bubbleCycles += 3;
            perfTracker.recordBranchBubble(3);
        } else if (programSize > 0 && pc >= programSize) {
            next_IF_ID.reset(); // Program completed, drain pipeline with bubbles
        } else if (!stall_IF) {
            Word instWord = 0;
            if (useMemoryHierarchy) {
                auto [w, lat] = memHierarchy.readWord(pc, true, pc, cycles);
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

            next_IF_ID.pc = pc;
            next_IF_ID.instruction = instWord;
            next_IF_ID.isBubble = false;
            next_IF_ID.disassembly = assembler::Disassembler::disassemble(instWord, pc);

            pc += 4;
        }

        // Latch synchronous updates
        if (bubble_EX) {
            l_RF_EX.reset();
        } else if (!stall_RF) {
            l_RF_EX = next_RF_EX;
        }

        if (!stall_ID) l_ID_RF = next_ID_RF;
        if (!stall_IF) l_IF_ID = next_IF_ID;
        l_EX_MA = next_EX_MA;
        l_MA_RW = next_MA_RW;

        perfTracker.totalCycles = cycles;
        perfTracker.retiredInstructions = retiredInstructions;
    }

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
        std::vector<visualizer::PipelineStageInfo> info;
        info.push_back({"IF", l_IF_ID.isBubble ? "" : l_IF_ID.disassembly, l_IF_ID.pc, l_IF_ID.isBubble, false});
        info.push_back({"ID", l_ID_RF.isBubble ? "" : l_ID_RF.disassembly, l_ID_RF.pc, l_ID_RF.isBubble, false});
        info.push_back({"RF", l_RF_EX.isBubble ? "" : l_RF_EX.disassembly, l_RF_EX.pc, l_RF_EX.isBubble, false});
        info.push_back({"EX", l_EX_MA.isBubble ? "" : l_EX_MA.disassembly, l_EX_MA.pc, l_EX_MA.isBubble, false});
        info.push_back({"MA", l_MA_RW.isBubble ? "" : l_MA_RW.disassembly, l_MA_RW.pc, l_MA_RW.isBubble, false});
        info.push_back({"RW", l_MA_RW.isBubble ? "" : l_MA_RW.disassembly, l_MA_RW.pc, l_MA_RW.isBubble, false});
        return info;
    }

    std::string printStats() const {
        std::ostringstream oss;
        double cpi = (retiredInstructions > 0) ? (static_cast<double>(cycles) / retiredInstructions) : 0.0;
        double ipc = (cycles > 0) ? (static_cast<double>(retiredInstructions) / cycles) : 0.0;

        oss << "=== 6-Stage Pipeline Statistics ===\n"
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

} // namespace stage6
} // namespace risc201

#endif // RISC201_PIPELINE_6STAGE_HPP
