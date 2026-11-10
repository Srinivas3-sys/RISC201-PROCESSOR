CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.

TARGETS = risc201_sim risc201_as risc201_disasm risc201_test

all: $(TARGETS)

risc201_sim: debugger/main_sim.cpp common/*.hpp alu/*.hpp microcode/*.hpp assembler/*.hpp visualizer/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp debugger/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) debugger/main_sim.cpp -o risc201_sim

risc201_as: assembler/main_as.cpp common/*.hpp assembler/*.hpp
	$(CXX) $(CXXFLAGS) assembler/main_as.cpp -o risc201_as

risc201_disasm: assembler/main_disasm.cpp common/*.hpp assembler/*.hpp
	$(CXX) $(CXXFLAGS) assembler/main_disasm.cpp -o risc201_disasm

risc201_test: tests/test_all.cpp common/*.hpp alu/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) tests/test_all.cpp -o risc201_test

test: risc201_test
	./risc201_test

test-pipe: risc201_test
	./risc201_test

test-isa: risc201_test
	./risc201_test

test-mem: risc201_test
	./risc201_test

test-vm: risc201_test
	./risc201_test

test-all: risc201_test
	./risc201_test

test-asan:
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I. tests/test_all.cpp -o risc201_test_asan
	./risc201_test_asan
	rm -f risc201_test_asan

clean:
	rm -f $(TARGETS) *.hex *.bin
