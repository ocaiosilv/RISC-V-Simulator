#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 

// function prototypes
int32_t sign_extension(uint32_t value, int bits);
void hex_file_to_memory(FILE *openfile, uint8_t *mem_array, uint32_t offset);
uint32_t read_word_from_mem(const uint8_t *mem_array, uint32_t array_pos_idx);
int instruction_read(uint32_t instruction, uint32_t *registers, uint32_t *pc, const char **registers_label, uint8_t *memory, FILE *output_file, const uint32_t mem_offset);

// extends a smaller number to 32 bits, keeping the sign
int32_t sign_extension(uint32_t value, int bits) {
    if (value & (1 << (bits - 1))) {
        return (int32_t)(value - (1 << bits));
    }
    return (int32_t)value;
}

// reads a hex file and loads it into our memory array
void hex_file_to_memory(FILE *openfile, uint8_t *mem_array, uint32_t offset) {
    char line_buffer[256];
    uint32_t actual_hex_pos = 0;
    int32_t next_mem_pos = -1;

    while (fgets(line_buffer, sizeof(line_buffer), openfile) != NULL) {
        if (line_buffer[0] == '\n' || line_buffer[0] == '\0') {
            continue;
        }

        // '@' indicates a new memory address
        if (line_buffer[0] == '@') {
            sscanf(line_buffer, "@%x", &actual_hex_pos);
            next_mem_pos = actual_hex_pos - offset;
        } else {
            char *ptr = line_buffer;
            int chars_consumed;
            uint32_t byte_value;
            while (sscanf(ptr, "%x%n", &byte_value, &chars_consumed) > 0) {
                mem_array[next_mem_pos] = (uint8_t)byte_value;
                next_mem_pos++;
                ptr += chars_consumed;
            }
        }
    }
}

// reads a 4-byte word from memory (little-endian)
uint32_t read_word_from_mem(const uint8_t *mem_array, uint32_t array_pos_idx) {
    uint32_t byte0 = mem_array[array_pos_idx];
    uint32_t byte1 = mem_array[array_pos_idx + 1];
    uint32_t byte2 = mem_array[array_pos_idx + 2];
    uint32_t byte3 = mem_array[array_pos_idx + 3];
    return (byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24));
}

