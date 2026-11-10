#ifndef RISC201_DEBUGGER_HPP
#define RISC201_DEBUGGER_HPP

#include "../common/Types.hpp"
#include "../common/RegisterFile.hpp"
#include "../common/Memory.hpp"
#include "../common/Exception.hpp"
#include "../alu/ALU.hpp"
#include "../microcode/MicroController.hpp"
#include "../assembler/Assembler.hpp"
#include "../assembler/Disassembler.hpp"
#include "../visualizer/StackVisualizer.hpp"
#include "../visualizer/PipelineVisualizer.hpp"
#include "../visualizer/MemoryVisualizer.hpp"
#include "../common/ISA.hpp"
#include "../processor_4stage/Pipeline4Stage.hpp"
#include "../processor_6stage/Pipeline6Stage.hpp"

#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>

namespace risc201 {
namespace debugger {

enum class SimMode {
    PIPELINE_4STAGE,
    PIPELINE_6STAGE,
    MICROCODED
};

/**
 * Interactive Command-Line Debugger & Execution Simulator (Requirements 2c, 2d, 3).
 * Supports:
 *   - 4-Stage and 6-Stage pipeline execution & visualization
 *   - Direct microprogrammed control logic execution with step-by-step microPC state
 *   - Active Stack Memory ASCII visualizer & boundary verification
 *   - Breakpoint management, memory inspection, and register inspection
 *   - Configurable ALU algorithms (RCA, CSLA, CLA; Iterative, Booth, Wallace; Restoring, Non-restoring)
 */
class Debugger {
public:
    stage4::Pipeline4Stage p4;
    stage6::Pipeline6Stage p6;
    microcode::MicroController mcu;

    SimMode currentSimMode{SimMode::MICROCODED};
    std::set<Word> breakpoints;
    std::string loadedProgramSource;
    bool enableScheduling{false};
    assembler::Assembler asmEngine;

public:
    Debugger()
        : p4(1024 * 1024), p6(1024 * 1024), mcu(&p4.regFile, &p4.memory, &p4.aluUnit) {}

    bool loadAssemblyString(const std::string& source, bool schedule = false) {
        loadedProgramSource = source;
        if (schedule) enableScheduling = true;
        bool ok = asmEngine.assemble(source, enableScheduling);
        if (!ok) {
            std::cerr << "Assembly failed!\n";
            return false;
        }

        auto words = asmEngine.getMachineWords();
        Word progSize = static_cast<Word>(words.size() * 4);

        // Load into both pipeline memories and MCU memory
        p4.reset();
        p4.setProgramSize(progSize);
        p4.memory.loadProgram(0, words);
        p4.memHierarchy.loadProgram(0, words);

        p6.reset();
        p6.setProgramSize(progSize);
        p6.memory.loadProgram(0, words);
        p6.memHierarchy.loadProgram(0, words);

        mcu.reset();
        mcu.setPC(0);

        std::cout << "Successfully assembled and loaded " << words.size()
                  << " instructions (" << (words.size() * 4) << " bytes) at address 0x00000000.\n";
        return true;
    }

    bool setCustomInput(Word n) {
        std::regex reg(R"(\bmov\s+r0\s*,\s*\d+)");
        std::string updated = std::regex_replace(loadedProgramSource, reg, "mov r0, " + std::to_string(n));
        return loadAssemblyString(updated);
    }

    void setBreakpoint(Word addr) {
        breakpoints.insert(addr);
        std::cout << "Breakpoint set at " << toHex(addr) << "\n";
    }

    void clearBreakpoint(Word addr) {
        breakpoints.erase(addr);
        std::cout << "Breakpoint removed from " << toHex(addr) << "\n";
    }

    void stepCycle() {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            p4.stepCycle();
            std::cout << "Cycle " << p4.cycles << " completed (4-Stage).\n";
            showPipeline();
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            p6.stepCycle();
            std::cout << "Cycle " << p6.cycles << " completed (6-Stage).\n";
            showPipeline();
        } else {
            // Microcoded execution mode
            bool instDone = mcu.stepMicroCycle();
            std::cout << "MicroCycle " << mcu.totalMicroCycles
                      << " | MicroPC: " << mcu.getMicroPC() << "\n";
            showMicroState();
            if (instDone) {
                std::cout << ">>> Program Instruction Completed <<<\n";
            }
        }
    }

    void stepInstruction() {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            uint64_t prevRetired = p4.retiredInstructions;
            while (!p4.halted && p4.retiredInstructions == prevRetired && !p4.isDrained()) {
                p4.stepCycle();
            }
            std::cout << "Instruction stepped. Retired: " << p4.retiredInstructions << "\n";
            showPipeline();
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            uint64_t prevRetired = p6.retiredInstructions;
            while (!p6.halted && p6.retiredInstructions == prevRetired && !p6.isDrained()) {
                p6.stepCycle();
            }
            std::cout << "Instruction stepped. Retired: " << p6.retiredInstructions << "\n";
            showPipeline();
        } else {
            mcu.stepInstruction();
            std::cout << "Microcoded instruction stepped.\n";
            showMicroState();
        }
    }

