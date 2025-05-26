/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// RISC-V instruction opcodes
#define OP_LUI      0x37
#define OP_AUIPC    0x17
#define OP_JAL      0x6F
#define OP_JALR     0x67
#define OP_BRANCH   0x63
#define OP_LOAD     0x03
#define OP_STORE    0x23
#define OP_IMM      0x13
#define OP_REG      0x33
#define OP_SYSTEM   0x73

// Function codes
#define FUNCT3_ADD_SUB  0x0
#define FUNCT3_SLL      0x1
#define FUNCT3_SLT      0x2
#define FUNCT3_SLTU     0x3
#define FUNCT3_XOR      0x4
#define FUNCT3_SRL_SRA  0x5
#define FUNCT3_OR       0x6
#define FUNCT3_AND      0x7

#define FUNCT3_BEQ      0x0
#define FUNCT3_BNE      0x1
#define FUNCT3_BLT      0x4
#define FUNCT3_BGE      0x5
#define FUNCT3_BLTU     0x6
#define FUNCT3_BGEU     0x7

#define FUNCT3_LB       0x0
#define FUNCT3_LH       0x1
#define FUNCT3_LW       0x2
#define FUNCT3_LBU      0x4
#define FUNCT3_LHU      0x5

#define FUNCT3_SB       0x0
#define FUNCT3_SH       0x1
#define FUNCT3_SW       0x2

#define FUNCT7_ADD      0x00
#define FUNCT7_SUB      0x20
#define FUNCT7_SRL      0x00
#define FUNCT7_SRA      0x20
#define FUNCT7_MUL      0x01

// Create emulator
emulator_state_t* create_emulator(size_t memory_size) {
    emulator_state_t* emu = malloc(sizeof(emulator_state_t));
    if (!emu) return NULL;
    
    emu->memory = calloc(memory_size, 1);
    if (!emu->memory) {
        free(emu);
        return NULL;
    }
    
    emu->memory_size = memory_size;
    reset_emulator(emu);
    return emu;
}

// Destroy emulator
void destroy_emulator(emulator_state_t* emu) {
    if (emu) {
        free(emu->memory);
        free(emu);
    }
}

// Reset emulator state
void reset_emulator(emulator_state_t* emu) {
    memset(emu->regs, 0, sizeof(emu->regs));
    emu->pc = 0;
    emu->halt = false;
    emu->instruction_count = 0;
    // x0 is always zero
    emu->regs[0] = 0;
}

// Memory operations with bounds checking
void write_memory_word(emulator_state_t* emu, uint32_t addr, uint32_t value) {
    if (addr + 3 >= emu->memory_size) {
        printf("Memory write out of bounds: 0x%08x\n", addr);
        return;
    }
    // Little endian
    emu->memory[addr] = value & 0xFF;
    emu->memory[addr + 1] = (value >> 8) & 0xFF;
    emu->memory[addr + 2] = (value >> 16) & 0xFF;
    emu->memory[addr + 3] = (value >> 24) & 0xFF;
}

uint32_t read_memory_word(emulator_state_t* emu, uint32_t addr) {
    if (addr + 3 >= emu->memory_size) {
        printf("Memory read out of bounds: 0x%08x\n", addr);
        return 0;
    }
    // Little endian
    return emu->memory[addr] |
           (emu->memory[addr + 1] << 8) |
           (emu->memory[addr + 2] << 16) |
           (emu->memory[addr + 3] << 24);
}

void write_memory_byte(emulator_state_t* emu, uint32_t addr, uint8_t value) {
    if (addr >= emu->memory_size) {
        printf("Memory write out of bounds: 0x%08x\n", addr);
        return;
    }
    emu->memory[addr] = value;
}

uint8_t read_memory_byte(emulator_state_t* emu, uint32_t addr) {
    if (addr >= emu->memory_size) {
        printf("Memory read out of bounds: 0x%08x\n", addr);
        return 0;
    }
    return emu->memory[addr];
}

void write_memory_halfword(emulator_state_t* emu, uint32_t addr, uint16_t value) {
    if (addr + 1 >= emu->memory_size) {
        printf("Memory write out of bounds: 0x%08x\n", addr);
        return;
    }
    emu->memory[addr] = value & 0xFF;
    emu->memory[addr + 1] = (value >> 8) & 0xFF;
}

