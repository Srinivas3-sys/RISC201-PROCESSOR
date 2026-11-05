# Module Notes: Microprogrammed Control Unit

## 1. Microprogrammed Architecture Overview (`microcode/`)
The simulator implements a complete microprogrammed control unit as an alternative to hardwired decode logic (fulfilling Requirements 1b and 3 of the course assignment).
* **Control Memory (ROM)**: Stores micro-instruction routines.
* **MicroPC ($\mu\text{PC}$)**: 10-bit program counter addressing the control memory.
* **Instruction Dispatch**: Maps the 5-bit canonical ISA opcode string to the starting address of the corresponding micro-routine in control store.
* **Dual Control Store Implementations**: Both **Horizontal** (unencoded) and **Vertical** (encoded) formats are implemented and compared.

---

## 2. Horizontal vs. Vertical Control Word Formats

### 2.1 Horizontal Micro-Instruction (`microcode/HorizontalControl.hpp`)
* **Width**: **65 bits** total.
* **Bit Allocation**:
  * $[32:0]$ (33 bits): Discrete hardware control signals (1 bit per datapath control line).
    * Bits 0-18 (19 bits): Register write-bus output enables (`CB_PC_OUT`, `CB_IR_OUT`, `CB_RD_OUT`, `CB_RS1_OUT`, `CB_ALU_RES_OUT`, `CB_MAR_OUT`, `CB_MDR_OUT`, etc.).
    * Bits 19-28 (10 bits): Register read-bus input enables (`CB_PC_IN`, `CB_A_IN`, `CB_B_IN`, `CB_MAR_IN`, `CB_MDR_IN`, `CB_LD_RES_IN`, etc.).
    * Bits 29-30 (2 bits): Transfer multiplexer select (`0=writeBus, 1=uimm, 2=uadder`).
    * Bits 31-32 (2 bits): $\mu\text{Fetch}$ multiplexer select (`0=next_upc, 1=branch_tgt, 2=switch_unit, 3=M1_cond`).
  * $[44:33]$ (12 bits): Micro-immediate constant (`uimm`).
  * $[54:45]$ (10 bits): Micro-branch target address (`ubranchTarget`).
  * $[64:55]$ (10 bits): Functional unit arguments (`args`).
* **Hardware Characteristics**: Zero decoding logic delay; widest ROM size; maximum micro-parallelism.

### 2.2 Vertical Micro-Instruction (`microcode/VerticalControl.hpp`)
* **Width**: **45 bits** total.
* **Bit Allocation**:
  * $[44:42]$ (3 bits): Micro-operation opcode (`MicroOp`: `MLOAD_IR`, `MDECODE`, `MREAD_REG`, `MALU`, `MLOAD_MEM`, `MSTORE_MEM`, `MBEQ`).
  * $[41:37]$ (5 bits): Encoded source register ID (`MicroReg`, up to 32 registers).
  * $[36:32]$ (5 bits): Encoded destination register ID (`MicroReg`, up to 32 registers).
  * $[31:20]$ (12 bits): Signed micro-immediate.
  * $[19:10]$ (10 bits): Micro-branch target address.
  * $[9:0]$ (10 bits): Functional unit argument flags.
* **Hardware Decoder (`MicroDecoder`)**:
  * Combinational logic expands the 45-bit packed vertical word into equivalent 33-bit horizontal control signals.
  * **Memory Savings**: $(65 - 45) / 65 = \mathbf{30.77\%}$ reduction in control store ROM bit count!

---

## 3. Micro-Routine Organization & Cycle Counts

| Routine Name | MicroPC Range | Micro-Cycles | Architectural Actions |
| :--- | :---: | :---: | :--- |
| **`uFetch`** | 0 – 3 | 4 cycles | $\text{MAR} \leftarrow \text{PC}$; $\text{MDR} \leftarrow \text{Mem}[\text{MAR}]$; $\text{IR} \leftarrow \text{MDR}$; $\text{PC} \leftarrow \text{PC} + 4$; Dispatch. |
| **`uAdd` / `uSub`**| 10 – 13 | 4 cycles | Read $r_{s1} \rightarrow A$; Read $r_{s2}/imm \rightarrow B$; $\text{ALU\_RES} \leftarrow \text{ALU}(A, B)$; Write $r_d \leftarrow \text{ALU\_RES}$. |
| **`uLd` (Load)** | 20 – 24 | 5 cycles | Compute $\text{MAR} \leftarrow r_{s1} + imm$; $\text{MDR} \leftarrow \text{Mem}[\text{MAR}]$; $r_d \leftarrow \text{MDR}$. |
| **`uSt` (Store)** | 30 – 33 | 4 cycles | Compute $\text{MAR} \leftarrow r_{s1} + imm$; $\text{MDR} \leftarrow r_d$; $\text{Mem}[\text{MAR}] \leftarrow \text{MDR}$. |
| **`uBeq` / `uBgt`**| 40 – 43 | 4 cycles | Test flag $E$ or $GT$; if true, $\text{PC} \leftarrow \text{PC} + (offset \times 4)$; return to `uFetch`. |
| **`uCall`** | 50 – 53 | 4 cycles | $r_{15} (\text{RA}) \leftarrow \text{PC}$; $\text{PC} \leftarrow \text{PC} + (offset \times 4)$. |
| **`uRet`** | 60 – 62 | 3 cycles | $\text{PC} \leftarrow r_{15} (\text{RA})$; return to `uFetch`. |
| **`uIllegal`** | 90 – 92 | 3 cycles | Trigger `ILLEGAL_OPCODE` exception; jump to trap handler. |

