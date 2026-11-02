#include "Debugger.hpp"
#include "../common/ISA.hpp"
#include "../visualizer/MemoryVisualizer.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace risc201;

void printUsage() {
    std::cout << "Usage: risc201_sim [assembly_file] [options]\n\n"
              << "Options:\n"
              << "  --pipeline <4|6>            Select pipeline depth (default: 4)\n"
              << "  --mem-preset <preset>       Memory hierarchy preset: ideal, simple, modern, modern-ddr5, lecture\n"
              << "  --forwarding <mode>         Forwarding mode: none, basic, full, full+\n"
              << "  --branch-policy <policy>    Branch prediction policy: not_taken, 1bit, 2bit (or stall, flush, etc.)\n"
              << "  --div-algo <algorithm>      Divider algorithm: rest, nonrest, newton\n"
              << "  --code, -c <assembly>       Inline assembly code string to execute (separate lines with ';')\n"
              << "  --diagram                   Render pipeline ASCII timing diagram\n"
              << "  --trace                     Display complete cycle-by-cycle pipeline trace & timing diagram\n"
              << "  --report                    Print detailed performance & stall reason report\n"
              << "  --cpi-breakdown             Render ASCII CPI stack breakdown bar chart\n"
              << "  --check-forwarding          Verify data forwarding invariant correctness\n"
              << "  --isa-table                 Display complete SimpleRisc ISA table and exit\n"
              << "  --export-csv <file>         Export execution metrics to CSV file\n"
              << "  --cycles <n>                Maximum simulation cycles (default: 100000)\n"
              << "  -i, --interactive           Launch interactive REPL debugger after loading\n"
              << "  -h, --help                  Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string asmFile = "";
    std::string inlineCode = "";
    int pipelineDepth = 4;
    std::string memPreset = "ideal";
    std::string forwardingMode = "full";
    std::string branchPolicy = "2bit";
    std::string divAlgo = "nonrest";
    bool showDiagram = false;
    bool showTrace = false;
    bool showReport = false;
    bool showCpiBreakdown = false;
    bool checkForwarding = false;
    bool interactive = (argc == 1);
    std::string exportCsv = "";
    uint64_t maxCycles = 100000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if (arg == "--isa-table") {
            std::cout << printIsaTable();
            return 0;
        } else if (arg == "--pipeline" && i + 1 < argc) {
            pipelineDepth = std::stoi(argv[++i]);
        } else if (arg == "--mem-preset" && i + 1 < argc) {
            memPreset = argv[++i];
        } else if (arg == "--forwarding" && i + 1 < argc) {
            forwardingMode = argv[++i];
        } else if (arg == "--branch-policy" && i + 1 < argc) {
            branchPolicy = argv[++i];
        } else if (arg == "--div-algo" && i + 1 < argc) {
            divAlgo = argv[++i];
        } else if ((arg == "--code" || arg == "-c") && i + 1 < argc) {
            inlineCode = argv[++i];
        } else if (arg == "--diagram") {
            showDiagram = true;
        } else if (arg == "--trace") {
            showTrace = true;
        } else if (arg == "--report") {
            showReport = true;
        } else if (arg == "--cpi-breakdown") {
            showCpiBreakdown = true;
        } else if (arg == "--check-forwarding") {
            checkForwarding = true;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        } else if (arg == "--export-csv" && i + 1 < argc) {
            exportCsv = argv[++i];
        } else if (arg == "--cycles" && i + 1 < argc) {
            maxCycles = std::stoull(argv[++i]);
        } else if (arg[0] != '-') {
            asmFile = arg;
        }
    }

    debugger::Debugger dbg;

    std::string sampleProgram = R"(
.text
    mov r0, 5
    call .factorial
    mov r2, r1
    b .done

.factorial:
    cmp r0, 1
    beq .base_case
    bgt .recurse
    mov r1, 1
    ret

.base_case:
    mov r1, 1
    ret

.recurse:
    push r0
    push ra
    sub r0, r0, 1
    call .factorial
    pop ra
    pop r0
    mul r1, r0, r1
    ret

