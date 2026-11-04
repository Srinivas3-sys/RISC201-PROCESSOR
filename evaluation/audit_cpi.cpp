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

int main() {
    std::vector<std::string> benchmarks = {
        "factorial_iter",
        "factorial_rec",
        "prime_test",
        "array_sum",
        "hazard_test",
        "stack_test"
    };

    std::vector<std::string> presets = {"ideal", "simple", "modern", "modern-ddr5"};
    std::vector<std::string> fwdModes = {"full", "none"};

    std::ofstream csv("experiments/pipeline/audit_results.csv");
    if (csv.is_open()) {
        csv << "Benchmark,Pipeline,Preset,Forwarding,IC,Cycles,CPI,RAW_NOFWD,RAW_LOAD_USE,BRANCH_MISPREDICT,BRANCH_STALL,ICACHE_MISS,DCACHE_MISS,TLB_MISS,PAGE_WALK,PAGE_FAULT,Exception\n";
    }

    std::cout << "========================================================================\n"
              << "             RISC201 REPEATABLE CPI AUDIT BENCHMARK MATRIX              \n"
              << "========================================================================\n\n";

    std::cout << "| Benchmark | Pipeline | Preset | Fwd | IC | Cycles | CPI | I-Miss | D-Miss | BranchBubbles | RAW |\n"
              << "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n";

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
                    double cpi = 0;
                    std::string exStr = "NONE";
                    PerformanceTracker perf(pipe);

                    if (pipe == 4) {
                        stage4::Pipeline4Stage p4;
                        p4.setMemPreset(preset);
                        p4.setForwardingMode(fwd);
                        p4.loadProgram(0, words);
                        p4.run(500000);
                        ic = p4.retiredInstructions;
                        cyc = p4.cycles;
                        cpi = (ic > 0) ? (static_cast<double>(cyc) / ic) : 0;
                        perf = p4.perfTracker;
                        if (p4.currentException.hasOccurred()) {
                            exStr = exceptionToString(p4.currentException.type);
                        }
                    } else {
                        stage6::Pipeline6Stage p6;
                        p6.setMemPreset(preset);
                        p6.setForwardingMode(fwd);
                        p6.loadProgram(0, words);
                        p6.run(500000);
                        ic = p6.retiredInstructions;
                        cyc = p6.cycles;
                        cpi = (ic > 0) ? (static_cast<double>(cyc) / ic) : 0;
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
                            << std::fixed << std::setprecision(3) << cpi << ","
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

                    if (fwd == "full" && (preset == "ideal" || preset == "modern")) {
                        std::cout << "| " << std::setw(15) << std::left << bm
                                  << " | " << pipe << "-Stage"
                                  << " | " << std::setw(6) << preset
                                  << " | " << std::setw(4) << fwd
                                  << " | " << std::setw(5) << std::right << ic
                                  << " | " << std::setw(6) << cyc
                                  << " | " << std::setw(6) << std::fixed << std::setprecision(3) << cpi
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
    std::cout << "\n[SUCCESS] Wrote experiments/pipeline/audit_results.csv\n";
    return 0;
}
