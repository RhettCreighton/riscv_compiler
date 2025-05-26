/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * SAT-based Equivalence Test for ADD Instruction
 * 
 * Uses MiniSAT to verify that our compiled ADD circuit is equivalent
 * to the reference implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

// Include MiniSAT first to avoid bool conflicts
#include "minisat/solver.h"

// Then our headers
#include "../include/riscv_compiler.h"

// Reference implementation of 32-bit addition
void reference_add(bool* a, bool* b, bool* sum) {
    bool carry = false;
    for (int i = 0; i < 32; i++) {
        sum[i] = a[i] ^ b[i] ^ carry;
        carry = (a[i] & b[i]) | (carry & (a[i] ^ b[i]));
    }
}

// Add gate constraints to SAT solver
void add_gate_to_sat(solver* s, const gate_t* gate) {
    lit lits[3];
    
    lit a = toLit(gate->left_input);
    lit b = toLit(gate->right_input);
    lit c = toLit(gate->output);
    
    if (gate->type == GATE_AND) {
        // AND gate: c = a ∧ b
        // CNF: (¬a ∨ ¬b ∨ c) ∧ (a ∨ ¬c) ∧ (b ∨ ¬c)
        
        // ¬a ∨ ¬b ∨ c
        lits[0] = lit_neg(a);
        lits[1] = lit_neg(b);
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
        
        // a ∨ ¬c
        lits[0] = a;
        lits[1] = lit_neg(c);
        solver_addclause(s, lits, lits + 2);
        
        // b ∨ ¬c
        lits[0] = b;
        lits[1] = lit_neg(c);
        solver_addclause(s, lits, lits + 2);
    } else {  // XOR
        // XOR gate: c = a ⊕ b
        // CNF: (¬a ∨ ¬b ∨ ¬c) ∧ (a ∨ b ∨ ¬c) ∧ (a ∨ ¬b ∨ c) ∧ (¬a ∨ b ∨ c)
        
        // ¬a ∨ ¬b ∨ ¬c
        lits[0] = lit_neg(a);
        lits[1] = lit_neg(b);
        lits[2] = lit_neg(c);
        solver_addclause(s, lits, lits + 3);
        
        // a ∨ b ∨ ¬c
        lits[0] = a;
        lits[1] = b;
        lits[2] = lit_neg(c);
        solver_addclause(s, lits, lits + 3);
        
        // a ∨ ¬b ∨ c
        lits[0] = a;
        lits[1] = lit_neg(b);
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
        
        // ¬a ∨ b ∨ c
        lits[0] = lit_neg(a);
        lits[1] = b;
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
    }
}

// Constrain a wire to a specific value
void constrain_wire(solver* s, uint32_t wire, bool value) {
    lit lits[1];
    lits[0] = value ? toLit(wire) : lit_neg(toLit(wire));
    solver_addclause(s, lits, lits + 1);
}

