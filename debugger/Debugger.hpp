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
    assembler::Assembler asmEngine;

public:
    Debugger()
        : p4(1024 * 1024), p6(1024 * 1024), mcu(&p4.regFile, &p4.memory, &p4.aluUnit) {}

    bool loadAssemblyString(const std::string& source) {
        loadedProgramSource = source;
        bool ok = asmEngine.assemble(source);
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

        p6.reset();
        p6.setProgramSize(progSize);
        p6.memory.loadProgram(0, words);

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

    void printHelp() const {
        std::cout << "\n================= RISC201 DEBUGGER COMMANDS =================\n"
                  << "  s, step                 : Step one clock cycle / micro-cycle\n"
                  << "  si, step_inst           : Step one complete program instruction\n"
                  << "  r, run                  : Run program to completion or breakpoint\n"
                  << "  c, continue             : Continue running from current state\n"
                  << "  b, break <addr|label>   : Set breakpoint (e.g. b 0x10 or b .loop)\n"
                  << "  db, delbreak <addr>     : Delete breakpoint at address\n"
                  << "  regs                    : Display register file (r0-r15, flags, PC)\n"
                  << "  p, pipeline             : Display pipeline stage latches & hazards\n"
                  << "  stack                   : Render ASCII active stack memory visualizer\n"
                  << "  micro                   : Display microPC state & control bus signals\n"
                  << "  mode <4|6|micro>        : Switch core: 4-stage, 6-stage, or microcode\n"
                  << "  micro_mode <horiz|vert> : Switch horizontal vs vertical microcode\n"
                  << "  forwarding <on|off>     : Enable/disable pipeline data forwarding\n"
                  << "  adder <rca|csla|cla>    : Select ALU adder algorithm\n"
                  << "  mul <iter|booth|wallace>: Select ALU multiplier algorithm\n"
                  << "  div <rest|nonrest>      : Select ALU divider algorithm\n"
                  << "  mem <start_hex> <count> : Dump memory words (e.g. mem 0x0 8)\n"
                  << "  disasm <start> <count>  : Disassemble memory instructions\n"
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
            } else if (cmd == "r" || cmd == "run" || cmd == "c" || cmd == "continue") {
                run();
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
                ss >> algo;
                alu::AluConfig cfg = p4.aluUnit.getConfig();
                if (algo == "rest") cfg.divAlgo = alu::DividerAlgorithm::RESTORING;
                else if (algo == "nonrest") cfg.divAlgo = alu::DividerAlgorithm::NON_RESTORING;
                else { std::cout << "Usage: div <rest|nonrest>\n"; continue; }
                p4.aluUnit.setConfig(cfg);
                p6.aluUnit.setConfig(cfg);
                std::cout << "Divider algorithm set to: " << algo << "\n";
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
