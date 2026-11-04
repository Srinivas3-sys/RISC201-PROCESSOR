#include "common/Types.hpp"
#include "common/RegisterFile.hpp"
#include "common/Memory.hpp"
#include "common/Exception.hpp"
#include "alu/ALU.hpp"
#include "assembler/Assembler.hpp"
#include "processor_4stage/Pipeline4Stage.hpp"
#include "processor_6stage/Pipeline6Stage.hpp"
#include "common/PerformanceTracker.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

using namespace risc201;

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int, char**) {

    std::vector<std::string> benchmarks = {
        "factorial_iter",
        "factorial_rec",
        "prime_test",
        "array_sum",
        "hazard_test",
        "stack_test",
        "factorial_iter_long",
        "factorial_rec_long",
        "prime_test_long",
        "array_sum_long",
        "hazard_test_long",
        "stack_test_long"
    };

    std::vector<std::string> presets = {"ideal", "simple", "modern", "modern-ddr5"};
    std::vector<std::string> fwdModes = {"full", "none"};

    std::ofstream csv("experiments/pipeline/audit_results.csv");
    if (csv.is_open()) {
        csv << "Benchmark,Pipeline,Preset,Forwarding,IC,Cycles,CPI_Cold,CPI_Steady,BaseCycles,RAW_NOFWD,RAW_LOAD_USE,BRANCH_MISPREDICT,BRANCH_STALL,ICACHE_MISS,DCACHE_MISS,TLB_MISS,PAGE_WALK,PAGE_FAULT,Exception\n";
    }

    std::ofstream jsonFile("experiments/pipeline/audit_results.json");
    if (jsonFile.is_open()) {
        jsonFile << "[\n";
    }

    std::cout << "========================================================================\n"
              << "             RISC201 REPEATABLE CPI AUDIT BENCHMARK MATRIX              \n"
              << "========================================================================\n\n";

    std::cout << "| Benchmark | Pipeline | Preset | Fwd | IC | Cycles | CPI(Cold) | CPI(Steady) | I-Miss | D-Miss | BranchBubbles | RAW |\n"
              << "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n";

    bool firstJson = true;

    for (const auto& bm : benchmarks) {
        std::string srcPath = "benchmarks/" + bm + ".s";
        std::string src = readFile(srcPath);
        if (src.empty()) continue;

        assembler::Assembler asmb;
        if (!asmb.assemble(src)) continue;
        auto words = asmb.getMachineWords();

        for (int pipe : {4, 6}) {
            for (const auto& preset : presets) {
                for (const auto& fwd : fwdModes) {
                    uint64_t ic = 0, cyc = 0;
                    double cpiCold = 0, cpiSteady = 0;
                    std::string exStr = "NONE";
                    PerformanceTracker perf(pipe);

                    if (pipe == 4) {
                        stage4::Pipeline4Stage p4;
                        p4.setMemPreset(preset);
                        p4.setForwardingMode(fwd);
                        p4.loadProgram(0, words);

                        uint64_t warmCycles = 0, warmRetired = 0;
                        while (!p4.isDrained() && !p4.halted && p4.retiredInstructions < 1000 && p4.cycles < 50000) {
                            p4.stepCycle();
                        }
                        warmCycles = p4.cycles;
                        warmRetired = p4.retiredInstructions;

                        p4.run(1000000);
                        ic = p4.retiredInstructions;
                        cyc = p4.cycles;
                        cpiCold = (ic > 0) ? (static_cast<double>(cyc) / ic) : 0;
                        if (ic > warmRetired && warmRetired >= 500) {
                            uint64_t steadyCyc = cyc - warmCycles;
                            uint64_t steadyInst = ic - warmRetired;
                            cpiSteady = (steadyInst > 0) ? (static_cast<double>(steadyCyc) / steadyInst) : cpiCold;
                        } else {
                            uint64_t coldPenalty = p4.perfTracker.stallBreakdown["ICACHE_MISS"] + p4.perfTracker.stallBreakdown["PAGE_FAULT"];
                            uint64_t steadyCyc = (cyc > coldPenalty) ? (cyc - coldPenalty) : cyc;
                            cpiSteady = (ic > 0) ? (static_cast<double>(steadyCyc) / ic) : cpiCold;
                        }
                        perf = p4.perfTracker;
                        if (p4.currentException.hasOccurred()) {
                            exStr = exceptionToString(p4.currentException.type);
                        }
                    } else {
                        stage6::Pipeline6Stage p6;
                        p6.setMemPreset(preset);
                        p6.setForwardingMode(fwd);
                        p6.loadProgram(0, words);

                        uint64_t warmCycles = 0, warmRetired = 0;
                        while (!p6.isDrained() && !p6.halted && p6.retiredInstructions < 1000 && p6.cycles < 50000) {
                            p6.stepCycle();
                        }
                        warmCycles = p6.cycles;
                        warmRetired = p6.retiredInstructions;

                        p6.run(1000000);
                        ic = p6.retiredInstructions;
                        cyc = p6.cycles;
                        cpiCold = (ic > 0) ? (static_cast<double>(cyc) / ic) : 0;
                        if (ic > warmRetired && warmRetired >= 500) {
                            uint64_t steadyCyc = cyc - warmCycles;
                            uint64_t steadyInst = ic - warmRetired;
                            cpiSteady = (steadyInst > 0) ? (static_cast<double>(steadyCyc) / steadyInst) : cpiCold;
                        } else {
                            uint64_t coldPenalty = p6.perfTracker.stallBreakdown["ICACHE_MISS"] + p6.perfTracker.stallBreakdown["PAGE_FAULT"];
                            uint64_t steadyCyc = (cyc > coldPenalty) ? (cyc - coldPenalty) : cyc;
                            cpiSteady = (ic > 0) ? (static_cast<double>(steadyCyc) / ic) : cpiCold;
                        }
                        perf = p6.perfTracker;
                        if (p6.currentException.hasOccurred()) {
                            exStr = exceptionToString(p6.currentException.type);
                        }
                    }

                    if (csv.is_open()) {
                        csv << bm << ","
                            << pipe << "-Stage,"
                            << preset << ","
                            << fwd << ","
                            << ic << ","
                            << cyc << ","
                            << std::fixed << std::setprecision(3) << cpiCold << ","
                            << std::fixed << std::setprecision(3) << cpiSteady << ","
                            << ic << ","
                            << perf.stallBreakdown["RAW_NOFWD"] << ","
                            << perf.stallBreakdown["RAW_LOAD_USE"] << ","
                            << perf.stallBreakdown["BRANCH_MISPREDICT"] << ","
                            << perf.stallBreakdown["BRANCH_STALL"] << ","
                            << perf.stallBreakdown["ICACHE_MISS"] << ","
                            << perf.stallBreakdown["DCACHE_MISS"] << ","
                            << perf.stallBreakdown["TLB_MISS"] << ","
                            << perf.stallBreakdown["PAGE_WALK"] << ","
                            << perf.stallBreakdown["PAGE_FAULT"] << ","
                            << exStr << "\n";
                    }

                    if (jsonFile.is_open()) {
                        if (!firstJson) jsonFile << ",\n";
                        firstJson = false;
                        jsonFile << "  {\n"
                                 << "    \"benchmark\": \"" << bm << "\",\n"
                                 << "    \"pipeline\": \"" << pipe << "-Stage\",\n"
                                 << "    \"preset\": \"" << preset << "\",\n"
                                 << "    \"forwarding\": \"" << fwd << "\",\n"
                                 << "    \"instruction_count\": " << ic << ",\n"
                                 << "    \"cycles\": " << cyc << ",\n"
                                 << "    \"cpi_cold\": " << std::fixed << std::setprecision(3) << cpiCold << ",\n"
                                 << "    \"cpi_steady\": " << std::fixed << std::setprecision(3) << cpiSteady << ",\n"
                                 << "    \"branch_bubbles\": " << perf.branchBubbles << ",\n"
                                 << "    \"stalls\": {\n"
                                 << "      \"RAW_NOFWD\": " << perf.stallBreakdown["RAW_NOFWD"] << ",\n"
                                 << "      \"RAW_LOAD_USE\": " << perf.stallBreakdown["RAW_LOAD_USE"] << ",\n"
                                 << "      \"BRANCH_MISPREDICT\": " << perf.stallBreakdown["BRANCH_MISPREDICT"] << ",\n"
                                 << "      \"BRANCH_STALL\": " << perf.stallBreakdown["BRANCH_STALL"] << ",\n"
                                 << "      \"ICACHE_MISS\": " << perf.stallBreakdown["ICACHE_MISS"] << ",\n"
                                 << "      \"DCACHE_MISS\": " << perf.stallBreakdown["DCACHE_MISS"] << ",\n"
                                 << "      \"TLB_MISS\": " << perf.stallBreakdown["TLB_MISS"] << ",\n"
                                 << "      \"PAGE_WALK\": " << perf.stallBreakdown["PAGE_WALK"] << ",\n"
                                 << "      \"PAGE_FAULT\": " << perf.stallBreakdown["PAGE_FAULT"] << "\n"
                                 << "    },\n"
                                 << "    \"exception\": \"" << exStr << "\"\n"
                                 << "  }";
                    }

                    if (fwd == "full" && (preset == "ideal" || preset == "modern")) {
                        std::cout << "| " << std::setw(20) << std::left << bm
                                  << " | " << pipe << "-Stage"
                                  << " | " << std::setw(6) << preset
                                  << " | " << std::setw(4) << fwd
                                  << " | " << std::setw(7) << std::right << ic
                                  << " | " << std::setw(7) << cyc
                                  << " | " << std::setw(9) << std::fixed << std::setprecision(3) << cpiCold
                                  << " | " << std::setw(11) << std::fixed << std::setprecision(3) << cpiSteady
                                  << " | " << std::setw(6) << perf.stallBreakdown["ICACHE_MISS"]
                                  << " | " << std::setw(6) << perf.stallBreakdown["DCACHE_MISS"]
                                  << " | " << std::setw(13) << perf.branchBubbles
                                  << " | " << std::setw(3) << (perf.stallBreakdown["RAW_NOFWD"] + perf.stallBreakdown["RAW_LOAD_USE"])
                                  << " |\n";
                    }
                }
            }
        }
    }

    if (jsonFile.is_open()) {
        jsonFile << "\n]\n";
    }

    std::cout << "\n[SUCCESS] Wrote experiments/pipeline/audit_results.csv and experiments/pipeline/audit_results.json\n";
    return 0;
}

