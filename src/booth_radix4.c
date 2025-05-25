#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Radix-4 Modified Booth Multiplier
// Goal: Achieve <5000 gates for 32x32 multiplication

// Booth recoding table for radix-4
// Examines 3 bits at positions 2i+1, 2i, 2i-1
static void get_booth_action(uint32_t b2, uint32_t b1, uint32_t b0,
                            uint32_t* negate, uint32_t* zero, uint32_t* shift) {
    // Table:
    // b2 b1 b0 | action
    // 0  0  0  | 0
    // 0  0  1  | +1M
    // 0  1  0  | +1M
    // 0  1  1  | +2M
    // 1  0  0  | -2M
    // 1  0  1  | -1M
    // 1  1  0  | -1M
    // 1  1  1  | 0
    
    // Simplified logic:
    *zero = (!b2 && !b1 && !b0) || (b2 && b1 && b0);
    *negate = b2;
    *shift = (!b2 && b1 && b0) || (b2 && !b1 && !b0);
}

// Build minimal Booth encoder using gates
static void build_booth_encoder_minimal(riscv_circuit_t* circuit,
                                      uint32_t b2, uint32_t b1, uint32_t b0,
                                      uint32_t* neg_out, uint32_t* zero_out, uint32_t* two_out) {
    // neg = b2
    *neg_out = b2;
    
    // zero = (b2 & b1 & b0) | (~b2 & ~b1 & ~b0)
    uint32_t all_ones = riscv_circuit_allocate_wire(circuit);
    uint32_t temp1 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, b2, b1, temp1, GATE_AND);
    riscv_circuit_add_gate(circuit, temp1, b0, all_ones, GATE_AND);
    
    uint32_t not_b2 = riscv_circuit_allocate_wire(circuit);
    uint32_t not_b1 = riscv_circuit_allocate_wire(circuit);
    uint32_t not_b0 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, b2, CONSTANT_1_WIRE, not_b2, GATE_XOR);
    riscv_circuit_add_gate(circuit, b1, CONSTANT_1_WIRE, not_b1, GATE_XOR);
    riscv_circuit_add_gate(circuit, b0, CONSTANT_1_WIRE, not_b0, GATE_XOR);
    
    uint32_t all_zeros = riscv_circuit_allocate_wire(circuit);
    uint32_t temp2 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, not_b2, not_b1, temp2, GATE_AND);
    riscv_circuit_add_gate(circuit, temp2, not_b0, all_zeros, GATE_AND);
    
    // OR using XOR+AND trick
    uint32_t zero_xor = riscv_circuit_allocate_wire(circuit);
    uint32_t zero_and = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, all_ones, all_zeros, zero_xor, GATE_XOR);
    riscv_circuit_add_gate(circuit, all_ones, all_zeros, zero_and, GATE_AND);
    *zero_out = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, zero_xor, zero_and, *zero_out, GATE_XOR);
    
    // two = (b1 & b0 & ~b2) | (~b1 & ~b0 & b2)
    uint32_t case1 = riscv_circuit_allocate_wire(circuit);
    uint32_t temp3 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, b1, b0, temp3, GATE_AND);
    riscv_circuit_add_gate(circuit, temp3, not_b2, case1, GATE_AND);
    
    uint32_t case2 = riscv_circuit_allocate_wire(circuit);
    uint32_t temp4 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, not_b1, not_b0, temp4, GATE_AND);
    riscv_circuit_add_gate(circuit, temp4, b2, case2, GATE_AND);
    
    uint32_t two_xor = riscv_circuit_allocate_wire(circuit);
    uint32_t two_and = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, case1, case2, two_xor, GATE_XOR);
    riscv_circuit_add_gate(circuit, case1, case2, two_and, GATE_AND);
    *two_out = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, two_xor, two_and, *two_out, GATE_XOR);
}

// Efficient partial product generator
static void generate_booth_partial_product(riscv_circuit_t* circuit,
                                         uint32_t* multiplicand, size_t bits,
                                         uint32_t neg, uint32_t zero, uint32_t two,
                                         uint32_t* pp_out) {
    // For each bit position
    for (size_t i = 0; i <= bits; i++) {
        // Get the multiplicand bit (0 if out of range)
        uint32_t m_bit = (i < bits) ? multiplicand[i] : CONSTANT_0_WIRE;
        
        // Get the shifted bit for 2M
        uint32_t m_shift = (i > 0 && i <= bits) ? multiplicand[i-1] : CONSTANT_0_WIRE;
        
        // Select between M and 2M
        uint32_t selected = riscv_circuit_allocate_wire(circuit);
        uint32_t not_two = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, two, CONSTANT_1_WIRE, not_two, GATE_XOR);
        
        uint32_t m_term = riscv_circuit_allocate_wire(circuit);
        uint32_t shift_term = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, not_two, m_bit, m_term, GATE_AND);
        riscv_circuit_add_gate(circuit, two, m_shift, shift_term, GATE_AND);
        
        riscv_circuit_add_gate(circuit, m_term, shift_term, selected, GATE_XOR);
        
        // Apply negation
        uint32_t possibly_negated = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, selected, neg, possibly_negated, GATE_XOR);
        
        // Apply zero mask
        uint32_t not_zero = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, zero, CONSTANT_1_WIRE, not_zero, GATE_XOR);
        
        pp_out[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, possibly_negated, not_zero, pp_out[i], GATE_AND);
    }
    
    // Handle two's complement correction for negative values
    // If neg and not zero, add 1 to LSB
    uint32_t need_correction = riscv_circuit_allocate_wire(circuit);
    uint32_t not_zero_for_corr = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, zero, CONSTANT_1_WIRE, not_zero_for_corr, GATE_XOR);
    riscv_circuit_add_gate(circuit, neg, not_zero_for_corr, need_correction, GATE_AND);
    
    // Add correction to LSB
    uint32_t new_lsb = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, pp_out[0], need_correction, new_lsb, GATE_XOR);
    pp_out[0] = new_lsb;
}

