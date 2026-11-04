#ifndef RISC201_ASSEMBLER_HPP
#define RISC201_ASSEMBLER_HPP

#include "../common/Types.hpp"
#include "MacroPreprocessor.hpp"
#include "Disassembler.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <stdexcept>
#include <algorithm>

namespace risc201 {
namespace assembler {

struct AssembledLine {
    Word address{0};
    Word machineCode{0};
    std::string sourceLine;
    std::string hexString;
    std::string binaryString;
};

/**
 * Two-Pass Assembler for the RISC201 / SimpleRisc ISA.
 * Conforms strictly to Sarangi Chapter 3 & 9 encoding rules.
 *
 * Pass 1:
 *   - Preprocesses macros (push, pop, pushm, popm)
 *   - Strips comments (@ ..., block comments, ;)
 *   - Collects label addresses into the Symbol Table
 *   - Calculates instruction memory offsets (4 bytes per instruction)
 *
 * Pass 2:
 *   - Encodes opcodes, operands, register specifiers, and immediate modifiers
 *   - Computes PC-relative word branch offsets: (target - PC) / 4
 *   - Emits 32-bit machine code, hexadecimal, and binary bit-strings
 */
class Assembler {
public:
    std::map<std::string, Word> symbolTable;
    std::vector<AssembledLine> assembledLines;

private:
    // Strip inline and block comments from text
    static std::string stripComments(const std::string& input) {
        std::string result;
        bool inBlockComment = false;
        size_t i = 0;
        while (i < input.size()) {
            if (!inBlockComment && i + 1 < input.size() && input[i] == '/' && input[i + 1] == '*') {
                inBlockComment = true;
                i += 2;
            } else if (inBlockComment && i + 1 < input.size() && input[i] == '*' && input[i + 1] == '/') {
                inBlockComment = false;
                i += 2;
            } else if (!inBlockComment && (input[i] == ';' || input[i] == '@')) {
                // Remainder of line is a comment
                break;
            } else if (!inBlockComment) {
                result += input[i];
                i++;
            } else {
                i++;
            }
        }
        return MacroPreprocessor::trim(result);
    }

    // Parse register name ("r0".."r15", "sp", "ra") to RegId (0..15)
    static RegId parseRegister(const std::string& raw) {
        std::string s = MacroPreprocessor::trim(raw);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        if (s == "sp") return REG_SP;
        if (s == "ra") return REG_RA;
        if (s.size() >= 2 && s[0] == 'r') {
            int num = std::stoi(s.substr(1));
            if (num >= 0 && num < 16) return static_cast<RegId>(num);
        }
        throw std::runtime_error("Invalid register: " + raw);
    }

    // Parse immediate operand: e.g. "10", "-4", "0xFA", "#12"
    static int32_t parseImmediate(const std::string& raw) {
        std::string s = MacroPreprocessor::trim(raw);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        s = MacroPreprocessor::trim(s);
        if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
            return static_cast<int32_t>(std::stoul(s, nullptr, 16));
        }
        return std::stoi(s);
    }

