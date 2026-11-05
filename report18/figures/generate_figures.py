import os

# 1. fig8_instruction_formats.svg
svg8 = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 240" width="100%" height="100%" style="background:#ffffff; font-family: sans-serif;">
  <rect x="10" y="10" width="820" height="220" rx="6" fill="#fcfcfc" stroke="#333333" stroke-width="1.2"/>
  <text x="420" y="32" text-anchor="middle" font-size="14" font-weight="bold" fill="#111111">SimpleRisc 32-Bit Instruction Word Formats</text>

  <!-- Format 1: Register Format -->
  <text x="30" y="62" font-size="11" font-weight="bold" fill="#222222">1. Register Format (R-Type): [31:27] Opcode, [26] I=0, [25:22] rd, [21:18] rs1, [17:14] rs2, [13:0] Unused</text>
  <g transform="translate(30, 70)">
    <rect x="0" y="0" width="125" height="26" fill="#333333" stroke="#111111"/>
    <text x="62" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">Opcode [31:27] (5b)</text>

    <rect x="125" y="0" width="30" height="26" fill="#777777" stroke="#111111"/>
    <text x="140" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">0</text>

    <rect x="155" y="0" width="95" height="26" fill="#eeeeee" stroke="#111111"/>
    <text x="202" y="17" text-anchor="middle" font-size="10" fill="#111111">rd [25:22] (4b)</text>

    <rect x="250" y="0" width="95" height="26" fill="#dddddd" stroke="#111111"/>
    <text x="297" y="17" text-anchor="middle" font-size="10" fill="#111111">rs1 [21:18] (4b)</text>

    <rect x="345" y="0" width="95" height="26" fill="#eeeeee" stroke="#111111"/>
    <text x="392" y="17" text-anchor="middle" font-size="10" fill="#111111">rs2 [17:14] (4b)</text>

    <rect x="440" y="0" width="340" height="26" fill="#f8f8f8" stroke="#111111" stroke-dasharray="3,3"/>
    <text x="610" y="17" text-anchor="middle" font-size="10" fill="#666666">Unused / Reserved [13:0] (14 bits)</text>
  </g>

  <!-- Format 2: Immediate Format -->
  <text x="30" y="122" font-size="11" font-weight="bold" fill="#222222">2. Immediate Format (I-Type): [31:27] Opcode, [26] I=1, [25:22] rd, [21:18] rs1, [17:16] Mod, [15:0] imm16</text>
  <g transform="translate(30, 130)">
    <rect x="0" y="0" width="125" height="26" fill="#333333" stroke="#111111"/>
    <text x="62" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">Opcode [31:27] (5b)</text>

    <rect x="125" y="0" width="30" height="26" fill="#555555" stroke="#111111"/>
    <text x="140" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">1</text>

    <rect x="155" y="0" width="95" height="26" fill="#eeeeee" stroke="#111111"/>
    <text x="202" y="17" text-anchor="middle" font-size="10" fill="#111111">rd [25:22] (4b)</text>

    <rect x="250" y="0" width="95" height="26" fill="#dddddd" stroke="#111111"/>
    <text x="297" y="17" text-anchor="middle" font-size="10" fill="#111111">rs1 [21:18] (4b)</text>

    <rect x="345" y="0" width="55" height="26" fill="#cccccc" stroke="#111111"/>
    <text x="372" y="17" text-anchor="middle" font-size="9" fill="#111111">Mod[17:16]</text>

    <rect x="400" y="0" width="380" height="26" fill="#ffffff" stroke="#111111"/>
    <text x="590" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#111111">imm16 [15:0] (16-bit signed / unsigned immediate constant)</text>
  </g>

  <!-- Format 3: Branch Format -->
  <text x="30" y="182" font-size="11" font-weight="bold" fill="#222222">3. Branch Format (B-Type): [31:27] Opcode, [26:0] offset27 (PC-Relative Signed Word Offset)</text>
  <g transform="translate(30, 190)">
    <rect x="0" y="0" width="125" height="26" fill="#333333" stroke="#111111"/>
    <text x="62" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">Opcode [31:27] (5b)</text>

    <rect x="125" y="0" width="655" height="26" fill="#f0f0f0" stroke="#111111"/>
    <text x="452" y="17" text-anchor="middle" font-size="10" font-weight="bold" fill="#111111">offset27 [26:0] (27-bit signed PC-relative word offset, branch target = PC + offset27 * 4)</text>
  </g>
