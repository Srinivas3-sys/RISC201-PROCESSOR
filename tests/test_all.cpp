#include "../common/ISA.hpp"
#include "../common/RegisterFile.hpp"
#include "../common/ForwardingUnit.hpp"
#include "../common/HazardUnit.hpp"
#include "../common/BranchPredictor.hpp"
#include "../common/PerformanceTracker.hpp"
#include "../common/ISS.hpp"
#include "../assembler/Assembler.hpp"
#include "../memory/MemoryHierarchy.hpp"
#include "../processor_4stage/Pipeline4Stage.hpp"
#include "../processor_6stage/Pipeline6Stage.hpp"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace risc201;

int testsRun = 0;
int testsPassed = 0;

#define TEST_ASSERT(cond, msg) do { \
    testsRun++; \
    if (cond) { \
        testsPassed++; \
    } else { \
        std::cerr << "FAIL: " << msg << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

// -------------------------------------------------------------
// Test 1: ISA Binary Strings & Encodings
// -------------------------------------------------------------
void test_isa_binary_strings() {
    std::cout << "[TEST] 1. ISA Binary Strings & Encodings...\n";

    TEST_ASSERT(intToBits(5, 5) == "00101", "intToBits(5, 5)");
    TEST_ASSERT(bitsToInt("00101", false) == 5, "bitsToInt('00101')");
    TEST_ASSERT(isa::OP_ADD == "00000", "OP_ADD binary string");
    TEST_ASSERT(isa::OP_SUB == "00001", "OP_SUB binary string");
    TEST_ASSERT(isa::OP_INVALID == "11111", "OP_INVALID binary string");

    // Test sliceField MSB-first [31:0]
    std::string wordBits = "10000000000000000000000000000001";
    TEST_ASSERT(sliceField(wordBits, 31, 31) == "1", "sliceField MSB");
    TEST_ASSERT(sliceField(wordBits, 0, 0) == "1", "sliceField LSB");
    TEST_ASSERT(sliceField(wordBits, 30, 1) == std::string(30, '0'), "sliceField middle zeros");

    // Decode add r1, r2, r3 (Opcode 0, I=0, rd=1, rs1=2, rs2=3)
    // Bits: 00000 0 0001 0010 0011 00 00000000000000 = 0x0044C000
    uint32_t addWord = (0b00000 << 27) | (0 << 26) | (1 << 22) | (2 << 18) | (3 << 14);
    Instruction addInst = decode(addWord);
    TEST_ASSERT(addInst.opcode == isa::OP_ADD, "add opcode match");
    TEST_ASSERT(addInst.rd == 1, "add rd match");
    TEST_ASSERT(addInst.rs1 == 2, "add rs1 match");
    TEST_ASSERT(addInst.rs2 == 3, "add rs2 match");
    TEST_ASSERT(!addInst.isImmediate, "add not immediate");

    // Decode mov r0, #5 (Opcode 9, I=1, rd=0, rs1=0, imm=5)
    uint32_t movWord = (0b01001 << 27) | (1 << 26) | (0 << 22) | (0 << 18) | 5;
    Instruction movInst = decode(movWord);
    TEST_ASSERT(movInst.opcode == isa::OP_MOV, "mov opcode match");
    TEST_ASSERT(movInst.isImmediate, "mov isImmediate match");
    TEST_ASSERT(movInst.immx == 5, "mov immx match");

    // Single source of truth verification
    TEST_ASSERT(isa::verifyIsaTable(), "ISA table verifies exactly 28 opcodes and rejects illegal patterns");
    TEST_ASSERT(isa::isValidOpcode(isa::OP_ADD), "OP_ADD is valid");
    TEST_ASSERT(isa::isValidOpcode(isa::OP_ABS), "OP_ABS is valid");
    TEST_ASSERT(!isa::isValidOpcode("11100"), "Pattern 11100 is unassigned/illegal");
    TEST_ASSERT(!isa::isValidOpcode("11101"), "Pattern 11101 is unassigned/illegal");
    TEST_ASSERT(!isa::isValidOpcode("11110"), "Pattern 11110 is unassigned/illegal");
    TEST_ASSERT(!isa::isValidOpcode(isa::OP_INVALID), "OP_INVALID is illegal");
    TEST_ASSERT(isa::mnemonicOf(isa::OP_ADD) == "add", "mnemonicOf OP_ADD");
    TEST_ASSERT(isa::mnemonicOf(isa::OP_ABS) == "abs", "mnemonicOf OP_ABS");
    TEST_ASSERT(isa::opcodeOf("add") == isa::OP_ADD, "opcodeOf add");
    TEST_ASSERT(isa::opcodeOf("abs") == isa::OP_ABS, "opcodeOf abs");
    TEST_ASSERT(isa::wordToOpcode(addWord) == isa::OP_ADD, "wordToOpcode addWord");
    TEST_ASSERT(isa::wordToOpcode(movWord) == isa::OP_MOV, "wordToOpcode movWord");
    TEST_ASSERT(isa::opcodeToWordBits(isa::OP_ADD) == 0, "opcodeToWordBits OP_ADD");
    TEST_ASSERT(isa::opcodeToWordBits(isa::OP_MOV) == (9u << 27), "opcodeToWordBits OP_MOV");
}

// -------------------------------------------------------------
// Test 2: Cache Hierarchy & DRAM
// -------------------------------------------------------------
void test_cache_and_dram() {
    std::cout << "[TEST] 2. Cache Hierarchy & DRAM...\n";

    memory::CacheConfig cfg;
    cfg.sizeBytes = 4096; // 4 KB
    cfg.lineSizeBytes = 64;
    cfg.associativity = 4; // 16 sets
    cfg.hitLatencyCycles = 4;
    cfg.replacementPolicy = "lru";

    memory::Cache cache(cfg);

    // Initial access is a compulsory miss
    auto r1 = cache.access(0x1000, false);
    TEST_ASSERT(!r1.hit, "Cold access is miss");
    TEST_ASSERT(r1.missType == "compulsory", "Cold access is compulsory");

    // Second access to same line is a hit
    auto r2 = cache.access(0x1004, false);
    TEST_ASSERT(r2.hit, "Repeated access is hit");

    // Test DRAM Open-Page policy
    memory::DRAMConfig dcfg;
    dcfg.pagePolicy = "open";
    dcfg.timings.tCL = 15;
    dcfg.timings.tBURST = 4;
    dcfg.timings.tRCD = 15;
    dcfg.timings.tRP = 15;

    memory::DRAMController dram(dcfg);
    uint64_t lat1 = dram.access(0x2000, false); // Row miss
    TEST_ASSERT(lat1 > 0, "DRAM first access latency");

    uint64_t lat2 = dram.access(0x2000, false); // Row hit (same bank, same row)
    TEST_ASSERT(lat2 < lat1, "DRAM open row hit latency is faster");
    TEST_ASSERT(dram.rowHits == 1, "DRAM row hit count");
}

// -------------------------------------------------------------
// Test 3: Virtual Memory & TLBs
// -------------------------------------------------------------
void test_virtual_memory() {
    std::cout << "[TEST] 3. Virtual Memory & TLBs...\n";

    memory::VMConfig vcfg;
    vcfg.enabled = true;
    vcfg.pageSizeBytes = 4096;
    vcfg.ptLevels = 2;
    vcfg.itlbEntries = 16;
    vcfg.dtlbEntries = 16;

    memory::VirtualMemorySubsystem vm(vcfg);

    // Initial translation causes page fault (minor fault / demand page allocation)
    auto t1 = vm.translate(0x00010000, false, false, 0);
    TEST_ASSERT(t1.success, "Translation succeeds");
    TEST_ASSERT(t1.pageFaultOccurred, "Demand page fault triggered");
    TEST_ASSERT(t1.faultType == "minor", "Minor fault on first access");

    // Subsequent access hits DTLB
    auto t2 = vm.translate(0x00010004, false, false, 0);
    TEST_ASSERT(t2.dtlbHit, "Subsequent access hits DTLB");
    TEST_ASSERT(t2.latencyCycles == vcfg.tlbHitLatencyCycles, "DTLB hit latency is 1 cycle");
}

// -------------------------------------------------------------
// Test 4: Forwarding, Hazards, & Branch Prediction
// -------------------------------------------------------------
void test_hazards_and_forwarding() {
    std::cout << "[TEST] 4. Hazards, Forwarding, & Branch Prediction...\n";

    // Forwarding Unit
    ForwardingUnit fwd(ForwardingMode::FULL);
    Instruction prod;
    prod.opcode = isa::OP_ADD;
    prod.rd = 3;
    prod.info.writesRd = true;

    Instruction cons;
    cons.opcode = isa::OP_ADD;
    cons.rs1 = 3;
    cons.rs2 = 4;
    cons.info.readsRs1 = true;
    cons.info.readsRs2 = true;

    auto dec = fwd.resolve4Stage(cons, 10, 20, true, prod, 999);
    TEST_ASSERT(dec.forwardedOp1, "Op1 forwarded from EX");
    TEST_ASSERT(dec.valOp1 == 999, "Op1 received forwarded value 999");
    TEST_ASSERT(!dec.forwardedOp2, "Op2 not forwarded (rs2 != rd)");

    // Store Buffer Store-to-Load Bypass
    StoreBuffer sb(4);
    sb.addStore(0x5000, 0xCAFEBABE, 10);
    uint32_t bypassedVal = 0;
    bool found = sb.forwardLoad(0x5000, bypassedVal);
    TEST_ASSERT(found, "StoreBuffer bypass hit");
    TEST_ASSERT(bypassedVal == 0xCAFEBABE, "StoreBuffer correct data bypass");

    // Branch Predictor (Smith 2-bit counter)
    BranchPredictor bp(BranchPolicy::BIMODAL_2BIT);
    Instruction br;
    br.opcode = isa::OP_BEQ;
    br.info.isBranch = true;
    br.branchTarget = 0x100;

    auto pred = bp.predict(0x20, br);
    TEST_ASSERT(pred.predictedTaken, "Bimodal 2-bit weakly taken default");
    bp.update(0x20, br, true, 0x100, pred.predictedTaken);
    TEST_ASSERT(bp.correctPredictions == 1, "Prediction recorded as correct");
}

// -------------------------------------------------------------
// Test 5: Golden ISS vs Pipelined Processors Differential Equivalence
// -------------------------------------------------------------
void test_differential_iss() {
    std::cout << "[TEST] 5. Golden ISS vs 4-Stage & 6-Stage Processors...\n";

    std::string testAsm = R"(
.text
    mov r0, 5
    mov r1, 10
    add r2, r0, r1
    sub r3, r2, 3
    mul r4, r3, 2
    nop
)";

    assembler::Assembler asmb;
    bool ok = asmb.assemble(testAsm);
    TEST_ASSERT(ok, "Assembly successful");

    auto words = asmb.getMachineWords();
    Word progSize = static_cast<Word>(words.size() * 4);

    // 1. Run Golden Reference ISS
    InstructionSetSimulator iss;
    iss.setProgramSize(progSize);
    iss.mem.loadProgram(0, words);
    iss.run(100);

    Word expR2 = iss.regFile.read(2);
    Word expR3 = iss.regFile.read(3);
    Word expR4 = iss.regFile.read(4);

    TEST_ASSERT(expR2 == 15, "ISS r2 == 15");
    TEST_ASSERT(expR3 == 12, "ISS r3 == 12");
    TEST_ASSERT(expR4 == 24, "ISS r4 == 24");

    // 2. Run 4-Stage Pipeline
    stage4::Pipeline4Stage p4;
    p4.setProgramSize(progSize);
    p4.memory.loadProgram(0, words);
    p4.setForwarding(true);
    p4.run(100);

    TEST_ASSERT(p4.regFile.read(2) == expR2, "4-Stage r2 matches ISS");
    TEST_ASSERT(p4.regFile.read(3) == expR3, "4-Stage r3 matches ISS");
    TEST_ASSERT(p4.regFile.read(4) == expR4, "4-Stage r4 matches ISS");

    // 3. Run 6-Stage Pipeline
    stage6::Pipeline6Stage p6;
    p6.setProgramSize(progSize);
    p6.memory.loadProgram(0, words);
    p6.setForwarding(true);
    p6.run(100);

    TEST_ASSERT(p6.regFile.read(2) == expR2, "6-Stage r2 matches ISS");
    TEST_ASSERT(p6.regFile.read(3) == expR3, "6-Stage r3 matches ISS");
    TEST_ASSERT(p6.regFile.read(4) == expR4, "6-Stage r4 matches ISS");
}