    // Split token by delimiters (comma, space)
    static std::vector<std::string> tokenize(const std::string& str) {
        std::vector<std::string> tokens;
        std::string current;
        for (char c : str) {
            if (c == ',' || std::isspace(c)) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) tokens.push_back(current);
        return tokens;
    }

public:
    bool assemble(const std::string& sourceCode, bool enableScheduling = false) {
        symbolTable.clear();
        assembledLines.clear();

        // Step 1: Stack preprocessor macro expansion
        std::string preprocessed = MacroPreprocessor::preprocess(sourceCode);

        // --- PASS 1: Symbol Table & Address Assignment ---
        std::stringstream ss(preprocessed);
        std::string line;
        Word currentPC = 0;
        std::vector<std::pair<Word, std::string>> intermediateLines;

        while (std::getline(ss, line)) {
            std::string cleaned = stripComments(line);
            if (cleaned.empty()) continue;

            // Directives check
            if (cleaned[0] == '.') {
                if (cleaned.rfind(".text", 0) == 0 || cleaned.rfind(".data", 0) == 0 ||
                    cleaned.rfind(".file", 0) == 0) {
                    continue;
                }
            }

            // Check for labels (e.g. "label:" or ".loop:")
            size_t colonPos = cleaned.find(':');
            if (colonPos != std::string::npos) {
                std::string label = MacroPreprocessor::trim(cleaned.substr(0, colonPos));
                symbolTable[label] = currentPC;
                cleaned = MacroPreprocessor::trim(cleaned.substr(colonPos + 1));
                if (cleaned.empty()) continue;
            }

            // Word directive in data: .word 17
            if (cleaned.rfind(".word", 0) == 0) {
                std::string valStr = MacroPreprocessor::trim(cleaned.substr(5));
                intermediateLines.push_back({currentPC, cleaned});
                currentPC += 4;
                continue;
            }

            intermediateLines.push_back({currentPC, cleaned});
            currentPC += 4;
        }

        // --- PASS 2: Instruction Encoding ---
        for (const auto& item : intermediateLines) {
            Word pc = item.first;
            const std::string& text = item.second;

            // Data directive: .word value
            if (text.rfind(".word", 0) == 0) {
                std::string valStr = MacroPreprocessor::trim(text.substr(5));
                Word dataVal = static_cast<Word>(parseImmediate(valStr));

                AssembledLine al;
                al.address = pc;
                al.machineCode = dataVal;
                al.sourceLine = text;
                al.hexString = toHex(dataVal);
                al.binaryString = std::bitset<32>(dataVal).to_string();
                assembledLines.push_back(al);
                continue;
            }

            auto tokens = tokenize(text);
            if (tokens.empty()) continue;

            std::string mnemonic = tokens[0];
            std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);
            std::string baseMnemonic = mnemonic;
            ImmModifier modifier = ImmModifier::DEFAULT;

            // Check for modifier suffix: 'u' (unsigned) or 'h' (high)
            // e.g. movh, addu, subu, etc. (Sarangi Section 3.3.13)
            if (mnemonic.size() > 2 && mnemonic != "push") {
                char lastChar = mnemonic.back();
                if (lastChar == 'u') {
                    modifier = ImmModifier::UNSIGNED;
                    baseMnemonic = mnemonic.substr(0, mnemonic.size() - 1);
                } else if (lastChar == 'h') {
                    modifier = ImmModifier::HIGH;
                    baseMnemonic = mnemonic.substr(0, mnemonic.size() - 1);
                }
            }
            Word code = 0;

            // Helper to encode register format (Figure 3.14):
            // [31:27] opcode, [26] I=0, [25:22] rd, [21:18] rs1, [17:14] rs2
            auto encodeReg = [](Opcode op, RegId rd, RegId rs1, RegId rs2) -> Word {
                Word w = 0;
                w |= (static_cast<Word>(op) & 0x1F) << 27;
                w |= (0u) << 26; // I = 0
                w |= (static_cast<Word>(rd) & 0xF) << 22;
                w |= (static_cast<Word>(rs1) & 0xF) << 18;
                w |= (static_cast<Word>(rs2) & 0xF) << 14;
                return w;
            };

            // Helper to encode immediate format (Figure 3.15):
            // [31:27] opcode, [26] I=1, [25:22] rd, [21:18] rs1, [17:16] mod, [15:0] imm16
            auto encodeImm = [](Opcode op, RegId rd, RegId rs1, ImmModifier mod, int16_t imm) -> Word {
                Word w = 0;
                w |= (static_cast<Word>(op) & 0x1F) << 27;
                w |= (1u) << 26; // I = 1
                w |= (static_cast<Word>(rd) & 0xF) << 22;
                w |= (static_cast<Word>(rs1) & 0xF) << 18;
                w |= (static_cast<Word>(mod) & 0x3) << 16;
                w |= (static_cast<Word>(static_cast<uint16_t>(imm)));
                return w;
            };

            // Helper to encode branch format (Figure 3.13):
            // [31:27] opcode, [26:0] offset27
            auto encodeBranch = [](Opcode op, int32_t wordOffset) -> Word {
                Word w = 0;
                w |= (static_cast<Word>(op) & 0x1F) << 27;
                w |= (static_cast<Word>(wordOffset) & 0x07FFFFFF);
                return w;
            };

            // 1. NOP
            if (baseMnemonic == "nop") {
                code = encodeBranch(OP_NOP, 0);
            }
            // 2. RET
            else if (baseMnemonic == "ret") {
                code = encodeBranch(OP_RET, 0);
            }
            // 3. RETZ (privileged return)
            else if (baseMnemonic == "retz") {
                code = encodeBranch(OP_RETZ, 0);
            }
            // 4. Branch instructions: b, beq, bgt, call
            else if (baseMnemonic == "b" || baseMnemonic == "beq" ||
                     baseMnemonic == "bgt" || baseMnemonic == "call") {
                if (tokens.size() < 2) throw std::runtime_error("Branch missing target: " + text);
                std::string targetLabel = tokens[1];
                if (symbolTable.find(targetLabel) == symbolTable.end()) {
                    throw std::runtime_error("Undefined label: " + targetLabel);
                }
                Word targetPC = symbolTable[targetLabel];
                // Word offset = (targetPC - currentPC) / 4 (Sarangi Section 3.3.14)
                int32_t byteOffset = static_cast<int32_t>(targetPC) - static_cast<int32_t>(pc);
                int32_t wordOffset = byteOffset >> 2;

                Opcode op = OP_B;
                if (baseMnemonic == "beq") op = OP_BEQ;
                else if (baseMnemonic == "bgt") op = OP_BGT;
                else if (baseMnemonic == "call") op = OP_CALL;

                code = encodeBranch(op, wordOffset);
            }
            // 5. Load / Store: ld rd, imm[rs1] | st rd, imm[rs1]
            else if (baseMnemonic == "ld" || baseMnemonic == "st") {
                if (tokens.size() < 3) throw std::runtime_error("Malformed load/store: " + text);
                RegId rd = parseRegister(tokens[1]);
                std::string memSpec = tokens[2];
                // format: offset[reg] e.g. 12[r2] or [r2]
                int16_t offset = 0;
                RegId rs1 = 0;
                size_t lbracket = memSpec.find('[');
                size_t rbracket = memSpec.find(']');
                if (lbracket != std::string::npos && rbracket != std::string::npos && rbracket > lbracket) {
                    std::string offStr = memSpec.substr(0, lbracket);
                    if (!offStr.empty()) offset = static_cast<int16_t>(parseImmediate(offStr));
                    std::string regStr = memSpec.substr(lbracket + 1, rbracket - lbracket - 1);
                    rs1 = parseRegister(regStr);
                } else {
                    throw std::runtime_error("Invalid memory operand format: " + memSpec);
                }

                Opcode op = (baseMnemonic == "ld") ? OP_LD : OP_ST;
                code = encodeImm(op, rd, rs1, ImmModifier::DEFAULT, offset);
            }
            // 6. MOV: mov rd, (rs2/imm)
            else if (baseMnemonic == "mov") {
                if (tokens.size() < 3) throw std::runtime_error("Malformed mov: " + text);
                RegId rd = parseRegister(tokens[1]);
                std::string op2 = tokens[2];
                bool isImm = false;
                try {
                    RegId rs2 = parseRegister(op2);
                    code = encodeReg(OP_MOV, rd, 0, rs2);
                } catch (...) {
                    isImm = true;
                }
                if (isImm) {
                    int32_t val = parseImmediate(op2);
                    code = encodeImm(OP_MOV, rd, 0, modifier, static_cast<int16_t>(val));
                }
            }
            // 7. NOT: not rd, (rs2/imm)
            else if (baseMnemonic == "not") {
                if (tokens.size() < 3) throw std::runtime_error("Malformed not: " + text);
                RegId rd = parseRegister(tokens[1]);
                std::string op2 = tokens[2];
                bool isImm = false;
                try {
                    RegId rs2 = parseRegister(op2);
                    code = encodeReg(OP_NOT, rd, 0, rs2);
                } catch (...) {
                    isImm = true;
                }
                if (isImm) {
                    int32_t val = parseImmediate(op2);
                    code = encodeImm(OP_NOT, rd, 0, modifier, static_cast<int16_t>(val));
                }
            }
            // 7b. ABS: abs rd, (rs2/imm)
            else if (baseMnemonic == "abs") {
                if (tokens.size() < 3) throw std::runtime_error("Malformed abs: " + text);
                RegId rd = parseRegister(tokens[1]);
                std::string op2 = tokens[2];
                bool isImm = false;
                try {
                    RegId rs2 = parseRegister(op2);
                    code = encodeReg(OP_ABS, rd, 0, rs2);
                } catch (...) {
                    isImm = true;
                }
                if (isImm) {
                    int32_t val = parseImmediate(op2);
                    code = encodeImm(OP_ABS, rd, 0, modifier, static_cast<int16_t>(val));
                }
            }
            // 8. CMP: cmp rs1, (rs2/imm)
            else if (baseMnemonic == "cmp") {
                if (tokens.size() < 3) throw std::runtime_error("Malformed cmp: " + text);
                RegId rs1 = parseRegister(tokens[1]);
                std::string op2 = tokens[2];
                bool isImm = false;
                try {
                    RegId rs2 = parseRegister(op2);
                    code = encodeReg(OP_CMP, 0, rs1, rs2);
                } catch (...) {
                    isImm = true;
                }
                if (isImm) {
                    int32_t val = parseImmediate(op2);
                    code = encodeImm(OP_CMP, 0, rs1, modifier, static_cast<int16_t>(val));
                }
            }
            // 9. Standard 3-address ALU instructions: add, sub, mul, div, mod, and, or, xor, lsl, lsr, asr
            else {
                Opcode op = OP_INVALID;
                if (baseMnemonic == "add") op = OP_ADD;
                else if (baseMnemonic == "sub") op = OP_SUB;
                else if (baseMnemonic == "mul") op = OP_MUL;
                else if (baseMnemonic == "div") op = OP_DIV;
                else if (baseMnemonic == "mod") op = OP_MOD;
                else if (baseMnemonic == "and") op = OP_AND;
                else if (baseMnemonic == "or")  op = OP_OR;
                else if (baseMnemonic == "xor") op = OP_XOR;
                else if (baseMnemonic == "lsl") op = OP_LSL;
                else if (baseMnemonic == "lsr") op = OP_LSR;
                else if (baseMnemonic == "asr") op = OP_ASR;
                else {
                    throw std::runtime_error("Unknown mnemonic: " + baseMnemonic);
                }

                if (tokens.size() < 4) throw std::runtime_error("Malformed 3-address inst: " + text);
                RegId rd = parseRegister(tokens[1]);
                RegId rs1 = parseRegister(tokens[2]);
                std::string op3 = tokens[3];

                bool isImm = false;
                try {
                    RegId rs2 = parseRegister(op3);
                    code = encodeReg(op, rd, rs1, rs2);
                } catch (...) {
                    isImm = true;
                }
                if (isImm) {
                    int32_t val = parseImmediate(op3);
                    code = encodeImm(op, rd, rs1, modifier, static_cast<int16_t>(val));
                }
            }

            AssembledLine al;
            al.address = pc;
            al.machineCode = code;
            al.sourceLine = text;
            al.hexString = toHex(code);
            al.binaryString = std::bitset<32>(code).to_string();
            assembledLines.push_back(al);
        }

