#include "Debugger.hpp"
#include <fstream>
#include <iostream>

using namespace risc201;

int main(int argc, char* argv[]) {
    debugger::Debugger dbg;

    std::string sampleProgram = R"(
; RISC201 Demonstration Assembly Program
; Demonstrates full-descending stack macros (push/pop),
; arithmetic ALU instructions, and loop control
.text
    mov r0, 5        ; Compute factorial of 5
    call .factorial
    mov r2, r1       ; r2 = result (5! = 120 = 0x78)
    b .done

.factorial:
    cmp r0, 1
    beq .base_case
    bgt .recurse
    mov r1, 1
    ret

.base_case:
    mov r1, 1
    ret

.recurse:
    ; Use stack preprocessor macro for full-descending stack push
    push r0          ; expands to: sub sp, sp, 4 ; st r0, 0[sp]
    push ra          ; expands to: sub sp, sp, 4 ; st ra, 0[sp]
    sub r0, r0, 1    ; n = n - 1
    call .factorial  ; factorial(n-1) in r1
    pop ra           ; expands to: ld ra, 0[sp] ; add sp, sp, 4
    pop r0           ; expands to: ld r0, 0[sp] ; add sp, sp, 4
    mul r1, r0, r1   ; r1 = n * factorial(n-1)
    ret

.done:
    nop
)";

    std::string programToLoad = sampleProgram;

    if (argc > 1) {
        std::string filename = argv[1];
        std::ifstream file(filename);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            programToLoad = buffer.str();
            std::cout << "Loaded assembly file: " << filename << "\n";
        } else {
            std::cerr << "Could not open file: " << filename << ", using default sample program.\n";
        }
    } else {
        std::cout << "No program file passed. Loaded default recursive factorial demonstration program.\n";
    }

    dbg.loadAssemblyString(programToLoad);

    if (argc > 2) {
        try {
            Word customN = std::stoul(argv[2]);
            dbg.setCustomInput(customN);
            std::cout << ">>> Configured custom input: n = " << customN << " <<<\n";
        } catch (...) {
            std::cerr << "Invalid custom input number: " << argv[2] << "\n";
        }
    }

    dbg.repl();

    return 0;
}
