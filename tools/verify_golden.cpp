#include "common/Types.hpp"
#include "common/ISA.hpp"
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
#include <cmath>
#include <chrono>

using namespace risc201;

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open()) return lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
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
    std::cout << "========================================================================\n";
    std::cout << "         RISC201 GOLDEN VERIFICATION & REGRESSION VALIDATOR             \n";
    std::cout << "========================================================================\n";

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

    size_t totalChecks = 0;
    size_t passedChecks = 0;
    std::vector<std::string> failures;

    // -------------------------------------------------------------
    // Part 1: Assembler & Disassembler Byte-for-Byte Golden Match
    // -------------------------------------------------------------
    std::cout << "[CHECK 1/4] Verifying Assembler & Disassembler against Golden Baselines...\n";
    for (const auto& bm : benchmarks) {
        std::string src = readFile("benchmarks/" + bm + ".s");
        if (src.empty()) {
            failures.push_back("Could not read source: benchmarks/" + bm + ".s");
            continue;
        }

        assembler::Assembler asmb;
        if (!asmb.assemble(src)) {
            failures.push_back("Assembly failed: " + bm);
            continue;
        }

        // Compare .hex
        std::vector<std::string> goldenHex = readLines("tests/golden_refactor/" + bm + ".hex");
        totalChecks++;
        bool hexOk = (asmb.assembledLines.size() == goldenHex.size());
        if (hexOk) {
            for (size_t i = 0; i < goldenHex.size(); ++i) {
                if (asmb.assembledLines[i].hexString != goldenHex[i]) {
                    hexOk = false;
                    break;
                }
            }
        }
        if (hexOk) passedChecks++;
        else failures.push_back(bm + " .hex mismatch with golden baseline");

        // Compare .bin
        std::vector<std::string> goldenBin = readLines("tests/golden_refactor/" + bm + ".bin");
        totalChecks++;
        bool binOk = (asmb.assembledLines.size() == goldenBin.size());
        if (binOk) {
            for (size_t i = 0; i < goldenBin.size(); ++i) {
                if (asmb.assembledLines[i].binaryString != goldenBin[i]) {
                    binOk = false;
                    break;
                }
            }
        }
        if (binOk) passedChecks++;
        else failures.push_back(bm + " .bin mismatch with golden baseline");

        // Compare .disasm
        std::vector<std::string> goldenDisasm = readLines("tests/golden_refactor/" + bm + ".disasm");
        totalChecks++;
        bool disasmOk = (asmb.assembledLines.size() == goldenDisasm.size());
        if (disasmOk) {
            for (size_t i = 0; i < goldenDisasm.size(); ++i) {
                const auto& line = asmb.assembledLines[i];
                std::string dasm = assembler::Disassembler::disassemble(line.machineCode, line.address);
                std::ostringstream oss;
                oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << line.address
                    << ": " << toHex(line.machineCode)
                    << " | " << dasm;
                if (oss.str() != goldenDisasm[i]) {
                    disasmOk = false;
                    break;
                }
            }
        }
        if (disasmOk) passedChecks++;
        else failures.push_back(bm + " .disasm mismatch with golden baseline");
    }

    // -------------------------------------------------------------
    // Part 2: Microcode Dumps Byte-for-Byte Match
    // -------------------------------------------------------------
    std::cout << "[CHECK 2/4] Verifying Microcode Control Store against Golden Baselines...\n";
    {
        RegisterFile rf;
        Memory mem;
        alu::ALU aluUnit;
        microcode::MicroController mc(&rf, &mem, &aluUnit);
        mc.buildControlStore();

        // Horizontal dump
        std::ostringstream hStream;
        hStream << "uPC | ControlSignals (Hex) | uImm | uBranchTarget | Args | Disassembly\n";
        hStream << "----+----------------------+------+---------------+------+------------------------\n";
        for (size_t i = 0; i < mc.controlStoreHorizontal.size(); ++i) {
            const auto& hw = mc.controlStoreHorizontal[i];
            hStream << std::setw(3) << i << " | "
                    << "0x" << std::hex << std::setw(16) << std::setfill('0') << hw.controlSignals << std::dec << std::setfill(' ') << " | "
                    << std::setw(4) << hw.uimm << " | "
                    << std::setw(13) << hw.ubranchTarget << " | "
                    << std::setw(4) << static_cast<int>(hw.args) << " | "
                    << hw.disassembly << "\n";
        }
        std::string goldenHStr = readFile("tests/golden_refactor/microcode_horizontal.txt");
        totalChecks++;
        if (hStream.str() == goldenHStr) passedChecks++;
        else failures.push_back("microcode_horizontal.txt mismatch with golden baseline");

        // Vertical dump
        std::ostringstream vStream;
        vStream << "uPC | Packed (Hex) | Type | Dest | Src | Imm | BranchTarget | Args | Disassembly\n";
        vStream << "----+--------------+------+------+-----+-----+--------------+------+------------------------\n";
        for (size_t i = 0; i < mc.controlStoreVertical.size(); ++i) {
            const auto& vi = mc.controlStoreVertical[i];
            vStream << std::setw(3) << i << " | "
                    << "0x" << std::hex << std::setw(12) << std::setfill('0') << vi.pack() << std::dec << std::setfill(' ') << " | "
                    << std::setw(4) << static_cast<int>(vi.type) << " | "
                    << std::setw(4) << static_cast<int>(vi.dest) << " | "
                    << std::setw(3) << static_cast<int>(vi.src) << " | "
                    << std::setw(3) << vi.immediate << " | "
                    << std::setw(12) << vi.branchTarget << " | "
                    << std::setw(4) << static_cast<int>(vi.args) << " | "
                    << vi.disassembly << "\n";
        }
        std::string goldenVStr = readFile("tests/golden_refactor/microcode_vertical.txt");
        totalChecks++;
        if (vStream.str() == goldenVStr) passedChecks++;
        else failures.push_back("microcode_vertical.txt mismatch with golden baseline");
    }

    // -------------------------------------------------------------
    // Part 3: Full Simulation Matrix vs Golden simulation_baseline.csv
    // -------------------------------------------------------------
    std::cout << "[CHECK 3/4] Verifying Pipeline Simulation Matrix (192 configurations)...\n";
    std::vector<std::string> baselineCsv = readLines("tests/golden_refactor/simulation_baseline.csv");
    if (baselineCsv.empty()) {
        failures.push_back("Could not read tests/golden_refactor/simulation_baseline.csv");
    } else {
        struct SimRecord {
            uint64_t ic{0};
            uint64_t cycles{0};
            double cpiCold{0.0};
            double cpiSteady{0.0};
            std::string exception;
            std::vector<Word> regs;
            uint32_t memCsum{0};
        };

        std::unordered_map<std::string, SimRecord> goldenMap;
        for (size_t i = 1; i < baselineCsv.size(); ++i) {
            const auto& row = baselineCsv[i];
            std::stringstream ss(row);
            std::string bm, pipe, preset, fwd, icStr, cycStr, cpiCStr, cpiSStr;
            std::getline(ss, bm, ',');
            std::getline(ss, pipe, ',');
            std::getline(ss, preset, ',');
            std::getline(ss, fwd, ',');
            std::getline(ss, icStr, ',');
            std::getline(ss, cycStr, ',');
            std::getline(ss, cpiCStr, ',');
            std::getline(ss, cpiSStr, ',');

            // Skip stack columns 8-17
            std::string tmp;
            for (int k = 0; k < 10; ++k) std::getline(ss, tmp, ',');

            std::string exc;
            std::getline(ss, exc, ',');
            if (!exc.empty() && exc.front() == '"') exc.erase(0, 1);
            if (!exc.empty() && exc.back() == '"') exc.pop_back();

            std::vector<Word> regs(16, 0);
            for (int r = 0; r < 16; ++r) {
                std::string rVal;
                std::getline(ss, rVal, ',');
                regs[r] = static_cast<Word>(std::stoul(rVal));
            }
            std::string csumStr;
            std::getline(ss, csumStr, ',');
            uint32_t csum = static_cast<uint32_t>(std::stoul(csumStr));

            std::string key = bm + "|" + pipe + "|" + preset + "|" + fwd;
            SimRecord rec;
            rec.ic = std::stoull(icStr);
            rec.cycles = std::stoull(cycStr);
            rec.cpiCold = std::stod(cpiCStr);
            rec.cpiSteady = std::stod(cpiSStr);
            rec.exception = exc;
            rec.regs = regs;
            rec.memCsum = csum;
            goldenMap[key] = rec;
        }

        std::vector<std::string> presets = {"ideal", "simple", "modern", "modern-ddr5"};
        std::vector<std::string> fwdModes = {"full", "none"};

        for (const auto& bm : benchmarks) {
            std::string src = readFile("benchmarks/" + bm + ".s");
            if (src.empty()) continue;

            assembler::Assembler asmb;
            if (!asmb.assemble(src)) continue;
            auto words = asmb.getMachineWords();
            Word progSize = static_cast<Word>(words.size() * 4);

            for (const auto& pipeline : {"4-Stage", "6-Stage"}) {
                for (const auto& preset : presets) {
                    for (const auto& fwd : fwdModes) {
                        totalChecks++;
                        std::string key = bm + "|" + pipeline + "|" + preset + "|" + fwd;
                        if (goldenMap.find(key) == goldenMap.end()) {
                            failures.push_back("Missing golden key: " + key);
                            continue;
                        }
                        const auto& golden = goldenMap[key];

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

                        bool match = (ic == golden.ic) &&
                                     (cycles == golden.cycles) &&
                                     (std::abs(cpiCold - golden.cpiCold) < 0.001) &&
                                     (std::abs(cpiSteady - golden.cpiSteady) < 0.001) &&
                                     (excStr == golden.exception) &&
                                     (regs == golden.regs) &&
                                     (memCsum == golden.memCsum);

                        if (match) {
                            passedChecks++;
                        } else {
                            std::ostringstream err;
                            err << key << " MISMATCH: "
                                << "IC (" << ic << " vs " << golden.ic << "), "
                                << "Cycles (" << cycles << " vs " << golden.cycles << "), "
                                << "CPI_Cold (" << cpiCold << " vs " << golden.cpiCold << "), "
                                << "MemChecksum (" << std::hex << memCsum << " vs " << golden.memCsum << ")";
                            failures.push_back(err.str());
                        }
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------
    // Part 4: Simulation Performance Benchmark
    // -------------------------------------------------------------
    std::cout << "[CHECK 4/4] Verifying Simulator Throughput (Speed Regression Check)...\n";
    totalChecks++;
    {
        std::string src = readFile("benchmarks/prime_test_long.s");
        assembler::Assembler asmb;
        asmb.assemble(src);
        auto words = asmb.getMachineWords();

        stage4::Pipeline4Stage p4;
        p4.setMemPreset("ideal");
        p4.setForwardingMode("full");
        p4.loadProgram(0, words);

        auto t0 = std::chrono::high_resolution_clock::now();
        p4.run(1000000);
        auto t1 = std::chrono::high_resolution_clock::now();

        double elapsedSec = std::chrono::duration<double>(t1 - t0).count();
        double instPerSec = static_cast<double>(p4.perfTracker.retiredInstructions) / (elapsedSec > 0.0 ? elapsedSec : 1e-6);

        std::cout << "  Throughput measured: " << std::fixed << std::setprecision(0) << instPerSec << " inst/sec\n";
        // Ensure simulation speed remains at least 400,000 inst/s
        if (instPerSec >= 400000.0) {
            passedChecks++;
        } else {
            failures.push_back("Simulation speed below 400,000 inst/s regression threshold");
        }
    }

    std::cout << "========================================================================\n";
    std::cout << "Verification Checks: " << passedChecks << " / " << totalChecks << " passed.\n";

    if (failures.empty()) {
        std::cout << "ALL GOLDEN BASELINES MATCH 100%! ZERO DEVIATIONS IN ARCHITECTURAL BEHAVIOR.\n";
        std::cout << "========================================================================\n";
        return 0;
    } else {
        std::cerr << "FAILURES ENCOUNTERED (" << failures.size() << "):\n";
        for (const auto& f : failures) {
            std::cerr << "  - " << f << "\n";
        }
        std::cerr << "========================================================================\n";
        return 1;
    }
}