uint16_t read_memory_halfword(emulator_state_t* emu, uint32_t addr) {
    if (addr + 1 >= emu->memory_size) {
        printf("Memory read out of bounds: 0x%08x\n", addr);
        return 0;
    }
    return emu->memory[addr] | (emu->memory[addr + 1] << 8);
}

// Load program into memory
void load_program(emulator_state_t* emu, uint32_t* program, size_t length, uint32_t start_addr) {
    emu->pc = start_addr;
    for (size_t i = 0; i < length; i++) {
        write_memory_word(emu, start_addr + i * 4, program[i]);
    }
}

// Sign extend utility
uint32_t sign_extend(uint32_t value, int bits) {
    if (value & (1U << (bits - 1))) {
        return value | (~0U << bits);
    }
    return value;
}

// Decode instruction
decoded_instruction_t decode_instruction(uint32_t instruction) {
    decoded_instruction_t decoded;
    
    decoded.opcode = instruction & 0x7F;
    decoded.rd = (instruction >> 7) & 0x1F;
    decoded.funct3 = (instruction >> 12) & 0x7;
    decoded.rs1 = (instruction >> 15) & 0x1F;
    decoded.rs2 = (instruction >> 20) & 0x1F;
    decoded.funct7 = (instruction >> 25) & 0x7F;
    
    // Decode immediate based on instruction type
    switch (decoded.opcode) {
        case OP_LUI:
        case OP_AUIPC:
            decoded.type = U_TYPE;
            decoded.imm = instruction & 0xFFFFF000;
            break;
            
        case OP_JAL:
            decoded.type = J_TYPE;
            decoded.imm = ((instruction & 0x80000000) >> 11) |  // [20]
                         ((instruction & 0x7FE00000) >> 20) |  // [10:1]
                         ((instruction & 0x00100000) >> 9) |   // [11]
                         (instruction & 0x000FF000);           // [19:12]
            decoded.imm = sign_extend(decoded.imm, 21);
            break;
            
        case OP_BRANCH:
            decoded.type = B_TYPE;
            decoded.imm = ((instruction & 0x80000000) >> 19) |  // [12]
                         ((instruction & 0x00000080) << 4) |   // [11]
                         ((instruction & 0x7E000000) >> 20) |  // [10:5]
                         ((instruction & 0x00000F00) >> 7);    // [4:1]
            decoded.imm = sign_extend(decoded.imm, 13);
            break;
            
        case OP_STORE:
            decoded.type = S_TYPE;
            decoded.imm = ((instruction & 0xFE000000) >> 20) |  // [11:5]
                         ((instruction & 0x00000F80) >> 7);    // [4:0]
            decoded.imm = sign_extend(decoded.imm, 12);
            break;
            
        case OP_LOAD:
        case OP_IMM:
        case OP_JALR:
            decoded.type = I_TYPE;
            decoded.imm = (instruction >> 20);
            decoded.imm = sign_extend(decoded.imm, 12);
            break;
            
        case OP_REG:
            decoded.type = R_TYPE;
            decoded.imm = 0;
            break;
            
        default:
            decoded.type = R_TYPE;
            decoded.imm = 0;
            break;
    }
    
    return decoded;
}