// -------------------------------------------------------------
// Test 6: CPI Invariants, Scheduling, & Micro-kernels
// -------------------------------------------------------------
void test_cpi_invariants_and_microkernels() {
    std::cout << "[TEST] 6. CPI Invariants, Scheduling, & Micro-kernels...\n";

    // 1. Hazard-free independent ALU stream
    std::string noHazardAsm = R"(
.text
    mov r0, 1
    mov r1, 2
    mov r2, 3
    mov r3, 4
    mov r4, 5
    mov r5, 6
    mov r6, 7
    mov r7, 8
    nop
)";
    assembler::Assembler asmb;
    bool ok = asmb.assemble(noHazardAsm);
    TEST_ASSERT(ok, "Hazard-free assembly ok");
    auto words = asmb.getMachineWords();
    Word progSize = static_cast<Word>(words.size() * 4);

    stage4::Pipeline4Stage p4_hazardFree;
    p4_hazardFree.setProgramSize(progSize);
    p4_hazardFree.memory.loadProgram(0, words);
    p4_hazardFree.setMemPreset("ideal");
    p4_hazardFree.setForwarding(true);
    p4_hazardFree.run(100);

    TEST_ASSERT(p4_hazardFree.perfTracker.stallBreakdown.at("RAW_LOAD_USE") == 0, "Hazard-free has 0 RAW stalls");
    TEST_ASSERT(p4_hazardFree.perfTracker.branchBubbles == 0, "Hazard-free has 0 branch bubbles");
    TEST_ASSERT(p4_hazardFree.perfTracker.stallBreakdown.at("ICACHE_MISS") == 0, "Ideal preset has 0 ICACHE stalls");
    TEST_ASSERT(p4_hazardFree.perfTracker.stallBreakdown.at("DCACHE_MISS") == 0, "Ideal preset has 0 DCACHE stalls");
    TEST_ASSERT(p4_hazardFree.perfTracker.verifyInvariant(), "4-Stage cycle accounting invariant holds");

    // 2. Load-Use Hazard with and without Instruction Scheduling
    std::string loadUseAsm = R"(
