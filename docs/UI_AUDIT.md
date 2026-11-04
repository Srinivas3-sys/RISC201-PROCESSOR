# UI Audit: RISC201 Processor Simulator & Visualizer

## 1. Git Commit History (Last 30 Days) Touching UI / Visualizer / Debugger / CLI

The following commits touch UI, visualizers, interactive debugger, and CLI tools:

| Commit Hash | Author | Date | Summary | Files Touched |
| :--- | :--- | :--- | :--- | :--- |
| `bbd9ea1` | Varshith Teja | 2026-11-02 23:46:45 | `feat: add Newton-Raphson division, branch predictor selection, XOR/ABS opcodes, trace visualizer, and startup assembly prompt` | `debugger/Debugger.hpp`<br>`debugger/main_sim.cpp`<br>`visualizer/PipelineVisualizer.hpp`<br>`processor_4stage/Pipeline4Stage.hpp`<br>`processor_6stage/Pipeline6Stage.hpp`<br>`evaluation/CompareProcessors.cpp` |
| `25b8151` | bandlapalliyss-max | 2026-11-02 19:43:42 | `feat: complete 100% pure modern C++17 conversion of memory hierarchy, pipeline hazards, test suite, and experiments` | `debugger/Debugger.hpp`<br>`debugger/main_sim.cpp`<br>`visualizer/MemoryVisualizer.hpp`<br>`evaluation/run_memory_experiments.cpp`<br>`evaluation/run_pipeline_experiments.cpp` |
| `9e70932` | bandlapalliyss-max | 2026-10-25 16:17:49 | `Baseline improvements: custom input in debugger and microcode branch fixes` | `debugger/Debugger.hpp`<br>`debugger/main_sim.cpp`<br>`visualizer/StackVisualizer.hpp` |
| `0a5416a` | bandlapalliyss-max | 2026-10-01 09:23:31 | `Fix pipeline stall interlocks, ret instruction decode, and compiler warnings` | `evaluation/CompareProcessors.cpp` |
| `a4e1f29` | bandlapalliyss-max | 2026-10-01 09:03:04 | `Initial commit: RISC201 Processor Design` | `debugger/Debugger.hpp`<br>`debugger/main_sim.cpp`<br>`evaluation/CompareProcessors.cpp`<br>`visualizer/PipelineVisualizer.hpp`<br>`visualizer/StackVisualizer.hpp` |

---

## 2. Changes Introduced by Varshith Teja (`bbd9ea1`)

Varshith Teja introduced substantial interactive and visual capabilities:
1. **Interactive Startup Assembly Prompt (`debugger/Debugger.hpp`, `debugger/main_sim.cpp`)**:
   - When `risc201_sim` is executed without positional file arguments, it directly prompts the user to enter multi-line SimpleRisc assembly code (`promptStartupAssembly()`).
   - If empty input is entered, it falls back to the default sample program (`factorial_rec.s`).
   - Added command line option `--code` / `-c` to pass semicolon-separated assembly directly.
2. **Cycle-by-Cycle Pipeline Trace & Timing Diagram (`visualizer/PipelineVisualizer.hpp`)**:
   - Implemented `renderTrace(const std::vector<CycleSnapshot>& history, const std::string& pipelineType)`:
     - **Part 1 (Stage State Table)**: Full tabular dump showing cycle number and instructions active in each stage (`IF`, `ID`, `RF`, `EX`, `MA`, `RW` for 6-stage; `IF`, `OF`, `EX`, `MA_RW` for 4-stage) with `[STALL]` and `[bubble]` markings.
     - **Part 2 (Space-Time Timing Diagram)**: Gantt chart-style matrix of instructions versus clock cycles, chunked into 15-cycle horizontal windows.
   - Added `--trace` CLI argument to `debugger/main_sim.cpp` and `trace` command in the debugger REPL.
