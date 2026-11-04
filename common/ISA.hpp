#ifndef RISC201_COMMON_ISA_HPP
#define RISC201_COMMON_ISA_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace risc201 {

// Width Constants
constexpr size_t WORD_BITS       = 32;
constexpr size_t OPCODE_BITS     = 5;
constexpr size_t REG_BITS        = 4;
constexpr size_t MOD_BITS        = 2;
constexpr size_t IMM16_BITS      = 16;
constexpr size_t BRANCH_OFF_BITS = 27;

// Special Architectural Register IDs and Bit Strings
constexpr uint8_t REG_ID_ZERO = 0;
constexpr uint8_t REG_ID_SP   = 14;
constexpr uint8_t REG_ID_RA   = 15;

static const std::string REG_ZERO_BITS = "0000";
static const std::string REG_SP_BITS   = "1110"; // r14
static const std::string REG_RA_BITS   = "1111"; // r15

// =========================================================================
// SimpleRisc Opcode Definitions (5 bits, 0-31)
// As specified in Sarangi Chapter 3 & 9, Table 3.10 / 9.2
// Canonical 5-Bit Binary-String Opcodes
// =========================================================================
namespace isa {
inline const std::string OP_ADD     = "00000"; // 0:  add  rd, rs1, (rs2/imm)
inline const std::string OP_SUB     = "00001"; // 1:  sub  rd, rs1, (rs2/imm)
inline const std::string OP_MUL     = "00010"; // 2:  mul  rd, rs1, (rs2/imm)
inline const std::string OP_DIV     = "00011"; // 3:  div  rd, rs1, (rs2/imm)
inline const std::string OP_MOD     = "00100"; // 4:  mod  rd, rs1, (rs2/imm)
inline const std::string OP_CMP     = "00101"; // 5:  cmp  rs1, (rs2/imm)
inline const std::string OP_AND     = "00110"; // 6:  and  rd, rs1, (rs2/imm)
inline const std::string OP_OR      = "00111"; // 7:  or   rd, rs1, (rs2/imm)
inline const std::string OP_NOT     = "01000"; // 8:  not  rd, (rs2/imm)
inline const std::string OP_MOV     = "01001"; // 9:  mov  rd, (rs2/imm)
inline const std::string OP_LSL     = "01010"; // 10: lsl  rd, rs1, (rs2/imm)
inline const std::string OP_LSR     = "01011"; // 11: lsr  rd, rs1, (rs2/imm)
inline const std::string OP_ASR     = "01100"; // 12: asr  rd, rs1, (rs2/imm)
inline const std::string OP_NOP     = "01101"; // 13: nop
inline const std::string OP_LD      = "01110"; // 14: ld   rd, imm[rs1]
inline const std::string OP_ST      = "01111"; // 15: st   rd, imm[rs1]
inline const std::string OP_BEQ     = "10000"; // 16: beq  offset
inline const std::string OP_BGT     = "10001"; // 17: bgt  offset
inline const std::string OP_B       = "10010"; // 18: b    offset
inline const std::string OP_CALL    = "10011"; // 19: call offset
inline const std::string OP_RET     = "10100"; // 20: ret  (PC <- ra)
inline const std::string OP_MOVZ    = "10101"; // 21: movz (transfer privileged <-> general)
inline const std::string OP_RETZ    = "10110"; // 22: retz (PC <- oldPC, CPL <- 1)
inline const std::string OP_MIN     = "10111"; // 23: min  rd, rs1, (rs2/imm)
inline const std::string OP_MAX     = "11000"; // 24: max  rd, rs1, (rs2/imm)
inline const std::string OP_ROTS    = "11001"; // 25: rots rd, rs1, (rs2/imm)
inline const std::string OP_XOR     = "11010"; // 26: xor  rd, rs1, (rs2/imm)
inline const std::string OP_ABS     = "11011"; // 27: abs  rd, (rs2/imm)
inline const std::string OP_INVALID = "11111"; // 31: illegal / invalid opcode
inline const std::string OP_BUBBLE  = OP_NOP;
} // namespace isa