// Execute single instruction
bool execute_instruction(emulator_state_t* emu, uint32_t instruction) {
    decoded_instruction_t decoded = decode_instruction(instruction);
    uint32_t next_pc = emu->pc + 4;
    
    // Ensure x0 is always zero
    emu->regs[0] = 0;
    
    switch (decoded.opcode) {
        case OP_LUI:
            emu->regs[decoded.rd] = decoded.imm;
            break;
            
        case OP_AUIPC:
            emu->regs[decoded.rd] = emu->pc + decoded.imm;
            break;
            
        case OP_JAL:
            emu->regs[decoded.rd] = next_pc;
            next_pc = emu->pc + decoded.imm;
            break;
            
        case OP_JALR:
            {
                uint32_t target = (emu->regs[decoded.rs1] + decoded.imm) & ~1;
                emu->regs[decoded.rd] = next_pc;
                next_pc = target;
            }
            break;
            
        case OP_BRANCH:
            {
                bool take_branch = false;
                uint32_t rs1_val = emu->regs[decoded.rs1];
                uint32_t rs2_val = emu->regs[decoded.rs2];
                
                switch (decoded.funct3) {
                    case FUNCT3_BEQ:  take_branch = (rs1_val == rs2_val); break;
                    case FUNCT3_BNE:  take_branch = (rs1_val != rs2_val); break;
                    case FUNCT3_BLT:  take_branch = ((int32_t)rs1_val < (int32_t)rs2_val); break;
                    case FUNCT3_BGE:  take_branch = ((int32_t)rs1_val >= (int32_t)rs2_val); break;
                    case FUNCT3_BLTU: take_branch = (rs1_val < rs2_val); break;
                    case FUNCT3_BGEU: take_branch = (rs1_val >= rs2_val); break;
                }
                
                if (take_branch) {
                    next_pc = emu->pc + decoded.imm;
                }
            }
            break;
            
        case OP_LOAD:
            {
                uint32_t addr = emu->regs[decoded.rs1] + decoded.imm;
                uint32_t value = 0;
                
                switch (decoded.funct3) {
                    case FUNCT3_LB:
                        value = sign_extend(read_memory_byte(emu, addr), 8);
                        break;
                    case FUNCT3_LH:
                        value = sign_extend(read_memory_halfword(emu, addr), 16);
                        break;
                    case FUNCT3_LW:
                        value = read_memory_word(emu, addr);
                        break;
                    case FUNCT3_LBU:
                        value = read_memory_byte(emu, addr);
                        break;
                    case FUNCT3_LHU:
                        value = read_memory_halfword(emu, addr);
                        break;
                }
                
                emu->regs[decoded.rd] = value;
            }
            break;
            
        case OP_STORE:
            {
                uint32_t addr = emu->regs[decoded.rs1] + decoded.imm;
                uint32_t value = emu->regs[decoded.rs2];
                
                switch (decoded.funct3) {
                    case FUNCT3_SB:
                        write_memory_byte(emu, addr, value & 0xFF);
                        break;
                    case FUNCT3_SH:
                        write_memory_halfword(emu, addr, value & 0xFFFF);
                        break;
                    case FUNCT3_SW:
                        write_memory_word(emu, addr, value);
                        break;
                }
            }
            break;
            
        case OP_IMM:
            {
                uint32_t rs1_val = emu->regs[decoded.rs1];
                uint32_t result = 0;
                
                switch (decoded.funct3) {
                    case FUNCT3_ADD_SUB:
                        result = rs1_val + decoded.imm;
                        break;
                    case FUNCT3_SLT:
                        result = ((int32_t)rs1_val < decoded.imm) ? 1 : 0;
                        break;
                    case FUNCT3_SLTU:
                        result = (rs1_val < (uint32_t)decoded.imm) ? 1 : 0;
                        break;
                    case FUNCT3_XOR:
                        result = rs1_val ^ decoded.imm;
                        break;
                    case FUNCT3_OR:
                        result = rs1_val | decoded.imm;
                        break;
                    case FUNCT3_AND:
                        result = rs1_val & decoded.imm;
                        break;
                    case FUNCT3_SLL:
                        result = rs1_val << (decoded.imm & 0x1F);
                        break;
                    case FUNCT3_SRL_SRA:
                        if (decoded.funct7 == FUNCT7_SRA) {
                            result = (uint32_t)((int32_t)rs1_val >> (decoded.imm & 0x1F));
                        } else {
                            result = rs1_val >> (decoded.imm & 0x1F);
                        }
                        break;
                }
                
                emu->regs[decoded.rd] = result;
            }
            break;
            
        case OP_REG:
            {
                uint32_t rs1_val = emu->regs[decoded.rs1];
                uint32_t rs2_val = emu->regs[decoded.rs2];
                uint32_t result = 0;
                
                if (decoded.funct7 == FUNCT7_MUL) {
                    // Multiplication instructions
                    switch (decoded.funct3) {
                        case 0: // MUL
                            result = rs1_val * rs2_val;
                            break;
                        case 1: // MULH
                                {
                                    int64_t prod = (int64_t)(int32_t)rs1_val * (int64_t)(int32_t)rs2_val;
                                    result = (uint32_t)(prod >> 32);
                                }
                            break;
                        case 2: // MULHSU
                                {
                                    int64_t prod = (int64_t)(int32_t)rs1_val * (uint64_t)rs2_val;
                                    result = (uint32_t)(prod >> 32);
                                }
                            break;
                        case 3: // MULHU
                                {
                                    uint64_t prod = (uint64_t)rs1_val * (uint64_t)rs2_val;
                                    result = (uint32_t)(prod >> 32);
                                }
                            break;
                        case 4: // DIV
                            if (rs2_val != 0) {
                                result = (uint32_t)((int32_t)rs1_val / (int32_t)rs2_val);
                            } else {
                                result = 0xFFFFFFFF;
                            }
                            break;
                        case 5: // DIVU
                            if (rs2_val != 0) {
                                result = rs1_val / rs2_val;
                            } else {
                                result = 0xFFFFFFFF;
                            }
                            break;
                        case 6: // REM
                            if (rs2_val != 0) {
                                result = (uint32_t)((int32_t)rs1_val % (int32_t)rs2_val);
                            } else {
                                result = rs1_val;
                            }
                            break;
                        case 7: // REMU
                            if (rs2_val != 0) {
                                result = rs1_val % rs2_val;
                            } else {
                                result = rs1_val;
                            }
                            break;
                    }
                } else {
                    // Regular ALU operations
                    switch (decoded.funct3) {
                        case FUNCT3_ADD_SUB:
                            if (decoded.funct7 == FUNCT7_SUB) {
                                result = rs1_val - rs2_val;
                            } else {
                                result = rs1_val + rs2_val;
                            }
                            break;
                        case FUNCT3_SLL:
                            result = rs1_val << (rs2_val & 0x1F);
                            break;
                        case FUNCT3_SLT:
                            result = ((int32_t)rs1_val < (int32_t)rs2_val) ? 1 : 0;
                            break;
                        case FUNCT3_SLTU:
                            result = (rs1_val < rs2_val) ? 1 : 0;
                            break;
                        case FUNCT3_XOR:
                            result = rs1_val ^ rs2_val;
                            break;
                        case FUNCT3_SRL_SRA:
                            if (decoded.funct7 == FUNCT7_SRA) {
                                result = (uint32_t)((int32_t)rs1_val >> (rs2_val & 0x1F));
                            } else {
                                result = rs1_val >> (rs2_val & 0x1F);
                            }
                            break;
                        case FUNCT3_OR:
                            result = rs1_val | rs2_val;
                            break;
                        case FUNCT3_AND:
                            result = rs1_val & rs2_val;
                            break;
                    }
                }
                
                emu->regs[decoded.rd] = result;
            }
            break;
            
        case OP_SYSTEM:
            // ECALL, EBREAK, etc.
            switch (decoded.funct3) {
                case 0:
                    if (decoded.imm == 0) {
                        // ECALL - for now, just halt
                        emu->halt = true;
                        return false;
                    } else if (decoded.imm == 1) {
                        // EBREAK - for now, just halt
                        emu->halt = true;
                        return false;
                    }
                    break;
            }
            break;
            
        default:
            printf("Unknown instruction: 0x%08x (opcode: 0x%02x)\n", instruction, decoded.opcode);
            return false;
    }
    
    // Ensure x0 remains zero
    emu->regs[0] = 0;
    emu->pc = next_pc;
    emu->instruction_count++;
    
    return true;
}

