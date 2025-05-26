/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * Test Verification API
 * 
 * Simple test to verify that the new verification API functions work correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../include/riscv_compiler.h"

int main() {
    printf("=== Verification API Test ===\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Initialize circuit inputs/outputs
    compiler->circuit->num_inputs = 66;   // 2 constants + 32 PC + 32 reg bits
    compiler->circuit->num_outputs = 32;  // Result register
    
    // Compile a simple ADD instruction
    // ADD x3, x0, x0 (result = 0 + 0 = 0)
    uint32_t add_instr = 0x000001B3;  // ADD x3, x0, x0
    printf("Compiling ADD x3, x0, x0 (0x%08X)\n", add_instr);
    
    if (riscv_compile_instruction(compiler, add_instr) != 0) {
        fprintf(stderr, "Failed to compile ADD instruction\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    // Test verification API functions
    printf("\nTesting verification API:\n");
    
    // Test riscv_circuit_get_num_gates
    size_t num_gates = riscv_circuit_get_num_gates(compiler->circuit);
    printf("  Number of gates: %zu\n", num_gates);
    assert(num_gates > 0);
    
    // Test riscv_circuit_get_num_inputs/outputs
    size_t num_inputs = riscv_circuit_get_num_inputs(compiler->circuit);
    size_t num_outputs = riscv_circuit_get_num_outputs(compiler->circuit);
    printf("  Inputs: %zu, Outputs: %zu\n", num_inputs, num_outputs);
    assert(num_inputs == 66);
    assert(num_outputs == 32);
    
    // Test riscv_circuit_get_next_wire
    uint32_t next_wire = riscv_circuit_get_next_wire(compiler->circuit);
    printf("  Next wire ID: %u\n", next_wire);
    assert(next_wire > 66);  // Should be after input wires
    
    // Test riscv_circuit_get_gate
    if (num_gates > 0) {
        const gate_t* first_gate = riscv_circuit_get_gate(compiler->circuit, 0);
        assert(first_gate != NULL);
        printf("  First gate: %s(%u, %u) -> %u\n",
               first_gate->type == GATE_AND ? "AND" : "XOR",
               first_gate->left_input,
               first_gate->right_input,
               first_gate->output);
        
        // Test out of bounds
        const gate_t* invalid_gate = riscv_circuit_get_gate(compiler->circuit, num_gates);
        assert(invalid_gate == NULL);
    }
    
    // Test riscv_circuit_get_gates
    const gate_t* all_gates = riscv_circuit_get_gates(compiler->circuit);
    assert(all_gates != NULL);
    printf("  Successfully retrieved gate array pointer\n");
    
    // Print first few gates
    printf("\nFirst 5 gates:\n");
    for (size_t i = 0; i < 5 && i < num_gates; i++) {
        printf("  Gate %zu: %s(%u, %u) -> %u\n",
               i,
               all_gates[i].type == GATE_AND ? "AND" : "XOR",
               all_gates[i].left_input,
               all_gates[i].right_input,
               all_gates[i].output);
    }
    
    printf("\n✅ All verification API tests passed!\n");
    
    riscv_compiler_destroy(compiler);
    return 0;
}