inline const std::string& BIN_OP_ADD = isa::OP_ADD;
inline const std::string& BIN_OP_SUB = isa::OP_SUB;
inline const std::string& BIN_OP_MUL = isa::OP_MUL;
inline const std::string& BIN_OP_DIV = isa::OP_DIV;
inline const std::string& BIN_OP_MOD = isa::OP_MOD;
inline const std::string& BIN_OP_CMP = isa::OP_CMP;
inline const std::string& BIN_OP_AND = isa::OP_AND;
inline const std::string& BIN_OP_OR  = isa::OP_OR;
inline const std::string& BIN_OP_NOT = isa::OP_NOT;
inline const std::string& BIN_OP_MOV = isa::OP_MOV;
inline const std::string& BIN_OP_LSL = isa::OP_LSL;
inline const std::string& BIN_OP_LSR = isa::OP_LSR;
inline const std::string& BIN_OP_ASR = isa::OP_ASR;
inline const std::string& BIN_OP_NOP = isa::OP_NOP;
inline const std::string& BIN_OP_LD  = isa::OP_LD;
inline const std::string& BIN_OP_ST  = isa::OP_ST;
inline const std::string& BIN_OP_BEQ = isa::OP_BEQ;
inline const std::string& BIN_OP_BGT = isa::OP_BGT;
inline const std::string& BIN_OP_B   = isa::OP_B;
inline const std::string& BIN_OP_CALL= isa::OP_CALL;
inline const std::string& BIN_OP_RET = isa::OP_RET;
inline const std::string& BIN_OP_MOVZ= isa::OP_MOVZ;
inline const std::string& BIN_OP_RETZ= isa::OP_RETZ;
inline const std::string& BIN_OP_MIN = isa::OP_MIN;
inline const std::string& BIN_OP_MAX = isa::OP_MAX;
inline const std::string& BIN_OP_ROTS= isa::OP_ROTS;
inline const std::string& BIN_OP_XOR = isa::OP_XOR;
inline const std::string& BIN_OP_ABS = isa::OP_ABS;
inline const std::string& BIN_OP_INVALID = isa::OP_INVALID;

// Bit-String Conversion & Slicing Helper Utilities
inline std::string intToBits(uint64_t value, size_t width) {
    std::string s(width, '0');
    for (size_t i = 0; i < width; ++i) {
        if ((value >> (width - 1 - i)) & 1ULL) {
            s[i] = '1';
        }
    }
    return s;
}

inline uint64_t bitsToInt(const std::string& bitstr, bool isSigned = false) {
    if (bitstr.empty()) return 0;
    for (char c : bitstr) {
        if (c != '0' && c != '1') {
            throw std::invalid_argument("Binary string must contain only '0' and '1': " + bitstr);
        }
    }
    uint64_t val = 0;
    for (char c : bitstr) {
        val = (val << 1) | (c == '1' ? 1ULL : 0ULL);
    }
    if (isSigned && bitstr[0] == '1') {
        size_t w = bitstr.length();
        if (w < 64) {
            uint64_t signExtendMask = (~0ULL) << w;
            val |= signExtendMask;
        }
    }
    return val;
}

inline std::string wordToBits(uint32_t word, size_t width = 32) {
    return intToBits(word, width);
}

inline uint32_t bitsToWord(const std::string& bitstr) {
    return static_cast<uint32_t>(bitsToInt(bitstr, false));
}

inline std::string sliceField(const std::string& wordBits, int hi, int lo) {
    if (wordBits.length() != 32) {
        throw std::invalid_argument("wordBits must be exactly 32 characters long");
    }
    if (hi < lo || hi >= 32 || lo < 0) {
        throw std::out_of_range("Invalid bit indices for slicing: hi=" + std::to_string(hi) + ", lo=" + std::to_string(lo));
    }
    // In MSB-first [31:0], bit 31 is index 0, bit 0 is index 31
    size_t startIdx = 31 - hi;
    size_t length = hi - lo + 1;
    return wordBits.substr(startIdx, length);
}