        if (enableScheduling) {
            scheduleBasicBlocks();
        }

        return true;
    }

    void scheduleBasicBlocks() {
        if (assembledLines.empty()) return;

        // Collect all label target addresses
        std::set<Word> labelAddresses;
        for (const auto& kv : symbolTable) {
            labelAddresses.insert(kv.second);
        }

        // Identify basic block boundaries [startIdx, endIdx)
        std::vector<std::pair<size_t, size_t>> blocks;
        size_t blockStart = 0;
        for (size_t i = 0; i < assembledLines.size(); ++i) {
            Word pc = assembledLines[i].address;
            if (i > blockStart && labelAddresses.count(pc)) {
                blocks.push_back({blockStart, i});
                blockStart = i;
            }

            DecodedInst d = Disassembler::decode(assembledLines[i].machineCode, pc);
            if (d.isBranch() || i == assembledLines.size() - 1) {
                blocks.push_back({blockStart, i + 1});
                blockStart = i + 1;
            }
        }

        // For each basic block, find load-use hazards and attempt instruction scheduling
        for (const auto& blk : blocks) {
            size_t bStart = blk.first;
            size_t bEnd = blk.second;
            if (bEnd <= bStart + 2) continue; // Need at least 3 instructions to reorder

            for (size_t i = bStart; i + 1 < bEnd; ++i) {
                DecodedInst di = Disassembler::decode(assembledLines[i].machineCode, assembledLines[i].address);
                if (!di.isLoad()) continue;

                RegId loadDest = di.getDestReg();
                DecodedInst dNext = Disassembler::decode(assembledLines[i + 1].machineCode, assembledLines[i + 1].address);

                // Check for load-use RAW hazard
                bool isLoadUse = false;
                if (dNext.readsRs1() && dNext.rs1 == loadDest) isLoadUse = true;
                if (dNext.readsRs2() && dNext.rs2 == loadDest) isLoadUse = true;
                if (dNext.isStore() && dNext.rd == loadDest) isLoadUse = true;

                if (!isLoadUse) continue;

                // Look for an independent candidate instruction after i+1 in the same basic block
                size_t candIdx = bEnd;
                for (size_t j = i + 2; j < bEnd; ++j) {
                    DecodedInst dj = Disassembler::decode(assembledLines[j].machineCode, assembledLines[j].address);
                    // Do not move branches, nops, or CMP instructions
                    if (dj.isBranch() || dj.opcode == isa::OP_NOP || dj.opcode == isa::OP_CMP) continue;
                    // Do not move memory instructions (loads or stores) to preserve memory ordering
                    if (dj.isLoad() || dj.isStore()) continue;

                    // dj must not read or write loadDest
                    if (dj.writesRegister() && dj.getDestReg() == loadDest) continue;
                    if (dj.readsRs1() && dj.rs1 == loadDest) continue;
                    if (dj.readsRs2() && dj.rs2 == loadDest) continue;

                    // If load reads rs1, dj must not write load's rs1
                    if (di.readsRs1() && dj.writesRegister() && dj.getDestReg() == di.rs1) continue;

                    // Verify total independence between dj and every instruction k in [i+1, j-1]
                    bool independent = true;
                    for (size_t k = i + 1; k < j; ++k) {
                        DecodedInst dk = Disassembler::decode(assembledLines[k].machineCode, assembledLines[k].address);
                        // RAW: dj reads what dk writes
                        if (dk.writesRegister() && ((dj.readsRs1() && dj.rs1 == dk.getDestReg()) ||
                                                    (dj.readsRs2() && dj.rs2 == dk.getDestReg()))) {
                            independent = false;
                            break;
                        }
                        // WAR: dj writes what dk reads
                        if (dj.writesRegister() && ((dk.readsRs1() && dk.rs1 == dj.getDestReg()) ||
                                                    (dk.readsRs2() && dk.rs2 == dj.getDestReg()) ||
                                                    (dk.isStore() && dk.rd == dj.getDestReg()))) {
                            independent = false;
                            break;
                        }
                        // WAW: dj writes what dk writes
                        if (dj.writesRegister() && dk.writesRegister() && dj.getDestReg() == dk.getDestReg()) {
                            independent = false;
                            break;
                        }
                    }

                    if (independent) {
                        candIdx = j;
                        break;
                    }
                }

                if (candIdx < bEnd) {
                    // Move instruction at candIdx to position i+1
                    auto lineToMove = assembledLines[candIdx];
                    assembledLines.erase(assembledLines.begin() + candIdx);
                    assembledLines.insert(assembledLines.begin() + i + 1, lineToMove);
                    // Skip over the newly inserted instruction
                    i++;
                }
            }
        }

        // Reassign sequential addresses to all lines
        for (size_t idx = 0; idx < assembledLines.size(); ++idx) {
            Word newAddr = static_cast<Word>(idx * 4);
            assembledLines[idx].address = newAddr;
            assembledLines[idx].hexString = toHex(assembledLines[idx].machineCode);
            assembledLines[idx].binaryString = std::bitset<32>(assembledLines[idx].machineCode).to_string();
        }
    }

    std::vector<Word> getMachineWords() const {
        std::vector<Word> words;
        for (const auto& l : assembledLines) {
            words.push_back(l.machineCode);
        }
        return words;
    }

    std::string getListing() const {
        std::ostringstream oss;
        oss << std::left << std::setw(12) << "Address"
            << std::setw(14) << "Machine (Hex)"
            << std::setw(36) << "Binary"
            << "Source\n";
        oss << std::string(80, '-') << "\n";
        for (const auto& l : assembledLines) {
            oss << std::left << std::setw(12) << toHex(l.address)
                << std::setw(14) << l.hexString
                << std::setw(36) << l.binaryString
                << l.sourceLine << "\n";
        }
        return oss.str();
    }
};

} // namespace assembler
} // namespace risc201

#endif // RISC201_ASSEMBLER_HPP