3. **Pipeline Instrumentation (`processor_4stage/`, `processor_6stage/`)**:
   - Added `nextInstId` and `cycleHistory` (`std::vector<visualizer::CycleSnapshot>`) to record pipeline latch transitions at every clock cycle.
   - Preserved instruction tracking IDs through all pipeline inter-stage latches (`Latch_IF_ID`, `Latch_ID_RF`, `Latch_RF_EX`, etc.).
4. **Interactive REPL Extensions**:
   - `trace`: Invokes `showTrace()`.
   - `asm` / `code`: Invokes `promptStartupAssembly()` to hot-reload and simulate custom assembly.
   - `bp_policy <always_not_taken|always_taken|bimodal_1bit|bimodal_2bit|gshare>`: Configures pipeline branch prediction policy.
   - `div <rest|nonrest|newton>`: Selects between Restoring, Non-Restoring, and Newton-Raphson division algorithms.
   - `r` / `run`: Automatically reloads and resets the pipeline before executing if the program previously halted.

---

## 3. Current UI Structure, Layout, and Visual Style

### Visual Philosophy & Styling Rules
- **Pure ASCII Art**: All UI elements are rendered using standard ASCII and Unicode block symbols (`+`, `-`, `|`, `=`, `#`, `█`). No terminal-specific ANSI escape codes or third-party curses libraries are used, guaranteeing clean output across terminals, pipe redirection, and log files.
- **Fixed Widths & Column Truncation**: Strings are truncated using a custom `truncateStr(str, maxLen)` helper that appends `..` if strings exceed the column budget.
- **Consistent Enclosures**:
  - Double-line headers: `========================================================================` (72 characters).
  - Box-framed panels: `+--------------------------------------------------------------------------+` (76 characters).
  - Table grid boundaries: `+-------+-------------------+-------------------+...+`
- **Naming Conventions**: PascalCase for visualizer classes (`PipelineVisualizer`, `StackVisualizer`, `MemoryVisualizer`), static `render*` methods returning `std::string`.

### Screens, Panels, and Visualizers

| Component | Source File | Description | Visual Layout Style |
| :--- | :--- | :--- | :--- |
| **Startup Prompt** | `debugger/Debugger.hpp` | Interactive multi-line assembly input prompt | Numbered line prompt `1>`, `2>`, etc. with double-line header banner |
| **Stage Latch Visualizer** | `visualizer/PipelineVisualizer.hpp` (`renderStages`) | Snapshot of currently active instructions in each pipeline stage | 76-column box frame with `[STAGE ] : <instruction> (PC: 0x...)` |
| **Execution Trace Table** | `visualizer/PipelineVisualizer.hpp` (`renderTrace`) | Full cycle history stage-by-stage | Fixed-width multi-column table (`Cycle`, `IF`, `ID`, `RF`, `EX`, `MA`, `RW`) |
| **Space-Time Timing Diagram** | `visualizer/PipelineVisualizer.hpp` (`renderTrace`) | Space-time grid of retired instructions against cycles | 15-cycle horizontal chunking grid: `Instruction \| C1 \| C2 \| ...` |
| **Stack Frame Visualizer** | `visualizer/StackVisualizer.hpp` (`render`) | Active stack memory frame from `stackBase` to `sp` | Table with `Address`, `Value (Hex)`, `Value (Signed)`, and heuristic annotations |
| **Memory Hierarchy Stats** | `visualizer/MemoryVisualizer.hpp` (`renderHierarchy`) | Hit/miss rates, access counts, and MSHR status for L1I, L1D, L2, L3 | Double-line banner with indented statistics and percentage displays |
| **DRAM Bank State** | `visualizer/MemoryVisualizer.hpp` (`renderDRAM`) | State of 8 DRAM banks (`IDLE`, `ACTIVE`, row open/closed) | Tabular bank status with open row hex display |
| **TLB State Visualizer** | `visualizer/MemoryVisualizer.hpp` (`renderTLB`) | Contents of ITLB/DTLB sets | Set, VPN, PFN, ASID, dirty flag table |
| **CPI Stack Visualizer** | `visualizer/MemoryVisualizer.hpp` (`renderCPIStack`) | Bar breakdown of CPI components | Horizontal ASCII bar graphs (`#` and `█`) with cycle counts and delta CPI |
| **Register File Display** | `debugger/Debugger.hpp` (`showRegisters`) | Values of r0-r15, flags, and PC | 4 columns of registers showing hex and signed integer values |
| **Disassembler / Mem Dump** | `debugger/Debugger.hpp` (`dumpMemory`, `disassembleMemory`) | Memory dump and disassembled instruction list | Address, hex words, and SimpleRisc mnemonics |