// this is the core function, decodes and executes one instruction
int instruction_read(uint32_t instruction, uint32_t *registers, uint32_t *pc, const char **registers_label, uint8_t *memory, FILE *output_file, const uint32_t mem_offset) {
    // breaking down the instruction word into fields
    uint32_t opcode = instruction & 0x7F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t funct7 = (instruction >> 25) & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;

    // immediates for each instruction type, need to calculate them all
    int32_t imm_itype = sign_extension((instruction >> 20), 12);
    int32_t imm_stype = sign_extension(((instruction >> 25) << 5) | ((instruction >> 7) & 0x1F), 12);
    // B-type immediate is weird, need to shuffle bits
    int32_t imm_btype = sign_extension((((instruction >> 31) & 0x1) << 12) | (((instruction >> 7) & 0x1) << 11) | (((instruction >> 25) & 0x3F) << 5) | (((instruction >> 8) & 0xF) << 1), 13);
    uint32_t imm_utype = instruction & 0xFFFFF000;
    // J-type immediate is also weird, more bit shuffling
    int32_t imm_jtype = sign_extension((((instruction >> 31) & 0x1) << 20) | (((instruction >> 12) & 0xFF) << 12) | (((instruction >> 20) & 0x1) << 11) | (((instruction >> 21) & 0x3FF) << 1), 21);

    // get values from source registers
    uint32_t rs1_val = registers[rs1];
    uint32_t rs2_val = registers[rs2];
    int32_t rs1_signed = (int32_t)rs1_val;
    int32_t rs2_signed = (int32_t)rs2_val;
    
    int keep_run = 1;
    uint32_t current_pc = *pc;
    *pc += 4; // PC standard increment, branches will override this

    switch (opcode) {

            // Load-Type
        case 0b0000011: { 
            uint32_t address = rs1_val + imm_itype;
            uint32_t mem_index = address - mem_offset;
            uint32_t value = 0;
            switch(funct3){
                case 0b000: // LB
                    value = (uint32_t)sign_extension(memory[mem_index], 8);
                    fprintf(output_file, "0x%08x:lb     %s,0x%03x(%s)  %s=mem[0x%08x]=0x%08x\n", current_pc, registers_label[rd], imm_itype & 0xFFF, registers_label[rs1], registers_label[rd], address, value);
                    break;
                case 0b001: // LH
                    value = (uint32_t)sign_extension(memory[mem_index] | (memory[mem_index+1] << 8), 16);
                    fprintf(output_file, "0x%08x:lh     %s,0x%03x(%s)  %s=mem[0x%08x]=0x%08x\n", current_pc, registers_label[rd], imm_itype & 0xFFF, registers_label[rs1], registers_label[rd], address, value);
                    break;
                case 0b010: // LW
                    value = read_word_from_mem(memory, mem_index);
                    fprintf(output_file, "0x%08x:lw     %s,0x%03x(%s)  %s=mem[0x%08x]=0x%08x\n", current_pc, registers_label[rd], imm_itype & 0xFFF, registers_label[rs1], registers_label[rd], address, value);
                    break;
                case 0b100: // LBU
                    value = memory[mem_index];
                    fprintf(output_file, "0x%08x:lbu    %s,0x%03x(%s)  %s=mem[0x%08x]=0x%08x\n", current_pc, registers_label[rd], imm_itype & 0xFFF, registers_label[rs1], registers_label[rd], address, value);
                    break;
                case 0b101: // LHU
                    value = memory[mem_index] | (memory[mem_index+1] << 8);
                    fprintf(output_file, "0x%08x:lhu    %s,0x%03x(%s)  %s=mem[0x%08x]=0x%08x\n", current_pc, registers_label[rd], imm_itype & 0xFFF, registers_label[rs1], registers_label[rd], address, value);
                    break;
            }
            if (rd != 0) {
                registers[rd] = value;
            }
            break;
        }

            // I-Type (ALU immediate)
        case 0b0010011: { 
            uint32_t result = 0;
            switch(funct3){
                case 0b001: { // SLLI
                    uint32_t shamt = (instruction >> 20) & 0x1F;
                    result = rs1_val << shamt;
                    fprintf(output_file, "0x%08x:slli   %s,%s,%d      %s=0x%08x<<%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], shamt, registers_label[rd], rs1_val, shamt, result);
                    break;
                }
                case 0b101: { // SRLI / SRAI
                    uint32_t shamt = (instruction >> 20) & 0x1F;
                    if (funct7 == 0b0000000) { // SRLI
                        result = rs1_val >> shamt;
                        fprintf(output_file, "0x%08x:srli   %s,%s,%d      %s=0x%08x>>%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], shamt, registers_label[rd], rs1_val, shamt, result);
                    } else { // SRAI
                        result = (uint32_t)(rs1_signed >> shamt);
                        fprintf(output_file, "0x%08x:srai   %s,%s,%d      %s=0x%08x>>>%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], shamt, registers_label[rd], rs1_val, shamt, result);
                    }
                    break;
                }
                case 0b000: // ADDI
                    result = rs1_val + imm_itype;
                    fprintf(output_file, "0x%08x:addi   %s,%s,0x%03x   %s=0x%08x+0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
                case 0b111: // ANDI
                    result = rs1_val & imm_itype;
                    fprintf(output_file, "0x%08x:andi   %s,%s,0x%03x   %s=0x%08x&0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
                case 0b010: // SLTI
                    result = (rs1_signed < imm_itype) ? 1 : 0;
                    fprintf(output_file, "0x%08x:slti   %s,%s,0x%03x   %s=(0x%08x<0x%08x)=%d\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
                case 0b011: // SLTIU
                    result = (rs1_val < (uint32_t)imm_itype) ? 1 : 0;
                    fprintf(output_file, "0x%08x:sltiu  %s,%s,0x%03x   %s=(0x%08x<0x%08x)=%d\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
                case 0b110: // ORI
                    result = rs1_val | imm_itype;
                    fprintf(output_file, "0x%08x:ori    %s,%s,0x%03x   %s=0x%08x|0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
                case 0b100: // XORI
                    result = rs1_val ^ imm_itype;
                    fprintf(output_file, "0x%08x:xori   %s,%s,0x%03x   %s=0x%08x^0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, registers_label[rd], rs1_val, (uint32_t)imm_itype, result);
                    break;
            }
            if (rd != 0) {
                registers[rd] = result;
            }
            break;
        }
            // AUIPC
        case 0b0010111: 
            if (rd != 0) {
                registers[rd] = current_pc + imm_utype;
            }
            fprintf(output_file, "0x%08x:auipc  %s,0x%05x     %s=0x%08x+0x%08x=0x%08x\n", current_pc, registers_label[rd], imm_utype >> 12, registers_label[rd], current_pc, imm_utype, registers[rd]);
            break;
        
            // Store-Type
        case 0b0100011: { 
            uint32_t address = rs1_val + imm_stype;
            uint32_t mem_index = address - mem_offset;
            switch(funct3){
                case 0b000: // SB
                    memory[mem_index] = rs2_val & 0xFF;
                    fprintf(output_file, "0x%08x:sb     %s,0x%03x(%s) mem[0x%08x]=0x%02x\n", current_pc, registers_label[rs2], imm_stype & 0xFFF, registers_label[rs1], address, rs2_val & 0xFF);
                    break;
                case 0b001: // SH
                    memory[mem_index] = rs2_val & 0xFF;
                    memory[mem_index+1] = (rs2_val >> 8) & 0xFF;
                    fprintf(output_file, "0x%08x:sh     %s,0x%03x(%s) mem[0x%08x]=0x%04x\n", current_pc, registers_label[rs2], imm_stype & 0xFFF, registers_label[rs1], address, rs2_val & 0xFFFF);
                    break;
                case 0b010: // SW
                    memory[mem_index] = rs2_val & 0xFF;
                    memory[mem_index+1] = (rs2_val >> 8) & 0xFF;
                    memory[mem_index+2] = (rs2_val >> 16) & 0xFF;
                    memory[mem_index+3] = (rs2_val >> 24) & 0xFF;
                    fprintf(output_file, "0x%08x:sw     %s,0x%03x(%s) mem[0x%08x]=0x%08x\n", current_pc, registers_label[rs2], imm_stype & 0xFFF, registers_label[rs1], address, rs2_val);
                    break;
            }
            break;
        }
        
            // R-Type
        case 0b0110011: {
            uint32_t result = 0;
            // The R-Type instructions are grouped by funct7 first.
            if (funct7 == 0b0000000) {
                switch(funct3){
                    case 0b001: { // SLL
                        uint32_t shamt = rs2_val & 0x1F;
                        result = rs1_val << shamt;
                        fprintf(output_file, "0x%08x:sll    %s,%s,%s     %s=0x%08x<<%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, shamt, result); 
                        break;
                    }
                    case 0b101: { // SRL
                        uint32_t shamt = rs2_val & 0x1F;
                        result = rs1_val >> shamt; 
                        fprintf(output_file, "0x%08x:srl    %s,%s,%s     %s=0x%08x>>%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, shamt, result); 
                        break;
                    }
                    case 0b010: // SLT
                        result = (rs1_signed < rs2_signed) ? 1 : 0; 
                        fprintf(output_file, "0x%08x:slt    %s,%s,%s     %s=(0x%08x<0x%08x)=%d\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                    case 0b011: // SLTU
                        result = (rs1_val < rs2_val) ? 1 : 0; 
                        fprintf(output_file, "0x%08x:sltu   %s,%s,%s     %s=(0x%08x<0x%08x)=%d\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                    case 0b000: // ADD
                        result = rs1_val + rs2_val;
                        fprintf(output_file, "0x%08x:add    %s,%s,%s     %s=0x%08x+0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                    case 0b111: // AND
                        result = rs1_val & rs2_val; 
                        fprintf(output_file, "0x%08x:and    %s,%s,%s     %s=0x%08x&0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                    case 0b110: // OR
                        result = rs1_val | rs2_val; 
                        fprintf(output_file, "0x%08x:or     %s,%s,%s     %s=0x%08x|0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                    case 0b100: // XOR
                        result = rs1_val ^ rs2_val; 
                        fprintf(output_file, "0x%08x:xor    %s,%s,%s     %s=0x%08x^0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                }
            } else if (funct7 == 0b0100000) {
                 switch(funct3){
                    case 0b101: { // SRA
                        uint32_t shamt = rs2_val & 0x1F;
                        result = (uint32_t)(rs1_signed >> shamt); 
                        fprintf(output_file, "0x%08x:sra    %s,%s,%s     %s=0x%08x>>>%d=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, shamt, result); 
                        break;
                    }
                    case 0b000: // SUB
                        result = rs1_val - rs2_val; 
                        fprintf(output_file, "0x%08x:sub    %s,%s,%s     %s=0x%08x-0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result); 
                        break;
                 }

                 // M-Extension
            } else if (funct7 == 0b0000001) { 
                 switch(funct3){
                    case 0b000: // MUL
                        result = rs1_val * rs2_val;
                        fprintf(output_file, "0x%08x:mul    %s,%s,%s     %s=0x%08x*0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    case 0b001: { // MULH
                        int64_t product = (int64_t)rs1_signed * (int64_t)rs2_signed;
                        result = (uint32_t)(product >> 32);
                        fprintf(output_file, "0x%08x:mulh   %s,%s,%s     %s=upper(0x%08x*0x%08x)=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    }
                    case 0b010: { // MULHSU
                        int64_t product = (int64_t)rs1_signed * (uint64_t)rs2_val;
                        result = (uint32_t)(product >> 32);
                        fprintf(output_file, "0x%08x:mulhsu %s,%s,%s     %s=upper(0x%08x(s)*0x%08x(u))=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    }
                    case 0b011: { // MULHU
                        uint64_t product = (uint64_t)rs1_val * (uint64_t)rs2_val;
                        result = (uint32_t)(product >> 32);
                        fprintf(output_file, "0x%08x:mulhu  %s,%s,%s     %s=upper(0x%08x*0x%08x)=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    }
                    case 0b100: // DIV
                        if (rs2_signed == 0){
                            result = 0xFFFFFFFF; 
                        } 
                        else if (rs1_signed == INT32_MIN && rs2_signed == -1){
                             result = (uint32_t)rs1_signed; 
                        } 
                        else{ 
                            result = (uint32_t)(rs1_signed / rs2_signed); 
                        }
                        fprintf(output_file, "0x%08x:div    %s,%s,%s     %s=0x%08x/0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    case 0b101: // DIVU
                        if (rs2_val == 0){
                            result = 0xFFFFFFFF; 
                        } 
                        else { 
                            result = rs1_val / rs2_val; 
                        }
                        fprintf(output_file, "0x%08x:divu   %s,%s,%s     %s=0x%08x/0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    case 0b110: // REM
                        if (rs2_signed == 0) {
                            result = rs1_val; 
                        } 
                        else if(rs1_signed == INT32_MIN && rs2_signed == -1) {
                             result = 0; 
                        } 
                        else { 
                            result = (uint32_t)(rs1_signed % rs2_signed); 
                        }
                        fprintf(output_file, "0x%08x:rem    %s,%s,%s     %s=0x%08x%%0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                    case 0b111: // REMU
                        if (rs2_val == 0) {
                            result = rs1_val; 
                        } 
                        else {
                            result = rs1_val % rs2_val; 
                        }
                        fprintf(output_file, "0x%08x:remu   %s,%s,%s     %s=0x%08x%%0x%08x=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], registers_label[rs2], registers_label[rd], rs1_val, rs2_val, result);
                        break;
                }
            }
            if (rd != 0) {
                registers[rd] = result;
            }
            break;
        }

        case 0b0110111: // LUI
            if (rd != 0) {
                registers[rd] = imm_utype;
            }
            fprintf(output_file, "0x%08x:lui    %s,0x%05x     %s=0x%08x\n", current_pc, registers_label[rd], imm_utype >> 12, registers_label[rd], registers[rd]);
            break;

            // B-Type
        case 0b1100011: { 
            const char* condition_str_ptr = "";
            int branch_taken = 0;
            switch (funct3) {
                case 0b000: // BEQ
                    if (rs1_val == rs2_val) {
                        branch_taken = 1; 
                        }
                    condition_str_ptr = "==";
                    fprintf(output_file, "0x%08x:beq    %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
                case 0b001: // BNE
                    if (rs1_val != rs2_val) { 
                        branch_taken = 1; 
                    }
                    condition_str_ptr = "!=";
                    fprintf(output_file, "0x%08x:bne    %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
                case 0b100: // BLT
                    if (rs1_signed < rs2_signed) { 
                        branch_taken = 1; 
                    }
                    condition_str_ptr = "<";
                    fprintf(output_file, "0x%08x:blt    %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
                case 0b101: // BGE
                    if (rs1_signed >= rs2_signed) { 
                        branch_taken = 1; 
                    }
                    condition_str_ptr = ">=";
                    fprintf(output_file, "0x%08x:bge    %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
                case 0b110: // BLTU
                    if (rs1_val < rs2_val) { 
                        branch_taken = 1; 
                    }
                    condition_str_ptr = "<";
                    fprintf(output_file, "0x%08x:bltu   %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
                case 0b111: // BGEU
                    if (rs1_val >= rs2_val) { 
                        branch_taken = 1; 
                    }
                    condition_str_ptr = ">=";
                    fprintf(output_file, "0x%08x:bgeu   %s,%s,0x%03x  (0x%08x%s0x%08x)=%d->pc=0x%08x\n", current_pc, registers_label[rs1], registers_label[rs2], imm_btype, rs1_val, condition_str_ptr, rs2_val, branch_taken, branch_taken ? (current_pc + imm_btype) : (*pc));
                    break;
            }
            if (branch_taken) {
                *pc = current_pc + imm_btype;
            }
            break;
        }

        case 0b1100111: // JALR
            if (rd != 0) {
                registers[rd] = current_pc + 4;
            }
            *pc = (rs1_val + imm_itype) & 0xFFFFFFFE;
            fprintf(output_file, "0x%08x:jalr   %s,%s,0x%03x   pc=0x%08x+0x%08x,rd=0x%08x\n", current_pc, registers_label[rd], registers_label[rs1], imm_itype & 0xFFF, rs1_val, (uint32_t)imm_itype, registers[rd]);
            break;

        case 0b1101111: // JAL
            if (rd != 0) {
                registers[rd] = current_pc + 4;
            }
            *pc = current_pc + imm_jtype;
            fprintf(output_file, "0x%08x:jal    %s,0x%05x     pc=0x%08x,rd=0x%08x\n", current_pc, registers_label[rd], imm_jtype, *pc, registers[rd]);
            break;
            
        case 0b1110011: // ebreak
            if (funct3 == 0b000 && instruction == 0x00100073) { 
                fprintf(output_file, "0x%08x:ebreak\n", current_pc);
                keep_run = 0;
            }
            break;
    }

    // x0 is hardwired to zero, so we always reset it
    registers[0] = 0;
    return keep_run;
}

int main(int argc, char *argv[]) {
    
    // check if user provided input and output files
    if (argc < 3) {
        printf("Usage: %s <input_file.hex> <output_file.txt>\n", argv[0]);
        return 1;
    }
    
    const char *input_path = argv[1];
    const char *output_path = argv[2];

    // --- Hardware Initialization ---
    // memory starts at 0x80000000 for this simulator
    const uint32_t mem_offset = 0x80000000;
    // 32KB of memory
    uint8_t memory[32 * 1024] = {0}; 
    uint32_t pc = mem_offset;
    uint32_t registers[32] = {0};
    // ABI names for the registers, useful for debbug
    const char *registers_label[32] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", 
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", 
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", 
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };

    FILE *input_file = fopen(input_path, "r");
    if (input_file == NULL) {
        perror("Error opening input file");
        return 1;
    }

    FILE *output_file = fopen(output_path, "w");
    if (output_file == NULL) {
        perror("Error opening output file");
        fclose(input_file);
        return 1;
    }

    // loads the file in the simulated memory
    hex_file_to_memory(input_file, memory, mem_offset);

    // main simulation loop
    int keep_running = 1;
    while (keep_running) {
        // fetch
        uint32_t idx = pc - mem_offset;
        uint32_t instruction = read_word_from_mem(memory, idx);
        // decode and execute
        keep_running = instruction_read(instruction, registers, &pc, registers_label, memory, output_file, mem_offset);
    }
    
    //Closes the file 
    fclose(input_file);
    fclose(output_file);

    return 0;
}