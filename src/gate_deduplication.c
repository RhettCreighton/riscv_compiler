/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Advanced gate deduplication for reducing circuit size
// Identifies and merges identical gate patterns across the entire circuit

#define DEDUP_HASH_SIZE 65536
#define MAX_GATE_INPUTS 8

// Hash table entry for gate deduplication
typedef struct gate_hash_entry {
    uint32_t left_input;
    uint32_t right_input;
    gate_type_t type;
    uint32_t output_wire;
    struct gate_hash_entry* next;
} gate_hash_entry_t;

// Global deduplication state
typedef struct {
    gate_hash_entry_t** hash_table;
    size_t original_gates;
    size_t deduplicated_gates;
    size_t gates_saved;
} gate_dedup_state_t;

static gate_dedup_state_t* g_dedup_state = NULL;

// Hash function for gate signatures
static uint32_t hash_gate(uint32_t left, uint32_t right, gate_type_t type) {
    uint32_t hash = 2166136261U;  // FNV-1a offset basis
    hash ^= left;
    hash *= 16777619U;
    hash ^= right;
    hash *= 16777619U;
    hash ^= (uint32_t)type;
    hash *= 16777619U;
    return hash % DEDUP_HASH_SIZE;
}

// Initialize deduplication system
void gate_dedup_init(void) {
    if (g_dedup_state) return;  // Already initialized
    
    g_dedup_state = calloc(1, sizeof(gate_dedup_state_t));
    g_dedup_state->hash_table = calloc(DEDUP_HASH_SIZE, sizeof(gate_hash_entry_t*));
    g_dedup_state->original_gates = 0;
    g_dedup_state->deduplicated_gates = 0;
    g_dedup_state->gates_saved = 0;
}

