#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>

// Helper: Create OR gate using AND/XOR
// a OR b = (a XOR b) XOR (a AND b)
static uint32_t build_or_gate(riscv_circuit_t* circuit, uint32_t a, uint32_t b) {
    uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
    uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, a, b, a_xor_b, GATE_XOR);
    riscv_circuit_add_gate(circuit, a, b, a_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, a_xor_b, a_and_b, result, GATE_XOR);
    
    return result;
}

// Helper: Create NOT gate using XOR with constant 1
static uint32_t build_not_gate(riscv_circuit_t* circuit, uint32_t a) {
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, CONSTANT_1_WIRE, result, GATE_XOR);  // XOR with constant 1
    return result;
}

// Helper: Create MUX (multiplexer)
// result = sel ? b : a = (sel AND b) OR ((NOT sel) AND a)
static uint32_t build_mux(riscv_circuit_t* circuit, uint32_t sel, uint32_t a, uint32_t b) {
    uint32_t not_sel = build_not_gate(circuit, sel);
    uint32_t sel_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t notsel_and_a = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, sel, b, sel_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, not_sel, a, notsel_and_a, GATE_AND);
    
    return build_or_gate(circuit, sel_and_b, notsel_and_a);
}

// Build a comparator for less-than operation
uint32_t build_comparator(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                          size_t num_bits, bool is_signed) {
    // For unsigned: a < b if a - b produces a borrow (carry = 0)
    // For signed: need to check sign bits and overflow
    
    uint32_t* diff_bits = riscv_circuit_allocate_wire_array(circuit, num_bits);
    uint32_t borrow = build_subtractor(circuit, a_bits, b_bits, diff_bits, num_bits);
    
    if (!is_signed) {
        // Unsigned comparison: less_than = NOT borrow
        uint32_t result = build_not_gate(circuit, borrow);
        free(diff_bits);
        return result;
    } else {
        // Signed comparison is more complex
        // less_than = (a_sign XOR b_sign) ? a_sign : diff_sign
        uint32_t a_sign = a_bits[num_bits - 1];
        uint32_t b_sign = b_bits[num_bits - 1];
        uint32_t diff_sign = diff_bits[num_bits - 1];
        
        uint32_t signs_differ = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_sign, b_sign, signs_differ, GATE_XOR);
        
        uint32_t result = build_mux(circuit, signs_differ, diff_sign, a_sign);
        free(diff_bits);
        return result;
    }
}

// Build a left shifter
uint32_t build_shifter(riscv_circuit_t* circuit, uint32_t* value_bits, uint32_t* shift_bits,
                       uint32_t* result_bits, size_t num_bits, bool is_left, bool is_arithmetic) {
    // For simplicity, we'll implement a barrel shifter
    // This uses MUXes to select shifted values
    
    // Copy input to working array
    uint32_t* current = malloc(num_bits * sizeof(uint32_t));
    memcpy(current, value_bits, num_bits * sizeof(uint32_t));
    
    // Process each shift bit (we only need log2(num_bits) bits)
    size_t max_shift_bits = 5;  // For 32-bit values
    for (size_t shift_bit = 0; shift_bit < max_shift_bits; shift_bit++) {
        uint32_t* shifted = riscv_circuit_allocate_wire_array(circuit, num_bits);
        size_t shift_amount = 1 << shift_bit;
        
        if (is_left) {
            // Left shift
            for (size_t i = 0; i < num_bits; i++) {
                if (i < shift_amount) {
                    shifted[i] = CONSTANT_0_WIRE;  // Fill with constant 0
                } else {
                    shifted[i] = current[i - shift_amount];
                }
            }
        } else {
            // Right shift
            for (size_t i = 0; i < num_bits; i++) {
                if (i + shift_amount < num_bits) {
                    shifted[i] = current[i + shift_amount];
                } else {
                    if (is_arithmetic) {
                        // Sign extend
                        shifted[i] = current[num_bits - 1];
                    } else {
                        // Zero extend
                        shifted[i] = CONSTANT_0_WIRE;  // Fill with constant 0
                    }
                }
            }
        }
        
        // MUX between current and shifted based on shift_bits[shift_bit]
        uint32_t* next = riscv_circuit_allocate_wire_array(circuit, num_bits);
        for (size_t i = 0; i < num_bits; i++) {
            next[i] = build_mux(circuit, shift_bits[shift_bit], current[i], shifted[i]);
        }
        
        free(current);
        free(shifted);
        current = next;
    }
    
    // Copy result
    memcpy(result_bits, current, num_bits * sizeof(uint32_t));
    free(current);
    
    return 0;  // No carry for shifts
}

// Build a multiplier using shift-and-add algorithm
// This is expensive in gates but works
uint32_t* build_multiplier(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                           size_t num_bits) {
    // Result is 2*num_bits wide
    uint32_t* result = riscv_circuit_allocate_wire_array(circuit, 2 * num_bits);
    
    // Initialize result to 0
    for (size_t i = 0; i < 2 * num_bits; i++) {
        result[i] = CONSTANT_0_WIRE;  // Initialize to constant 0
    }
    
    // For each bit of b
    for (size_t i = 0; i < num_bits; i++) {
        // Create shifted version of a
        uint32_t* shifted_a = riscv_circuit_allocate_wire_array(circuit, 2 * num_bits);
        for (size_t j = 0; j < 2 * num_bits; j++) {
            if (j < i) {
                shifted_a[j] = CONSTANT_0_WIRE;  // Zero fill left
            } else if (j - i < num_bits) {
                shifted_a[j] = a_bits[j - i];
            } else {
                shifted_a[j] = CONSTANT_0_WIRE;  // Zero fill right
            }
        }
        
        // Conditionally add shifted_a to result based on b[i]
        uint32_t* new_result = riscv_circuit_allocate_wire_array(circuit, 2 * num_bits);
        uint32_t* addend = riscv_circuit_allocate_wire_array(circuit, 2 * num_bits);
        
        // If b[i] is 1, addend = shifted_a, else addend = 0
        for (size_t j = 0; j < 2 * num_bits; j++) {
            uint32_t b_and_shifted = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, b_bits[i], shifted_a[j], b_and_shifted, GATE_AND);
            addend[j] = b_and_shifted;
        }
        
        // Add to result
        build_adder(circuit, result, addend, new_result, 2 * num_bits);
        
        free(result);
        free(shifted_a);
        free(addend);
        result = new_result;
    }
    
    return result;
}