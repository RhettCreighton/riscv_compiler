/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include "riscv_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations to avoid type conflicts
void compile_lw(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction);
void compile_sw(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction);
void compile_lb(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction);
void compile_lbu(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction);
void compile_sb(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction);

// Extract I-type immediate
static int32_t get_i_immediate(uint32_t instruction) {
    int32_t imm = (int32_t)instruction >> 20;  // Sign extend
    return imm;
}

// Extract S-type immediate
static int32_t get_s_immediate(uint32_t instruction) {
    int32_t imm = 0;
    imm |= ((instruction >> 7) & 0x1F);         // imm[4:0]
    imm |= ((instruction >> 25) & 0x7F) << 5;   // imm[11:5]
    
    // Sign extend
    if (imm & 0x800) {
        imm |= 0xFFFFF000;
    }
    
    return imm;
}

// Compile LW instruction: rd = memory[rs1 + imm]
void compile_lw(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    int32_t imm = get_i_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Calculate address: rs1 + imm
    uint32_t* imm_bits = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        imm_bits[i] = (imm & (1 << i)) ? 2 : 1;
    }
    
    uint32_t* address = riscv_circuit_allocate_wire_array(circuit, 32);
    build_adder(circuit, compiler->reg_wires[rs1], imm_bits, address, 32);
    
    // Perform memory read
    uint32_t* read_data = riscv_circuit_allocate_wire_array(circuit, 32);
    uint32_t* dummy_write_data = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        dummy_write_data[i] = 1;  // All zeros
    }
    
    memory->access(memory, address, dummy_write_data, 1, read_data);  // write_enable = 0
    
    // Store result in rd (if not x0)
    if (rd != 0) {
        memcpy(compiler->reg_wires[rd], read_data, 32 * sizeof(uint32_t));
    }
    
    free(imm_bits);
    free(address);
    free(read_data);
    free(dummy_write_data);
}

// Compile SW instruction: memory[rs1 + imm] = rs2
void compile_sw(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction) {
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = get_s_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Calculate address: rs1 + imm
    uint32_t* imm_bits = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        imm_bits[i] = (imm & (1 << i)) ? 2 : 1;
    }
    
    uint32_t* address = riscv_circuit_allocate_wire_array(circuit, 32);
    build_adder(circuit, compiler->reg_wires[rs1], imm_bits, address, 32);
    
    // Perform memory write
    uint32_t* dummy_read_data = riscv_circuit_allocate_wire_array(circuit, 32);
    
    memory->access(memory, address, compiler->reg_wires[rs2], 2, dummy_read_data);  // write_enable = 1
    
    free(imm_bits);
    free(address);
    free(dummy_read_data);
}

// Compile LB instruction: rd = sign_extend(memory[rs1 + imm][7:0])
void compile_lb(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    int32_t imm = get_i_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Calculate address: rs1 + imm
    uint32_t* imm_bits = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        imm_bits[i] = (imm & (1 << i)) ? 2 : 1;
    }
    
    uint32_t* address = riscv_circuit_allocate_wire_array(circuit, 32);
    build_adder(circuit, compiler->reg_wires[rs1], imm_bits, address, 32);
    
    // Perform memory read
    uint32_t* read_data = riscv_circuit_allocate_wire_array(circuit, 32);
    uint32_t* dummy_write_data = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        dummy_write_data[i] = 1;  // All zeros
    }
    
    memory->access(memory, address, dummy_write_data, 1, read_data);
    
    // Extract byte and sign extend
    if (rd != 0) {
        // Copy lower 8 bits
        for (int i = 0; i < 8; i++) {
            compiler->reg_wires[rd][i] = read_data[i];
        }
        
        // Sign extend (copy bit 7 to bits 8-31)
        uint32_t sign_bit = read_data[7];
        for (int i = 8; i < 32; i++) {
            compiler->reg_wires[rd][i] = sign_bit;
        }
    }
    
    free(imm_bits);
    free(address);
    free(read_data);
    free(dummy_write_data);
}

// Compile LBU instruction: rd = zero_extend(memory[rs1 + imm][7:0])
void compile_lbu(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    // uint32_t rs1 = (instruction >> 15) & 0x1F;
    // int32_t imm = get_i_immediate(instruction);
    
    // riscv_circuit_t* circuit = compiler->circuit;
    
    // Similar to LB but zero extend instead of sign extend
    // ... (similar address calculation and memory read)
    
    // Zero extend (set bits 8-31 to 0)
    if (rd != 0) {
        uint32_t* read_data = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
        // ... memory access ...
        
        // Copy lower 8 bits
        for (int i = 0; i < 8; i++) {
            compiler->reg_wires[rd][i] = read_data[i];
        }
        
        // Zero extend
        for (int i = 8; i < 32; i++) {
            compiler->reg_wires[rd][i] = 1;  // Wire 1 is constant 0
        }
        
        free(read_data);
    }
}

// Compile SB instruction: memory[rs1 + imm][7:0] = rs2[7:0]
void compile_sb(riscv_compiler_t* compiler, riscv_memory_t* memory, uint32_t instruction) {
    // uint32_t rs1 = (instruction >> 15) & 0x1F;
    // uint32_t rs2 = (instruction >> 20) & 0x1F;
    // int32_t imm = get_s_immediate(instruction);
    
    // riscv_circuit_t* circuit = compiler->circuit;
    
    // For byte operations, we need to:
    // 1. Read the current word
    // 2. Replace the appropriate byte
    // 3. Write back the modified word
    
    // This requires additional logic to handle byte selection
    // For now, simplified to just write the whole word
    printf("Note: SB instruction simplified to word write\n");
    
    compile_sw(compiler, memory, instruction);
}

// Add load/store support to main compiler
int compile_memory_instruction(riscv_compiler_t* compiler, struct riscv_memory_t* memory, uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    
    switch (opcode) {
        case 0x03:  // Load instructions
            switch (funct3) {
                case 0x0:  // LB
                    compile_lb(compiler, (riscv_memory_t*)memory, instruction);
                    break;
                case 0x1:  // LH
                    printf("LH not implemented yet\n");
                    break;
                case 0x2:  // LW
                    compile_lw(compiler, (riscv_memory_t*)memory, instruction);
                    break;
                case 0x4:  // LBU
                    compile_lbu(compiler, (riscv_memory_t*)memory, instruction);
                    break;
                case 0x5:  // LHU
                    printf("LHU not implemented yet\n");
                    break;
                default:
                    return -1;
            }
            break;
            
        case 0x23:  // Store instructions
            switch (funct3) {
                case 0x0:  // SB
                    compile_sb(compiler, (riscv_memory_t*)memory, instruction);
                    break;
                case 0x1:  // SH
                    printf("SH not implemented yet\n");
                    break;
                case 0x2:  // SW
                    compile_sw(compiler, (riscv_memory_t*)memory, instruction);
                    break;
                default:
                    return -1;
            }
            break;
            
        default:
            return -1;  // Not a memory instruction
    }
    
    return 0;
}