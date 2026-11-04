#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

bool checkFile(const fs::path& filePath, bool fix, int& errorCount) {
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) return false;

    std::vector<std::string> lines;
    std::string line;
    bool modified = false;
    int lineNum = 0;

    while (std::getline(inFile, line)) {
        lineNum++;
        size_t pos = 0;
        std::string newLine = "";
        size_t lastPos = 0;

        while ((pos = line.find("\\text{", pos)) != std::string::npos) {
            size_t start = pos + 6; // after "\\text{"
            size_t end = line.find('}', start);
            if (end == std::string::npos) {
                pos = start;
                continue;
            }

            // Check content inside \text{...}
            std::string content = line.substr(start, end - start);
            std::string fixedContent = "";
            bool hasUnescaped = false;

            for (size_t i = 0; i < content.size(); ++i) {
                if (content[i] == '_') {
                    if (i == 0 || content[i - 1] != '\\') {
                        hasUnescaped = true;
                        fixedContent += "\\_";
                    } else {
                        fixedContent += content[i];
                    }
                } else {
                    fixedContent += content[i];
                }
            }

            if (hasUnescaped) {
                errorCount++;
                std::cerr << "[LINT ERROR] " << filePath.string() << ":" << lineNum
                          << " unescaped underscore in \\text{" << content << "}\n";
                if (fix) {
                    newLine += line.substr(lastPos, start - lastPos);
                    newLine += fixedContent;
                    lastPos = end;
                    modified = true;
                }
            }
            pos = end + 1;
        }

        if (fix && modified) {
            newLine += line.substr(lastPos);
            lines.push_back(newLine);
        } else {
            lines.push_back(line);
        }
    }
    inFile.close();

    if (fix && modified) {
        std::ofstream outFile(filePath);
        for (size_t i = 0; i < lines.size(); ++i) {
            outFile << lines[i] << (i + 1 < lines.size() ? "\n" : "");
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    bool fix = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--fix") fix = true;
    }

    std::vector<fs::path> targetDirs = {"docs"};
    int totalErrors = 0;

    for (const auto& dir : targetDirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".md") {
                checkFile(entry.path(), fix, totalErrors);
            }
        }
    }

    if (fs::exists("README.md")) {
        checkFile("README.md", fix, totalErrors);
    }

    if (totalErrors > 0) {
        if (fix) {
            std::cout << "[SUCCESS] Fixed " << totalErrors << " unescaped underscore(s) in \\text{...}.\n";
            return 0;
        } else {
            std::cerr << "[FAILURE] Found " << totalErrors << " unescaped underscore(s) in \\text{...}. Run 'make lint-docs-fix' to auto-fix.\n";
            return 1;
        }
    }

    std::cout << "[PASSED] All markdown docs have clean KaTeX math rendering (zero unescaped underscores in \\text{}).\n";
    return 0;
}