// Cleanup deduplication system
void gate_dedup_cleanup(void) {
    if (!g_dedup_state) return;
    
    for (size_t i = 0; i < DEDUP_HASH_SIZE; i++) {
        gate_hash_entry_t* entry = g_dedup_state->hash_table[i];
        while (entry) {
            gate_hash_entry_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
    free(g_dedup_state->hash_table);
    free(g_dedup_state);
    g_dedup_state = NULL;
}

// Find or create deduplicated gate
uint32_t gate_dedup_add(riscv_circuit_t* circuit, uint32_t left, uint32_t right, gate_type_t type) {
    if (!g_dedup_state) gate_dedup_init();
    
    g_dedup_state->original_gates++;
    
    // Normalize inputs for commutative operations
    if (type == GATE_AND || type == GATE_XOR) {
        if (left > right) {
            uint32_t temp = left;
            left = right;
            right = temp;
        }
    }
    
    uint32_t hash = hash_gate(left, right, type);
    gate_hash_entry_t* entry = g_dedup_state->hash_table[hash];
    
    // Search for existing gate
    while (entry) {
        if (entry->left_input == left && 
            entry->right_input == right && 
            entry->type == type) {
            // Found duplicate! Return existing output wire
            g_dedup_state->gates_saved++;
            return entry->output_wire;
        }
        entry = entry->next;
    }
    
    // No duplicate found, create new gate
    uint32_t output = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, left, right, output, type);
    g_dedup_state->deduplicated_gates++;
    
    // Add to hash table
    gate_hash_entry_t* new_entry = malloc(sizeof(gate_hash_entry_t));
    new_entry->left_input = left;
    new_entry->right_input = right;
    new_entry->type = type;
    new_entry->output_wire = output;
    new_entry->next = g_dedup_state->hash_table[hash];
    g_dedup_state->hash_table[hash] = new_entry;
    
    return output;
}

// Report deduplication statistics
void gate_dedup_report(void) {
    if (!g_dedup_state) {
        printf("Gate deduplication not initialized\n");
        return;
    }
    
    printf("\n=== Gate Deduplication Report ===\n");
    printf("Original gates requested: %zu\n", g_dedup_state->original_gates);
    printf("Actual gates created: %zu\n", g_dedup_state->deduplicated_gates);
    printf("Gates saved: %zu\n", g_dedup_state->gates_saved);
    
    if (g_dedup_state->original_gates > 0) {
        double savings_percent = (100.0 * g_dedup_state->gates_saved) / g_dedup_state->original_gates;
        printf("Gate reduction: %.1f%%\n", savings_percent);
    }
}

// Pattern-based deduplication for common subcircuits
typedef struct {
    const char* name;
    size_t input_count;
    size_t gate_count;
    uint32_t (*builder)(riscv_circuit_t*, uint32_t*, uint32_t*);
} common_pattern_t;

// Build optimized 2-bit adder (used in many places)
static uint32_t build_2bit_adder_optimized(riscv_circuit_t* circuit, uint32_t* inputs, uint32_t* outputs) {
    uint32_t a0 = inputs[0], a1 = inputs[1];
    uint32_t b0 = inputs[2], b1 = inputs[3];
    uint32_t cin = inputs[4];
    
    // First bit: sum0 = a0 XOR b0 XOR cin
    uint32_t a0_xor_b0 = gate_dedup_add(circuit, a0, b0, GATE_XOR);
    uint32_t sum0 = gate_dedup_add(circuit, a0_xor_b0, cin, GATE_XOR);
    
    // First carry: c0 = (a0 AND b0) OR (cin AND (a0 XOR b0))
    uint32_t a0_and_b0 = gate_dedup_add(circuit, a0, b0, GATE_AND);
    uint32_t cin_and_xor = gate_dedup_add(circuit, cin, a0_xor_b0, GATE_AND);
    uint32_t c0_xor = gate_dedup_add(circuit, a0_and_b0, cin_and_xor, GATE_XOR);
    uint32_t c0_and = gate_dedup_add(circuit, a0_and_b0, cin_and_xor, GATE_AND);
    uint32_t c0 = gate_dedup_add(circuit, c0_xor, c0_and, GATE_XOR);
    
    // Second bit: sum1 = a1 XOR b1 XOR c0
    uint32_t a1_xor_b1 = gate_dedup_add(circuit, a1, b1, GATE_XOR);
    uint32_t sum1 = gate_dedup_add(circuit, a1_xor_b1, c0, GATE_XOR);
    
    // Second carry: cout = (a1 AND b1) OR (c0 AND (a1 XOR b1))
    uint32_t a1_and_b1 = gate_dedup_add(circuit, a1, b1, GATE_AND);
    uint32_t c0_and_xor = gate_dedup_add(circuit, c0, a1_xor_b1, GATE_AND);
    uint32_t cout_xor = gate_dedup_add(circuit, a1_and_b1, c0_and_xor, GATE_XOR);
    uint32_t cout_and = gate_dedup_add(circuit, a1_and_b1, c0_and_xor, GATE_AND);
    uint32_t cout = gate_dedup_add(circuit, cout_xor, cout_and, GATE_XOR);
    
    outputs[0] = sum0;
    outputs[1] = sum1;
    outputs[2] = cout;
    
    return cout;
}

// Build optimized 4-to-1 MUX (used in shifts and branches)
static uint32_t build_4to1_mux_optimized(riscv_circuit_t* circuit, uint32_t* inputs, uint32_t* outputs) {
    uint32_t sel0 = inputs[0], sel1 = inputs[1];
    uint32_t in0 = inputs[2], in1 = inputs[3], in2 = inputs[4], in3 = inputs[5];
    
    // Build using tree of 2-to-1 muxes with deduplication
    // Level 1: Two 2-to-1 muxes
    uint32_t not_sel0 = gate_dedup_add(circuit, sel0, CONSTANT_1_WIRE, GATE_XOR);
    
    uint32_t sel0_and_in1 = gate_dedup_add(circuit, sel0, in1, GATE_AND);
    uint32_t notsel0_and_in0 = gate_dedup_add(circuit, not_sel0, in0, GATE_AND);
    uint32_t mux0_xor = gate_dedup_add(circuit, sel0_and_in1, notsel0_and_in0, GATE_XOR);
    uint32_t mux0_and = gate_dedup_add(circuit, sel0_and_in1, notsel0_and_in0, GATE_AND);
    uint32_t mux0 = gate_dedup_add(circuit, mux0_xor, mux0_and, GATE_XOR);
    
    uint32_t sel0_and_in3 = gate_dedup_add(circuit, sel0, in3, GATE_AND);
    uint32_t notsel0_and_in2 = gate_dedup_add(circuit, not_sel0, in2, GATE_AND);
    uint32_t mux1_xor = gate_dedup_add(circuit, sel0_and_in3, notsel0_and_in2, GATE_XOR);
    uint32_t mux1_and = gate_dedup_add(circuit, sel0_and_in3, notsel0_and_in2, GATE_AND);
    uint32_t mux1 = gate_dedup_add(circuit, mux1_xor, mux1_and, GATE_XOR);
    
    // Level 2: Final 2-to-1 mux
    uint32_t not_sel1 = gate_dedup_add(circuit, sel1, CONSTANT_1_WIRE, GATE_XOR);
    uint32_t sel1_and_mux1 = gate_dedup_add(circuit, sel1, mux1, GATE_AND);
    uint32_t notsel1_and_mux0 = gate_dedup_add(circuit, not_sel1, mux0, GATE_AND);
    uint32_t result_xor = gate_dedup_add(circuit, sel1_and_mux1, notsel1_and_mux0, GATE_XOR);
    uint32_t result_and = gate_dedup_add(circuit, sel1_and_mux1, notsel1_and_mux0, GATE_AND);
    uint32_t result = gate_dedup_add(circuit, result_xor, result_and, GATE_XOR);
    
    outputs[0] = result;
    return result;
}

// Wrapper functions to use deduplication in existing code
uint32_t riscv_circuit_add_gate_dedup(riscv_circuit_t* circuit, 
                                     uint32_t left, uint32_t right, 
                                     uint32_t output, gate_type_t type) {
    // Instead of using the provided output wire, get one from deduplication
    return gate_dedup_add(circuit, left, right, type);
}

// Build deduplicated adder using the optimized patterns
void build_adder_dedup(riscv_circuit_t* circuit, uint32_t* a, uint32_t* b, 
                      uint32_t* sum, size_t bits) {
    uint32_t carry = CONSTANT_0_WIRE;
    
    // Process in 2-bit chunks when possible
    size_t full_chunks = bits / 2;
    for (size_t i = 0; i < full_chunks; i++) {
        uint32_t inputs[5] = {a[i*2], a[i*2+1], b[i*2], b[i*2+1], carry};
        uint32_t outputs[3];
        
        carry = build_2bit_adder_optimized(circuit, inputs, outputs);
        sum[i*2] = outputs[0];
        sum[i*2+1] = outputs[1];
    }
    
    // Handle remaining bit if odd number
    if (bits % 2 == 1) {
        size_t last_bit = bits - 1;
        uint32_t a_xor_b = gate_dedup_add(circuit, a[last_bit], b[last_bit], GATE_XOR);
        sum[last_bit] = gate_dedup_add(circuit, a_xor_b, carry, GATE_XOR);
    }
}

// Initialize deduplication for a compilation session
void riscv_compiler_enable_deduplication(riscv_compiler_t* compiler) {
    gate_dedup_init();
    printf("Gate deduplication enabled - will reduce duplicate subcircuits\n");
}

// Finalize and report deduplication results
void riscv_compiler_finalize_deduplication(riscv_compiler_t* compiler) {
    gate_dedup_report();
    gate_dedup_cleanup();
}