</svg>"""

with open("report18/figures/fig8_instruction_formats.svg", "w") as fp:
    fp.write(svg8)

# 2. fig9_toolchain_flow.svg
svg9 = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 220" width="100%" height="100%" style="background:#ffffff; font-family: sans-serif;">
  <defs>
    <marker id="arrowB" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 8 5 L 0 9 z" fill="#222222"/>
    </marker>
  </defs>
  <rect x="10" y="10" width="820" height="200" rx="6" fill="#fcfcfc" stroke="#333333" stroke-width="1.2"/>
  <text x="420" y="32" text-anchor="middle" font-size="14" font-weight="bold" fill="#111111">RISC201 Software Toolchain &amp; Execution Simulator Dataflow</text>

  <!-- Assembly File -->
  <rect x="30" y="65" width="100" height="60" rx="4" fill="#ffffff" stroke="#222222" stroke-width="1.2"/>
  <text x="80" y="92" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Assembly</text>
  <text x="80" y="108" text-anchor="middle" font-size="9" fill="#555555">.s Source</text>

  <line x1="130" y1="95" x2="165" y2="95" stroke="#222222" stroke-width="1.5" marker-end="url(#arrowB)"/>

  <!-- Preprocessor & Assembler -->
  <rect x="170" y="55" width="140" height="80" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="240" y="78" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">risc201_as</text>
  <text x="240" y="95" text-anchor="middle" font-size="9" fill="#333333">Pass 1: Symbol Table</text>
  <text x="240" y="110" text-anchor="middle" font-size="9" fill="#333333">Pass 2: Encoding</text>
  <text x="240" y="125" text-anchor="middle" font-size="8.5" fill="#666666">Macro Expansion</text>

  <line x1="310" y1="95" x2="345" y2="95" stroke="#222222" stroke-width="1.5" marker-end="url(#arrowB)"/>

  <!-- Machine Code -->
  <rect x="350" y="65" width="105" height="60" rx="4" fill="#ffffff" stroke="#222222" stroke-width="1.2"/>
  <text x="402" y="90" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Binary / Hex</text>
  <text x="402" y="106" text-anchor="middle" font-size="9" fill="#555555">.hex / .bin</text>

  <line x1="455" y1="95" x2="490" y2="95" stroke="#222222" stroke-width="1.5" marker-end="url(#arrowB)"/>

  <!-- Simulator Core -->
  <rect x="495" y="50" width="155" height="90" rx="4" fill="#222222" stroke="#111111" stroke-width="1.2"/>
  <text x="572" y="75" text-anchor="middle" font-size="11" font-weight="bold" fill="#ffffff">risc201_sim</text>
  <text x="572" y="92" text-anchor="middle" font-size="9" fill="#cccccc">4-Stage (500 MHz)</text>
  <text x="572" y="106" text-anchor="middle" font-size="9" fill="#cccccc">6-Stage (800 MHz)</text>
  <text x="572" y="122" text-anchor="middle" font-size="8.5" fill="#aaaaaa">Forwarding &amp; Hazard Units</text>

  <line x1="650" y1="95" x2="685" y2="95" stroke="#222222" stroke-width="1.5" marker-end="url(#arrowB)"/>

  <!-- UI & Diagnostics -->
  <rect x="690" y="55" width="125" height="80" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="752" y="78" text-anchor="middle" font-size="10.5" font-weight="bold" fill="#111111">Diagnostics</text>
  <text x="752" y="95" text-anchor="middle" font-size="8.5" fill="#333333">CLI Debugger</text>
  <text x="752" y="110" text-anchor="middle" font-size="8.5" fill="#333333">Pipeline ASCII Art</text>
  <text x="752" y="125" text-anchor="middle" font-size="8.5" fill="#333333">Stack Visualizer</text>

  <!-- Bottom Disassembler Loop -->
  <path d="M 402 125 L 402 175 L 240 175 L 240 140" fill="none" stroke="#666666" stroke-width="1.2" stroke-dasharray="4,3" marker-end="url(#arrowB)"/>
  <text x="320" y="170" text-anchor="middle" font-size="8.5" fill="#555555">Round-Trip Disassembly Verification (risc201_disasm)</text>
</svg>"""

with open("report18/figures/fig9_toolchain_flow.svg", "w") as fp:
    fp.write(svg9)

