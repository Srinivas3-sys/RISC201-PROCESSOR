#include "common/Types.hpp"
#include "common/RegisterFile.hpp"
#include "common/Memory.hpp"
#include "common/Exception.hpp"
#include "alu/ALU.hpp"
#include "assembler/Assembler.hpp"
#include "assembler/Disassembler.hpp"
#include "processor_4stage/Pipeline4Stage.hpp"
#include "processor_6stage/Pipeline6Stage.hpp"
#include "common/PerformanceTracker.hpp"
#include "microcode/MicroController.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>

using namespace risc201;

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static uint32_t computeMemChecksum(const Memory& mem, Word progSize) {
    uint32_t csum = 0x811c9dc5; // FNV-1a 32-bit
    for (size_t i = 0; i < progSize; i += 4) {
        Word w = mem.readWord(static_cast<Word>(i));
        csum ^= (w & 0xFF); csum *= 0x01000193;
        csum ^= ((w >> 8) & 0xFF); csum *= 0x01000193;
        csum ^= ((w >> 16) & 0xFF); csum *= 0x01000193;
        csum ^= ((w >> 24) & 0xFF); csum *= 0x01000193;
    }
    return csum;
}

int main() {
    std::cout << "Capturing Golden Baselines into tests/golden_refactor/ ...\n";

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

    // -------------------------------------------------------------
    // A) Assembler and Disassembler Golden Dumps
    // -------------------------------------------------------------
    std::cout << "[1/4] Capturing Assembler & Disassembler baselines...\n";
    for (const auto& bm : benchmarks) {
        std::string srcPath = "benchmarks/" + bm + ".s";
        std::string src = readFile(srcPath);
        if (src.empty()) {
            std::cerr << "Warning: Could not read " << srcPath << "\n";
            continue;
        }

        assembler::Assembler asmb;
        if (!asmb.assemble(src)) {
            std::cerr << "Assembly failed for " << bm << "\n";
            continue;
        }

        // Write .hex and .bin
        std::string hexPath = "tests/golden_refactor/" + bm + ".hex";
        std::string binPath = "tests/golden_refactor/" + bm + ".bin";
        std::string disasmPath = "tests/golden_refactor/" + bm + ".disasm";

        {
            std::ofstream outHex(hexPath);
            for (const auto& line : asmb.assembledLines) {
                outHex << line.hexString << "\n";
            }
        }
        {
            std::ofstream outBin(binPath);
            for (const auto& line : asmb.assembledLines) {
                outBin << line.binaryString << "\n";
            }
        }

        // Disassemble machine words and write .disasm
        std::ofstream disasmFile(disasmPath);
        for (const auto& line : asmb.assembledLines) {
            std::string dasm = assembler::Disassembler::disassemble(line.machineCode, line.address);
            disasmFile << "0x" << std::hex << std::setw(8) << std::setfill('0') << line.address
                       << ": " << toHex(line.machineCode)
                       << " | " << dasm << "\n";
        }
    }

    // -------------------------------------------------------------
    // B) Simulation Baselines across Pipelines, Presets, and Fwd Modes
    // -------------------------------------------------------------
    std::cout << "[2/4] Capturing full Pipeline Simulation baselines...\n";
    std::vector<std::string> presets = {"ideal", "simple", "modern", "modern-ddr5"};
    std::vector<std::string> fwdModes = {"full", "none"};

    std::ofstream simCsv("tests/golden_refactor/simulation_baseline.csv");
    simCsv << "Benchmark,Pipeline,Preset,Forwarding,IC,Cycles,CPI_Cold,CPI_Steady,"
           << "BaseCycles,RAW_NOFWD,RAW_LOAD_USE,BRANCH_MISPREDICT,BRANCH_STALL,"
           << "ICACHE_MISS,DCACHE_MISS,TLB_MISS,PAGE_WALK,PAGE_FAULT,Exception,"
           << "r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,sp,ra,MemChecksum\n";

    for (const auto& bm : benchmarks) {
        std::string srcPath = "benchmarks/" + bm + ".s";
        std::string src = readFile(srcPath);
        if (src.empty()) continue;

        assembler::Assembler asmb;
        if (!asmb.assemble(src)) continue;
        auto words = asmb.getMachineWords();
        Word progSize = static_cast<Word>(words.size() * 4);

        for (const auto& pipeline : {"4-Stage", "6-Stage"}) {
            for (const auto& preset : presets) {
                for (const auto& fwd : fwdModes) {
                    uint64_t ic = 0;
                    uint64_t cycles = 0;
                    double cpiCold = 0.0;
                    double cpiSteady = 0.0;
                    std::string excStr = "NONE";
                    std::vector<Word> regs(16, 0);
                    uint32_t memCsum = 0;

                    PerformanceTracker tracker(pipeline == std::string("4-Stage") ? 4 : 6);

                    if (pipeline == std::string("4-Stage")) {
                        stage4::Pipeline4Stage p4;
                        p4.setMemPreset(preset);
                        p4.setForwardingMode(fwd);
                        p4.loadProgram(0, words);

                        uint64_t limit = (bm.find("long") != std::string::npos) ? 2000000 : 20000;
                        p4.run(limit);

                        ic = p4.perfTracker.retiredInstructions;
                        cycles = p4.perfTracker.totalCycles;
                        cpiCold = p4.perfTracker.getCPI();
                        tracker = p4.perfTracker;
                        if (p4.currentException.hasOccurred()) {
                            excStr = exceptionToString(p4.currentException.type);
                        }
                        for (int r = 0; r < 16; ++r) {
                            regs[r] = p4.regFile.read(r);
                        }
                        memCsum = computeMemChecksum(p4.memory, progSize);
                    } else {
                        stage6::Pipeline6Stage p6;
                        p6.setMemPreset(preset);
                        p6.setForwardingMode(fwd);
                        p6.loadProgram(0, words);

                        uint64_t limit = (bm.find("long") != std::string::npos) ? 2000000 : 20000;
                        p6.run(limit);

                        ic = p6.perfTracker.retiredInstructions;
                        cycles = p6.perfTracker.totalCycles;
                        cpiCold = p6.perfTracker.getCPI();
                        tracker = p6.perfTracker;
                        if (p6.currentException.hasOccurred()) {
                            excStr = exceptionToString(p6.currentException.type);
                        }
                        for (int r = 0; r < 16; ++r) {
                            regs[r] = p6.regFile.read(r);
                        }
                        memCsum = computeMemChecksum(p6.memory, progSize);
                    }

                    uint64_t coldMissStalls = tracker.stallBreakdown["ICACHE_MISS"] + tracker.stallBreakdown["DCACHE_MISS"];
                    if (cycles > coldMissStalls && ic > 0) {
                        cpiSteady = static_cast<double>(cycles - coldMissStalls) / ic;
                    } else {
                        cpiSteady = cpiCold;
                    }

                    simCsv << bm << "," << pipeline << "," << preset << "," << fwd << ","
                           << ic << "," << cycles << ","
                           << std::fixed << std::setprecision(4) << cpiCold << "," << cpiSteady << ","
                           << ic << ","
                           << tracker.stallBreakdown["RAW_NOFWD"] << ","
                           << tracker.stallBreakdown["RAW_LOAD_USE"] << ","
                           << tracker.stallBreakdown["BRANCH_MISPREDICT"] << ","
                           << tracker.stallBreakdown["BRANCH_STALL"] << ","
                           << tracker.stallBreakdown["ICACHE_MISS"] << ","
                           << tracker.stallBreakdown["DCACHE_MISS"] << ","
                           << tracker.stallBreakdown["TLB_MISS"] << ","
                           << tracker.stallBreakdown["PAGE_WALK"] << ","
                           << tracker.stallBreakdown["PAGE_FAULT"] << ","
                           << "\"" << excStr << "\"";

                    for (int r = 0; r < 16; ++r) {
                        simCsv << "," << regs[r];
                    }
                    simCsv << "," << memCsum << "\n";
                }
            }
        }
    }

    // -------------------------------------------------------------
    // C) Microcode Control Store Dumps
    // -------------------------------------------------------------
    std::cout << "[3/4] Capturing Microcode Control Store Dumps...\n";
    {
        RegisterFile rf;
        Memory mem;
        alu::ALU alu;
        microcode::MicroController mc(&rf, &mem, &alu);
        mc.buildControlStore();

        // Horizontal dump
        std::ofstream hDump("tests/golden_refactor/microcode_horizontal.txt");
        hDump << "uPC | ControlSignals (Hex) | uImm | uBranchTarget | Args | Disassembly\n";
        hDump << "----+----------------------+------+---------------+------+------------------------\n";
        for (size_t i = 0; i < mc.controlStoreHorizontal.size(); ++i) {
            const auto& hw = mc.controlStoreHorizontal[i];
            hDump << std::setw(3) << i << " | "
                  << "0x" << std::hex << std::setw(16) << std::setfill('0') << hw.controlSignals << std::dec << std::setfill(' ') << " | "
                  << std::setw(4) << hw.uimm << " | "
                  << std::setw(13) << hw.ubranchTarget << " | "
                  << std::setw(4) << static_cast<int>(hw.args) << " | "
                  << hw.disassembly << "\n";
        }

        // Vertical dump
        std::ofstream vDump("tests/golden_refactor/microcode_vertical.txt");
        vDump << "uPC | Packed (Hex) | Type | Dest | Src | Imm | BranchTarget | Args | Disassembly\n";
        vDump << "----+--------------+------+------+-----+-----+--------------+------+------------------------\n";
        for (size_t i = 0; i < mc.controlStoreVertical.size(); ++i) {
            const auto& vi = mc.controlStoreVertical[i];
            vDump << std::setw(3) << i << " | "
                  << "0x" << std::hex << std::setw(12) << std::setfill('0') << vi.pack() << std::dec << std::setfill(' ') << " | "
                  << std::setw(4) << static_cast<int>(vi.type) << " | "
                  << std::setw(4) << static_cast<int>(vi.dest) << " | "
                  << std::setw(3) << static_cast<int>(vi.src) << " | "
                  << std::setw(3) << vi.immediate << " | "
                  << std::setw(12) << vi.branchTarget << " | "
                  << std::setw(4) << static_cast<int>(vi.args) << " | "
                  << vi.disassembly << "\n";
        }

        // Dispatch table dump
        std::ofstream dDump("tests/golden_refactor/microcode_dispatch.txt");
        dDump << "Opcode (Int) | Start uPC\n";
        dDump << "-------------+----------\n";
        for (const auto& kv : mc.opcodeDispatchTable) {
            dDump << std::setw(12) << static_cast<int>(kv.first) << " | " << kv.second << "\n";
        }
    }

    // -------------------------------------------------------------
    // D) Simulation Speed Baseline
    // -------------------------------------------------------------
    std::cout << "[4/4] Capturing Simulation Speed Baseline...\n";
    {
        std::ofstream speedFile("tests/golden_refactor/simulation_speed.txt");
        speedFile << "Benchmark | Pipeline | Instructions | ElapsedTime (ms) | Speed (Instructions/sec)\n";
        speedFile << "----------+----------+--------------+------------------+------------------------\n";

        std::vector<std::string> speedBms = {"factorial_iter_long", "array_sum_long", "prime_test_long"};
        for (const auto& bm : speedBms) {
            std::string src = readFile("benchmarks/" + bm + ".s");
            assembler::Assembler asmb;
            asmb.assemble(src);
            auto words = asmb.getMachineWords();

            for (const auto& pipeline : {"4-Stage", "6-Stage"}) {
                auto start = std::chrono::high_resolution_clock::now();
                uint64_t ic = 0;

                if (pipeline == std::string("4-Stage")) {
                    stage4::Pipeline4Stage p4;
                    p4.setMemPreset("ideal");
                    p4.setForwarding(true);
                    p4.loadProgram(0, words);
                    p4.run(2000000);
                    ic = p4.perfTracker.retiredInstructions;
                } else {
                    stage6::Pipeline6Stage p6;
                    p6.setMemPreset("ideal");
                    p6.setForwarding(true);
                    p6.loadProgram(0, words);
                    p6.run(2000000);
                    ic = p6.perfTracker.retiredInstructions;
                }

                auto end = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(end - start).count();
                double ips = (ms > 0.0) ? (ic / (ms / 1000.0)) : 0.0;

                speedFile << std::setw(18) << bm << " | "
                          << std::setw(8) << pipeline << " | "
                          << std::setw(12) << ic << " | "
                          << std::setw(16) << std::fixed << std::setprecision(2) << ms << " | "
                          << std::fixed << std::setprecision(0) << ips << "\n";

                std::cout << "  " << bm << " (" << pipeline << "): " << ic << " insts in " << ms << " ms ("
                          << static_cast<uint64_t>(ips) << " inst/sec)\n";
            }
        }
    }

    std::cout << "[SUCCESS] Golden Baselines captured successfully into tests/golden_refactor/!\n";
    return 0;
}
