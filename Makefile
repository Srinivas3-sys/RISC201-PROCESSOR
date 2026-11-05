CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.

TARGETS = risc201_sim risc201_as risc201_disasm risc201_eval risc201_test exp_pipeline exp_memory audit_cpi generate_perf_docs lint_docs lint_isa verify_golden

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

audit_cpi: tools/audit_cpi.cpp common/*.hpp alu/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) tools/audit_cpi.cpp -o audit_cpi

audit-cpi: audit_cpi
	./audit_cpi

generate_perf_docs: tools/generate_perf_docs.cpp
	$(CXX) $(CXXFLAGS) tools/generate_perf_docs.cpp -o generate_perf_docs

perf-docs: audit_cpi generate_perf_docs
	./audit_cpi
	./generate_perf_docs

lint_docs: tools/lint_docs.cpp
	$(CXX) $(CXXFLAGS) tools/lint_docs.cpp -o lint_docs

lint-docs: lint_docs
	./lint_docs

lint-docs-fix: lint_docs
	./lint_docs --fix

lint_isa: tools/lint_isa.cpp
	$(CXX) $(CXXFLAGS) tools/lint_isa.cpp -o lint_isa

lint-isa: lint_isa
	./lint_isa

verify_golden: tools/verify_golden.cpp common/*.hpp alu/*.hpp microcode/*.hpp assembler/*.hpp processor_4stage/*.hpp processor_6stage/*.hpp memory/*.hpp
	$(CXX) $(CXXFLAGS) tools/verify_golden.cpp -o verify_golden

verify-golden: verify_golden
	./verify_golden

test-asan:
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I. tests/test_all.cpp -o risc201_test_asan
	./risc201_test_asan
	rm -f risc201_test_asan

demo-mem: risc201_sim
	./risc201_sim benchmarks/factorial_iter.s --mem-preset modern

demo-vm: risc201_sim
	./risc201_sim benchmarks/factorial_rec.s --mem-preset modern --pipeline 6

demo-pipe: risc201_sim
	./risc201_sim benchmarks/hazard_test.s --pipeline 6 --forwarding full --diagram --report

notes:
	cat notes/00_project_map.md \
	    notes/01_how_to_read_these_notes.md \
	    notes/glossary.md \
	    notes/cheat_sheets/CHEAT_SHEETS.md \
	    notes/parts/part_a_foundations.md \
	    notes/parts/part_b_isa.md \
	    notes/parts/part_c_software_tools.md \
	    notes/parts/part_d_processors.md \
	    notes/parts/part_e_hazards_forwarding.md \
	    notes/parts/part_f_control_unit.md \
	    notes/parts/part_g_alu.md \
	    notes/parts/part_h_memory.md \
	    notes/parts/part_i_stack_visualizers.md \
	    notes/parts/part_j_performance_evaluation.md \
	    notes/parts/part_k_quality_verification.md \
	    notes/worked_examples/WORKED_EXAMPLES.md \
	    notes/code_walkthroughs/CODE_WALKTHROUGHS.md \
	    notes/presentation/PRESENTATION_GUIDE.md \
	    notes/qa/120_EXAMINER_QUESTIONS.md \
	    notes/self_test/MOCK_VIVAS.md > notes/ALL_NOTES.md
	sed -i '7,$$s/^---$$/***/g' notes/ALL_NOTES.md
	pandoc notes/ALL_NOTES.md -o notes/ALL_NOTES.pdf --pdf-engine=typst
	pandoc notes/ALL_NOTES.md -o notes/ALL_NOTES.docx

clean:
	rm -f $(TARGETS) *.hex *.bin

.PHONY: all clean test test-pipe test-isa test-mem test-vm fuzz test-all exp exp-pipe demo-mem demo-vm demo-pipe audit-cpi perf-docs lint-docs lint-docs-fix lint-isa verify-golden test-asan notes