.text
    mov r0, 0
    ld r1, 0[r0]
    add r2, r1, 10
    mov r3, 4
    ld r4, 0[r3]
    add r5, r4, 20
    mov r6, 30
    nop
)";
    // A: Without scheduling
    assembler::Assembler asmbUnscheduled;
    TEST_ASSERT(asmbUnscheduled.assemble(loadUseAsm, false), "Load-use unscheduled assembly ok");
    auto wordsUnscheduled = asmbUnscheduled.getMachineWords();

    stage4::Pipeline4Stage p4_unscheduled;
    p4_unscheduled.setProgramSize(static_cast<Word>(wordsUnscheduled.size() * 4));
    p4_unscheduled.memory.loadProgram(0, wordsUnscheduled);
    p4_unscheduled.setMemPreset("ideal");
    p4_unscheduled.setForwarding(true);
    p4_unscheduled.run(100);

    TEST_ASSERT(p4_unscheduled.perfTracker.stallBreakdown.at("RAW_LOAD_USE") == 2, "Unscheduled load-use incurs 2 stall bubbles");

    // B: With scheduling pass
    assembler::Assembler asmbScheduled;
    TEST_ASSERT(asmbScheduled.assemble(loadUseAsm, true), "Load-use scheduled assembly ok");
    auto wordsScheduled = asmbScheduled.getMachineWords();

    stage4::Pipeline4Stage p4_scheduled;
    p4_scheduled.setProgramSize(static_cast<Word>(wordsScheduled.size() * 4));
    p4_scheduled.memory.loadProgram(0, wordsScheduled);
    p4_scheduled.setMemPreset("ideal");
    p4_scheduled.setForwarding(true);
    p4_scheduled.run(100);

    TEST_ASSERT(p4_scheduled.perfTracker.stallBreakdown.at("RAW_LOAD_USE") == 0, "Scheduled load-use completely eliminates load-use bubbles (0 stalls)");
    TEST_ASSERT(p4_scheduled.regFile.read(2) == p4_unscheduled.regFile.read(2), "Scheduled r2 matches unscheduled");
    TEST_ASSERT(p4_scheduled.regFile.read(5) == p4_unscheduled.regFile.read(5), "Scheduled r5 matches unscheduled");
    TEST_ASSERT(p4_scheduled.perfTracker.verifyInvariant(), "Scheduled 4-Stage invariant holds");

    // 3. Tight Loop Branch Prediction Training
    std::string loopAsm = R"(
