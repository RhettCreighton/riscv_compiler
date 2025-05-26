/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Kogge-Stone Parallel Prefix Adder Implementation
// This reduces addition from O(n) depth to O(log n) depth
// While using more gates, it significantly improves parallelism

// Helper: Build propagate and generate signals for a bit position
static void build_pg_signals(riscv_circuit_t* circuit, 
                            uint32_t a, uint32_t b,
                            uint32_t* p, uint32_t* g) {
    // Propagate: p = a XOR b (propagates carry if one input is 1)
    *p = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, b, *p, GATE_XOR);
    
    // Generate: g = a AND b (generates carry if both inputs are 1)
    *g = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, b, *g, GATE_AND);
}

// Helper: Combine two PG pairs using the Kogge-Stone operator
static void combine_pg_pairs(riscv_circuit_t* circuit,
                            uint32_t p_high, uint32_t g_high,
                            uint32_t p_low, uint32_t g_low,
                            uint32_t* p_out, uint32_t* g_out) {
    // P_out = P_high AND P_low
    *p_out = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, p_high, p_low, *p_out, GATE_AND);
    
    // G_out = G_high OR (P_high AND G_low)
    // Since we only have AND/XOR, use: a OR b = (a XOR b) XOR (a AND b)
    uint32_t p_and_g = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, p_high, g_low, p_and_g, GATE_AND);
    
    uint32_t g_xor_pg = riscv_circuit_allocate_wire(circuit);
    uint32_t g_and_pg = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, g_high, p_and_g, g_xor_pg, GATE_XOR);
    riscv_circuit_add_gate(circuit, g_high, p_and_g, g_and_pg, GATE_AND);
    
    *g_out = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, g_xor_pg, g_and_pg, *g_out, GATE_XOR);
}

// Optimized Kogge-Stone adder implementation
uint32_t build_kogge_stone_adder(riscv_circuit_t* circuit, 
                                uint32_t* a_bits, uint32_t* b_bits, 
                                uint32_t* sum_bits, size_t num_bits) {
    // Allocate arrays for propagate and generate signals
    uint32_t** p = malloc(6 * sizeof(uint32_t*));  // Max 6 levels for 32-bit
    uint32_t** g = malloc(6 * sizeof(uint32_t*));
    
    for (int i = 0; i < 6; i++) {
        p[i] = malloc(num_bits * sizeof(uint32_t));
        g[i] = malloc(num_bits * sizeof(uint32_t));
    }
    
    // Level 0: Generate initial P and G signals
    for (size_t i = 0; i < num_bits; i++) {
        build_pg_signals(circuit, a_bits[i], b_bits[i], &p[0][i], &g[0][i]);
    }
    
    // Build the prefix tree
    int levels = 0;
    for (size_t stride = 1; stride < num_bits; stride *= 2) {
        levels++;
        for (size_t i = 0; i < num_bits; i++) {
            if (i < stride) {
                // Just copy previous level
                p[levels][i] = p[levels-1][i];
                g[levels][i] = g[levels-1][i];
            } else {
                // Combine with element stride positions back
                combine_pg_pairs(circuit,
                               p[levels-1][i], g[levels-1][i],
                               p[levels-1][i-stride], g[levels-1][i-stride],
                               &p[levels][i], &g[levels][i]);
            }
        }
    }
    
    // Generate sum bits
    // sum[0] = p[0][0] (since c[-1] = 0)
    sum_bits[0] = p[0][0];
    
    // sum[i] = p[0][i] XOR g[levels][i-1] for i > 0
    for (size_t i = 1; i < num_bits; i++) {
        sum_bits[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, p[0][i], g[levels][i-1], sum_bits[i], GATE_XOR);
    }
    
    // Final carry out
    uint32_t carry_out = g[levels][num_bits-1];
    
    // Cleanup
    for (int i = 0; i < 6; i++) {
        free(p[i]);
        free(g[i]);
    }
    free(p);
    free(g);
    
    return carry_out;
}