// Step emulator one instruction
void step_emulator(emulator_state_t* emu) {
    if (emu->halt) return;
    
    uint32_t instruction = read_memory_word(emu, emu->pc);
    execute_instruction(emu, instruction);
}

// Run emulator for max_instructions
void run_emulator(emulator_state_t* emu, uint32_t max_instructions) {
    uint32_t count = 0;
    while (!emu->halt && count < max_instructions) {
        step_emulator(emu);
        count++;
    }
}

// Print register state
void print_registers(const emulator_state_t* emu) {
    printf("PC: 0x%08x  Instructions: %u\n", emu->pc, emu->instruction_count);
    for (int i = 0; i < 32; i += 4) {
        printf("x%02d: 0x%08x  x%02d: 0x%08x  x%02d: 0x%08x  x%02d: 0x%08x\n",
               i, emu->regs[i], i+1, emu->regs[i+1], 
               i+2, emu->regs[i+2], i+3, emu->regs[i+3]);
    }
}

// Print memory range
void print_memory_range(const emulator_state_t* emu, uint32_t start, uint32_t end) {
    printf("Memory from 0x%08x to 0x%08x:\n", start, end);
    for (uint32_t addr = start; addr <= end; addr += 4) {
        if (addr + 3 < emu->memory_size) {
            printf("0x%08x: 0x%08x\n", addr, read_memory_word((emulator_state_t*)emu, addr));
        }
    }
}

