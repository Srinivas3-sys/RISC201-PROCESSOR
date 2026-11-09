#ifndef RISC201_MACRO_PREPROCESSOR_HPP
#define RISC201_MACRO_PREPROCESSOR_HPP

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace risc201 {
namespace assembler {

/**
 * MacroPreprocessor provides macro expansion for full-descending stack operations
 * as specified in Requirement 2e:
 *   - push reg           -> sub sp, sp, 4; st reg, 0[sp]
 *   - pop reg            -> ld reg, 0[sp]; add sp, sp, 4
 *   - pushm {r1, r2, ..} -> sub sp, sp, 4*K; st r1, 0[sp]; st r2, 4[sp]; ...
 *   - popm {r1, r2, ..}  -> ld r1, 0[sp]; ld r2, 4[sp]; ...; add sp, sp, 4*K
 *
 * Full-descending stack convention (Sarangi Section 4.3.2):
 * - SP points to the last stored valid item.
 * - Pushing decrements SP before writing.
 * - Popping reads at SP and then increments SP.
 */
class MacroPreprocessor {
public:
    static std::string trim(const std::string& s) {
        auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){return std::isspace(c);});
        auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){return std::isspace(c);}).base();
        return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }

    static std::vector<std::string> parseRegisterList(const std::string& listStr) {
        std::vector<std::string> regs;
        size_t start = listStr.find('{');
        size_t end = listStr.find('}');
        if (start == std::string::npos || end == std::string::npos || end <= start) {
            return regs;
        }
        std::string inner = listStr.substr(start + 1, end - start - 1);
        std::stringstream ss(inner);
        std::string token;
        while (std::getline(ss, token, ',')) {
            std::string r = trim(token);
            if (!r.empty()) regs.push_back(r);
        }
        return regs;
    }

    static std::vector<std::string> expandLine(const std::string& rawLine) {
        std::vector<std::string> outputLines;
        std::string line = trim(rawLine);

        // Preserve empty lines or pure comment lines
        if (line.empty() || line[0] == ';' || line[0] == '@' || line.rfind("/*", 0) == 0) {
            outputLines.push_back(rawLine);
            return outputLines;
        }

        // Separate trailing inline comments
        std::string commentSuffix = "";
        size_t commentPos = line.find(';');
        if (commentPos == std::string::npos) commentPos = line.find('@');
        if (commentPos != std::string::npos) {
            commentSuffix = " " + line.substr(commentPos);
            line = trim(line.substr(0, commentPos));
        }

        // Separate possible label prefix
        std::string labelPrefix = "";
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            labelPrefix = line.substr(0, colonPos + 1) + " ";
            line = trim(line.substr(colonPos + 1));
            if (line.empty()) {
                outputLines.push_back(labelPrefix + commentSuffix);
                return outputLines;
            }
        }

        std::stringstream ss(line);
        std::string mnemonic;
        ss >> mnemonic;
        std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);

        if (mnemonic == "push") {
            std::string reg;
            ss >> reg;
            reg = trim(reg);
            // push reg -> sub sp, sp, 4 ; st reg, 0[sp]
            outputLines.push_back(labelPrefix + "sub sp, sp, 4" + commentSuffix);
            outputLines.push_back("st " + reg + ", 0[sp]");
        } else if (mnemonic == "pop") {
            std::string reg;
            ss >> reg;
            reg = trim(reg);
            // pop reg -> ld reg, 0[sp] ; add sp, sp, 4
            outputLines.push_back(labelPrefix + "ld " + reg + ", 0[sp]" + commentSuffix);
            outputLines.push_back("add sp, sp, 4");
        } else if (mnemonic == "pushm") {
            std::string rest;
            std::getline(ss, rest);
            auto regList = parseRegisterList(rest);
            if (!regList.empty()) {
                int totalBytes = static_cast<int>(regList.size()) * 4;
                outputLines.push_back(labelPrefix + "sub sp, sp, " + std::to_string(totalBytes) + commentSuffix);
                for (size_t i = 0; i < regList.size(); ++i) {
                    int offset = static_cast<int>(i) * 4;
                    outputLines.push_back("st " + regList[i] + ", " + std::to_string(offset) + "[sp]");
                }
            } else {
                outputLines.push_back(rawLine);
            }
        } else if (mnemonic == "popm") {
            std::string rest;
            std::getline(ss, rest);
            auto regList = parseRegisterList(rest);
            if (!regList.empty()) {
                int totalBytes = static_cast<int>(regList.size()) * 4;
                for (size_t i = 0; i < regList.size(); ++i) {
                    int offset = static_cast<int>(i) * 4;
                    std::string pfx = (i == 0) ? labelPrefix : "";
                    outputLines.push_back(pfx + "ld " + regList[i] + ", " + std::to_string(offset) + "[sp]");
                }
                outputLines.push_back("add sp, sp, " + std::to_string(totalBytes) + commentSuffix);
            } else {
                outputLines.push_back(rawLine);
            }
        } else if (mnemonic == "cbeq" || mnemonic == "cbgt") {
            // Compare and branch pseudo-instructions:
            // cbeq rs1, rs2, target -> cmp rs1, rs2 ; beq target
            // cbgt rs1, rs2, target -> cmp rs1, rs2 ; bgt target
            std::string rest;
            std::getline(ss, rest);
            std::vector<std::string> args;
            std::string cur;
            for (char c : rest) {
                if (c == ',' || std::isspace(c)) {
                    if (!cur.empty()) { args.push_back(cur); cur.clear(); }
                } else {
                    cur += c;
                }
            }
            if (!cur.empty()) args.push_back(cur);
            if (args.size() >= 3) {
                std::string brMnemonic = (mnemonic == "cbeq") ? "beq" : "bgt";
                outputLines.push_back(labelPrefix + "cmp " + args[0] + ", " + args[1] + commentSuffix);
                outputLines.push_back(brMnemonic + " " + args[2]);
            } else {
                outputLines.push_back(rawLine);
            }
        } else {
            // Not a macro, preserve line as is
            outputLines.push_back(rawLine);
        }

        return outputLines;
    }

    static std::string preprocess(const std::string& sourceCode) {
        std::stringstream in(sourceCode);
        std::ostringstream out;
        std::string line;
        while (std::getline(in, line)) {
            auto expanded = expandLine(line);
            for (const auto& el : expanded) {
                out << el << "\n";
            }
        }
        return out.str();
    }
};

} // namespace assembler
} // namespace risc201

#endif // RISC201_MACRO_PREPROCESSOR_HPP
