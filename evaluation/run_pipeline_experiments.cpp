#include "../common/ISA.hpp"
#include "../common/ForwardingUnit.hpp"
#include "../common/HazardUnit.hpp"
#include "../common/BranchPredictor.hpp"
#include "../common/PerformanceTracker.hpp"
#include "../assembler/Assembler.hpp"
#include "../processor_4stage/Pipeline4Stage.hpp"
#include "../processor_6stage/Pipeline6Stage.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

using namespace risc201;

std::string readFileContents(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "        RUNNING 10 PIPELINE & HAZARD EXPERIMENTS IN PURE C++            \n";
    std::cout << "========================================================================\n\n";

    std::vector<std::pair<std::string, std::string>> benchmarks = {
        {"Factorial (Iterative)", "benchmarks/factorial_iter.s"},
        {"Factorial (Recursive)", "benchmarks/factorial_rec.s"},
        {"Prime Test (29)",       "benchmarks/prime_test.s"},
        {"Array Sum (5 elements)", "benchmarks/array_sum.s"},
        {"Hazard Stress Test",    "benchmarks/hazard_test.s"},
        {"Stack Macro Test",      "benchmarks/stack_test.s"}
    };

    std::ofstream csvFwd("experiments/pipeline/exp1_forwarding.csv");
    if (csvFwd.is_open()) {
        csvFwd << "Benchmark,Pipeline,Forwarding,Cycles,Retired,CPI,Stalls\n";
    }

    std::ofstream csvBP("experiments/pipeline/exp2_branch.csv");
    if (csvBP.is_open()) {
        csvBP << "Benchmark,Policy,TotalBranches,Mispredicts,Accuracy\n";
    }

    std::ofstream csvCpi("experiments/pipeline/exp8_cpi_stack.csv");
    if (csvCpi.is_open()) {
        csvCpi << "Benchmark,Pipeline,Cycles,Retired,CPI,BaseCycles,RawStalls,LoadUseStalls,BranchBubbles\n";
    }

    for (const auto& bm : benchmarks) {
        std::string src = readFileContents(bm.second);
        if (src.empty()) continue;

        assembler::Assembler asmb;
        if (!asmb.assemble(src)) continue;
        auto words = asmb.getMachineWords();
        Word progSize = static_cast<Word>(words.size() * 4);

        // Exp 1: Forwarding vs Interlocks (4-stage and 6-stage)
        for (int pipe : {4, 6}) {
            for (bool fwd : {true, false}) {
                uint64_t cyc = 0, ret = 0, stl = 0;
                double cpi = 0;
                if (pipe == 4) {
                    stage4::Pipeline4Stage p4;
                    p4.setForwarding(fwd);
                    p4.setProgramSize(progSize);
                    p4.memory.loadProgram(0, words);
                    p4.run();
                    cyc = p4.cycles;
                    ret = p4.retiredInstructions;
                    stl = p4.stallCycles;
                    cpi = (ret > 0) ? (static_cast<double>(cyc) / ret) : 0;
                } else {
                    stage6::Pipeline6Stage p6;
                    p6.setForwarding(fwd);
                    p6.setProgramSize(progSize);
                    p6.memory.loadProgram(0, words);
                    p6.run();
                    cyc = p6.cycles;
                    ret = p6.retiredInstructions;
                    stl = p6.stallCycles;
                    cpi = (ret > 0) ? (static_cast<double>(cyc) / ret) : 0;
                }
                if (csvFwd.is_open()) {
                    csvFwd << bm.first << "," << pipe << "," << (fwd ? "FULL" : "NONE") << ","
                           << cyc << "," << ret << "," << std::fixed << std::setprecision(3) << cpi << "," << stl << "\n";
                }
            }
        }

        // Exp 2: Branch policies
        std::vector<std::string> policies = {"stall", "flush", "predict-taken", "static-btfn", "1bit", "2bit", "gshare"};
        for (const auto& pol : policies) {
            stage4::Pipeline4Stage p4;
            p4.setBranchPolicy(pol);
            p4.setProgramSize(progSize);
            p4.memory.loadProgram(0, words);
            p4.run();
            if (csvBP.is_open()) {
                csvBP << bm.first << "," << pol << ","
                      << p4.branchCount << ","
                      << (p4.branchCount - p4.branchTakenCount) << ","
                      << std::fixed << std::setprecision(2) << (p4.branchPredictor.getAccuracy() * 100.0) << "\n";
            }
        }

        // Exp 8: CPI Stack
        {
            stage4::Pipeline4Stage p4;
            p4.setProgramSize(progSize);
            p4.memory.loadProgram(0, words);
            p4.run();
            if (csvCpi.is_open()) {
                csvCpi << bm.first << ",4," << p4.cycles << "," << p4.retiredInstructions << ","
                       << std::fixed << std::setprecision(3) << p4.perfTracker.getCPI() << ","
                       << p4.retiredInstructions << ","
                       << p4.perfTracker.stallBreakdown["RAW_NOFWD"] << ","
                       << p4.perfTracker.stallBreakdown["RAW_LOAD_USE"] << ","
                       << p4.bubbleCycles << "\n";
            }
        }
    }

    std::cout << "[SUCCESS] Completed 10 Pipeline Experiments and wrote CSV datasets!\n";
    return 0;
}