    void run(uint64_t maxCycles = 100000) {
        uint64_t stepCount = 0;
        while (stepCount < maxCycles) {
            Word currentPc = (currentSimMode == SimMode::PIPELINE_4STAGE) ? p4.pc :
                             (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.pc : mcu.getPC();

            if (stepCount > 0 && breakpoints.find(currentPc) != breakpoints.end()) {
                std::cout << "Hit breakpoint at " << toHex(currentPc) << "!\n";
                break;
            }

            if (currentSimMode == SimMode::PIPELINE_4STAGE) {
                if (p4.halted || p4.isDrained()) break;
                p4.stepCycle();
            } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
                if (p6.halted || p6.isDrained()) break;
                p6.stepCycle();
            } else {
                bool done = mcu.stepMicroCycle();
                if (mcu.pc >= p4.memory.getSize() || (mcu.ir == 0 && done)) break;
            }
            stepCount++;
        }
        std::cout << "Execution stopped after " << stepCount << " cycles.\n";
        showRegisters();
    }

    uint64_t getCycles() const {
        return (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.cycles : p4.cycles;
    }
    uint64_t getRetiredInstructions() const {
        return (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.retiredInstructions : p4.retiredInstructions;
    }
    bool isHalted() const {
        return (currentSimMode == SimMode::PIPELINE_6STAGE) ? (p6.halted || p6.isDrained()) : (p4.halted || p4.isDrained());
    }

    void showRegisters() const {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            std::cout << p4.regFile.dumpState();
            std::cout << "PC : " << toHex(p4.pc) << "\n";
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << p6.regFile.dumpState();
            std::cout << "PC : " << toHex(p6.pc) << "\n";
        } else {
            std::cout << p4.regFile.dumpState();
            std::cout << "PC : " << toHex(mcu.getPC()) << "\n";
        }
    }

    void showPipeline() const {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            std::cout << visualizer::PipelineVisualizer::renderStages(p4.getStageInfo(), p4.cycles);
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::PipelineVisualizer::renderStages(p6.getStageInfo(), p6.cycles);
        } else {
            showMicroState();
        }
    }