inline void validateOpcodeBits(const std::string& opBits) {
    if (opBits.length() != OPCODE_BITS) {
        throw std::invalid_argument("Opcode bits must be exactly 5 bits: " + opBits);
    }
    for (char c : opBits) {
        if (c != '0' && c != '1') {
            throw std::invalid_argument("Opcode contains invalid character: " + opBits);
        }
    }
}

// Architectural Instruction Metadata Structure
struct InstructionInfo {
    std::string mnemonic;
    std::string opcode;
    std::string format; // "R", "I", "B", "PRIV"
    std::string operandSyntax;
    bool readsRs1{false};
    bool readsRs2{false};
    bool writesRd{false};
    bool usesImm{false};
    bool isLoad{false};
    bool isStore{false};
    bool isBranch{false};
    bool isJump{false};
    bool isCall{false};
    bool isReturn{false};
    bool isAlu{false};
    std::string aluOp{"00000"};
    std::string aluName{"NONE"};
    uint32_t execLatency{1};
    std::string functionalUnit{"ALU"};
    bool canRaiseException{false};
    std::string earliestResultStage{"EX"};
    bool isPrivileged{false};
};

// Global Instruction Metadata Lookup Table
inline const std::unordered_map<std::string, InstructionInfo>& getInstructionTable() {
    static const std::unordered_map<std::string, InstructionInfo> table = {
        {isa::OP_ADD,  {"add",  isa::OP_ADD,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00000", "ADD",    1, "ALU",    false, "EX", false}},
        {isa::OP_SUB,  {"sub",  isa::OP_SUB,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00001", "SUB",    1, "ALU",    false, "EX", false}},
        {isa::OP_MUL,  {"mul",  isa::OP_MUL,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00010", "MUL",    2, "ALU",    false, "EX", false}},
        {isa::OP_DIV,  {"div",  isa::OP_DIV,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00011", "DIV",    4, "ALU",    true,  "EX", false}},
        {isa::OP_MOD,  {"mod",  isa::OP_MOD,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00100", "MOD",    4, "ALU",    true,  "EX", false}},
        {isa::OP_CMP,  {"cmp",  isa::OP_CMP,  "R", "rs1, (rs2/imm)",     true,  true,  false, true,  false, false, false, false, false, false, true,  "00101", "CMP",    1, "ALU",    false, "EX", false}},
        {isa::OP_AND,  {"and",  isa::OP_AND,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00110", "AND",    1, "ALU",    false, "EX", false}},
        {isa::OP_OR,   {"or",   isa::OP_OR,   "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "00111", "OR",     1, "ALU",    false, "EX", false}},
        {isa::OP_NOT,  {"not",  isa::OP_NOT,  "R", "rd, (rs2/imm)",      false, true,  true,  true,  false, false, false, false, false, false, true,  "01000", "NOT",    1, "ALU",    false, "EX", false}},
        {isa::OP_MOV,  {"mov",  isa::OP_MOV,  "R", "rd, (rs2/imm)",      false, true,  true,  true,  false, false, false, false, false, false, true,  "01001", "MOV",    1, "ALU",    false, "EX", false}},
        {isa::OP_LSL,  {"lsl",  isa::OP_LSL,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "01010", "LSL",    1, "ALU",    false, "EX", false}},
        {isa::OP_LSR,  {"lsr",  isa::OP_LSR,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "01011", "LSR",    1, "ALU",    false, "EX", false}},
        {isa::OP_ASR,  {"asr",  isa::OP_ASR,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "01100", "ASR",    1, "ALU",    false, "EX", false}},
        {isa::OP_NOP,  {"nop",  isa::OP_NOP,  "B", "",                   false, false, false, false, false, false, false, false, false, false, false, "11111", "NOP",    1, "ALU",    false, "EX", false}},
        {isa::OP_LD,   {"ld",   isa::OP_LD,   "I", "rd, imm[rs1]",       true,  false, true,  true,  true,  false, false, false, false, false, false, "00000", "LOAD",   1, "LSU",    true,  "MEM", false}},
        {isa::OP_ST,   {"st",   isa::OP_ST,   "I", "rd, imm[rs1]",       true,  false, false, true,  false, true,  false, false, false, false, false, "00000", "STORE",  1, "LSU",    true,  "MEM", false}},
        {isa::OP_BEQ,  {"beq",  isa::OP_BEQ,  "B", "offset",             false, false, false, false, false, false, true,  false, false, false, false, "11111", "BRANCH", 1, "BRANCH", false, "EX", false}},
        {isa::OP_BGT,  {"bgt",  isa::OP_BGT,  "B", "offset",             false, false, false, false, false, false, true,  false, false, false, false, "11111", "BRANCH", 1, "BRANCH", false, "EX", false}},
        {isa::OP_B,    {"b",    isa::OP_B,    "B", "offset",             false, false, false, false, false, false, true,  true,  false, false, false, "11111", "BRANCH", 1, "BRANCH", false, "EX", false}},
        {isa::OP_CALL, {"call", isa::OP_CALL, "B", "offset",             false, false, true,  false, false, false, true,  true,  true,  false, false, "11111", "CALL",   1, "BRANCH", false, "EX", false}},
        {isa::OP_RET,  {"ret",  isa::OP_RET,  "R", "",                   true,  false, false, false, false, false, true,  true,  false, true,  false, "11111", "RET",    1, "BRANCH", false, "EX", false}},
        {isa::OP_MOVZ, {"movz", isa::OP_MOVZ, "PRIV", "rd, priv_reg",   false, false, true,  false, false, false, false, false, false, false, false, "11111", "PRIV",   1, "ALU",    true,  "EX", true}},
        {isa::OP_RETZ, {"retz", isa::OP_RETZ, "PRIV", "",                false, false, false, false, false, false, true,  true,  false, true,  false, "11111", "PRIV",   1, "BRANCH", true,  "EX", true}},
        {isa::OP_MIN,  {"min",  isa::OP_MIN,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "10111", "MIN",    1, "ALU",    false, "EX", false}},
        {isa::OP_MAX,  {"max",  isa::OP_MAX,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "11000", "MAX",    1, "ALU",    false, "EX", false}},
        {isa::OP_ROTS, {"rots", isa::OP_ROTS, "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "11001", "ROTS",   1, "ALU",    false, "EX", false}},
        {isa::OP_XOR,  {"xor",  isa::OP_XOR,  "R", "rd, rs1, (rs2/imm)", true,  true,  true,  true,  false, false, false, false, false, false, true,  "11010", "XOR",    1, "ALU",    false, "EX", false}},
        {isa::OP_ABS,  {"abs",  isa::OP_ABS,  "R", "rd, (rs2/imm)",      false, true,  true,  true,  false, false, false, false, false, false, true,  "11011", "ABS",    1, "ALU",    false, "EX", false}},
    };
    return table;
}

// Helpers for validation, mnemonic lookup, and word-boundary bit conversion
inline bool isValidOpcode(const std::string& bits) {
    if (bits.length() != OPCODE_BITS) return false;
    const auto& tbl = getInstructionTable();
    return tbl.find(bits) != tbl.end();
}

inline const std::unordered_map<std::string, std::string>& getMnemonicToOpcodeTable() {
    static const auto table = []() {
        std::unordered_map<std::string, std::string> m;
        for (const auto& kv : getInstructionTable()) {
            m[kv.second.mnemonic] = kv.first;
        }
        return m;
    }();
    return table;
}

inline std::string mnemonicOf(const std::string& bits) {
    const auto& tbl = getInstructionTable();
    auto it = tbl.find(bits);
    if (it != tbl.end()) return it->second.mnemonic;
    return "unknown";
}

inline std::string opcodeOf(const std::string& mnemonic) {
    const auto& tbl = getMnemonicToOpcodeTable();
    auto it = tbl.find(mnemonic);
    if (it != tbl.end()) return it->second;
    return isa::OP_INVALID;
}

inline const std::string& wordToOpcode(uint32_t word) {
    static const std::string opcodeStrings[32] = {
        "00000", "00001", "00010", "00011",
        "00100", "00101", "00110", "00111",
        "01000", "01001", "01010", "01011",
        "01100", "01101", "01110", "01111",
        "10000", "10001", "10010", "10011",
        "10100", "10101", "10110", "10111",
        "11000", "11001", "11010", "11011",
        "11100", "11101", "11110", "11111"
    };
    uint32_t opVal = (word >> 27) & 0x1FU;
    return opcodeStrings[opVal];
}

inline uint32_t opcodeToWordBits(const std::string& op) {
    validateOpcodeBits(op);
    uint32_t val = static_cast<uint32_t>(bitsToInt(op, false));
    return (val & 0x1FU) << 27;
}

inline bool verifyIsaTable() {
    const auto& tbl = getInstructionTable();
    if (tbl.size() != 28) return false;
    for (const auto& kv : tbl) {
        if (kv.first.length() != 5) return false;
        if (kv.first != kv.second.opcode) return false;
        for (char c : kv.first) {
            if (c != '0' && c != '1') return false;
        }
    }
    // Verify unassigned patterns 28, 29, 30 and 31 (OP_INVALID) are illegal
    if (tbl.find("11100") != tbl.end()) return false;
    if (tbl.find("11101") != tbl.end()) return false;
    if (tbl.find("11110") != tbl.end()) return false;
    if (tbl.find("11111") != tbl.end()) return false;
    return true;
}

namespace isa {
    using risc201::InstructionInfo;
    using risc201::getInstructionTable;
    using risc201::isValidOpcode;
    using risc201::getMnemonicToOpcodeTable;
    using risc201::mnemonicOf;
    using risc201::opcodeOf;
    using risc201::wordToOpcode;
    using risc201::opcodeToWordBits;
    using risc201::verifyIsaTable;
} // namespace isa

inline uint64_t getNextInstId() {
    static uint64_t counter = 0;
    return ++counter;
}

// Typed Decoded Instruction Object
class Instruction {
public:
    uint64_t instId{0};
    uint32_t rawWord{0x68000000};
    std::string opcode{isa::OP_NOP};
    std::string mnemonic{"nop"};
    std::string format{"B"};
    bool isImmediate{false};
    uint8_t rd{0};
    std::string rdBits{"0000"};
    uint8_t rs1{0};
    std::string rs1Bits{"0000"};
    uint8_t rs2{0};
    std::string rs2Bits{"0000"};
    uint8_t immModifier{0};
    std::string immModifierBits{"00"};
    int16_t imm16{0};
    uint32_t immx{0};
    int32_t branchOffset{0};
    uint32_t branchTarget{0};
    InstructionInfo info{};
    bool isBubble{false};
    std::string disassembly{"nop"};
    bool hasTrap{false};
    uint32_t trapType{0};
    std::string trapDetails{""};

    Instruction() {
        auto& tbl = getInstructionTable();
        auto it = tbl.find(isa::OP_NOP);
        if (it != tbl.end()) info = it->second;
        instId = getNextInstId();
    }

    explicit Instruction(bool bubble) : isBubble(bubble) {
        auto& tbl = getInstructionTable();
        auto it = tbl.find(isa::OP_NOP);
        if (it != tbl.end()) info = it->second;
        instId = getNextInstId();
        if (bubble) disassembly = "bubble";
    }

    bool isNop() const { return opcode == isa::OP_NOP || isBubble; }
    bool isBranch() const { return info.isBranch; }
    bool isJump() const { return info.isJump; }
    bool isConditionalBranch() const { return opcode == isa::OP_BEQ || opcode == isa::OP_BGT; }
    bool isCall() const { return info.isCall; }
    bool isRet() const { return info.isReturn; }
    bool isLoad() const { return info.isLoad; }
    bool isStore() const { return info.isStore; }
    bool isAlu() const { return info.isAlu; }
    bool writesRegister() const { return !isBubble && info.writesRd; }
    uint8_t getDestReg() const {
        if (opcode == isa::OP_CALL) return REG_ID_RA;
        return rd;
    }
    std::string getDestRegBits() const {
        if (opcode == isa::OP_CALL) return REG_RA_BITS;
        return rdBits;
    }
    bool readsRs1() const { return !isBubble && info.readsRs1; }
    bool readsRs2() const { return !isBubble && !isImmediate && info.readsRs2; }
};

class IllegalInstructionException : public std::runtime_error {
public:
    std::string opcodeBits;
    uint32_t word;
    IllegalInstructionException(const std::string& op, uint32_t w)
        : std::runtime_error("IllegalInstructionException: Opcode '" + op + "' not recognized. Word: 0x" + intToBits(w, 32)),
          opcodeBits(op), word(w) {}
};

inline Instruction decode(uint32_t word, uint32_t pc = 0) {
    using namespace isa;
    std::string wordBits = wordToBits(word, WORD_BITS);
    std::string opBits   = sliceField(wordBits, 31, 27);
    std::string iBit     = sliceField(wordBits, 26, 26);
    std::string rdBits   = sliceField(wordBits, 25, 22);
    std::string rs1Bits  = sliceField(wordBits, 21, 18);
    std::string rs2Bits  = sliceField(wordBits, 17, 14);
    std::string modBits  = sliceField(wordBits, 17, 16);
    std::string immBits  = sliceField(wordBits, 15, 0);
    std::string offBits  = sliceField(wordBits, 26, 0);

    validateOpcodeBits(opBits);
    const auto& tbl = getInstructionTable();
    auto it = tbl.find(opBits);
    if (it == tbl.end()) {
        throw IllegalInstructionException(opBits, word);
    }

    Instruction inst;
    inst.rawWord = word;
    inst.opcode = opBits;
    inst.info = it->second;
    inst.mnemonic = inst.info.mnemonic;
    inst.format = inst.info.format;
    inst.isImmediate = (iBit == "1");
    inst.rd = static_cast<uint8_t>(bitsToInt(rdBits, false));
    inst.rdBits = rdBits;
    inst.rs1 = static_cast<uint8_t>(bitsToInt(rs1Bits, false));
    inst.rs1Bits = rs1Bits;
    inst.rs2 = static_cast<uint8_t>(bitsToInt(rs2Bits, false));
    inst.rs2Bits = rs2Bits;
    inst.immModifier = static_cast<uint8_t>(bitsToInt(modBits, false));
    inst.immModifierBits = modBits;
    inst.imm16 = static_cast<int16_t>(bitsToInt(immBits, true));

    // Immediate expansion
    if (inst.immModifier == 1) {        // unsigned
        inst.immx = static_cast<uint32_t>(bitsToInt(immBits, false));
    } else if (inst.immModifier == 2) { // high
        inst.immx = static_cast<uint32_t>(bitsToInt(immBits, false) << 16);
    } else {                            // default signed
        inst.immx = static_cast<uint32_t>(inst.imm16);
    }

    // Branch target calculation
    inst.branchOffset = static_cast<int32_t>(bitsToInt(offBits, true));
    inst.branchTarget = pc + (static_cast<uint32_t>(inst.branchOffset) << 2);

    if (opBits == isa::OP_RET) {
        inst.rs1 = REG_ID_RA;
        inst.rs1Bits = REG_RA_BITS;
    }

    // Build disassembly string
    std::ostringstream oss;
    if (opBits == isa::OP_NOP) {
        oss << "nop";
    } else if (opBits == isa::OP_RET) {
        oss << "ret";
    } else if (opBits == isa::OP_RETZ) {
        oss << "retz";
    } else if (inst.info.isBranch) {
        oss << inst.info.mnemonic << " 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << inst.branchTarget;
    } else if (opBits == isa::OP_MOV || opBits == isa::OP_NOT || opBits == isa::OP_ABS) {
        oss << inst.info.mnemonic << " r" << (int)inst.rd << ", ";
        if (inst.isImmediate) oss << "#" << std::dec << inst.imm16;
        else oss << "r" << (int)inst.rs2;
    } else if (opBits == isa::OP_CMP) {
        oss << "cmp r" << (int)inst.rs1 << ", ";
        if (inst.isImmediate) oss << "#" << std::dec << inst.imm16;
        else oss << "r" << (int)inst.rs2;
    } else if (opBits == isa::OP_LD) {
        oss << "ld r" << (int)inst.rd << ", " << std::dec << inst.imm16 << "[r" << (int)inst.rs1 << "]";
    } else if (opBits == isa::OP_ST) {
        oss << "st r" << (int)inst.rd << ", " << std::dec << inst.imm16 << "[r" << (int)inst.rs1 << "]";
    } else {
        oss << inst.info.mnemonic << " r" << (int)inst.rd << ", r" << (int)inst.rs1 << ", ";
        if (inst.isImmediate) oss << "#" << std::dec << inst.imm16;
        else oss << "r" << (int)inst.rs2;
    }
    inst.disassembly = oss.str();
    return inst;
}

inline std::string disassembleInstruction(uint32_t word, uint32_t pc = 0) {
    try {
        return decode(word, pc).disassembly;
    } catch (...) {
        std::ostringstream oss;
        oss << "<illegal 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << word << ">";
        return oss.str();
    }
}

inline std::string printIsaTable() {
    using namespace isa;
    std::ostringstream oss;
    oss << "==================================================================================================\n";
    oss << "                               RISC201 / SimpleRisc ARCHITECTURAL ISA TABLE                      \n";
    oss << "==================================================================================================\n";
    oss << "Mnemonic   Opcode   Format   Syntax                   ALU Op   Latency  Stage  Exception?\n";
    oss << "--------------------------------------------------------------------------------------------------\n";

    std::vector<std::string> orderedOps = {
        isa::OP_ADD, isa::OP_SUB, isa::OP_MUL, isa::OP_DIV, isa::OP_MOD, isa::OP_CMP, isa::OP_AND, isa::OP_OR, isa::OP_NOT, isa::OP_MOV,
        isa::OP_LSL, isa::OP_LSR, isa::OP_ASR, isa::OP_NOP, isa::OP_LD,  isa::OP_ST,  isa::OP_BEQ, isa::OP_BGT, isa::OP_B,   isa::OP_CALL,
        isa::OP_RET, isa::OP_MOVZ, isa::OP_RETZ, isa::OP_MIN, isa::OP_MAX, isa::OP_ROTS, isa::OP_XOR, isa::OP_ABS
    };

    const auto& tbl = getInstructionTable();
    for (const auto& op : orderedOps) {
        auto it = tbl.find(op);
        if (it != tbl.end()) {
            const auto& info = it->second;
            oss << std::left << std::setw(11) << info.mnemonic
                << std::setw(9)  << info.opcode
                << std::setw(9)  << info.format
                << std::setw(25) << info.operandSyntax
                << std::setw(9)  << info.aluOp
                << std::setw(9)  << info.execLatency
                << std::setw(7)  << info.earliestResultStage
                << (info.canRaiseException ? "YES" : "NO") << "\n";
        }
    }
    oss << "==================================================================================================\n";
    return oss.str();
}

} // namespace risc201

#endif // RISC201_COMMON_ISA_HPP