# 3. fig10_microcode_control.svg
svg10 = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 240" width="100%" height="100%" style="background:#ffffff; font-family: sans-serif;">
  <rect x="10" y="10" width="820" height="220" rx="6" fill="#fcfcfc" stroke="#333333" stroke-width="1.2"/>
  <text x="420" y="32" text-anchor="middle" font-size="14" font-weight="bold" fill="#111111">Microcode Control Store Architecture: Horizontal (65b) vs Vertical (45b)</text>

  <!-- Horizontal Control Word -->
  <text x="30" y="62" font-size="11" font-weight="bold" fill="#222222">A. Horizontal Microinstruction (65 Bits): 33 Unencoded Control Signals directly driving Datapath</text>
  <g transform="translate(30, 70)">
    <rect x="0" y="0" width="370" height="28" fill="#333333" stroke="#111111"/>
    <text x="185" y="18" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">33 Discrete Datapath Control Signals [64:32]</text>

    <rect x="370" y="0" width="135" height="28" fill="#777777" stroke="#111111"/>
    <text x="437" y="18" text-anchor="middle" font-size="10" fill="#ffffff">uimm [31:20] (12b)</text>

    <rect x="505" y="0" width="135" height="28" fill="#aaaaaa" stroke="#111111"/>
    <text x="572" y="18" text-anchor="middle" font-size="10" fill="#111111">ubranch [19:10] (10b)</text>

    <rect x="640" y="0" width="140" height="28" fill="#dddddd" stroke="#111111"/>
    <text x="710" y="18" text-anchor="middle" font-size="10" fill="#111111">args [9:0] (10b)</text>
  </g>

  <!-- Vertical Control Word -->
  <text x="30" y="132" font-size="11" font-weight="bold" fill="#222222">B. Vertical Microinstruction (45 Bits): Encoded Function Fields with Micro-Decoder (30.8% Footprint Cut)</text>
  <g transform="translate(30, 140)">
    <rect x="0" y="0" width="70" height="28" fill="#333333" stroke="#111111"/>
    <text x="35" y="18" text-anchor="middle" font-size="9.5" font-weight="bold" fill="#ffffff">Type (3b)</text>

    <rect x="70" y="0" width="80" height="28" fill="#555555" stroke="#111111"/>
    <text x="110" y="18" text-anchor="middle" font-size="9.5" fill="#ffffff">src (5b)</text>

    <rect x="150" y="0" width="80" height="28" fill="#777777" stroke="#111111"/>
    <text x="190" y="18" text-anchor="middle" font-size="9.5" fill="#ffffff">dest (5b)</text>

    <rect x="230" y="0" width="170" height="28" fill="#aaaaaa" stroke="#111111"/>
    <text x="315" y="18" text-anchor="middle" font-size="9.5" fill="#111111">immediate [31:20] (12b)</text>

    <rect x="400" y="0" width="170" height="28" fill="#cccccc" stroke="#111111"/>
    <text x="485" y="18" text-anchor="middle" font-size="9.5" fill="#111111">branchTarget [19:10] (10b)</text>

    <rect x="570" y="0" width="210" height="28" fill="#eeeeee" stroke="#111111"/>
    <text x="675" y="18" text-anchor="middle" font-size="9.5" fill="#111111">args [9:0] (10b)</text>
  </g>

  <text x="30" y="198" font-size="9.5" fill="#444444">• Horizontal: 140 words × 65 bits = 9,100 bits | Direct execution without decode delay.</text>
  <text x="30" y="213" font-size="9.5" fill="#444444">• Vertical: 140 words × 45 bits = 6,300 bits | Hardware decoder expands fields to 33 datapath control signals.</text>
</svg>"""

with open("report18/figures/fig10_microcode_control.svg", "w") as fp:
    fp.write(svg10)

# 4. fig11_alu_structure.svg
svg11 = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 240" width="100%" height="100%" style="background:#ffffff; font-family: sans-serif;">
  <rect x="10" y="10" width="820" height="220" rx="6" fill="#fcfcfc" stroke="#333333" stroke-width="1.2"/>
  <text x="420" y="32" text-anchor="middle" font-size="14" font-weight="bold" fill="#111111">Modular Arithmetic Logic Unit (ALU) Architecture</text>

  <!-- Adders -->
  <rect x="30" y="55" width="175" height="135" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="117" y="78" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Binary Adders</text>
  <line x1="45" y1="85" x2="190" y2="85" stroke="#cccccc"/>
  <text x="45" y="105" font-size="9.5" fill="#333333">• Ripple-Carry (RCA)</text>
  <text x="45" y="125" font-size="9.5" fill="#333333">• Carry-Lookahead (CLA)</text>
  <text x="45" y="145" font-size="9.5" fill="#333333">• Carry-Select (CSLA)</text>
  <text x="45" y="172" font-size="8.5" fill="#666666">Flags: Z, N, C, V</text>

  <!-- Shifter -->
  <rect x="225" y="55" width="175" height="135" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="312" y="78" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Barrel Shifter</text>
  <line x1="240" y1="85" x2="385" y2="85" stroke="#cccccc"/>
  <text x="240" y="105" font-size="9.5" fill="#333333">• Logarithmic 5-Stage</text>
  <text x="240" y="125" font-size="9.5" fill="#333333">• LSL (Logical Left)</text>
  <text x="240" y="145" font-size="9.5" fill="#333333">• LSR (Logical Right)</text>
  <text x="240" y="165" font-size="9.5" fill="#333333">• ASR (Arithmetic Right)</text>
  <text x="240" y="180" font-size="8.5" fill="#666666">1-cycle latency</text>

  <!-- Multipliers -->
  <rect x="420" y="55" width="185" height="135" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="512" y="78" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Multipliers</text>
  <line x1="435" y1="85" x2="590" y2="85" stroke="#cccccc"/>
  <text x="435" y="105" font-size="9.5" fill="#333333">• Sequential Shift-Add</text>
  <text x="435" y="125" font-size="9.5" fill="#333333">• Booth Radix-4 Recoding</text>
  <text x="435" y="145" font-size="9.5" fill="#333333">• Wallace Tree 3:2 CSA</text>
  <text x="435" y="172" font-size="8.5" fill="#666666">Signed 64-bit UV product</text>

  <!-- Dividers -->
  <rect x="625" y="55" width="185" height="135" rx="4" fill="#f4f4f4" stroke="#222222" stroke-width="1.2"/>
  <text x="717" y="78" text-anchor="middle" font-size="11" font-weight="bold" fill="#111111">Dividers</text>
  <line x1="640" y1="85" x2="795" y2="85" stroke="#cccccc"/>
  <text x="640" y="105" font-size="9.5" fill="#333333">• Restoring Algorithm</text>
  <text x="640" y="125" font-size="9.5" fill="#333333">• Non-Restoring Algorithm</text>
  <text x="640" y="145" font-size="9.5" fill="#333333">• Newton-Raphson Reciprocal</text>
  <text x="640" y="172" font-size="8.5" fill="#666666">Division-by-zero detection</text>

  <text x="420" y="210" text-anchor="middle" font-size="9.5" fill="#444444">All functional units are coordinated by ALU.hpp keyed by canonical 5-bit string opcodes.</text>
</svg>"""