    void showStack() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::StackVisualizer::render(p6.memory, p6.regFile);
        } else {
            std::cout << visualizer::StackVisualizer::render(p4.memory, p4.regFile);
        }
    }

    void showMicroState() const {
        std::cout << "\n+--------------------------------------------------------------------------+\n"
                  << "|                MICROPROGRAMMED CONTROL UNIT STEP STATE                   |\n"
                  << "+--------------------------------------------------------------------------+\n"
                  << "  MicroPC (uPC)   : " << mcu.upc << "\n"
                  << "  Control Mode    : " << (mcu.mode == microcode::MicroControlMode::HORIZONTAL ? "HORIZONTAL (Direct 65-bit)" : "VERTICAL (Decoded 45-bit)") << "\n";

        if (mcu.upc < mcu.controlStoreHorizontal.size()) {
            const auto& hw = mcu.controlStoreHorizontal[mcu.upc];
            std::cout << "  Micro-Assembly  : " << hw.disassembly << "\n"
                      << "  Control Signals : 0x" << std::hex << hw.controlSignals << std::dec << "\n"
                      << "  Xfer Mux Select : " << static_cast<int>(hw.getXferMux())
                      << " (0=writeBus, 1=uimm, 2=uadder)\n"
                      << "  uFetch Mux      : " << static_cast<int>(hw.getUfetchMux())
                      << " (0=next_upc, 1=branchTgt, 2=mswitch, 3=cond)\n"
                      << "  uImm            : " << hw.uimm << "\n"
                      << "  uBranchTarget   : " << hw.ubranchTarget << "\n"
                      << "  Active Args     : " << functionalArgName(hw.args) << "\n";
        }
        std::cout << "  DataPath Registers:\n"
                  << "    pc=" << toHex(mcu.pc) << " ir=" << toHex(mcu.ir)
                  << " immx=" << toHex(mcu.immx) << " branchTgt=" << toHex(mcu.branchTarget) << "\n"
                  << "    A=" << toHex(mcu.A) << " B=" << toHex(mcu.B)
                  << " aluResult=" << toHex(mcu.aluResult) << " ldResult=" << toHex(mcu.ldResult) << "\n"
                  << "    regSrc=" << static_cast<int>(mcu.regSrc)
                  << " regData=" << toHex(mcu.regData)
                  << " regVal=" << toHex(mcu.regVal)
                  << " mar=" << toHex(mcu.mar) << " mdr=" << toHex(mcu.mdr) << "\n"
                  << "+--------------------------------------------------------------------------+\n";
    }

    void showStats() const {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            std::cout << p4.printStats();
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << p6.printStats();
        } else {
            std::cout << "=== Microcoded Controller Statistics ===\n"
                      << "Total Micro Cycles Executed : " << mcu.totalMicroCycles << "\n"
                      << "Current PC                  : " << toHex(mcu.pc) << "\n"
                      << "MicroControl Mode           : "
                      << (mcu.mode == microcode::MicroControlMode::HORIZONTAL ? "HORIZONTAL" : "VERTICAL") << "\n";
        }
    }

    void dumpMemory(Word start, Word count) const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << p6.memory.dump(start, count);
        } else {
            std::cout << p4.memory.dump(start, count);
        }
    }

    void disassembleMemory(Word start, Word count) const {
        const Memory& m = (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.memory : p4.memory;
        std::cout << "=== Disassembly at " << toHex(start) << " ===\n";
        for (Word i = 0; i < count; ++i) {
            Word a = start + i * 4;
            Word w = m.readWord(a, nullptr);
            std::cout << toHex(a) << ": " << toHex(w) << "  "
                      << assembler::Disassembler::disassemble(w, a) << "\n";
        }
    }

    void setSimMode(SimMode mode) {
        currentSimMode = mode;
        std::cout << "Switched simulation mode to: "
                  << (mode == SimMode::PIPELINE_4STAGE ? "4-STAGE PIPELINE (Merged MA_RW)" :
                      mode == SimMode::PIPELINE_6STAGE ? "6-STAGE PIPELINE (Split ID/RF)" :
                      "MICROPROGRAMMED CONTROLLER") << "\n";
    }

    void showCache() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderCache(p6.memHierarchy.l1d);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderCache(p4.memHierarchy.l1d);
        }
    }

    void showDRAM() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderDRAM(p6.memHierarchy.dram);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderDRAM(p4.memHierarchy.dram);
        }
    }

    void showTLB() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderTLB(p6.memHierarchy.vm.dtlb);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderTLB(p4.memHierarchy.vm.dtlb);
        }
    }

    void showCPI() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderCPIStack(p6.perfTracker);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderCPIStack(p4.perfTracker);
        }
    }

    void showCPIStack(bool steadyState = false) const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderCPIStack(p6.perfTracker, steadyState);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderCPIStack(p4.perfTracker, steadyState);
        }
    }

    void showMemorySummary() const {
        if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            std::cout << visualizer::MemoryVisualizer::renderMemorySummary(p6.memHierarchy);
        } else {
            std::cout << visualizer::MemoryVisualizer::renderMemorySummary(p4.memHierarchy);
        }
    }

    void showCompare() {
        if (loadedProgramSource.empty()) {
            std::cout << "No program loaded to compare. Load a program or assembly first.\n";
            return;
        }

        auto words = asmEngine.getMachineWords();
        Word progSize = static_cast<Word>(words.size() * 4);

        // Run 4-stage pipeline
        stage4::Pipeline4Stage testP4(1024 * 1024);
        testP4.setMemPreset("ideal");
        testP4.setForwarding(true);
        testP4.setProgramSize(progSize);
        testP4.memory.loadProgram(0, words);
        testP4.run(500000);

        // Run 6-stage pipeline
        stage6::Pipeline6Stage testP6(1024 * 1024);
        testP6.setMemPreset("ideal");
        testP6.setForwarding(true);
        testP6.setProgramSize(progSize);
        testP6.memory.loadProgram(0, words);
        testP6.run(500000);

        double p4ClockGhz = 0.5; // 500 MHz
        double p6ClockGhz = 0.8; // 800 MHz
        double p4TimeUs = (testP4.cycles / (p4ClockGhz * 1000.0));
        double p6TimeUs = (testP6.cycles / (p6ClockGhz * 1000.0));
        double speedup = (p6TimeUs > 0) ? (p4TimeUs / p6TimeUs) : 1.0;

        std::cout << "\n========================================================================\n"
                  << "             4-STAGE VS 6-STAGE PIPELINE COMPARISON VIEW                \n"
                  << "========================================================================\n"
                  << std::left << std::setw(30) << "Metric" 
                  << std::setw(24) << "4-Stage Pipeline" 
                  << "6-Stage Pipeline\n"
                  << "------------------------------------------------------------------------\n"
                  << std::left << std::setw(30) << "Stage Partitioning" 
                  << std::setw(24) << "IF-OF-EX-MA/RW" 
                  << "IF-ID-RF-EX-MA-RW\n"
                  << std::left << std::setw(30) << "Clock Frequency" 
                  << std::setw(24) << "500 MHz (2.0 ns)" 
                  << "800 MHz (1.25 ns)\n"
                  << std::left << std::setw(30) << "Retired Instructions" 
                  << std::setw(24) << testP4.retiredInstructions 
                  << testP6.retiredInstructions << "\n"
                  << std::left << std::setw(30) << "Total Clock Cycles" 
                  << std::setw(24) << testP4.cycles 
                  << testP6.cycles << "\n"
                  << std::left << std::setw(30) << "Cycles Per Inst (CPI)" 
                  << std::setw(24) << std::fixed << std::setprecision(3) << testP4.perfTracker.getCPI() 
                  << std::fixed << std::setprecision(3) << testP6.perfTracker.getCPI() << "\n"
                  << std::left << std::setw(30) << "Inst Per Cycle (IPC)" 
                  << std::setw(24) << std::fixed << std::setprecision(3) << testP4.perfTracker.getIPC() 
                  << std::fixed << std::setprecision(3) << testP6.perfTracker.getIPC() << "\n"
                  << std::left << std::setw(30) << "RAW Stall Cycles" 
                  << std::setw(24) << testP4.stallCycles 
                  << testP6.stallCycles << "\n"
                  << std::left << std::setw(30) << "Branch Flushes / Bubbles" 
                  << std::setw(24) << testP4.bubbleCycles 
                  << testP6.bubbleCycles << "\n"
                  << std::left << std::setw(30) << "Wall-Clock Time" 
                  << std::setw(24) << (std::to_string(p4TimeUs).substr(0, 6) + " us") 
                  << (std::to_string(p6TimeUs).substr(0, 6) + " us") << "\n"
                  << "------------------------------------------------------------------------\n"
                  << "Speedup (6-Stage vs 4-Stage) : " << std::fixed << std::setprecision(2) << speedup << "x "
                  << (speedup >= 1.0 ? "(6-Stage is faster due to higher clock frequency)" : "(4-Stage is faster on this code)") << "\n"
                  << "========================================================================\n\n";
    }

    void explainStall(uint64_t targetCycle = 0) const {
        const auto& history = (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.cycleHistory : p4.cycleHistory;
        if (history.empty()) {
            std::cout << "No pipeline execution history available. Run the program (e.g. 'r') first.\n";
            return;
        }

        if (targetCycle == 0 || targetCycle > history.size()) {
            targetCycle = history.size();
        }

        const auto& snap = history[targetCycle - 1];
        std::cout << "\n========================================================================\n"
                  << "                STALL & HAZARD EXPLANATION FOR CYCLE " << std::setw(5) << targetCycle << "          \n"
                  << "========================================================================\n";

        bool foundEvent = false;
        for (const auto& st : snap.stages) {
            if (st.isStalled) {
                foundEvent = true;
                std::cout << "[STALL IN STAGE " << st.stageName << "]:\n"
                          << "  Instruction  : " << st.instructionText << " (PC: " << toHex(st.pc) << ")\n"
                          << "  Hazard Reason: " << (st.stallReason.empty() ? "Interlock Stall" : st.stallReason) << "\n";
                if (st.stallReason == "RAW_LOAD_USE") {
                    std::cout << "  Root Cause   : An instruction requires data from a preceding load instruction\n"
                              << "                 that is currently accessing memory. A 1-cycle bubble was injected.\n";
                } else if (st.stallReason == "RAW_NOFWD") {
                    std::cout << "  Root Cause   : Data forwarding is disabled. Pipeline interlock stalled until\n"
                              << "                 the producing instruction cleared the execution stage.\n";
                } else {
                    std::cout << "  Root Cause   : Stage could not advance due to structural or data hazards.\n";
                }
            } else if (st.isBubble && !st.stallReason.empty()) {
                foundEvent = true;
                std::cout << "[BUBBLE INSERTED IN " << st.stageName << "]:\n"
                          << "  Bubble Reason: " << st.stallReason << "\n";
                if (st.stallReason == "BRANCH_MISPREDICT") {
                    std::cout << "  Root Cause   : Dynamic branch predictor mispredicted branch direction/target.\n"
                              << "                 Wrong-path instructions flushed and target refetched.\n";
                } else if (st.stallReason == "RAW_LOAD_USE") {
                    std::cout << "  Root Cause   : Bubble inserted to delay consumer instruction by 1 cycle.\n";
                }
            }
            if (!st.forwardingInfo.empty()) {
                std::cout << "  Forwarding   : " << st.forwardingInfo << "\n";
            }
        }

        if (!foundEvent) {
            std::cout << "No stalls or hazard bubbles occurred in cycle " << targetCycle << ".\n"
                      << "All active pipeline stages advanced smoothly without latency penalty.\n";
        }
        std::cout << "========================================================================\n";
    }

    void showHazards() const {
        const auto& hz = (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.hazardUnit : p4.hazardUnit;
        std::cout << "=== Hazard Unit Telemetry ===\n"
                  << "RAW Hazards Detected: " << hz.rawHazardsDetected << "\n"
                  << "Load-Use Stalls     : " << hz.loadUseStalls << "\n"
                  << "RAW No-Fwd Stalls   : " << hz.rawNoFwdStalls << "\n"
                  << "Structural Stalls   : " << hz.structuralMemStalls << "\n"
                  << "Branch Flushes      : " << hz.branchFlushCount << "\n";
    }

    void showForwarding() const {
        const auto& fw = (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.fwdUnit : p4.fwdUnit;
        std::cout << "=== Forwarding Unit Telemetry ===\n"
                  << "Forwarding Mode     : " << forwardingModeToString(fw.mode) << "\n"
                  << "EX-to-EX Forwards   : " << fw.exToExForwards << "\n"
                  << "MEM-to-EX Forwards  : " << fw.memToExForwards << "\n"
                  << "WB-to-EX Forwards   : " << fw.wbToExForwards << "\n"
                  << "Store Data Forwards : " << fw.storeDataForwards << "\n"
                  << "StoreBuffer Hits    : " << fw.storeBuffer.forwardHits << "\n";
    }

    void showAMAT() const {
        const auto& memH = (currentSimMode == SimMode::PIPELINE_6STAGE) ? p6.memHierarchy : p4.memHierarchy;
        std::cout << "=== Memory Hierarchy Latency Analysis ===\n"
                  << "AMAT (Average Memory Access Time) : " << std::fixed << std::setprecision(2) << memH.calculateAMAT() << " cycles\n"
                  << "EAT  (Effective Access Time + VM) : " << std::fixed << std::setprecision(2) << memH.calculateEAT() << " cycles\n";
    }

    void reloadProgram() {
        if (!loadedProgramSource.empty()) {
            loadAssemblyString(loadedProgramSource);
        }
    }

    void showTrace() {
        if (currentSimMode == SimMode::PIPELINE_4STAGE) {
            if (p4.cycles == 0) {
                std::cout << "Program has not run yet. Executing 4-stage pipeline...\n";
                run();
            }
            std::cout << visualizer::PipelineVisualizer::renderTrace(p4.cycleHistory, "4-Stage");
        } else if (currentSimMode == SimMode::PIPELINE_6STAGE) {
            if (p6.cycles == 0) {
                std::cout << "Program has not run yet. Executing 6-stage pipeline...\n";
                run();
            }
            std::cout << visualizer::PipelineVisualizer::renderTrace(p6.cycleHistory, "6-Stage");
        } else {
            showMicroState();
        }
    }

    bool promptStartupAssembly() {
        std::cout << "\n========================================================================\n"
                  << "                      RISC201 PROCESSOR SIMULATOR                       \n"
                  << "========================================================================\n"
                  << "Enter your SimpleRisc assembly code below.\n"
                  << "(Type 'run', 'end', or press Enter on an empty line when finished):\n"
                  << "(Press Enter directly without typing code to load default sample program)\n\n";

        std::string fullCode = "";
        std::string inputLine;
        int lineNum = 1;
        while (true) {
            std::cout << "  " << lineNum << "> ";
            if (!std::getline(std::cin, inputLine)) break;
            std::string trimmed = assembler::MacroPreprocessor::trim(inputLine);
            if (trimmed == "run" || trimmed == "end" || trimmed == "." || trimmed == "done" || (lineNum == 1 && trimmed.empty()) || (lineNum > 1 && trimmed.empty())) {
                break;
            }
            fullCode += trimmed + "\n";
            lineNum++;
        }

        if (fullCode.empty()) {
            std::cout << "\nNo custom code entered. Loading default sample program (recursive factorial)...\n";
            return false;
        }

        std::cout << "\nAssembling and loading custom assembly code...\n";
        if (loadAssemblyString(fullCode)) {
            std::cout << "\nRunning pipeline for your custom code...\n";
            run();
            showTrace();
            return true;
        }
        return false;
    }

    void printHelp() const {
        std::cout << "\n================= RISC201 DEBUGGER COMMANDS =================\n"
                  << "  r, run                  : Run program to completion (or re-run from start)\n"
                  << "  c, continue             : Continue running from current state\n"
                  << "  trace                   : Display complete cycle-by-cycle pipeline trace & timing diagram\n"
                  << "  asm, code               : Enter custom assembly code interactively to run pipeline\n"
                  << "  regs                    : Display register file (r0-r15, flags, PC)\n"
                  << "  p, pipeline             : Display current pipeline stage latches & hazards\n"
                  << "  hazards                 : Display hazard detection telemetry\n"
                  << "  fwd                     : Display data forwarding bypass telemetry\n"
                  << "  cpi                     : Display ASCII CPI stack breakdown chart\n"
                  << "  cpistack [steady|cold]  : Render CPI stack with steady-state vs cold-start breakdown\n"
                  << "  compare                 : Compare 4-Stage vs 6-Stage CPI and wall-clock execution time\n"
                  << "  explain [cycle]         : Explain stall cause, hazard type, and bubble counts for a cycle\n"
                  << "  mem, memory             : Render cache/TLB summary, MSHR occupancy, and memory access trace\n"
                  << "  cache                   : Render cache sets/ways visualizer\n"
                  << "  dram                    : Render DRAM banks and row buffer status\n"
                  << "  tlb                     : Render TLB translation entries\n"
                  << "  amat                    : Compute AMAT and EAT latencies\n"
                  << "  opcode                  : Display SimpleRisc ISA table\n"
                  << "  preset <name>           : Switch memory preset (ideal, simple, modern, etc.)\n"
                  << "  bp_policy <not_taken|1bit|2bit>: Select branch predictor (always not-taken, 1-bit, 2-bit)\n"
                  << "  stack                   : Render ASCII active stack memory visualizer\n"
                  << "  micro                   : Display microPC state & control bus signals\n"
                  << "  mode <4|6|micro>        : Switch core: 4-stage, 6-stage, or microcode\n"
                  << "  micro_mode <horiz|vert> : Switch horizontal vs vertical microcode\n"
                  << "  forwarding <on|off>     : Enable/disable pipeline data forwarding\n"
                  << "  adder <rca|csla|cla>    : Select ALU adder algorithm\n"
                  << "  mul <iter|booth|wallace>: Select ALU multiplier algorithm\n"
                  << "  div <rest|nonrest|newton>: Select ALU divider algorithm (Newton-Raphson, Restoring, Non-restoring)\n"
                  << "  set_n <n>, input <n>    : Set custom input number for r0 (e.g. set_n 7)\n"
                  << "  stats                   : Display performance & hazard counters\n"
                  << "  help                    : Show this command list\n"
                  << "  q, quit                 : Exit debugger\n"
                  << "=============================================================\n\n";
    }

    void repl() {
        printHelp();
        std::string line;
        while (true) {
            std::cout << "RISC201["
              << (currentSimMode == SimMode::PIPELINE_4STAGE ? "4-Stage" :
                  currentSimMode == SimMode::PIPELINE_6STAGE ? "6-Stage" : "Micro")
              << "]> ";
            if (!std::getline(std::cin, line)) break;
            line = assembler::MacroPreprocessor::trim(line);
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::string cmd;
            ss >> cmd;
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if (cmd == "q" || cmd == "quit" || cmd == "exit") {
                break;
            } else if (cmd == "help" || cmd == "h") {
                printHelp();
            } else if (cmd == "s" || cmd == "step") {
                stepCycle();
            } else if (cmd == "si" || cmd == "step_inst") {
                stepInstruction();
            } else if (cmd == "r" || cmd == "run") {
                if ((currentSimMode == SimMode::PIPELINE_4STAGE && (p4.halted || p4.isDrained())) ||
                    (currentSimMode == SimMode::PIPELINE_6STAGE && (p6.halted || p6.isDrained()))) {
                    reloadProgram();
                }
                run();
            } else if (cmd == "c" || cmd == "continue") {
                run();
            } else if (cmd == "trace") {
                showTrace();
            } else if (cmd == "regs") {
                showRegisters();
            } else if (cmd == "p" || cmd == "pipeline") {
                showPipeline();
            } else if (cmd == "stack") {
                showStack();
            } else if (cmd == "micro") {
                showMicroState();
            } else if (cmd == "stats") {
                showStats();
            } else if (cmd == "hazards") {
                showHazards();
            } else if (cmd == "fwd") {
                showForwarding();
            } else if (cmd == "cpi") {
                showCPI();
            } else if (cmd == "cpistack") {
                std::string mode;
                ss >> mode;
                bool steady = (mode == "steady" || mode == "warm" || mode == "steady-state");
                showCPIStack(steady);
            } else if (cmd == "compare") {
                showCompare();
            } else if (cmd == "explain") {
                uint64_t targetCycle = 0;
                ss >> targetCycle;
                explainStall(targetCycle);
            } else if (cmd == "mem" || cmd == "memory") {
                showMemorySummary();
            } else if (cmd == "cache") {
                showCache();
            } else if (cmd == "dram") {
                showDRAM();
            } else if (cmd == "tlb") {
                showTLB();
            } else if (cmd == "amat") {
                showAMAT();
            } else if (cmd == "opcode") {
                std::cout << printIsaTable();
            } else if (cmd == "asm" || cmd == "code") {
                std::string remainder;
                std::getline(ss, remainder);
                remainder = assembler::MacroPreprocessor::trim(remainder);
                std::string fullCode = "";
                if (!remainder.empty()) {
                    for (char ch : remainder) {
                        if (ch == ';') fullCode += "\n";
                        else fullCode += ch;
                    }
                } else {
                    std::cout << "\n=== Custom Assembly Pipeline Execution ===\n"
                              << "Enter your SimpleRisc assembly instructions.\n"
                              << "Type 'run', 'end', or 'done' on a new line when finished:\n";
                    std::string inputLine;
                    int lineNum = 1;
                    while (true) {
                        std::cout << "  " << lineNum << "> ";
                        if (!std::getline(std::cin, inputLine)) break;
                        std::string trimmed = assembler::MacroPreprocessor::trim(inputLine);
                        if (trimmed == "run" || trimmed == "end" || trimmed == "." || trimmed == "done") {
                            break;
                        }
                        if (!trimmed.empty()) {
                            fullCode += trimmed + "\n";
                            lineNum++;
                        }
                    }
                }
                if (fullCode.empty()) {
                    std::cout << "No assembly instructions entered.\n";
                } else {
                    std::cout << "\nAssembling and loading custom assembly code...\n";
                    if (loadAssemblyString(fullCode)) {
                        std::cout << "\nRunning pipeline for your custom code...\n";
                        run();
                        showTrace();
                    }
                }
            } else if (cmd == "preset") {
                std::string pr;
                ss >> pr;
                p4.setMemPreset(pr);
                p6.setMemPreset(pr);
                std::cout << "Configured memory preset: " << pr << "\n";
            } else if (cmd == "bp_policy" || cmd == "branch") {
                std::string bp;
                if (ss >> bp) {
                    p4.setBranchPolicy(bp);
                    p6.setBranchPolicy(bp);
                    std::string label = "2-Bit Saturating Counter Predictor";
                    if (bp == "not_taken" || bp == "not-taken" || bp == "flush" || bp == "always-not-taken" || bp == "nottaken") {
                        label = "Always Not-Taken (Flush)";
                    } else if (bp == "1bit" || bp == "bimodal-1bit") {
                        label = "1-Bit Bimodal Predictor";
                    }
                    std::cout << "Branch predictor set to: " << label << " (" << bp << ")\n";
                } else {
                    std::cout << "\n=== Branch Predictor Options (Choose 1 of 3) ===\n"
                              << "  1. not_taken  : Always Not-Taken (Flush)\n"
                              << "  2. 1bit       : 1-Bit Bimodal Counter Table\n"
                              << "  3. 2bit       : 2-Bit Saturating Counter Table\n"
                              << "Usage: bp_policy <not_taken | 1bit | 2bit>\n";
                }
            } else if (cmd == "b" || cmd == "break") {
                std::string target;
                ss >> target;
                Word addr = 0;
                if (asmEngine.symbolTable.find(target) != asmEngine.symbolTable.end()) {
                    addr = asmEngine.symbolTable[target];
                } else {
                    addr = assembler::Disassembler::parseHexString(target);
                }
                setBreakpoint(addr);
            } else if (cmd == "db" || cmd == "delbreak") {
                std::string target;
                ss >> target;
                Word addr = assembler::Disassembler::parseHexString(target);
                clearBreakpoint(addr);
            } else if (cmd == "mode") {
                std::string m;
                ss >> m;
                if (m == "4") setSimMode(SimMode::PIPELINE_4STAGE);
                else if (m == "6") setSimMode(SimMode::PIPELINE_6STAGE);
                else if (m == "micro") setSimMode(SimMode::MICROCODED);
                else std::cout << "Usage: mode <4|6|micro>\n";
            } else if (cmd == "micro_mode") {
                std::string mm;
                ss >> mm;
                if (mm == "horiz" || mm == "horizontal") {
                    mcu.setMode(microcode::MicroControlMode::HORIZONTAL);
                    std::cout << "Microcode mode set to HORIZONTAL (65-bit direct control word)\n";
                } else if (mm == "vert" || mm == "vertical") {
                    mcu.setMode(microcode::MicroControlMode::VERTICAL);
                    std::cout << "Microcode mode set to VERTICAL (45-bit with micro-decoder)\n";
                } else {
                    std::cout << "Usage: micro_mode <horiz|vert>\n";
                }
            } else if (cmd == "forwarding") {
                std::string arg;
                ss >> arg;
                bool enable = (arg == "on" || arg == "1" || arg == "true");
                p4.setForwarding(enable);
                p6.setForwarding(enable);
                std::cout << "Pipeline data forwarding: " << (enable ? "ENABLED" : "DISABLED") << "\n";
            } else if (cmd == "adder") {
                std::string algo;
                ss >> algo;
                alu::AluConfig cfg = p4.aluUnit.getConfig();
                if (algo == "rca") cfg.adderAlgo = alu::AdderAlgorithm::RIPPLE_CARRY;
                else if (algo == "csla") cfg.adderAlgo = alu::AdderAlgorithm::CARRY_SELECT;
                else if (algo == "cla") cfg.adderAlgo = alu::AdderAlgorithm::CARRY_LOOKAHEAD;
                else { std::cout << "Usage: adder <rca|csla|cla>\n"; continue; }
                p4.aluUnit.setConfig(cfg);
                p6.aluUnit.setConfig(cfg);
                std::cout << "Adder algorithm set to: " << algo << "\n";
            } else if (cmd == "mul") {
                std::string algo;
                ss >> algo;
                alu::AluConfig cfg = p4.aluUnit.getConfig();
                if (algo == "iter") cfg.mulAlgo = alu::MultiplierAlgorithm::ITERATIVE;
                else if (algo == "booth") cfg.mulAlgo = alu::MultiplierAlgorithm::BOOTH;
                else if (algo == "wallace") cfg.mulAlgo = alu::MultiplierAlgorithm::WALLACE_TREE;
                else { std::cout << "Usage: mul <iter|booth|wallace>\n"; continue; }
                p4.aluUnit.setConfig(cfg);
                p6.aluUnit.setConfig(cfg);
                std::cout << "Multiplier algorithm set to: " << algo << "\n";
            } else if (cmd == "div") {
                std::string algo;
                if (ss >> algo) {
                    alu::AluConfig cfg = p4.aluUnit.getConfig();
                    if (algo == "rest") cfg.divAlgo = alu::DividerAlgorithm::RESTORING;
                    else if (algo == "nonrest") cfg.divAlgo = alu::DividerAlgorithm::NON_RESTORING;
                    else if (algo == "newton" || algo == "nr" || algo == "newton_raphson" || algo == "newton-raphson") cfg.divAlgo = alu::DividerAlgorithm::NEWTON_RAPHSON;
                    else {
                        std::cout << "\n=== Division Algorithm Options (Choose 1 of 3) ===\n"
                                  << "  1. rest    : Restoring Division\n"
                                  << "  2. nonrest : Non-Restoring Division\n"
                                  << "  3. newton  : Newton-Raphson Iterative Division\n"
                                  << "Usage: div <rest | nonrest | newton>\n";
                        continue;
                    }
                    p4.aluUnit.setConfig(cfg);
                    p6.aluUnit.setConfig(cfg);
                    std::cout << "Divider algorithm set to: "
                              << (cfg.divAlgo == alu::DividerAlgorithm::RESTORING ? "Restoring" :
                                 (cfg.divAlgo == alu::DividerAlgorithm::NON_RESTORING ? "Non-Restoring" : "Newton-Raphson")) << " (" << algo << ")\n";
                } else {
                    std::cout << "\n=== Division Algorithm Options (Choose 1 of 3) ===\n"
                              << "  1. rest    : Restoring Division\n"
                              << "  2. nonrest : Non-Restoring Division\n"
                              << "  3. newton  : Newton-Raphson Iterative Division\n"
                              << "Usage: div <rest | nonrest | newton>\n";
                }
            } else if (cmd == "mem") {
                std::string startStr;
                Word count = 8;
                ss >> startStr >> count;
                Word start = assembler::Disassembler::parseHexString(startStr);
                dumpMemory(start, count);
            } else if (cmd == "disasm") {
                std::string startStr = "0x0";
                Word count = 16;
                ss >> startStr >> count;
                Word start = assembler::Disassembler::parseHexString(startStr);
                disassembleMemory(start, count);
            } else if (cmd == "set_n" || cmd == "input" || cmd == "n") {
                std::string nStr;
                if (ss >> nStr) {
                    try {
                        Word nVal = std::stoul(nStr);
                        if (setCustomInput(nVal)) {
                            std::cout << "Successfully updated input: n = " << nVal << ". Type 'r' to execute.\n";
                        }
                    } catch (...) {
                        std::cout << "Invalid number: " << nStr << "\n";
                    }
                } else {
                    std::cout << "Usage: set_n <number> (e.g. set_n 7)\n";
                }
            } else {
                std::cout << "Unknown command: '" << cmd << "'. Type 'help' for instructions.\n";
            }
        }
    }
};

} // namespace debugger
} // namespace risc201

#endif // RISC201_DEBUGGER_HPP