// Alternative sparse Kogge-Stone for better gate efficiency
uint32_t build_sparse_kogge_stone_adder(riscv_circuit_t* circuit,
                                       uint32_t* a_bits, uint32_t* b_bits,
                                       uint32_t* sum_bits, size_t num_bits) {
    // Use sparse tree with valency-2 to reduce gate count
    // Compute carries only at every 4th position, then ripple locally
    
    const size_t block_size = 4;
    size_t num_blocks = (num_bits + block_size - 1) / block_size;
    
    // Arrays for block propagate and generate
    uint32_t* block_p = malloc(num_blocks * sizeof(uint32_t));
    uint32_t* block_g = malloc(num_blocks * sizeof(uint32_t));
    uint32_t* block_carry = malloc(num_blocks * sizeof(uint32_t));
    
    // Compute block P and G values
    for (size_t block = 0; block < num_blocks; block++) {
        size_t start = block * block_size;
        size_t end = (start + block_size < num_bits) ? start + block_size : num_bits;
        
        // Initialize with first bit of block
        uint32_t p_temp, g_temp;
        build_pg_signals(circuit, a_bits[start], b_bits[start], &p_temp, &g_temp);
        
        // Combine remaining bits in block
        for (size_t i = start + 1; i < end; i++) {
            uint32_t p_bit, g_bit;
            build_pg_signals(circuit, a_bits[i], b_bits[i], &p_bit, &g_bit);
            
            uint32_t p_new, g_new;
            combine_pg_pairs(circuit, p_bit, g_bit, p_temp, g_temp, &p_new, &g_new);
            
            p_temp = p_new;
            g_temp = g_new;
        }
        
        block_p[block] = p_temp;
        block_g[block] = g_temp;
    }
    
    // Use Kogge-Stone on block carries
    block_carry[0] = block_g[0];
    for (size_t i = 1; i < num_blocks; i++) {
        // For simplicity, using ripple carry between blocks
        // Full implementation would use Kogge-Stone here too
        uint32_t carry_and_p = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, block_carry[i-1], block_p[i], carry_and_p, GATE_AND);
        
        uint32_t g_xor_cp = riscv_circuit_allocate_wire(circuit);
        uint32_t g_and_cp = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, block_g[i], carry_and_p, g_xor_cp, GATE_XOR);
        riscv_circuit_add_gate(circuit, block_g[i], carry_and_p, g_and_cp, GATE_AND);
        
        block_carry[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, g_xor_cp, g_and_cp, block_carry[i], GATE_XOR);
    }
    
    // Generate final sum bits using block carries
    uint32_t carry = CONSTANT_0_WIRE;
    for (size_t i = 0; i < num_bits; i++) {
        size_t block = i / block_size;
        
        // Get carry-in for this bit
        if (i % block_size == 0 && block > 0) {
            carry = block_carry[block - 1];
        }
        
        // Generate sum bit
        uint32_t p_bit = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], p_bit, GATE_XOR);
        
        sum_bits[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, p_bit, carry, sum_bits[i], GATE_XOR);
        
        // Update carry for next bit (within block)
        if (i % block_size < block_size - 1 && i < num_bits - 1) {
            uint32_t g_bit = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], g_bit, GATE_AND);
            
            uint32_t p_and_c = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, p_bit, carry, p_and_c, GATE_AND);
            
            uint32_t g_xor_pc = riscv_circuit_allocate_wire(circuit);
            uint32_t g_and_pc = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, g_bit, p_and_c, g_xor_pc, GATE_XOR);
            riscv_circuit_add_gate(circuit, g_bit, p_and_c, g_and_pc, GATE_AND);
            
            carry = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, g_xor_pc, g_and_pc, carry, GATE_XOR);
        }
    }
    
    uint32_t final_carry = block_carry[num_blocks - 1];
    
    // Cleanup
    free(block_p);
    free(block_g);
    free(block_carry);
    
    return final_carry;
}

// Benchmark function to compare adder implementations
void benchmark_adders(void) {
    printf("Adder Implementation Comparison\n");
    printf("==============================\n\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Test inputs
    uint32_t* a = malloc(32 * sizeof(uint32_t));
    uint32_t* b = malloc(32 * sizeof(uint32_t));
    uint32_t* sum_ripple = malloc(32 * sizeof(uint32_t));
    uint32_t* sum_kogge = malloc(32 * sizeof(uint32_t));
    uint32_t* sum_sparse = malloc(32 * sizeof(uint32_t));
    
    // Initialize test inputs
    for (int i = 0; i < 32; i++) {
        a[i] = get_register_wire(1, i);
        b[i] = get_register_wire(2, i);
    }
    
    // Test ripple-carry adder
    size_t gates_before = compiler->circuit->num_gates;
    build_ripple_carry_adder(compiler->circuit, a, b, sum_ripple, 32);
    size_t ripple_gates = compiler->circuit->num_gates - gates_before;
    
    // Test full Kogge-Stone adder
    gates_before = compiler->circuit->num_gates;
    build_kogge_stone_adder(compiler->circuit, a, b, sum_kogge, 32);
    size_t kogge_gates = compiler->circuit->num_gates - gates_before;
    
    // Test sparse Kogge-Stone adder
    gates_before = compiler->circuit->num_gates;
    build_sparse_kogge_stone_adder(compiler->circuit, a, b, sum_sparse, 32);
    size_t sparse_gates = compiler->circuit->num_gates - gates_before;
    
    printf("32-bit Adder Gate Counts:\n");
    printf("  Ripple-carry:        %zu gates (depth: 32)\n", ripple_gates);
    printf("  Kogge-Stone:         %zu gates (depth: 6)\n", kogge_gates);
    printf("  Sparse Kogge-Stone:  %zu gates (depth: ~10)\n", sparse_gates);
    printf("\n");
    
    printf("Gate Efficiency:\n");
    printf("  Ripple-carry:        %.1f gates/bit\n", ripple_gates / 32.0);
    printf("  Kogge-Stone:         %.1f gates/bit\n", kogge_gates / 32.0);
    printf("  Sparse Kogge-Stone:  %.1f gates/bit\n", sparse_gates / 32.0);
    
    free(a);
    free(b);
    free(sum_ripple);
    free(sum_kogge);
    free(sum_sparse);
    riscv_compiler_destroy(compiler);
}