---

## 4. Simulator-to-UI Interface

The UI is cleanly decoupled from core hardware simulation via non-intrusive query methods:

1. **Host Container (`risc201::debugger::Debugger`)**:
   - Contains instances of `stage4::Pipeline4Stage p4`, `stage6::Pipeline6Stage p6`, and `microcode::MicroController mcu`.
   - Controls stepping: `stepCycle()`, `stepInstruction()`, `run(maxCycles)`.
2. **Pipeline State Extraction**:
   - `p4.getStageInfo()` / `p6.getStageInfo()` returns `std::vector<PipelineStageInfo>`.
   - `p4.cycleHistory` / `p6.cycleHistory` stores complete `std::vector<CycleSnapshot>` captured at each clock tick.
3. **Memory & Architectural State Extraction**:
   - `visualizer::StackVisualizer::render(p.memory, p.regFile)` inspects memory frames and checks stack boundary limits via `memory.getStackBounds()`.
   - `visualizer::MemoryVisualizer::renderHierarchy(p.memHierarchy)` inspects cache statistics (`L1I`, `L1D`, `L2`, `L3`) and prefetcher counters.
   - `visualizer::MemoryVisualizer::renderDRAM(p.memHierarchy.dram)` reads bank state machines.
   - `visualizer::MemoryVisualizer::renderTLB(p.memHierarchy.itlb / dtlb)` reads TLB entry validity, VPN, and PFN mappings.
   - `visualizer::MemoryVisualizer::renderCPIStack(p.perfTracker)` reads `perfTracker.stallBreakdown` and `retiredInstructions`.
4. **Hazard & Forwarding Telemetry**:
   - Queries `p.hazardUnit` for raw hazard counters and stall cycles.
   - Queries `p.forwardingUnit` for bypass path activations (e.g. `EX_TO_EX`, `MEM_TO_EX`, `WB_TO_EX`).

---

## 5. UI Integration Strategy for Part 4

To strictly adhere to the rule of preserving Varshith Teja's visual style, naming, and module architecture:
1. **Live CPI-Stack Panel**:
   - Location: `visualizer/MemoryVisualizer.hpp` and integrated into `debugger/Debugger.hpp`.
   - Style: Keep `renderCPIStack` layout, adding steady-state vs cold-start toggles.
2. **Pipeline Diagram with Labeled Bubbles & Forwarding Arrows**:
   - Location: `visualizer/PipelineVisualizer.hpp`.
   - Style: Enhance stage state rendering to display stall reason code labels (e.g. `[STALL: RAW_LOAD_USE]`, `[BUBBLE: BRANCH_MISPREDICT]`) and ASCII forwarding path arrows (`r1 forwarded from EX_MARW -> EX_IN1`).
3. **Side-by-Side 4-Stage vs 6-Stage Compare View**:
   - Location: `visualizer/PipelineVisualizer.hpp` / `debugger/Debugger.hpp`.
   - Command: `compare`.
   - Style: 76-column side-by-side comparison table showing cycles, retired instructions, CPI, stall breakdown, and wall-clock execution time calculated at 500 MHz (4-stage) vs 800 MHz (6-stage).
4. **Memory View (Hit rates, MSHR, Trace Line)**:
   - Location: `visualizer/MemoryVisualizer.hpp`.
   - Enhancements: Add recent memory access trace line buffer and MSHR queue occupancy view.
5. **Explain Stall Action**:
   - Command: `explain [cycle]`.
   - Details: Inspects specific stall or bubble in `cycleHistory`, identifies the dependent instruction, hazard condition, and contributing cycle counts.
