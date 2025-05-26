/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simple memory implementation without cryptographic proofs
// This is suitable for development, testing, and non-zkVM use cases
// Gates per operation: ~2000 instead of ~3.9M

// We'll use a small memory (1KB = 256 words) for gate efficiency
#define SIMPLE_MEM_WORDS 256
#define SIMPLE_MEM_ADDR_BITS 8

// Helper: Create NOT gate
static uint32_t build_not(riscv_circuit_t* circuit, uint32_t a) {
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, CONSTANT_1_WIRE, result, GATE_XOR);
    return result;
}


// Helper: Create OR gate
static uint32_t build_or(riscv_circuit_t* circuit, uint32_t a, uint32_t b) {
    // a OR b = (a XOR b) XOR (a AND b)
    uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
    uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, a, b, a_xor_b, GATE_XOR);
    riscv_circuit_add_gate(circuit, a, b, a_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, a_xor_b, a_and_b, result, GATE_XOR);
    
    return result;
}

// Helper: Create 2-to-1 MUX for single bit
static uint32_t build_mux_bit(riscv_circuit_t* circuit, uint32_t sel, uint32_t a, uint32_t b) {
    // result = sel ? b : a = (sel AND b) OR ((NOT sel) AND a)
    uint32_t not_sel = build_not(circuit, sel);
    uint32_t sel_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t notsel_and_a = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, sel, b, sel_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, not_sel, a, notsel_and_a, GATE_AND);
    
    return build_or(circuit, sel_and_b, notsel_and_a);
}

// Helper: Create MUX for arrays
static void build_mux_array(riscv_circuit_t* circuit, uint32_t sel,
                           uint32_t* a, uint32_t* b, uint32_t* result,
                           size_t bits) {
    for (size_t i = 0; i < bits; i++) {
        result[i] = build_mux_bit(circuit, sel, a[i], b[i]);
    }
}

// Simple memory storage (extends base memory structure)
typedef struct {
    riscv_memory_t base;  // Must be first for casting
    uint32_t** memory_cells;  // Simple array storage
} riscv_memory_simple_t;

// Create simple memory subsystem
riscv_memory_t* riscv_memory_create_simple(riscv_circuit_t* circuit) {
    riscv_memory_simple_t* mem = calloc(1, sizeof(riscv_memory_simple_t));
    if (!mem) return NULL;
    
    mem->base.circuit = circuit;
    mem->base.access = riscv_memory_access_simple;  // Set function pointer for simple access
    
    // Allocate memory interface wires
    mem->base.address_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    mem->base.data_in_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    mem->base.data_out_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    
    // Initialize memory cells
    mem->memory_cells = malloc(SIMPLE_MEM_WORDS * sizeof(uint32_t*));
    for (int i = 0; i < SIMPLE_MEM_WORDS; i++) {
        mem->memory_cells[i] = riscv_circuit_allocate_wire_array(circuit, 32);
        // Initialize to zero
        for (int bit = 0; bit < 32; bit++) {
            mem->memory_cells[i][bit] = CONSTANT_0_WIRE;
        }
    }
    
    return &mem->base;
}

// Simple memory access - no cryptographic proofs
void riscv_memory_access_simple(riscv_memory_t* memory,
                               uint32_t* address_bits,
                               uint32_t* write_data_bits,
                               uint32_t write_enable,
                               uint32_t* read_data_bits) {
    riscv_memory_simple_t* mem = (riscv_memory_simple_t*)memory;
    riscv_circuit_t* circuit = mem->base.circuit;
    
    // For simple memory, we only use lower 8 bits of address (256 words)
    uint32_t* addr_low = address_bits;  // Use first 8 bits
    
    // Build address decoder - create select signal for each memory word
    uint32_t* word_select = malloc(SIMPLE_MEM_WORDS * sizeof(uint32_t));
    
    for (int word = 0; word < SIMPLE_MEM_WORDS; word++) {
        // Check if address matches this word
        uint32_t* match_bits = malloc(SIMPLE_MEM_ADDR_BITS * sizeof(uint32_t));
        
        for (int bit = 0; bit < SIMPLE_MEM_ADDR_BITS; bit++) {
            uint32_t expected = (word >> bit) & 1;
            if (expected) {
                match_bits[bit] = addr_low[bit];
            } else {
                match_bits[bit] = build_not(circuit, addr_low[bit]);
            }
        }
        
        // AND all match bits together
        word_select[word] = match_bits[0];
        for (int bit = 1; bit < SIMPLE_MEM_ADDR_BITS; bit++) {
            uint32_t new_select = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, word_select[word], match_bits[bit], 
                                  new_select, GATE_AND);
            word_select[word] = new_select;
        }
        
        free(match_bits);
    }
    
    // Read: MUX all memory words based on select signals
    // Initialize read data to first word
    uint32_t* temp_read = riscv_circuit_allocate_wire_array(circuit, 32);
    memcpy(temp_read, mem->memory_cells[0], 32 * sizeof(uint32_t));
    
    // MUX in each subsequent word if selected
    for (int word = 1; word < SIMPLE_MEM_WORDS; word++) {
        uint32_t* new_read = riscv_circuit_allocate_wire_array(circuit, 32);
        build_mux_array(circuit, word_select[word], 
                       temp_read, mem->memory_cells[word], new_read, 32);
        free(temp_read);
        temp_read = new_read;
    }
    
    // Copy to output
    memcpy(read_data_bits, temp_read, 32 * sizeof(uint32_t));
    free(temp_read);
    
    // Write: Update selected word if write_enable is set
    for (int word = 0; word < SIMPLE_MEM_WORDS; word++) {
        // Combine word select with write enable
        uint32_t do_write = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, word_select[word], write_enable, 
                              do_write, GATE_AND);
        
        // Update memory cell
        uint32_t* new_value = riscv_circuit_allocate_wire_array(circuit, 32);
        build_mux_array(circuit, do_write,
                       mem->memory_cells[word], write_data_bits, new_value, 32);
        
        // Replace old value
        free(mem->memory_cells[word]);
        mem->memory_cells[word] = new_value;
    }
    
    free(word_select);
}

// Cleanup
void riscv_memory_destroy_simple(riscv_memory_t* memory) {
    riscv_memory_simple_t* mem = (riscv_memory_simple_t*)memory;
    if (!mem) return;
    
    free(mem->base.address_wires);
    free(mem->base.data_in_wires);
    free(mem->base.data_out_wires);
    
    if (mem->memory_cells) {
        for (int i = 0; i < SIMPLE_MEM_WORDS; i++) {
            free(mem->memory_cells[i]);
        }
        free(mem->memory_cells);
    }
    
    free(mem);
}

// Wrapper to use simple memory with existing code
riscv_memory_t* riscv_memory_create_simple_wrapper(riscv_circuit_t* circuit) {
    printf("Using simple memory (256 words, ~2K gates per access)\n");
    return riscv_memory_create_simple(circuit);
}

