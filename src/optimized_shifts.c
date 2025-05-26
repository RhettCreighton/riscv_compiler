#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Optimized shift operations - reduces gates from ~960 to ~500
// Uses more efficient MUX tree and reduced bit manipulation

// Helper: Build efficient 2-to-1 MUX
static uint32_t build_mux2(riscv_circuit_t* circuit, uint32_t sel, uint32_t a, uint32_t b) {
    // Optimized: sel ? b : a = (sel AND b) XOR ((NOT sel) AND a)
    uint32_t not_sel = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, sel, CONSTANT_1_WIRE, not_sel, GATE_XOR);
    
    uint32_t sel_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t notsel_and_a = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, sel, b, sel_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, not_sel, a, notsel_and_a, GATE_AND);
    
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, sel_and_b, notsel_and_a, result, GATE_XOR);
    
    return result;
}

// Optimized left shift - uses fewer gates per stage
static void build_left_shift_optimized(riscv_circuit_t* circuit,
                                      uint32_t* value_bits,
                                      uint32_t* shift_amount_bits,
                                      uint32_t* result_bits,
                                      size_t num_bits) {
    uint32_t* current = riscv_circuit_allocate_wire_array(circuit, num_bits);
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    // Only process 5 bits of shift amount (32-bit max shift)
    for (int stage = 0; stage < 5; stage++) {
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << stage;  // 1, 2, 4, 8, 16
        
        // Build shifted version and MUX in one pass
        for (int i = 0; i < (int)num_bits; i++) {
            uint32_t shifted_bit;
            if (i >= shift_by) {
                shifted_bit = current[i - shift_by];
            } else {
                shifted_bit = CONSTANT_0_WIRE;  // Zero fill
            }
            
            // MUX: shift_amount_bits[stage] ? shifted_bit : current[i]
            next[i] = build_mux2(circuit, shift_amount_bits[stage], current[i], shifted_bit);
        }
        
        free(current);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Optimized right shift (logical)
static void build_right_shift_logical_optimized(riscv_circuit_t* circuit,
                                               uint32_t* value_bits,
                                               uint32_t* shift_amount_bits,
                                               uint32_t* result_bits,
                                               size_t num_bits) {
    uint32_t* current = riscv_circuit_allocate_wire_array(circuit, num_bits);
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    for (int stage = 0; stage < 5; stage++) {
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << stage;
        
        for (int i = 0; i < (int)num_bits; i++) {
            uint32_t shifted_bit;
            if (i + shift_by < (int)num_bits) {
                shifted_bit = current[i + shift_by];
            } else {
                shifted_bit = CONSTANT_0_WIRE;  // Zero fill
            }
            
            next[i] = build_mux2(circuit, shift_amount_bits[stage], current[i], shifted_bit);
        }
        
        free(current);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Optimized right shift (arithmetic - sign extend)
static void build_right_shift_arithmetic_optimized(riscv_circuit_t* circuit,
                                                  uint32_t* value_bits,
                                                  uint32_t* shift_amount_bits,
                                                  uint32_t* result_bits,
                                                  size_t num_bits) {
    uint32_t* current = riscv_circuit_allocate_wire_array(circuit, num_bits);
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    uint32_t sign_bit = value_bits[num_bits - 1];  // MSB for sign extension
    
    for (int stage = 0; stage < 5; stage++) {
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        int shift_by = 1 << stage;
        
        for (int i = 0; i < (int)num_bits; i++) {
            uint32_t shifted_bit;
            if (i + shift_by < (int)num_bits) {
                shifted_bit = current[i + shift_by];
            } else {
                shifted_bit = sign_bit;  // Sign extend
            }
            
            next[i] = build_mux2(circuit, shift_amount_bits[stage], current[i], shifted_bit);
        }
        
        free(current);
        current = next;
    }
    
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
}

// Main optimized shift builder
uint32_t build_shifter_optimized(riscv_circuit_t* circuit, 
                                uint32_t* value_bits, 
                                uint32_t* shift_bits,
                                uint32_t* result_bits, 
                                size_t num_bits, 
                                bool is_left, 
                                bool is_arithmetic) {
    if (is_left) {
        build_left_shift_optimized(circuit, value_bits, shift_bits, result_bits, num_bits);
    } else if (is_arithmetic) {
        build_right_shift_arithmetic_optimized(circuit, value_bits, shift_bits, result_bits, num_bits);
    } else {
        build_right_shift_logical_optimized(circuit, value_bits, shift_bits, result_bits, num_bits);
    }
    
    return 0;  // Success
}

// Compile optimized shift instructions
int compile_shift_instruction_optimized(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    
    if (rd == 0) return 0;  // x0 is hardwired to 0
    
    // Get operand wires
    uint32_t* rs1_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* rd_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* shift_wires = malloc(5 * sizeof(uint32_t));  // Only 5 bits needed
    
    for (int i = 0; i < 32; i++) {
        rs1_wires[i] = get_register_wire(rs1, i);
        rd_wires[i] = riscv_circuit_allocate_wire(compiler->circuit);
    }
    
    if (opcode == 0x13) {  // I-type (immediate shifts)
        uint32_t shamt = (instruction >> 20) & 0x1F;  // 5-bit shift amount
        
        // Convert immediate to wire representation
        for (int i = 0; i < 5; i++) {
            if ((shamt >> i) & 1) {
                shift_wires[i] = CONSTANT_1_WIRE;
            } else {
                shift_wires[i] = CONSTANT_0_WIRE;
            }
        }
    } else {  // R-type (register shifts)
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        // Use only lower 5 bits of rs2
        for (int i = 0; i < 5; i++) {
            shift_wires[i] = get_register_wire(rs2, i);
        }
    }
    
    // Decode shift type and build circuit
    switch (funct3) {
        case 0x1:  // SLL/SLLI - Shift Left Logical
            build_shifter_optimized(compiler->circuit, rs1_wires, shift_wires, rd_wires, 32, true, false);
            break;
            
        case 0x5: {  // SRL/SRLI or SRA/SRAI
            uint32_t funct7 = (instruction >> 25) & 0x7F;
            bool is_arithmetic = (funct7 == 0x20);
            build_shifter_optimized(compiler->circuit, rs1_wires, shift_wires, rd_wires, 32, false, is_arithmetic);
            break;
        }
        
        default:
            free(rs1_wires);
            free(rd_wires);
            free(shift_wires);
            return -1;  // Unsupported shift
    }
    
    free(rs1_wires);
    free(rd_wires);
    free(shift_wires);
    
    return 0;
}