with open("report18/figures/fig11_alu_structure.svg", "w") as fp:
    fp.write(svg11)

# 5. fig12_stack_memory_frame.svg
svg12 = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 230" width="100%" height="100%" style="background:#ffffff; font-family: sans-serif;">
  <rect x="10" y="10" width="820" height="210" rx="6" fill="#fcfcfc" stroke="#333333" stroke-width="1.2"/>
  <text x="420" y="32" text-anchor="middle" font-size="14" font-weight="bold" fill="#111111">Full-Descending Stack Memory Frame and Protection Guard Architecture</text>

  <!-- Stack diagram -->
  <g transform="translate(50, 50)">
    <!-- High memory: Stack Base -->
    <rect x="0" y="0" width="450" height="28" fill="#dddddd" stroke="#111111"/>
    <text x="225" y="18" text-anchor="middle" font-size="10" font-weight="bold" fill="#111111">Stack Base (0x000F F000) - Initial SP Position</text>
    <text x="465" y="18" font-size="10" fill="#444444">&lt;-- Initial SP (r15)</text>

    <!-- Frame 1 -->
    <rect x="0" y="28" width="450" height="28" fill="#ffffff" stroke="#111111"/>
    <text x="225" y="46" text-anchor="middle" font-size="9.5" fill="#222222">Caller Saved Return Address: RA (r14 = PC + 4)</text>

    <!-- Frame 2 -->
    <rect x="0" y="56" width="450" height="28" fill="#f8f8f8" stroke="#111111"/>
    <text x="225" y="74" text-anchor="middle" font-size="9.5" fill="#222222">Saved Register r1 / Local Arguments</text>

    <!-- Current SP -->
    <rect x="0" y="84" width="450" height="28" fill="#eeeeee" stroke="#111111"/>
    <text x="225" y="102" text-anchor="middle" font-size="9.5" font-weight="bold" fill="#111111">Top of Stack (Current SP: 0x000F EFF4) &lt;-- Valid Data</text>
    <text x="465" y="102" font-size="10" font-weight="bold" fill="#111111">&lt;-- Current SP (Points to last pushed item)</text>

    <!-- Growth direction -->
    <rect x="0" y="112" width="450" height="22" fill="#fafafa" stroke="#111111" stroke-dasharray="3,3"/>
    <text x="225" y="127" text-anchor="middle" font-size="9" fill="#777777">Stack Growth Downward (SP decreases on Push: sub sp, sp, 4)</text>

    <!-- Guard Band -->
    <rect x="0" y="134" width="450" height="26" fill="#333333" stroke="#111111"/>
    <text x="225" y="151" text-anchor="middle" font-size="10" font-weight="bold" fill="#ffffff">Stack Limit &amp; Guard Page (0x000E 0000) - STACK_OVERFLOW Exception</text>
    <text x="465" y="151" font-size="10" font-weight="bold" fill="#111111">&lt;-- Hardware Boundary Check</text>
  </g>
</svg>"""

with open("report18/figures/fig12_stack_memory_frame.svg", "w") as fp:
    fp.write(svg12)

print("Generated all 5 new vector SVGs (fig8 through fig12).")

