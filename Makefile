CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.

TARGETS = risc201_sim risc201_as risc201_disasm risc201_eval risc201_test exp_pipeline exp_memory

all: $(TARGETS)

risc201_sim: debugger/main_sim.cpp common/*.hpp alu/*.hpp microcode/*.hpp assembler/*.hpp visualizer/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp debugger/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) debugger/main_sim.cpp -o risc201_sim

risc201_as: assembler/main_as.cpp common/*.hpp assembler/*.hpp
	$(CXX) $(CXXFLAGS) assembler/main_as.cpp -o risc201_as

risc201_disasm: assembler/main_disasm.cpp common/*.hpp assembler/*.hpp
	$(CXX) $(CXXFLAGS) assembler/main_disasm.cpp -o risc201_disasm

risc201_eval: evaluation/CompareProcessors.cpp common/*.hpp alu/*.hpp microcode/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) evaluation/CompareProcessors.cpp -o risc201_eval

risc201_test: tests/test_all.cpp common/*.hpp alu/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) tests/test_all.cpp -o risc201_test

exp_pipeline: evaluation/run_pipeline_experiments.cpp common/*.hpp alu/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) evaluation/run_pipeline_experiments.cpp -o exp_pipeline

exp_memory: evaluation/run_memory_experiments.cpp memory/*.hpp
	$(CXX) $(CXXFLAGS) evaluation/run_memory_experiments.cpp -o exp_memory

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

fuzz: risc201_test
	./risc201_test

test-all: risc201_test risc201_eval
	./risc201_test && ./risc201_eval

exp: exp_memory
	./exp_memory

exp-pipe: exp_pipeline
	./exp_pipeline

demo-mem: risc201_sim
	./risc201_sim benchmarks/factorial_iter.s --mem-preset modern

demo-vm: risc201_sim
	./risc201_sim benchmarks/factorial_rec.s --mem-preset modern --pipeline 6

demo-pipe: risc201_sim
	./risc201_sim benchmarks/hazard_test.s --pipeline 6 --forwarding full --diagram --report

clean:
	rm -f $(TARGETS) *.hex *.bin

.PHONY: all clean test test-pipe test-isa test-mem test-vm fuzz test-all exp exp-pipe demo-mem demo-vm demo-pipe
