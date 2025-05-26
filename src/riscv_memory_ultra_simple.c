/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Ultra-simple memory for gate optimization
// Only supports 8 memory locations (words) - perfect for small demos
// Gates per operation: ~200 instead of ~101K

#define ULTRA_SIMPLE_MEM_WORDS 8
#define ULTRA_SIMPLE_ADDR_BITS 3

// Ultra-simple memory storage
typedef struct {
    riscv_memory_t base;
    uint32_t** memory_cells;
} riscv_memory_ultra_simple_t;

// Helper: Create NOT gate
static uint32_t build_not(riscv_circuit_t* circuit, uint32_t a) {
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, CONSTANT_1_WIRE, result, GATE_XOR);
    return result;
}

// Helper: Build 8-to-1 MUX for single bit (optimized)
static uint32_t build_mux8_bit(riscv_circuit_t* circuit, 
                                uint32_t* sel,  // 3 select bits
                                uint32_t* inputs) { // 8 input bits
    // Level 1: 4 2-to-1 muxes
    uint32_t level1[4];
    for (int i = 0; i < 4; i++) {
        uint32_t sel_bit = sel[0];
        uint32_t not_sel = build_not(circuit, sel_bit);
        
        uint32_t and0 = riscv_circuit_allocate_wire(circuit);
        uint32_t and1 = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, not_sel, inputs[i*2], and0, GATE_AND);
        riscv_circuit_add_gate(circuit, sel_bit, inputs[i*2+1], and1, GATE_AND);
        
        level1[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, and0, and1, level1[i], GATE_XOR);
    }
    
    // Level 2: 2 2-to-1 muxes
    uint32_t level2[2];
    for (int i = 0; i < 2; i++) {
        uint32_t sel_bit = sel[1];
        uint32_t not_sel = build_not(circuit, sel_bit);
        
        uint32_t and0 = riscv_circuit_allocate_wire(circuit);
        uint32_t and1 = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, not_sel, level1[i*2], and0, GATE_AND);
        riscv_circuit_add_gate(circuit, sel_bit, level1[i*2+1], and1, GATE_AND);
        
        level2[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, and0, and1, level2[i], GATE_XOR);
    }
    
    // Level 3: Final 2-to-1 mux
    uint32_t sel_bit = sel[2];
    uint32_t not_sel = build_not(circuit, sel_bit);
    
    uint32_t and0 = riscv_circuit_allocate_wire(circuit);
    uint32_t and1 = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, not_sel, level2[0], and0, GATE_AND);
    riscv_circuit_add_gate(circuit, sel_bit, level2[1], and1, GATE_AND);
    
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, and0, and1, result, GATE_XOR);
    
    return result;
}

// Create ultra-simple memory
riscv_memory_t* riscv_memory_create_ultra_simple(riscv_circuit_t* circuit) {
    riscv_memory_ultra_simple_t* mem = calloc(1, sizeof(riscv_memory_ultra_simple_t));
    if (!mem) return NULL;
    
    mem->base.circuit = circuit;
    mem->base.access = riscv_memory_access_ultra_simple;
    
    // Allocate interface wires
    mem->base.address_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    mem->base.data_in_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    mem->base.data_out_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    
    // Initialize memory cells
    mem->memory_cells = malloc(ULTRA_SIMPLE_MEM_WORDS * sizeof(uint32_t*));
    for (int i = 0; i < ULTRA_SIMPLE_MEM_WORDS; i++) {
        mem->memory_cells[i] = riscv_circuit_allocate_wire_array(circuit, 32);
        // Initialize to zero
        for (int bit = 0; bit < 32; bit++) {
            mem->memory_cells[i][bit] = CONSTANT_0_WIRE;
        }
    }
    
    return &mem->base;
}

// Ultra-simple memory access
void riscv_memory_access_ultra_simple(riscv_memory_t* memory,
                                     uint32_t* address_bits,
                                     uint32_t* write_data_bits,
                                     uint32_t write_enable,
                                     uint32_t* read_data_bits) {
    riscv_memory_ultra_simple_t* mem = (riscv_memory_ultra_simple_t*)memory;
    riscv_circuit_t* circuit = mem->base.circuit;
    
    // Use only bottom 3 bits of address
    uint32_t* addr_select = address_bits;
    
    // Read: 8-to-1 MUX for each bit
    for (int bit = 0; bit < 32; bit++) {
        uint32_t inputs[8];
        for (int word = 0; word < 8; word++) {
            inputs[word] = mem->memory_cells[word][bit];
        }
        read_data_bits[bit] = build_mux8_bit(circuit, addr_select, inputs);
    }
    
    // Write: Decoder + conditional update
    // Build 3-to-8 decoder
    uint32_t decoder[8];
    for (int word = 0; word < 8; word++) {
        // Each decoder output is AND of appropriate addr bits
        uint32_t decode = CONSTANT_1_WIRE;
        for (int bit = 0; bit < 3; bit++) {
            uint32_t addr_bit = ((word >> bit) & 1) ? 
                               addr_select[bit] : build_not(circuit, addr_select[bit]);
            uint32_t new_decode = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, decode, addr_bit, new_decode, GATE_AND);
            decode = new_decode;
        }
        
        // Combine with write enable
        decoder[word] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, decode, write_enable, decoder[word], GATE_AND);
    }
    
    // Update each memory word conditionally
    for (int word = 0; word < 8; word++) {
        uint32_t* new_value = riscv_circuit_allocate_wire_array(circuit, 32);
        for (int bit = 0; bit < 32; bit++) {
            // MUX between old value and write data
            uint32_t not_sel = build_not(circuit, decoder[word]);
            uint32_t keep_old = riscv_circuit_allocate_wire(circuit);
            uint32_t take_new = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, not_sel, mem->memory_cells[word][bit], 
                                  keep_old, GATE_AND);
            riscv_circuit_add_gate(circuit, decoder[word], write_data_bits[bit], 
                                  take_new, GATE_AND);
            
            new_value[bit] = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, keep_old, take_new, new_value[bit], GATE_XOR);
        }
        
        free(mem->memory_cells[word]);
        mem->memory_cells[word] = new_value;
    }
}

// Cleanup
void riscv_memory_destroy_ultra_simple(riscv_memory_t* memory) {
    riscv_memory_ultra_simple_t* mem = (riscv_memory_ultra_simple_t*)memory;
    if (!mem) return;
    
    free(mem->base.address_wires);
    free(mem->base.data_in_wires);
    free(mem->base.data_out_wires);
    
    if (mem->memory_cells) {
        for (int i = 0; i < ULTRA_SIMPLE_MEM_WORDS; i++) {
            free(mem->memory_cells[i]);
        }
        free(mem->memory_cells);
    }
    
    free(mem);
}