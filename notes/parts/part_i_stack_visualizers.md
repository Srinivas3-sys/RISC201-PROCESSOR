# Part I: Stack Safety & Visual Debuggers

This section covers stack frame mechanics, recursive calling conventions, runtime boundary enforcement (stack overflow prevention via guard pages), and the interactive ASCII visualizers contributed by teammates.

---

# Chapter I1: Stack Architecture & Calling Conventions

### 1. In One Sentence
The stack is a contiguous, full-descending memory region starting at high memory that dynamically manages subroutine activation records, function parameters, and saved return addresses.

---

### 2. Anatomy of a Stack Activation Record (Stack Frame)

```
                       HIGH MEMORY (0x000FFFFC)
             +-----------------------------------------+
             |       Caller's Stack Frame              |
             +-----------------------------------------+
             | Parameter 1 (r0)                        |
             +-----------------------------------------+
             | Saved Return Address (ra = r15)         |
             +-----------------------------------------+
             | Saved Frame Pointer (fp / old sp)       |
  sp ------> +-----------------------------------------+ <--- sp points to top of stack
             |             (Free Stack Area)           |
             v                                         v
                      LOW MEMORY (0x00010000)
```

#### The Function Call Sequence
1. **Prologue (Function Entry)**:
   - Push `ra` (Save return address): `sub sp, sp, 4 ; st ra, [sp]`
   - Push callee-saved registers or parameters: `sub sp, sp, 4 ; st r0, [sp]`
2. **Body**:
   - Executes function logic. Local variables are placed on stack if registers are exhausted.
3. **Epilogue (Function Exit)**:
   - Restore saved registers in reverse order: `ld r0, [sp] ; add sp, sp, 4`
   - Restore `ra`: `ld ra, [sp] ; add sp, sp, 4`
   - Execute `ret` ($PC = ra$).

---

# Chapter I2: Stack Safety & Runtime Exception Framework

### 1. In One Sentence
RISC201 protects against stack overflows, corrupted return addresses, and buffer overrun exploits by establishing a memory-mapped Guard Page beneath the stack space and enforcing bounds in `common/Exception.hpp`.

---

### 2. The Stack Guard Page (`memory/VirtualMemory.hpp`)
- Maximum Stack Allocation: **$1\text{ MB}$** (from `0x000FFFFC` down to `0x00010000`).
- Directly beneath `0x00010000`, the operating system maps a **Guard Page** (`0x0000F000` to `0x0000FFFF`).
- The Guard Page PTE is marked:
  $$\text{Present} = 1, \quad \text{Readable} = 0, \quad \text{Writable} = 0$$
- If an infinite recursive loop (like a broken factorial or stack smashing attack) pushes too many frames, the `sp` eventually crosses into the Guard Page.
- The MMU triggers an immediate, unmaskable **`GUARD_PAGE_FAULT`** / **`STACK_OVERFLOW_EXCEPTION`**, halting execution cleanly without overwriting code or heap memory!

---

# Chapter I3: The ASCII Visualizers & UI Enhancements

### 1. In One Sentence
The simulator includes terminal-based ASCII visualizers for pipeline latches, cache line states, DRAM banks, and execution stacks, enhanced with live forwarding arrows and stall explanations.

---

### 2. Teammate Contribution: UI Enhancements by Varshith Teja
From git history (commit `bbd9ea1`):
- **Teammate**: Varshith Teja contributed the visual ASCII pipeline timing trace visualizer (`visualizer/PipelineVisualizer.hpp`), the interactive startup assembly prompt, Newton-Raphson division in `alu/Dividers.hpp`, and CLI flags for branch predictor selection.
- **Visual Style**: Renders clean box-drawing borders (`+---+`), cycle markers, labeled stall causes (`[BUBBLE: RAW_LOAD_USE]`), and operand forwarding arrows (`====>`).

---

### 3. Visualizer Screen Examples

#### A. Pipeline Timing Trace (`visualizer/PipelineVisualizer.hpp`)
```
+--------------------------------------------------------------------------+
|               PIPELINE EXECUTION STATE (Clock Cycle:     25)             |
+--------------------------------------------------------------------------+
  [IF    ] : -- BUBBLE / NOP --
  [ID    ] : -- BUBBLE / NOP --
  [RF    ] : -- BUBBLE / NOP --
  [EX    ] : -- BUBBLE / NOP --
  [MA    ] : -- BUBBLE / NOP --
  [RW    ] : nop (PC: 0x00000048)
+--------------------------------------------------------------------------+
```

#### B. Cache Set-Way Grid (`visualizer/MemoryVisualizer.hpp`)
```
========================================================================
       CACHE STATE: L1D (32 KB, 8-Way, 64 B Lines)
========================================================================
Hit Rate: 94.12% (Hits: 32, Misses: 2, Writebacks: 0)
3C Misses: Compulsory: 2, Capacity: 0, Conflict: 0
------------------------------------------------------------------------
Set     Way 0       Way 1       Way 2       Way 3       ...
------------------------------------------------------------------------
[0   ]  [ INV ]     [ INV ]     [ INV ]     [ INV ]     
[1   ]  [ 0x0040 ]  [ INV ]     [ INV ]     [ INV ]     
[2   ]  [ 0x0080 ]  [ 0x1200 ]  [ INV ]     [ INV ]     
========================================================================
```

#### C. ASCII Stack Frame Visualizer (`visualizer/StackVisualizer.hpp`)
```
========================================================================
                      ACTIVE EXECUTION STACK
========================================================================
Address      Value         Role
------------------------------------------------------------------------
0x000FFFFC : 0x00000000    [Stack Base / Initial Frame]
0x000FFFF8 : 0x00000018    [Saved RA: main caller]
0x000FFFF4 : 0x00000003    [Local Param: n = 3]
0x000FFFF0 : 0x00000024    [Saved RA: fact_rec recursion]
0x000FFFEC : 0x00000002    [Local Param: n = 2]  <=== SP points here!
------------------------------------------------------------------------
Depth: 5 Words (20 Bytes) | Guard Page Distance: 1044460 Bytes (SAFE)
========================================================================
```

---

### 4. Summary: If You Only Remember 5 Things
1. The stack is **Full-Descending** starting at `0x000FFFFC`.
2. A **Stack Frame** stores return addresses (`ra`), frame pointers (`fp`), and local parameters.
3. The **Stack Guard Page** prevents stack overflows from corrupting adjacent heap/code memory.
4. Terminal ASCII visualizers provide immediate visibility into **Pipelines, Caches, DRAM, and Stack Frames**.
5. Teammate Varshith Teja contributed the ASCII trace visualizer, startup prompt, Newton-Raphson division, and CLI predictor flags.
