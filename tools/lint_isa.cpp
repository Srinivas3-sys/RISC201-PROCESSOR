#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

struct Violation {
    std::string file;
    size_t lineNum;
    std::string line;
    std::string reason;
};

int main() {
    std::cout << "========================================================================\n";
    std::cout << "                 RISC201 ISA SINGLE SOURCE OF TRUTH LINTER             \n";
    std::cout << "========================================================================\n";

    std::vector<std::string> searchDirs = {
        "common", "alu", "assembler", "microcode",
        "processor_4stage", "processor_6stage",
        "visualizer", "debugger", "evaluation", "tests", "tools"
    };

    std::vector<Violation> violations;

    // RegEx patterns for checks
    std::regex reEnumOpcode(R"(\benum\s+(class\s+)?Opcode\b)");
    std::regex reOpcodeType(R"(\bOpcode\s+[a-zA-Z0-9_]+)");
    std::regex reCaseOp(R"(\bcase\s+OP_[A-Z0-9_]+\s*:)");
    std::regex reBinOp(R"(\bBIN_OP_[A-Z0-9_]+\b)");
    std::regex re5BitLiteral(R"("(0|1){5}")");

    size_t filesChecked = 0;

    for (const auto& dir : searchDirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();

            // Only check .hpp, .cpp, .h, .c
            std::string ext = entry.path().extension().string();
            if (ext != ".hpp" && ext != ".cpp" && ext != ".h" && ext != ".c") continue;

            // Skip lint_isa.cpp and capture_golden.cpp themselves
            if (path.find("lint_isa.cpp") != std::string::npos ||
                path.find("capture_golden.cpp") != std::string::npos) {
                continue;
            }

            filesChecked++;
            std::ifstream file(path);
            if (!file.is_open()) continue;

            std::string line;
            size_t lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;

                // Skip pure comments or empty lines
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                if (trimmed.rfind("//", 0) == 0 || trimmed.rfind("/*", 0) == 0 || trimmed.rfind("*", 0) == 0) {
                    continue;
                }

                // Helper: strip string literals from line to check code structure
                auto stripStringLiterals = [](const std::string& s) {
                    std::string out;
                    bool inStr = false;
                    for (size_t i = 0; i < s.size(); ++i) {
                        if (s[i] == '\\' && inStr && i + 1 < s.size()) {
                            i++; // skip escaped char
                            continue;
                        }
                        if (s[i] == '"') {
                            inStr = !inStr;
                            continue;
                        }
                        if (!inStr) out += s[i];
                    }
                    return out;
                };

                std::string codeOnly = stripStringLiterals(trimmed);

                // 1. Check for enum Opcode
                if (std::regex_search(codeOnly, reEnumOpcode)) {
                    violations.push_back({path, lineNum, line, "Definition of enum Opcode is forbidden"});
                }

                // 2. Check for Opcode type usage
                if (std::regex_search(codeOnly, reOpcodeType)) {
                    violations.push_back({path, lineNum, line, "Usage of integer Opcode type is forbidden; use std::string"});
                }

                // 3. Check for case OP_
                if (std::regex_search(codeOnly, reCaseOp)) {
                    violations.push_back({path, lineNum, line, "Switch case on OP_* integer is forbidden; dispatch using isa:: constants"});
                }

                // 4. Check for BIN_OP_
                if (std::regex_search(line, reBinOp)) {
                    violations.push_back({path, lineNum, line, "Deprecated BIN_OP_* alias used; use canonical isa::OP_* instead"});
                }

                // 5. Check for raw 5-bit binary string literal outside common/ISA.hpp
                // (e.g. "00110", "11001", etc. must not be hardcoded in pipeline/alu/iss)
                if (path.find("common/ISA.hpp") == std::string::npos &&
                    path.find("tests/test_all.cpp") == std::string::npos) {
                    if (std::regex_search(line, re5BitLiteral)) {
                        violations.push_back({path, lineNum, line, "Raw 5-bit binary string literal found outside ISA.hpp/tests; use isa::OP_* constant"});
                    }
                }
            }
        }
    }

    std::cout << "Files checked: " << filesChecked << "\n";
    if (violations.empty()) {
        std::cout << "[LINT SUCCESS] Zero integer opcodes, zero case OP_, zero BIN_OP_, zero quoted 5-bit literals outside ISA.hpp!\n";
        std::cout << "All modules strictly conform to canonical 5-bit string ISA source of truth.\n";
        std::cout << "========================================================================\n";
        return 0;
    } else {
        std::cerr << "[LINT FAILURE] Found " << violations.size() << " violation(s):\n";
        for (const auto& v : violations) {
            std::cerr << "  " << v.file << ":" << v.lineNum << " - " << v.reason << "\n";
            std::cerr << "    Line: " << v.line << "\n";
        }
        std::cerr << "========================================================================\n";
        return 1;
    }
}
