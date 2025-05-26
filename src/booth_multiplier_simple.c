/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simplified Booth multiplier that focuses on gate reduction
// Key insight: Process 2 bits at a time, reducing partial products by half

// Build a simple 5:1 multiplexer for Booth encoding
// Selects between 0, +M, +2M, -M, -2M based on 3-bit window
static void booth_select_partial_product(riscv_circuit_t* circuit,
                                       uint32_t* multiplicand, size_t bits,
                                       uint32_t bit2, uint32_t bit1, uint32_t bit0,
                                       uint32_t* output) {
    // Booth encoding logic:
    // 000 -> 0
    // 001 -> +M
    // 010 -> +M  
    // 011 -> +2M
    // 100 -> -2M
    // 101 -> -M
    // 110 -> -M
    // 111 -> 0
    
    // Create selection signals
    uint32_t not_bit2 = riscv_circuit_allocate_wire(circuit);
    uint32_t not_bit1 = riscv_circuit_allocate_wire(circuit);
    uint32_t not_bit0 = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, bit2, CONSTANT_1_WIRE, not_bit2, GATE_XOR);
    riscv_circuit_add_gate(circuit, bit1, CONSTANT_1_WIRE, not_bit1, GATE_XOR);
    riscv_circuit_add_gate(circuit, bit0, CONSTANT_1_WIRE, not_bit0, GATE_XOR);
    
    // Detect zero case: (000 or 111)
    uint32_t all_zero = riscv_circuit_allocate_wire(circuit);
    uint32_t temp1 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, not_bit2, not_bit1, temp1, GATE_AND);
    riscv_circuit_add_gate(circuit, temp1, not_bit0, all_zero, GATE_AND);
    
    uint32_t all_one = riscv_circuit_allocate_wire(circuit);
    uint32_t temp2 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, bit2, bit1, temp2, GATE_AND);
    riscv_circuit_add_gate(circuit, temp2, bit0, all_one, GATE_AND);
    
    uint32_t is_zero = riscv_circuit_allocate_wire(circuit);
    uint32_t zero_xor = riscv_circuit_allocate_wire(circuit);
    uint32_t zero_and = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, all_zero, all_one, zero_xor, GATE_XOR);
    riscv_circuit_add_gate(circuit, all_zero, all_one, zero_and, GATE_AND);
    riscv_circuit_add_gate(circuit, zero_xor, zero_and, is_zero, GATE_XOR); // OR
    
    // Detect +2M case: 011
    uint32_t is_plus_two = riscv_circuit_allocate_wire(circuit);
    uint32_t temp3 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, not_bit2, bit1, temp3, GATE_AND);
    riscv_circuit_add_gate(circuit, temp3, bit0, is_plus_two, GATE_AND);
    
    // Detect -2M case: 100
    uint32_t is_minus_two = riscv_circuit_allocate_wire(circuit);
    uint32_t temp4 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, bit2, not_bit1, temp4, GATE_AND);
    riscv_circuit_add_gate(circuit, temp4, not_bit0, is_minus_two, GATE_AND);
    
    // Detect if we need negation (bit2 = 1)
    uint32_t need_negate = bit2;
    
    // Generate output for each bit
    for (size_t i = 0; i <= bits; i++) {
        uint32_t m_bit = (i < bits) ? multiplicand[i] : CONSTANT_0_WIRE;
        uint32_t m_shift = (i > 0 && i <= bits) ? multiplicand[i-1] : CONSTANT_0_WIRE;
        
        // Select between M and 2M (shifted)
        uint32_t pos_value = riscv_circuit_allocate_wire(circuit);
        uint32_t m_term = riscv_circuit_allocate_wire(circuit);
        uint32_t shift_term = riscv_circuit_allocate_wire(circuit);
        
        // If is_plus_two, use shifted value
        riscv_circuit_add_gate(circuit, is_plus_two, m_shift, shift_term, GATE_AND);
        
        // If not is_plus_two and not is_minus_two, use normal value
        uint32_t not_plus_two = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, is_plus_two, CONSTANT_1_WIRE, not_plus_two, GATE_XOR);
        
        uint32_t use_normal = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, not_plus_two, is_minus_two, use_normal, GATE_XOR); // NOR
        
        riscv_circuit_add_gate(circuit, use_normal, m_bit, m_term, GATE_AND);
        
        // Combine terms
        riscv_circuit_add_gate(circuit, m_term, shift_term, pos_value, GATE_XOR);
        
        // Apply negation if needed
        uint32_t value = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, pos_value, need_negate, value, GATE_XOR);
        
        // Apply zero mask
        uint32_t not_zero = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, is_zero, CONSTANT_1_WIRE, not_zero, GATE_XOR);
        
        output[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, value, not_zero, output[i], GATE_AND);
    }
    
    // Add correction bit for negative values
    // When negating, we need to add 1 (two's complement)
    output[0] = riscv_circuit_allocate_wire(circuit);
    uint32_t correction = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, need_negate, is_zero, correction, GATE_XOR); // need_negate AND NOT is_zero
    riscv_circuit_add_gate(circuit, output[0], correction, output[0], GATE_XOR);
}

// Optimized Booth multiplier implementation
void build_booth_multiplier_optimized(riscv_circuit_t* circuit,
                                    uint32_t* multiplicand, uint32_t* multiplier,
                                    uint32_t* product, size_t bits) {
    // We'll accumulate the result directly
    uint32_t* accumulator = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
    
    // Initialize accumulator to 0
    for (size_t i = 0; i < 2 * bits; i++) {
        accumulator[i] = CONSTANT_0_WIRE;
    }
    
    // Process multiplier 2 bits at a time
    for (size_t i = 0; i < bits; i += 2) {
        // Get 3-bit window
        uint32_t bit0 = (i == 0) ? CONSTANT_0_WIRE : multiplier[i - 1];
        uint32_t bit1 = multiplier[i];
        uint32_t bit2 = (i + 1 < bits) ? multiplier[i + 1] : CONSTANT_0_WIRE;
        
        // Get Booth-encoded partial product
        uint32_t* partial = riscv_circuit_allocate_wire_array(circuit, bits + 1);
        booth_select_partial_product(circuit, multiplicand, bits, bit2, bit1, bit0, partial);
        
        // Shift partial product to correct position
        uint32_t* shifted = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
        for (size_t j = 0; j < 2 * bits; j++) {
            if (j < i) {
                shifted[j] = CONSTANT_0_WIRE;
            } else if (j - i <= bits) {
                shifted[j] = partial[j - i];
            } else {
                shifted[j] = partial[bits]; // Sign extend
            }
        }
        
        // Add to accumulator
        uint32_t* new_accumulator = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
        build_adder(circuit, accumulator, shifted, new_accumulator, 2 * bits);
        
        free(accumulator);
        free(partial);
        free(shifted);
        accumulator = new_accumulator;
    }
    
    // Copy result
    memcpy(product, accumulator, 2 * bits * sizeof(uint32_t));
    free(accumulator);
}

// Use the optimized version
void build_booth_multiplier(riscv_circuit_t* circuit,
                           uint32_t* multiplicand, uint32_t* multiplier,
                           uint32_t* product, size_t bits) {
    size_t gates_before = circuit->num_gates;
    build_booth_multiplier_optimized(circuit, multiplicand, multiplier, product, bits);
    size_t gates_used = circuit->num_gates - gates_before;
    // Debug: print gate count
    if (getenv("DEBUG_BOOTH")) {
        printf("Booth multiplier used %zu gates for %zu-bit multiply\n", gates_used, bits);
    }
}