.text
    mov r0, 10
.loop:
    sub r0, r0, 1
    cmp r0, 0
    bgt .loop
    nop
)";
    assembler::Assembler asmbLoop;
    TEST_ASSERT(asmbLoop.assemble(loopAsm), "Loop assembly ok");
    auto wordsLoop = asmbLoop.getMachineWords();

    stage4::Pipeline4Stage p4_loop;
    p4_loop.setProgramSize(static_cast<Word>(wordsLoop.size() * 4));
    p4_loop.memory.loadProgram(0, wordsLoop);
    p4_loop.setMemPreset("ideal");
    p4_loop.setForwarding(true);
    p4_loop.setBranchPolicy("2bit");
    p4_loop.run(200);

    // 10 branch executions, bimodal predictor correctly predicts iterations 2-9 taken
    // Only iteration 10 loop exit mispredicts (and possibly iteration 1 if cold)
    TEST_ASSERT(p4_loop.perfTracker.branchBubbles <= 2, "Bimodal 2-bit predictor trains in tight loop, branch bubbles <= 2");
    TEST_ASSERT(p4_loop.regFile.read(0) == 0, "Loop terminates with r0 == 0");
    TEST_ASSERT(p4_loop.perfTracker.verifyInvariant(), "Loop 4-Stage invariant holds");

    // 4. 6-Stage Pipeline Invariant Check
    stage6::Pipeline6Stage p6_loop;
    p6_loop.setProgramSize(static_cast<Word>(wordsLoop.size() * 4));
    p6_loop.memory.loadProgram(0, wordsLoop);
    p6_loop.setMemPreset("ideal");
    p6_loop.setForwarding(true);
    p6_loop.setBranchPolicy("2bit");
    p6_loop.run(200);

    TEST_ASSERT(p6_loop.perfTracker.branchBubbles <= 4, "6-Stage tight loop branch bubbles <= 4 (2 bubbles per mispredict)");
    TEST_ASSERT(p6_loop.perfTracker.verifyInvariant(), "Loop 6-Stage invariant holds");
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "           RISC201 C++ EXHAUSTIVE VERIFICATION TEST SUITE               \n";
    std::cout << "========================================================================\n\n";

    test_isa_binary_strings();
    test_cache_and_dram();
    test_virtual_memory();
    test_hazards_and_forwarding();
    test_differential_iss();
    test_cpi_invariants_and_microkernels();

    std::cout << "\n========================================================================\n";
    std::cout << "TEST RESULTS: " << testsPassed << " / " << testsRun << " PASSED\n";
    std::cout << "ALL TESTS PASSED WITH 100% SUCCESS!\n";
    std::cout << "========================================================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}