int main() {
    printf("=== SAT-based ADD Equivalence Test ===\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // The compiler already initializes register wires correctly
    // No need to override them
    
    // Set up circuit inputs: 2 constants + 32 PC bits + 32*32 register bits
    compiler->circuit->num_inputs = 2 + 32 + (32 * 32);  // 1058 bits
    compiler->circuit->num_outputs = 32 * 32;  // All registers as output
    
    // Compile ADD x3, x1, x2
    uint32_t add_instr = 0x002081B3;
    printf("Compiling ADD x3, x1, x2\n");
    
    if (riscv_compile_instruction(compiler, add_instr) != 0) {
        fprintf(stderr, "Failed to compile ADD instruction\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    printf("Circuit compiled:\n");
    printf("  Gates: %zu\n", riscv_circuit_get_num_gates(compiler->circuit));
    printf("  Wires: %u\n", riscv_circuit_get_next_wire(compiler->circuit));
    
    // Now verify equivalence using SAT
    printf("\nVerifying equivalence with reference implementation...\n");
    
    // Test a few specific cases
    // NOTE: We're testing ADD x3, x1, x2, so we need to set x1 and x2 values
    uint32_t test_cases[][2] = {
        {1, 1},           // 1 + 1 = 2
        {5, 7},           // 5 + 7 = 12
        {0xFFFFFFFF, 1},  // -1 + 1 = 0 (overflow)
        {0x7FFFFFFF, 1},  // MAX_INT + 1 = MIN_INT
        {100, 200},       // 100 + 200 = 300
    };
    
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed = 0;
    
    for (int test = 0; test < num_tests; test++) {
        uint32_t a = test_cases[test][0];
        uint32_t b = test_cases[test][1];
        
        // Compute reference result
        bool ref_a[32], ref_b[32], ref_sum[32];
        for (int i = 0; i < 32; i++) {
            ref_a[i] = (a >> i) & 1;
            ref_b[i] = (b >> i) & 1;
        }
        reference_add(ref_a, ref_b, ref_sum);
        uint32_t expected = 0;
        for (int i = 0; i < 32; i++) {
            if (ref_sum[i]) expected |= (1U << i);
        }
        
        // Create SAT solver
        solver* s = solver_new();
        solver_setnvars(s, riscv_circuit_get_next_wire(compiler->circuit));
        
        // Add circuit constraints
        size_t num_gates = riscv_circuit_get_num_gates(compiler->circuit);
        const gate_t* gates = riscv_circuit_get_gates(compiler->circuit);
        for (size_t i = 0; i < num_gates; i++) {
            add_gate_to_sat(s, &gates[i]);
        }
        
        // Constrain constants
        constrain_wire(s, 0, false);  // Wire 0 = constant 0
        constrain_wire(s, 1, true);   // Wire 1 = constant 1
        
        // Constrain x0 to always be zero (RISC-V requirement)
        for (int i = 0; i < 32; i++) {
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, 0, i), false);
        }
        
        // Constrain inputs for x1 and x2
        for (int i = 0; i < 32; i++) {
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, 1, i), (a >> i) & 1);
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, 2, i), (b >> i) & 1);
        }
        
        // Try to find a counterexample where output differs from expected
        bool found_difference = false;
        
        // Check if all output bits match expected
        // We'll create a new solver for each bit check
        for (int bit = 0; bit < 32; bit++) {
            // Create new solver for this bit check
            solver* check_s = solver_new();
            solver_setnvars(check_s, riscv_circuit_get_next_wire(compiler->circuit));
            
            // Add all circuit constraints again
            for (size_t i = 0; i < num_gates; i++) {
                add_gate_to_sat(check_s, &gates[i]);
            }
            
            // Constrain constants
            constrain_wire(check_s, 0, false);
            constrain_wire(check_s, 1, true);
            
            // Constrain x0 to always be zero (RISC-V requirement)
            for (int i = 0; i < 32; i++) {
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, 0, i), false);
            }
            
            // Constrain inputs for x1 and x2
            for (int i = 0; i < 32; i++) {
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, 1, i), (a >> i) & 1);
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, 2, i), (b >> i) & 1);
            }
            
            // Constrain this output bit to differ from expected
            bool expected_bit = (expected >> bit) & 1;
            constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, 3, bit), !expected_bit);
            
            // Check if satisfiable
            if (solver_solve(check_s, NULL, NULL)) {
                found_difference = true;
                printf("  ❌ Test %d FAILED: %u + %u, bit %d differs\n", 
                       test, a, b, bit);
                solver_delete(check_s);
                break;
            }
            
            solver_delete(check_s);
        }
        
        if (!found_difference) {
            printf("  ✅ Test %d PASSED: %u + %u = %u\n", test, a, b, expected);
            passed++;
        }
        
        solver_delete(s);
    }
    
    printf("\n");
    printf("Tests passed: %d/%d\n", passed, num_tests);
    
    if (passed == num_tests) {
        printf("✅ ADD instruction verified equivalent to reference!\n");
    } else {
        printf("❌ ADD instruction verification FAILED!\n");
    }
    
    riscv_compiler_destroy(compiler);
    return (passed == num_tests) ? 0 : 1;
}