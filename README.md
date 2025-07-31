# RV32IM RISC-V Simulator

A functional, command-line simulator for the 32-bit RISC-V architecture, supporting the base integer instruction set (I) and the multiplication/division extension (M).

<sub> Further updates with Uart input and output, interruptions and exceptions implementation will come soon, im just waiting for the submission and review day of the project in the course pass, to avoid plagiarism issues as recommended by the teacher.</sub>

## Project Context

This is first part of a simulator developed as a project for the **Computer Architecture** class. Its main goal is to provide a hands-on, low-level understanding of how a processor fetches, decodes, and executes instructions.

## Features

- **Supported Instruction Set:** Implements the RV32I Base Integer ISA and the "M" Standard Extension for Integer Multiplication and Division.
- **Instruction Cycle Simulation:** Executes the fetch, decode, and execute cycle for each instruction.
- **Trace File Generation:** Creates a detailed output log that reflects the state of registers and memory at each step.

### Compilation

To compile the simulator, you can use any C compiler that supports the C99 standard.  
Simply compile the main source file (src/RiscV.c) using your preferred compiler.

For example, with GCC:

```gcc -std=c99 -Wall src/RiscV.c -o riscv-sim```

### Execution

To run the simulator with one of the provided examples, use the following command:

```./riscv-sim examples/1_factorial.hex trace_output.txt```

This will execute the 1_factorial.hex program and write the detailed execution log to trace_output.txt.

<sub>🚧 In the future update, the simulator will support UART input/output through terminal.in and terminal.out, adding two additional command-line arguments.</sub>

### Input and Output Formats

#### Input File (.hex)
The simulator uses the Verilog HEX format for its memory input file. This is a common standard in hardware design for initializing memories in simulations and FPGAs. The format is as follows:

- A line starting with @<hex_address> sets the memory location where the following data will be written.

- Subsequent lines contain the program's machine code bytes, written in hexadecimal and separated by spaces.

#### Output Trace File
The output log format was designed to follow the specifications provided by the course professor to the project. It details the effect of each executed instruction. Below are a few examples of the output format:

- Load Word (lw): ```0x????????:lw     rd,0x???(rs1)  rd=mem[0x????????]=0x????????```

- Add (add): ```0x????????:add    rd,rs1,rs2     rd=0x????????+0x????????=0x????????```

- Branch if Equal (beq): ```0x????????:beq    rs1,rs2,0x???  (0x????????==0x????????)=u1->pc=0x????????```

- Jump and Link (jal): ```0x????????:jal    rd,0x?????     pc=0x????????,rd=0x????????```

All the log format info its contained in OUTPUT.txt file.

#### Advanced Usage (Optional)
For users who wish to write and test their own assembly programs, a RISC-V GNU Toolchain and make can be used to convert a .s assembly file into the required .hex format. This is not necessary to run the included examples.

#### Reference Documentation
This simulator was developed based on the official RISC-V architecture specifications. The manuals can be found at the official website:

- The RISC-V Instruction Set Manual, Volume I: Unprivileged ISA

- The RISC-V Instruction Set Manual, Volume II: Privileged Architecture

- RISC-V Green Card: A quick reference for the RISC-V instruction set architecture, invaluable for understanding instruction formats and opcodes.


### License
This project is distributed under the MIT License. See the LICENSE file for more details.
