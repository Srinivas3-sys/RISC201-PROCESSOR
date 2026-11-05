# Chapter 7: Control Unit: Hardwired & Dual Microprogrammed Controllers

## 7.1 Overview of Control Unit Implementations
The RISC201 processor provides two distinct control mechanisms:
1. **Hardwired Pipeline Decoders** (`processor_4stage/`, `processor_6stage/`): Combinational decoding logic that translates opcode strings directly into pipeline latch control signals with zero cycle delay.
2. **Microprogrammed Control Unit (`microcode/MicroController.hpp`)**: An autonomous execution engine that drives the datapath via sequences of micro-instructions stored in a dedicated **Control Memory (ROM)**, fulfilling Requirements 1b and 3 of the course assignment.

---

## 7.2 Microprogrammed Controller Architecture

### 7.2.1 Control Store Organization & MicroPC Sequencing
* **Control Memory**: A 1024-entry ROM addressed by a 10-bit **MicroPC ($\mu\text{PC}$)**.
* **Sequencing Modes**:
  1. **Sequential Step**: $\mu\text{PC} \leftarrow \mu\text{PC} + 1$.
  2. **Micro-Branch (`ubranchTarget`)**: $\mu\text{PC} \leftarrow \text{target}$.
  3. **Instruction Dispatch**: When an instruction is decoded, a dispatch table maps the 5-bit opcode string (e.g. `"00000"` for `add`) to the entry address of its micro-routine in control store.
  4. **Conditional Micro-Branch**: Branches based on datapath status flags (e.g. $E, GT$, zero bit).

```
          +---------------------------------------------------+
          |            Opcode String [31:27]                  |
          +-------------------------+-------------------------+
                                    |
                                    v
          +---------------------------------------------------+
          |      Microcode Dispatch Table (String -> uPC)     |
          +-------------------------+-------------------------+
                                    | Entry Address
                                    v
+----------+      +-------------------------------------------+
| MicroPC  | ===> |            Control Memory (ROM)           |
+----+-----+      +---------------------+---------------------+
     ^                                  |
     | Next uPC                         v
     +---------[ uFetch Multiplexer ]<--+ Micro-Instruction Word
```

---

## 7.3 Dual Micro-Instruction Formats: Horizontal vs. Vertical

### 7.3.1 Horizontal Micro-Instruction (`microcode/HorizontalControl.hpp`)
* **Total Word Width**: **65 bits**.
* **Field Breakdown**:
  * **Discrete Control Signals ($[32:0]$, 33 bits)**:
    * `CB_PC_OUT` (Bit 0): Directs `PC` onto the write bus.
    * `CB_IR_OUT` (Bit 1): Directs `IR` onto the write bus.
    * `CB_RD_OUT` (Bit 3): Directs `rd` register index to register file address port.
    * `CB_RS1_OUT` (Bit 4): Directs `rs1` to register file read port.
    * `CB_RS2_OUT` (Bit 5): Directs `rs2` to register file read port.
    * `CB_ALU_RES_OUT` (Bit 15): Directs ALU output register onto the bus.
    * `CB_MAR_OUT` (Bit 16): Directs Memory Address Register onto memory bus.
    * `CB_MDR_OUT` (Bit 17): Directs Memory Data Register onto memory bus.
    * `CB_PC_IN` (Bit 19): Latches bus value into `PC`.
    * `CB_A_IN` (Bit 23): Latches bus value into ALU operand register $A$.
    * `CB_B_IN` (Bit 24): Latches bus value into ALU operand register $B$.
    * `CB_MAR_IN` (Bit 26): Latches bus value into `MAR`.
    * `CB_MDR_IN` (Bit 27): Latches bus value into `MDR`.
    * `CB_XFER_MUX` (Bits 29-30): Transfer mux select (0=writeBus, 1=uimm, 2=uadder).
    * `CB_UFETCH_MUX` (Bits 31-32): Micro-sequencer next address select.
  * **Micro-Immediate (`uimm`, $[44:33]$, 12 bits)**: Signed 12-bit constant.
  * **Micro-Branch Target (`ubranchTarget`, $[54:45]$, 10 bits)**: Control store branch target.
  * **Functional Arguments (`args`, $[64:55]$, 10 bits)**: Flags for ALU function code.

### 7.3.2 Vertical Micro-Instruction (`microcode/VerticalControl.hpp`)
* **Total Word Width**: **45 bits** (packed into a 64-bit integer container).
* **Field Breakdown**:
  * $[44:42]$ (3 bits): Micro-operation opcode (`MicroOp`: `MLOAD_IR`, `MDECODE`, `MREAD_REG`, `MALU`, `MLOAD_MEM`, `MSTORE_MEM`, `MBEQ`).
  * $[41:37]$ (5 bits): Source Register ID (`MicroReg`, up to 32 registers).
  * $[36:32]$ (5 bits): Destination Register ID (`MicroReg`, up to 32 registers).
  * $[31:20]$ (12 bits): Signed micro-immediate.
  * $[19:10]$ (10 bits): Micro-branch target address.
  * $[9:0]$ (10 bits): Functional argument flags.
