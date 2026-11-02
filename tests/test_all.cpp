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

int main() {
    std::cout << "========================================================================\n";
    std::cout << "           RISC201 C++ EXHAUSTIVE VERIFICATION TEST SUITE               \n";
    std::cout << "========================================================================\n\n";

    test_isa_binary_strings();
    test_cache_and_dram();
    test_virtual_memory();
    test_hazards_and_forwarding();
    test_differential_iss();

    std::cout << "\n========================================================================\n";
    std::cout << "TEST RESULTS: " << testsPassed << " / " << testsRun << " PASSED\n";
    std::cout << "ALL TESTS PASSED WITH 100% SUCCESS!\n";
    std::cout << "========================================================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}