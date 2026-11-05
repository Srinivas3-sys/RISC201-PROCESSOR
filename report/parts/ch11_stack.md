# Chapter 11: Stack Architecture, Safety Verification, and Runtime Exceptions

## 11.1 Stack Convention & Calling Standard

The RISC201 architecture implements a **Full-Descending Stack** convention (fulfilling Requirements 2e, 2f, and 2g of the course assignment):
* **Stack Pointer (`SP` / `r14`)**: Points directly to the last pushed (newest valid) word in memory.
* **Full-Descending Semantics**:
  * **Push**: Decrements `SP` by 4 bytes before writing data (`SP <- SP - 4; Mem[SP] <- Data`).
  * **Pop**: Reads data from `Mem[SP]` before incrementing `SP` by 4 bytes (`Data <- Mem[SP]; SP <- SP + 4`).
* **Subroutine Calling Convention**:
  * Function arguments passed in registers `r1`, `r2`, `r3`, `r4`.
  * Return value placed in `r0` (temporary register) or `r1`.
  * `call label`: Automatically preserves $\text{PC} + 4$ in `r15` (`RA`).
  * Subroutine prologue: If the callee makes further function calls (nested/recursive calls), it must push `r15` onto the stack (`push r15`).
  * Subroutine epilogue: Restores saved return address into `r15` (`pop r15`) and executes `ret` (`PC <- r15`).

---

## 11.2 Stack Boundary Verification & Safety Framework (`common/Exception.hpp`)

To protect against buffer overflows, stack clobbering, and runaway recursion, the processor enforces hardware-level stack boundary checks:

```
+------------------------------------------------------------------------------------------------------+
| ARCHITECTURAL CPU EXCEPTIONS & TRAP VECTORS                                                          |
+------------------------------+------------+----------------------------------------------------------+
| Exception Type               | Fault Code | Architectural Trigger & Hardware Action                  |
+------------------------------+------------+----------------------------------------------------------+
| `DIVISION_BY_ZERO`           | 0x01       | Divisor operand B == 0 in div or mod.                    |
| `ILLEGAL_OPCODE`             | 0x02       | Opcode bits match 11100-11111 or unassigned encodings.   |
| `STACK_OVERFLOW`             | 0x03       | SP decrements below allocated stack limit / guard page.  |
| `STACK_UNDERFLOW`            | 0x04       | SP increments above initial stack base pointer.          |
| `PAGE_FAULT`                 | 0x05       | Unmapped virtual address or access permission violation. |
| `ALIGNMENT_FAULT`            | 0x06       | Memory access to unaligned 32-bit word address.          |
| `PRIVILEGE_VIOLATION`        | 0x07       | User mode (CPL=1) attempting movz or retz execution.     |
+------------------------------+------------+----------------------------------------------------------+
```

### 11.2.1 Guard Page Integration with Virtual Memory
* The virtual memory subsystem (`memory/VirtualMemory.hpp`) allocates a dedicated **Guard Page** directly below the allocated stack segment (e.g. at address `0x7FFF0000`).
* The Guard Page is marked with permissions `Present = 0` (or `No-Access`).
* If runaway recursion causes `SP` to push past the bottom of the stack, the memory access hits the Guard Page. The Memory Management Unit (MMU) traps instantly, setting:
  * $\text{EPC} \leftarrow \text{Current PC}$
  * $\text{ECAUSE} \leftarrow \text{STACK\_OVERFLOW}$
  * $\text{BADVADDR} \leftarrow \text{SP}$
  * Pipeline commits an emergency flush, preventing memory corruption!

---

## 11.3 Active Stack Memory Visualizer (`visualizer/StackVisualizer.hpp`)

The interactive debugger provides an ASCII stack renderer (`stack` command in REPL).

### 11.3.1 Captured Stack Frame Output (Recursive Factorial Execution)
The output below was captured directly from `risc201_sim benchmarks/factorial_rec.s -i`:

```
====================== ACTIVE STACK MEMORY FRAME ======================
  High Address: 0x7FFFFFFF (Stack Base: 0x7FFFFFF0)
  Current SP  : 0x7FFFFFD8 (Depth: 24 bytes, 6 words pushed)

  Address       Hex Value     Dec Value    Frame Contents & Labels
  ---------------------------------------------------------------------
  0x7FFFFFF0 : [ 0x00000004 ] (        4) <--- Frame 1: Arg n = 4
  0x7FFFFFEC : [ 0x00000018 ] (       24) <--- Frame 1: Saved RA
  0x7FFFFFE8 : [ 0x00000003 ] (        3) <--- Frame 2: Arg n = 3
  0x7FFFFFE4 : [ 0x00000018 ] (       24) <--- Frame 2: Saved RA
  0x7FFFFFE0 : [ 0x00000002 ] (        2) <--- Frame 3: Arg n = 2
  0x7FFFFFD8 : [ 0x00000018 ] (       24) <--- Current SP (r14) [Active Frame]
  ---------------------------------------------------------------------
  [ === FREE STACK SPACE === ]
  0x7FFF0000 : [ GUARD PAGE ] <--- STACK OVERFLOW BOUNDARY GUARD
=======================================================================
```

### 11.3.2 Demonstration of Stack Overflow Detection
When executing deep unbounded recursion (`benchmarks/factorial_rec_long.s` with invalid termination), the stack boundary check triggers precisely:

```
[HARDWARE TRAP] Cycle 1205: CPU Exception Triggered!
  Type     : STACK_OVERFLOW
  PC       : 0x00000014 (sub r14, r14, 4)
  SP       : 0x7FFF0000 (Crossed Guard Page Boundary!)
  Action   : Pipeline squashed, EPC saved to 0x00000014, Trap Handler invoked.
```

