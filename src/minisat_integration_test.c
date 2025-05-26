/*
 * MiniSAT Integration Test
 * 
 * Simple test to verify MiniSAT C version works correctly
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// MiniSAT header
#include "minisat/solver.h"

// Test basic SAT solving
void test_simple_sat() {
    printf("=== Testing MiniSAT Basic Functionality ===\n");
    
    // Create solver
    solver* s = solver_new();
    
    // Create problem: (x1 ∨ x2) ∧ (¬x1 ∨ x3) ∧ (¬x2 ∨ ¬x3)
    solver_setnvars(s, 3);
    
    lit clause1[2];
    lit clause2[2];
    lit clause3[2];
    
    // (x1 ∨ x2)
    clause1[0] = toLit(1);  // x1
    clause1[1] = toLit(2);  // x2
    solver_addclause(s, clause1, clause1 + 2);
    
    // (¬x1 ∨ x3)
    clause2[0] = lit_neg(toLit(1));  // ¬x1
    clause2[1] = toLit(3);           // x3
    solver_addclause(s, clause2, clause2 + 2);
    
    // (¬x2 ∨ ¬x3)
    clause3[0] = lit_neg(toLit(2));  // ¬x2
    clause3[1] = lit_neg(toLit(3));  // ¬x3
    solver_addclause(s, clause3, clause3 + 2);
    
    // Solve
    bool result = solver_solve(s, NULL, NULL);
    
    if (result) {
        printf("SAT! Found satisfying assignment\n");
        
        // MiniSAT doesn't have a direct way to get assignments in this C version
        // In practice, would need to extend the API
        printf("Solver statistics:\n");
        printf("  Variables: %d\n", solver_nvars(s));
        printf("  Clauses: %d\n", solver_nclauses(s));
        printf("  Conflicts: %d\n", solver_nconflicts(s));
    } else {
        printf("UNSAT!\n");
    }
    
    solver_delete(s);
}

// Test a simple circuit equivalence
void test_circuit_sat() {
    printf("\n=== Testing Circuit Encoding ===\n");
    
    solver* s = solver_new();
    
    // Simple circuit: c = a AND b
    // Variables: a=1, b=2, c=3
    solver_setnvars(s, 3);
    
    lit lits[3];
    
    // AND gate constraints:
    // (¬a ∨ ¬b ∨ c)
    lits[0] = lit_neg(toLit(1));  // ¬a
    lits[1] = lit_neg(toLit(2));  // ¬b
    lits[2] = toLit(3);           // c
    solver_addclause(s, lits, lits + 3);
    
    // (a ∨ ¬c)
    lits[0] = toLit(1);           // a
    lits[1] = lit_neg(toLit(3));  // ¬c
    solver_addclause(s, lits, lits + 2);
    
    // (b ∨ ¬c)
    lits[0] = toLit(2);           // b
    lits[1] = lit_neg(toLit(3));  // ¬c
    solver_addclause(s, lits, lits + 2);
    
    // Test: Can we make c=1?
    lit assumption = toLit(3);  // c must be true
    
    if (solver_solve(s, &assumption, &assumption + 1)) {
        printf("SAT: Can make output c=1\n");
        printf("This means both inputs must be 1 (AND gate)\n");
    } else {
        printf("UNSAT: Cannot make c=1\n");
    }
    
    // Test: Can we make c=0 with a=1?
    lit assumptions[2];
    assumptions[0] = toLit(1);           // a=1
    assumptions[1] = lit_neg(toLit(3));  // c=0
    
    if (solver_solve(s, assumptions, assumptions + 2)) {
        printf("\nSAT: Can make c=0 with a=1\n");
        printf("This means b must be 0\n");
    } else {
        printf("\nUNSAT: Cannot make c=0 when a=1\n");
    }
    
    solver_delete(s);
}

// Test XOR gate encoding
void test_xor_sat() {
    printf("\n=== Testing XOR Gate ===\n");
    
    solver* s = solver_new();
    solver_setnvars(s, 3);
    
    lit lits[3];
    
    // XOR gate: c = a ⊕ b
    // (¬a ∨ ¬b ∨ ¬c)
    lits[0] = lit_neg(toLit(1));
    lits[1] = lit_neg(toLit(2));
    lits[2] = lit_neg(toLit(3));
    solver_addclause(s, lits, lits + 3);
    
    // (a ∨ b ∨ ¬c)
    lits[0] = toLit(1);
    lits[1] = toLit(2);
    lits[2] = lit_neg(toLit(3));
    solver_addclause(s, lits, lits + 3);
    
    // (a ∨ ¬b ∨ c)
    lits[0] = toLit(1);
    lits[1] = lit_neg(toLit(2));
    lits[2] = toLit(3);
    solver_addclause(s, lits, lits + 3);
    
    // (¬a ∨ b ∨ c)
    lits[0] = lit_neg(toLit(1));
    lits[1] = toLit(2);
    lits[2] = toLit(3);
    solver_addclause(s, lits, lits + 3);
    
    // Test all input combinations
    printf("Testing XOR truth table:\n");
    
    for (int a_val = 0; a_val <= 1; a_val++) {
        for (int b_val = 0; b_val <= 1; b_val++) {
            lit assumptions[2];
            assumptions[0] = a_val ? toLit(1) : lit_neg(toLit(1));
            assumptions[1] = b_val ? toLit(2) : lit_neg(toLit(2));
            
            if (solver_solve(s, assumptions, assumptions + 2)) {
                printf("  a=%d, b=%d => SAT (c=%d expected)\n", 
                       a_val, b_val, a_val ^ b_val);
            } else {
                printf("  a=%d, b=%d => UNSAT\n", a_val, b_val);
            }
        }
    }
    
    solver_delete(s);
}

int main() {
    printf("MiniSAT C Integration Test\n");
    printf("==========================\n\n");
    
    test_simple_sat();
    test_circuit_sat();
    test_xor_sat();
    
    printf("\n✓ All MiniSAT tests completed!\n");
    
    return 0;
}