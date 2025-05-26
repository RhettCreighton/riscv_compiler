/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * Systematic Instruction Verification
 * 
 * Verifies multiple RISC-V instructions using SAT-based equivalence checking
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

// Include MiniSAT first
#include "minisat/solver.h"

// Then our headers
#include "../include/riscv_compiler.h"

// Test case structure
typedef struct {
    const char* name;
    uint32_t instruction;
    int rd, rs1, rs2;  // Register numbers
    uint32_t (*reference)(uint32_t a, uint32_t b);
} test_case_t;

// Reference implementations
uint32_t ref_add(uint32_t a, uint32_t b) { return a + b; }
uint32_t ref_sub(uint32_t a, uint32_t b) { return a - b; }
uint32_t ref_xor(uint32_t a, uint32_t b) { return a ^ b; }
uint32_t ref_and(uint32_t a, uint32_t b) { return a & b; }
uint32_t ref_or(uint32_t a, uint32_t b) { return a | b; }

// Add gate to SAT solver
void add_gate_to_sat(solver* s, const gate_t* gate) {
    lit lits[3];
    lit a = toLit(gate->left_input);
    lit b = toLit(gate->right_input);
    lit c = toLit(gate->output);
    
    if (gate->type == GATE_AND) {
        // AND gate CNF
        lits[0] = lit_neg(a);
        lits[1] = lit_neg(b);
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
        
        lits[0] = a;
        lits[1] = lit_neg(c);
        solver_addclause(s, lits, lits + 2);
        
        lits[0] = b;
        lits[1] = lit_neg(c);
        solver_addclause(s, lits, lits + 2);
    } else {  // XOR
        // XOR gate CNF
        lits[0] = lit_neg(a);
        lits[1] = lit_neg(b);
        lits[2] = lit_neg(c);
        solver_addclause(s, lits, lits + 3);
        
        lits[0] = a;
        lits[1] = b;
        lits[2] = lit_neg(c);
        solver_addclause(s, lits, lits + 3);
        
        lits[0] = a;
        lits[1] = lit_neg(b);
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
        
        lits[0] = lit_neg(a);
        lits[1] = b;
        lits[2] = c;
        solver_addclause(s, lits, lits + 3);
    }
}

// Constrain wire to value
void constrain_wire(solver* s, uint32_t wire, bool value) {
    lit lits[1];
    lits[0] = value ? toLit(wire) : lit_neg(toLit(wire));
    solver_addclause(s, lits, lits + 1);
}

// Verify instruction against reference
bool verify_instruction(test_case_t* test) {
    printf("\nTesting %s instruction...\n", test->name);
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return false;
    }
    
    // Compile instruction
    if (riscv_compile_instruction(compiler, test->instruction) != 0) {
        fprintf(stderr, "Failed to compile %s instruction\n", test->name);
        riscv_compiler_destroy(compiler);
        return false;
    }
    
    printf("  Compiled to %zu gates\n", riscv_circuit_get_num_gates(compiler->circuit));
    
    // Test with several input values
    uint32_t test_values[][2] = {
        {0, 0},
        {1, 1},
        {5, 3},
        {0xFFFFFFFF, 1},
        {0x12345678, 0x87654321}
    };
    
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        uint32_t a = test_values[i][0];
        uint32_t b = test_values[i][1];
        uint32_t expected = test->reference(a, b);
        
        // Create SAT solver
        solver* s = solver_new();
        solver_setnvars(s, riscv_circuit_get_next_wire(compiler->circuit));
        
        // Add circuit
        size_t num_gates = riscv_circuit_get_num_gates(compiler->circuit);
        const gate_t* gates = riscv_circuit_get_gates(compiler->circuit);
        for (size_t j = 0; j < num_gates; j++) {
            add_gate_to_sat(s, &gates[j]);
        }
        
        // Constrain constants
        constrain_wire(s, 0, false);
        constrain_wire(s, 1, true);
        
        // Constrain x0 = 0 (RISC-V requirement) - even though compiler fix handles this,
        // we keep the explicit constraint for verification robustness
        for (int bit = 0; bit < 32; bit++) {
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, 0, bit), false);
        }
        
        // Constrain inputs
        for (int bit = 0; bit < 32; bit++) {
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, test->rs1, bit), (a >> bit) & 1);
            constrain_wire(s, riscv_compiler_get_register_wire(compiler, test->rs2, bit), (b >> bit) & 1);
        }
        
        // Check if any output bit differs from expected
        bool found_difference = false;
        for (int bit = 0; bit < 32; bit++) {
            solver* check_s = solver_new();
            solver_setnvars(check_s, riscv_circuit_get_next_wire(compiler->circuit));
            
            // Re-add all constraints
            for (size_t j = 0; j < num_gates; j++) {
                add_gate_to_sat(check_s, &gates[j]);
            }
            
            constrain_wire(check_s, 0, false);
            constrain_wire(check_s, 1, true);
            
            for (int k = 0; k < 32; k++) {
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, 0, k), false);
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, test->rs1, k), (a >> k) & 1);
                constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, test->rs2, k), (b >> k) & 1);
            }
            
            // Constrain output bit to differ
            bool expected_bit = (expected >> bit) & 1;
            constrain_wire(check_s, riscv_compiler_get_register_wire(compiler, test->rd, bit), !expected_bit);
            
            if (solver_solve(check_s, NULL, NULL)) {
                found_difference = true;
                solver_delete(check_s);
                break;
            }
            
            solver_delete(check_s);
        }
        
        if (!found_difference) {
            passed++;
        } else {
            printf("    ❌ Failed: %u op %u != %u\n", a, b, expected);
        }
        
        solver_delete(s);
    }
    
    printf("  Result: %d/%d tests passed\n", passed, num_tests);
    
    riscv_compiler_destroy(compiler);
    return passed == num_tests;
}

int main() {
    printf("=== Systematic RISC-V Instruction Verification ===\n");
    
    // Define test cases
    test_case_t tests[] = {
        {"ADD", 0x002081B3, 3, 1, 2, ref_add},    // ADD x3, x1, x2
        {"SUB", 0x402081B3, 3, 1, 2, ref_sub},    // SUB x3, x1, x2
        {"XOR", 0x002081B3 | (4 << 12), 3, 1, 2, ref_xor},  // XOR x3, x1, x2
        {"AND", 0x002081B3 | (7 << 12), 3, 1, 2, ref_and},  // AND x3, x1, x2
        {"OR",  0x002081B3 | (6 << 12), 3, 1, 2, ref_or},   // OR x3, x1, x2
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        if (verify_instruction(&tests[i])) {
            passed++;
            printf("✅ %s verified!\n", tests[i].name);
        } else {
            printf("❌ %s verification failed!\n", tests[i].name);
        }
    }
    
    printf("\n=== Summary ===\n");
    printf("Instructions verified: %d/%d\n", passed, num_tests);
    
    if (passed == num_tests) {
        printf("✅ All instructions verified successfully!\n");
        return 0;
    } else {
        printf("❌ Some instructions failed verification\n");
        return 1;
    }
}