// Carry-save adder for accumulation
static void carry_save_add(riscv_circuit_t* circuit,
                          uint32_t* sum, uint32_t* carry, uint32_t* addend,
                          uint32_t* new_sum, uint32_t* new_carry, size_t bits) {
    for (size_t i = 0; i < bits; i++) {
        // Full adder logic but keep sum and carry separate
        uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, sum[i], addend[i], a_xor_b, GATE_XOR);
        
        new_sum[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_xor_b, carry[i], new_sum[i], GATE_XOR);
        
        // New carry = (a & b) | (c & (a ^ b))
        uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
        uint32_t c_and_xor = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, sum[i], addend[i], a_and_b, GATE_AND);
        riscv_circuit_add_gate(circuit, carry[i], a_xor_b, c_and_xor, GATE_AND);
        
        uint32_t carry_xor = riscv_circuit_allocate_wire(circuit);
        uint32_t carry_and = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_and_b, c_and_xor, carry_xor, GATE_XOR);
        riscv_circuit_add_gate(circuit, a_and_b, c_and_xor, carry_and, GATE_AND);
        
        new_carry[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, carry_xor, carry_and, new_carry[i], GATE_XOR);
    }
}

// Optimized radix-4 Booth multiplier
void build_booth_multiplier(riscv_circuit_t* circuit,
                           uint32_t* multiplicand, uint32_t* multiplier,
                           uint32_t* product, size_t bits) {
    size_t gates_start = circuit->num_gates;
    
    // Use carry-save representation for accumulation
    uint32_t* sum = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
    uint32_t* carry = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
    
    // Initialize to zero
    for (size_t i = 0; i < 2 * bits; i++) {
        sum[i] = CONSTANT_0_WIRE;
        carry[i] = CONSTANT_0_WIRE;
    }
    
    // Process 2 bits at a time
    for (size_t i = 0; i < bits; i += 2) {
        // Get 3-bit window
        uint32_t b0 = (i == 0) ? CONSTANT_0_WIRE : multiplier[i - 1];
        uint32_t b1 = multiplier[i];
        uint32_t b2 = (i + 1 < bits) ? multiplier[i + 1] : CONSTANT_0_WIRE;
        
        // Booth encode
        uint32_t neg, zero, two;
        build_booth_encoder_minimal(circuit, b2, b1, b0, &neg, &zero, &two);
        
        // Generate partial product
        uint32_t* pp = riscv_circuit_allocate_wire_array(circuit, bits + 1);
        generate_booth_partial_product(circuit, multiplicand, bits, neg, zero, two, pp);
        
        // Shift and sign-extend partial product
        uint32_t* shifted_pp = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
        for (size_t j = 0; j < 2 * bits; j++) {
            if (j < i) {
                shifted_pp[j] = CONSTANT_0_WIRE;
            } else if (j - i <= bits) {
                shifted_pp[j] = pp[j - i];
            } else {
                shifted_pp[j] = pp[bits]; // Sign extend
            }
        }
        
        // Accumulate using carry-save adder
        uint32_t* new_sum = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
        uint32_t* new_carry = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
        
        carry_save_add(circuit, sum, carry, shifted_pp, new_sum, new_carry, 2 * bits);
        
        free(sum);
        free(carry);
        free(pp);
        free(shifted_pp);
        
        sum = new_sum;
        carry = new_carry;
    }
    
    // Final addition to convert from carry-save to normal form
    // Shift carry left by 1
    uint32_t* shifted_carry = riscv_circuit_allocate_wire_array(circuit, 2 * bits);
    shifted_carry[0] = CONSTANT_0_WIRE;
    for (size_t i = 1; i < 2 * bits; i++) {
        shifted_carry[i] = carry[i - 1];
    }
    
    // Final add
    build_adder(circuit, sum, shifted_carry, product, 2 * bits);
    
    free(sum);
    free(carry);
    free(shifted_carry);
    
    if (getenv("DEBUG_BOOTH")) {
        size_t gates_used = circuit->num_gates - gates_start;
        printf("Radix-4 Booth multiplier: %zu gates for %zu-bit multiply\n", gates_used, bits);
    }
}