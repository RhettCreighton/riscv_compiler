/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper: Build a barrel shifter for left shift
static void build_left_shift(riscv_circuit_t* circuit,
                            uint32_t* value_bits,
                            uint32_t* shift_amount_bits,
                            uint32_t* result_bits,
                            size_t num_bits) {
    // Barrel shifter: cascade of multiplexers
    // For each bit position in shift amount, shift by 2^i positions
    
    uint32_t* current = malloc(num_bits * sizeof(uint32_t));
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    // Process each shift amount bit (only need 5 bits for 32-bit shifts)
    for (int shift_bit = 0; shift_bit < 5; shift_bit++) {
        uint32_t* shifted = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << shift_bit;  // 1, 2, 4, 8, 16
        
        // Create shifted version
        for (int i = 0; i < num_bits; i++) {
            if (i < shift_by) {
                shifted[i] = 1;  // Fill with zeros
            } else {
                shifted[i] = current[i - shift_by];
            }
        }
        
        // MUX between current and shifted based on shift_amount_bits[shift_bit]
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        for (int i = 0; i < num_bits; i++) {
            // result = shift_bit ? shifted : current
            uint32_t not_shift = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], 2, not_shift, GATE_XOR);
            
            uint32_t keep_current = riscv_circuit_allocate_wire(circuit);
            uint32_t take_shifted = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, not_shift, current[i], keep_current, GATE_AND);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], shifted[i], take_shifted, GATE_AND);
            
            // OR
            uint32_t xor_result = riscv_circuit_allocate_wire(circuit);
            uint32_t and_result = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, xor_result, GATE_XOR);
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, and_result, GATE_AND);
            riscv_circuit_add_gate(circuit, xor_result, and_result, next[i], GATE_XOR);
        }
        
        free(current);
        free(shifted);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Helper: Build a barrel shifter for right shift (logical)
static void build_right_shift_logical(riscv_circuit_t* circuit,
                                     uint32_t* value_bits,
                                     uint32_t* shift_amount_bits,
                                     uint32_t* result_bits,
                                     size_t num_bits) {
    uint32_t* current = malloc(num_bits * sizeof(uint32_t));
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    // Similar to left shift but in opposite direction
    for (int shift_bit = 0; shift_bit < 5; shift_bit++) {
        uint32_t* shifted = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << shift_bit;
        
        // Create shifted version
        for (int i = 0; i < num_bits; i++) {
            if (i + shift_by < num_bits) {
                shifted[i] = current[i + shift_by];
            } else {
                shifted[i] = 1;  // Fill with zeros
            }
        }
        
        // MUX logic (same as left shift)
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        for (int i = 0; i < num_bits; i++) {
            uint32_t not_shift = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], 2, not_shift, GATE_XOR);
            
            uint32_t keep_current = riscv_circuit_allocate_wire(circuit);
            uint32_t take_shifted = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, not_shift, current[i], keep_current, GATE_AND);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], shifted[i], take_shifted, GATE_AND);
            
            uint32_t xor_result = riscv_circuit_allocate_wire(circuit);
            uint32_t and_result = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, xor_result, GATE_XOR);
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, and_result, GATE_AND);
            riscv_circuit_add_gate(circuit, xor_result, and_result, next[i], GATE_XOR);
        }
        
        free(current);
        free(shifted);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Helper: Build a barrel shifter for right shift (arithmetic)
static void build_right_shift_arithmetic(riscv_circuit_t* circuit,
                                        uint32_t* value_bits,
                                        uint32_t* shift_amount_bits,
                                        uint32_t* result_bits,
                                        size_t num_bits) {
    uint32_t* current = malloc(num_bits * sizeof(uint32_t));
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    uint32_t sign_bit = value_bits[num_bits - 1];  // MSB for sign extension
    
    // Similar to logical right shift but fill with sign bit
    for (int shift_bit = 0; shift_bit < 5; shift_bit++) {
        uint32_t* shifted = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << shift_bit;
        
        // Create shifted version
        for (int i = 0; i < num_bits; i++) {
            if (i + shift_by < num_bits) {
                shifted[i] = current[i + shift_by];
            } else {
                shifted[i] = sign_bit;  // Fill with sign bit
            }
        }
        
        // MUX logic (same as other shifts)
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        for (int i = 0; i < num_bits; i++) {
            uint32_t not_shift = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], 2, not_shift, GATE_XOR);
            
            uint32_t keep_current = riscv_circuit_allocate_wire(circuit);
            uint32_t take_shifted = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, not_shift, current[i], keep_current, GATE_AND);
            riscv_circuit_add_gate(circuit, shift_amount_bits[shift_bit], shifted[i], take_shifted, GATE_AND);
            
            uint32_t xor_result = riscv_circuit_allocate_wire(circuit);
            uint32_t and_result = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, xor_result, GATE_XOR);
            riscv_circuit_add_gate(circuit, keep_current, take_shifted, and_result, GATE_AND);
            riscv_circuit_add_gate(circuit, xor_result, and_result, next[i], GATE_XOR);
        }
        
        free(current);
        free(shifted);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Compile SLL instruction: rd = rs1 << rs2[4:0]