* **Hardware Micro-Decoder (`MicroDecoder`)**:
  * Decodes the 3-bit `MicroOp` and 5-bit register fields into the 33 discrete horizontal signals.
  * **Area Savings**: Consumes 45 bits per entry versus 65 bits for horizontal microcode:
    $$\text{ROM Bit Reduction} = \frac{65 - 45}{65} \times 100\% = \mathbf{30.77\%}$$

---

## 7.4 Micro-Routine Catalog & Cycle Breakdown

Table 7.1 catalogs the micro-routines stored in control memory, their $\mu\text{PC}$ entry addresses, and their cycle costs:

| Micro-Routine | Entry $\mu\text{PC}$ | Micro-Cycles | Micro-Operation Sequence |
| :--- | :---: | :---: | :--- |
| **`uFetch`** | 0 | 4 | $\text{MAR} \leftarrow \text{PC}$; $\text{MDR} \leftarrow \text{Mem}[\text{MAR}]$; $\text{IR} \leftarrow \text{MDR}$; $\text{PC} \leftarrow \text{PC} + 4$; Dispatch. |
| **`uAdd` / `uSub`** | 10 | 4 | $A \leftarrow \text{Reg}[r_{s1}]$; $B \leftarrow \text{Reg}[r_{s2}] \mid imm$; $\text{ALU\_RES} \leftarrow A \pm B$; $\text{Reg}[r_d] \leftarrow \text{ALU\_RES}$. |
| **`uMul`** | 15 | 16 | Multicycle Booth multiplier execution; writeback to $r_d$. |
| **`uDiv` / `uMod`** | 20 | 32 | Multicycle divider execution; writeback to $r_d$; check zero divisor. |
| **`uCmp`** | 25 | 3 | $A \leftarrow \text{Reg}[r_{s1}]$; $B \leftarrow \text{Reg}[r_{s2}] \mid imm$; $\text{FLAGS} \leftarrow \text{Compare}(A, B)$. |
| **`uLd` (Load)** | 30 | 5 | $A \leftarrow \text{Reg}[r_{s1}]$; $\text{MAR} \leftarrow A + imm$; $\text{MDR} \leftarrow \text{Mem}[\text{MAR}]$; $\text{Reg}[r_d] \leftarrow \text{MDR}$. |
| **`uSt` (Store)** | 40 | 4 | $A \leftarrow \text{Reg}[r_{s1}]$; $\text{MAR} \leftarrow A + imm$; $\text{MDR} \leftarrow \text{Reg}[r_d]$; $\text{Mem}[\text{MAR}] \leftarrow \text{MDR}$. |
| **`uBeq` / `uBgt`** | 50 | 4 | Evaluate flag ($E$ or $GT$); if true, $\text{PC} \leftarrow \text{PC} + (\text{Offset} \times 4)$; return to `uFetch`. |
| **`uB` (Uncond)** | 55 | 3 | $\text{PC} \leftarrow \text{PC} + (\text{Offset} \times 4)$; return to `uFetch`. |
| **`uCall`** | 60 | 4 | $r_{15} (\text{RA}) \leftarrow \text{PC}$; $\text{PC} \leftarrow \text{PC} + (\text{Offset} \times 4)$; return to `uFetch`. |
| **`uRet`** | 70 | 3 | $\text{PC} \leftarrow r_{15} (\text{RA})$; return to `uFetch`. |
| **`uMovz`** | 80 | 3 | Transfer general register $\leftrightarrow$ privileged register. |
| **`uRetz`** | 85 | 3 | Restore $\text{PC} \leftarrow \text{EPC}$; set privilege $\text{CPL} \leftarrow 1$. |
| **`uIllegal`** | 90 | 3 | Save $\text{EPC} \leftarrow \text{PC}$; set $\text{ECAUSE} \leftarrow \text{ILLEGAL\_OPCODE}$; jump to kernel trap. |

---

## 7.5 Step-by-Step MicroPC Trace Output

Using the interactive debugger command `micro` or `stepCycle()` in microcode mode (`mode micro`), the simulator outputs the exact cycle-by-cycle micro-state:

```
RISC201[Micro]> s
MicroCycle 1 | MicroPC: 0 | Routine: uFetch
  MicroInst: MAR <- PC (PC=0x00000000)
  Signals: [CB_PC_OUT, CB_MAR_IN]

RISC201[Micro]> s
MicroCycle 2 | MicroPC: 1 | Routine: uFetch
  MicroInst: MDR <- Mem[MAR] (Reading address 0x00000000)
  Signals: [CB_MAR_OUT, CB_MDR_IN]

RISC201[Micro]> s
MicroCycle 3 | MicroPC: 2 | Routine: uFetch
  MicroInst: IR <- MDR (IR=0x00C48000), PC <- PC + 4 (PC=0x00000004)
  Signals: [CB_MDR_OUT, CB_IR_IN, CB_PC_IN, CB_XFER_MUX_ADDER]

RISC201[Micro]> s
MicroCycle 4 | MicroPC: 3 | Routine: uFetch
  MicroInst: Dispatch to opcode '00000' (Target uPC: 10)
  Signals: [CB_UFETCH_MUX_SWITCH]
>>> Dispatching to uAdd Routine <<<

RISC201[Micro]> s
MicroCycle 5 | MicroPC: 10 | Routine: uAdd
  MicroInst: A <- Reg[r1] (A=5)
  Signals: [CB_RS1_OUT, CB_A_IN]
```