.done:
    nop
)";

    std::string programToLoad = sampleProgram;
    if (!inlineCode.empty()) {
        std::string formatted = "";
        for (char ch : inlineCode) {
            if (ch == ';') formatted += "\n";
            else formatted += ch;
        }
        programToLoad = formatted;
        std::cout << "Loaded custom inline assembly code.\n";
    } else if (!asmFile.empty()) {
        std::ifstream file(asmFile);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            programToLoad = buffer.str();
            std::cout << "Loaded assembly file: " << asmFile << "\n";
        } else {
            std::cerr << "Could not open file: " << asmFile << ", using default sample program.\n";
        }
    } else if (interactive) {
        // Startup interface proactively asks for assembly code input!
        if (pipelineDepth == 6) {
            dbg.setSimMode(debugger::SimMode::PIPELINE_6STAGE);
            dbg.p6.setMemPreset(memPreset);
            dbg.p6.setForwardingMode(forwardingMode);
            dbg.p6.setBranchPolicy(branchPolicy);
        } else {
            dbg.setSimMode(debugger::SimMode::PIPELINE_4STAGE);
            dbg.p4.setMemPreset(memPreset);
            dbg.p4.setForwardingMode(forwardingMode);
            dbg.p4.setBranchPolicy(branchPolicy);
        }

        alu::AluConfig aluCfg = dbg.p4.aluUnit.getConfig();
        if (divAlgo == "rest") aluCfg.divAlgo = alu::DividerAlgorithm::RESTORING;
        else if (divAlgo == "newton" || divAlgo == "nr" || divAlgo == "newton_raphson") aluCfg.divAlgo = alu::DividerAlgorithm::NEWTON_RAPHSON;
        else aluCfg.divAlgo = alu::DividerAlgorithm::NON_RESTORING;
        dbg.p4.aluUnit.setConfig(aluCfg);
        dbg.p6.aluUnit.setConfig(aluCfg);

        std::cout << "=== Configuration: " << pipelineDepth << "-Stage Pipeline | MemPreset: "
                  << memPreset << " | Forwarding: " << forwardingMode
                  << " | BranchPolicy: " << branchPolicy
                  << " | Divider: " << (aluCfg.divAlgo == alu::DividerAlgorithm::RESTORING ? "restoring" :
                                       (aluCfg.divAlgo == alu::DividerAlgorithm::NON_RESTORING ? "non-restoring" : "newton-raphson"))
                  << " ===\n";

        bool userProvided = dbg.promptStartupAssembly();
        if (!userProvided) {
            dbg.loadAssemblyString(sampleProgram);
        }
        dbg.repl();
        return 0;
    } else {
        std::cout << "No program file passed. Loaded default recursive factorial demonstration program.\n";
    }

    if (!dbg.loadAssemblyString(programToLoad)) {
        return 1;
    }

    // Configure pipeline depth
    if (pipelineDepth == 6) {
        dbg.setSimMode(debugger::SimMode::PIPELINE_6STAGE);
        dbg.p6.setMemPreset(memPreset);
        dbg.p6.setForwardingMode(forwardingMode);
        dbg.p6.setBranchPolicy(branchPolicy);
    } else {
        dbg.setSimMode(debugger::SimMode::PIPELINE_4STAGE);
        dbg.p4.setMemPreset(memPreset);
        dbg.p4.setForwardingMode(forwardingMode);
        dbg.p4.setBranchPolicy(branchPolicy);
    }

    alu::AluConfig aluCfg = dbg.p4.aluUnit.getConfig();
    if (divAlgo == "rest") aluCfg.divAlgo = alu::DividerAlgorithm::RESTORING;
    else if (divAlgo == "newton" || divAlgo == "nr" || divAlgo == "newton_raphson") aluCfg.divAlgo = alu::DividerAlgorithm::NEWTON_RAPHSON;
    else aluCfg.divAlgo = alu::DividerAlgorithm::NON_RESTORING;
    dbg.p4.aluUnit.setConfig(aluCfg);
    dbg.p6.aluUnit.setConfig(aluCfg);

    std::cout << "=== Configuration: " << pipelineDepth << "-Stage Pipeline | MemPreset: "
              << memPreset << " | Forwarding: " << forwardingMode
              << " | BranchPolicy: " << branchPolicy
              << " | Divider: " << (aluCfg.divAlgo == alu::DividerAlgorithm::RESTORING ? "restoring" :
                                   (aluCfg.divAlgo == alu::DividerAlgorithm::NON_RESTORING ? "non-restoring" : "newton-raphson"))
              << " ===\n";

    if (interactive) {
        dbg.repl();
    } else {
        dbg.run(maxCycles);

        if (showTrace) {
            dbg.showTrace();
        } else if (showDiagram) {
            dbg.showPipeline();
        }

        if (showReport || showCpiBreakdown) {
            if (pipelineDepth == 6) {
                if (showReport) std::cout << dbg.p6.perfTracker.generateReport("6-STAGE PROCESSOR REPORT");
                if (showCpiBreakdown) std::cout << visualizer::MemoryVisualizer::renderCPIStack(dbg.p6.perfTracker);
            } else {
                if (showReport) std::cout << dbg.p4.perfTracker.generateReport("4-STAGE PROCESSOR REPORT");
                if (showCpiBreakdown) std::cout << visualizer::MemoryVisualizer::renderCPIStack(dbg.p4.perfTracker);
            }
        }

        if (memPreset != "ideal") {
            dbg.showCache();
            dbg.showDRAM();
            dbg.showAMAT();
        }

        if (checkForwarding) {
            dbg.showForwarding();
            dbg.showHazards();
        }

        if (!exportCsv.empty()) {
            std::ofstream out(exportCsv);
            if (out.is_open()) {
                const auto& perf = (pipelineDepth == 6) ? dbg.p6.perfTracker : dbg.p4.perfTracker;
                out << "Benchmark,Pipeline,MemPreset,Forwarding,Cycles,RetiredInsts,CPI,IPC,Stalls\n";
                out << (asmFile.empty() ? "sample" : asmFile) << ","
                    << pipelineDepth << ","
                    << memPreset << ","
                    << forwardingMode << ","
                    << perf.totalCycles << ","
                    << perf.retiredInstructions << ","
                    << perf.getCPI() << ","
                    << perf.getIPC() << ","
                    << perf.getTotalStallCycles() << "\n";
                std::cout << "Exported metrics to CSV: " << exportCsv << "\n";
            }
        }
    }

    return 0;
}