void compile_sll(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Extract lower 5 bits of rs2 for shift amount
    uint32_t shift_amount[5];
    for (int i = 0; i < 5; i++) {
        shift_amount[i] = compiler->reg_wires[rs2][i];
    }
    
    // Perform left shift
    if (rd != 0) {
        uint32_t* result = riscv_circuit_allocate_wire_array(circuit, 32);
        build_left_shift(circuit, compiler->reg_wires[rs1], shift_amount, result, 32);
        memcpy(compiler->reg_wires[rd], result, 32 * sizeof(uint32_t));
        free(result);
    }
}

// Compile SRL instruction: rd = rs1 >> rs2[4:0] (logical)
void compile_srl(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Extract lower 5 bits of rs2 for shift amount
    uint32_t shift_amount[5];
    for (int i = 0; i < 5; i++) {
        shift_amount[i] = compiler->reg_wires[rs2][i];
    }
    
    // Perform logical right shift
    if (rd != 0) {
        uint32_t* result = riscv_circuit_allocate_wire_array(circuit, 32);
        build_right_shift_logical(circuit, compiler->reg_wires[rs1], shift_amount, result, 32);
        memcpy(compiler->reg_wires[rd], result, 32 * sizeof(uint32_t));
        free(result);
    }
}

// Compile SRA instruction: rd = rs1 >> rs2[4:0] (arithmetic)
void compile_sra(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Extract lower 5 bits of rs2 for shift amount
    uint32_t shift_amount[5];
    for (int i = 0; i < 5; i++) {
        shift_amount[i] = compiler->reg_wires[rs2][i];
    }
    
    // Perform arithmetic right shift
    if (rd != 0) {
        uint32_t* result = riscv_circuit_allocate_wire_array(circuit, 32);
        build_right_shift_arithmetic(circuit, compiler->reg_wires[rs1], shift_amount, result, 32);
        memcpy(compiler->reg_wires[rd], result, 32 * sizeof(uint32_t));
        free(result);
    }
}

// Compile SLLI instruction: rd = rs1 << shamt
void compile_slli(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t shamt) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Create constant shift amount
    uint32_t shift_amount[5];
    for (int i = 0; i < 5; i++) {
        shift_amount[i] = (shamt & (1 << i)) ? 2 : 1;  // Constant wires
    }
    
    // Perform left shift
    if (rd != 0) {
        uint32_t* result = riscv_circuit_allocate_wire_array(circuit, 32);
        build_left_shift(circuit, compiler->reg_wires[rs1], shift_amount, result, 32);
        memcpy(compiler->reg_wires[rd], result, 32 * sizeof(uint32_t));
        free(result);
    }
}

// Add shift instruction support to main compiler
int compile_shift_instruction(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    
    if (opcode == 0x33) {  // R-type shifts
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct7 = (instruction >> 25) & 0x7F;
        
        switch (funct3) {
            case 0x1:  // SLL
                compile_sll(compiler, rd, rs1, rs2);
                break;
            case 0x5:  // SRL/SRA
                if (funct7 == 0x00) {
                    compile_srl(compiler, rd, rs1, rs2);
                } else if (funct7 == 0x20) {
                    compile_sra(compiler, rd, rs1, rs2);
                }
                break;
            default:
                return -1;
        }
    } else if (opcode == 0x13) {  // I-type shifts
        uint32_t shamt = (instruction >> 20) & 0x1F;
        uint32_t funct7 = (instruction >> 25) & 0x7F;
        
        switch (funct3) {
            case 0x1:  // SLLI
                compile_slli(compiler, rd, rs1, shamt);
                break;
            case 0x5:  // SRLI/SRAI
                if (funct7 == 0x00) {
                    // SRLI - similar to SLLI but right shift
                } else if (funct7 == 0x20) {
                    // SRAI - similar to SLLI but arithmetic right shift
                }
                break;
            default:
                return -1;
        }
    } else {
        return -1;
    }
    
    return 0;
}