// Compare two emulator states
bool compare_states(const emulator_state_t* emu1, const emulator_state_t* emu2) {
    // Compare registers
    for (int i = 0; i < 32; i++) {
        if (emu1->regs[i] != emu2->regs[i]) {
            printf("Register x%d differs: 0x%08x vs 0x%08x\n", i, emu1->regs[i], emu2->regs[i]);
            return false;
        }
    }
    
    // Compare PC
    if (emu1->pc != emu2->pc) {
        printf("PC differs: 0x%08x vs 0x%08x\n", emu1->pc, emu2->pc);
        return false;
    }
    
    return true;
}

// Get instruction name for debugging
const char* get_instruction_name(uint32_t instruction) {
    decoded_instruction_t decoded = decode_instruction(instruction);
    
    switch (decoded.opcode) {
        case OP_LUI: return "LUI";
        case OP_AUIPC: return "AUIPC";
        case OP_JAL: return "JAL";
        case OP_JALR: return "JALR";
        case OP_BRANCH:
            switch (decoded.funct3) {
                case FUNCT3_BEQ: return "BEQ";
                case FUNCT3_BNE: return "BNE";
                case FUNCT3_BLT: return "BLT";
                case FUNCT3_BGE: return "BGE";
                case FUNCT3_BLTU: return "BLTU";
                case FUNCT3_BGEU: return "BGEU";
                default: return "BRANCH";
            }
        case OP_LOAD:
            switch (decoded.funct3) {
                case FUNCT3_LB: return "LB";
                case FUNCT3_LH: return "LH";
                case FUNCT3_LW: return "LW";
                case FUNCT3_LBU: return "LBU";
                case FUNCT3_LHU: return "LHU";
                default: return "LOAD";
            }
        case OP_STORE:
            switch (decoded.funct3) {
                case FUNCT3_SB: return "SB";
                case FUNCT3_SH: return "SH";
                case FUNCT3_SW: return "SW";
                default: return "STORE";
            }
        case OP_IMM:
            switch (decoded.funct3) {
                case FUNCT3_ADD_SUB: return "ADDI";
                case FUNCT3_SLT: return "SLTI";
                case FUNCT3_SLTU: return "SLTIU";
                case FUNCT3_XOR: return "XORI";
                case FUNCT3_OR: return "ORI";
                case FUNCT3_AND: return "ANDI";
                case FUNCT3_SLL: return "SLLI";
                case FUNCT3_SRL_SRA: return (decoded.funct7 == FUNCT7_SRA) ? "SRAI" : "SRLI";
                default: return "IMM";
            }
        case OP_REG:
            if (decoded.funct7 == FUNCT7_MUL) {
                switch (decoded.funct3) {
                    case 0: return "MUL";
                    case 1: return "MULH";
                    case 2: return "MULHSU";
                    case 3: return "MULHU";
                    case 4: return "DIV";
                    case 5: return "DIVU";
                    case 6: return "REM";
                    case 7: return "REMU";
                    default: return "MUL_DIV";
                }
            } else {
                switch (decoded.funct3) {
                    case FUNCT3_ADD_SUB: return (decoded.funct7 == FUNCT7_SUB) ? "SUB" : "ADD";
                    case FUNCT3_SLL: return "SLL";
                    case FUNCT3_SLT: return "SLT";
                    case FUNCT3_SLTU: return "SLTU";
                    case FUNCT3_XOR: return "XOR";
                    case FUNCT3_SRL_SRA: return (decoded.funct7 == FUNCT7_SRA) ? "SRA" : "SRL";
                    case FUNCT3_OR: return "OR";
                    case FUNCT3_AND: return "AND";
                    default: return "REG";
                }
            }
        case OP_SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}