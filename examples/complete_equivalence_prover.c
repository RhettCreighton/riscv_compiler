/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * complete_equivalence_prover.c - Complete formal equivalence proof for circuits
 * 
 * This proves that two circuits are 100% equivalent for ALL possible inputs
 * by encoding the equivalence check as a SAT problem and proving UNSAT.
 * 
 * Key insight: If we can't find ANY input where outputs differ, they're equivalent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "../src/minisat/solver.h"
#include "../include/riscv_compiler.h"

// Structure to track circuit I/O for equivalence checking
typedef struct {
    uint32_t* input_wires;
    uint32_t* output_wires;
    size_t num_inputs;
    size_t num_outputs;
} circuit_io_t;

// Add gate constraints to SAT solver
static void add_gate_to_sat(solver* s, const gate_t* gate, uint32_t wire_offset) {
    lit lits[3];
    
    // Offset all wires to avoid conflicts between circuits
    lit a = toLit(gate->left_input + wire_offset);
    lit b = toLit(gate->right_input + wire_offset);
    lit c = toLit(gate->output + wire_offset);
    
    if (gate->type == GATE_AND) {
        // c = a ∧ b
        // CNF: (¬a ∨ ¬b ∨ c) ∧ (a ∨ ¬c) ∧ (b ∨ ¬c)
        
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
        
    } else if (gate->type == GATE_XOR) {
        // c = a ⊕ b
        // CNF: (¬a ∨ ¬b ∨ ¬c) ∧ (a ∨ b ∨ ¬c) ∧ (a ∨ ¬b ∨ c) ∧ (¬a ∨ b ∨ c)
        
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

// Add constant wire constraints
static void add_constants_to_sat(solver* s, uint32_t offset) {
    lit lits[1];
    
    // CONSTANT_0_WIRE must be false
    lits[0] = lit_neg(toLit(CONSTANT_0_WIRE + offset));
    solver_addclause(s, lits, lits + 1);
    
    // CONSTANT_1_WIRE must be true
    lits[0] = toLit(CONSTANT_1_WIRE + offset);
    solver_addclause(s, lits, lits + 1);
}

// Add equality constraint between two wires
static void add_wire_equality(solver* s, uint32_t wire1, uint32_t wire2) {
    lit lits[2];
    
    // wire1 → wire2
    lits[0] = lit_neg(toLit(wire1));
    lits[1] = toLit(wire2);
    solver_addclause(s, lits, lits + 2);
    
    // wire2 → wire1
    lits[0] = toLit(wire1);
    lits[1] = lit_neg(toLit(wire2));
    solver_addclause(s, lits, lits + 2);
}

// Add inequality constraint between two wires (at least one must differ)
static void add_wire_inequality(solver* s, uint32_t wire1, uint32_t wire2) {
    lit lits[4];
    
    // (wire1 ∧ ¬wire2) ∨ (¬wire1 ∧ wire2)
    // CNF: (wire1 ∨ wire2) ∧ (¬wire1 ∨ ¬wire2)
    
    lits[0] = toLit(wire1);
    lits[1] = toLit(wire2);
    solver_addclause(s, lits, lits + 2);
    
    lits[0] = lit_neg(toLit(wire1));
    lits[1] = lit_neg(toLit(wire2));
    solver_addclause(s, lits, lits + 2);
}

// Main equivalence checking function
int prove_circuit_equivalence(
    riscv_circuit_t* circuit1, circuit_io_t* io1,
    riscv_circuit_t* circuit2, circuit_io_t* io2
) {
    printf("=== Complete Circuit Equivalence Proof ===\n");
    printf("Circuit 1: %zu gates, %zu inputs, %zu outputs\n", 
           circuit1->num_gates, io1->num_inputs, io1->num_outputs);
    printf("Circuit 2: %zu gates, %zu inputs, %zu outputs\n", 
           circuit2->num_gates, io2->num_inputs, io2->num_outputs);
    
    // Circuits must have same I/O signature
    if (io1->num_inputs != io2->num_inputs || 
        io1->num_outputs != io2->num_outputs) {
        printf("ERROR: Circuits have different I/O signatures!\n");
        return 0;
    }
    
    // Create SAT solver
    solver* s = solver_new();
    
    // Calculate wire offsets to avoid conflicts
    uint32_t circuit1_offset = 0;
    uint32_t circuit2_offset = circuit1->max_wire_id + 1000;
    
    // Set number of variables
    size_t max_var = circuit2_offset + circuit2->max_wire_id + 1000;
    solver_setnvars(s, max_var);
    
    printf("\nBuilding SAT instance...\n");
    
    // Add constant constraints for both circuits
    add_constants_to_sat(s, circuit1_offset);
    add_constants_to_sat(s, circuit2_offset);
    
    // Add circuit 1 gates
    for (size_t i = 0; i < circuit1->num_gates; i++) {
        add_gate_to_sat(s, &circuit1->gates[i], circuit1_offset);
    }
    
    // Add circuit 2 gates
    for (size_t i = 0; i < circuit2->num_gates; i++) {
        add_gate_to_sat(s, &circuit2->gates[i], circuit2_offset);
    }
    
    // Add input equality constraints
    printf("Adding %zu input equality constraints...\n", io1->num_inputs);
    for (size_t i = 0; i < io1->num_inputs; i++) {
        add_wire_equality(s, 
            io1->input_wires[i] + circuit1_offset,
            io2->input_wires[i] + circuit2_offset);
    }
    
    // Create output difference tracker
    // We'll use auxiliary variables to track if ANY output differs
    uint32_t* diff_vars = malloc(io1->num_outputs * sizeof(uint32_t));
    uint32_t any_diff_var = max_var - 1;  // Variable tracking if any output differs
    
    printf("Adding %zu output comparison constraints...\n", io1->num_outputs);
    
    // For each output bit, create a variable that's true if outputs differ
    for (size_t i = 0; i < io1->num_outputs; i++) {
        diff_vars[i] = max_var - 1000 - i;
        
        uint32_t out1 = io1->output_wires[i] + circuit1_offset;
        uint32_t out2 = io2->output_wires[i] + circuit2_offset;
        
        // diff_vars[i] ↔ (out1 ≠ out2)
        // This is: diff_vars[i] ↔ (out1 ⊕ out2)
        
        lit lits[3];
        lit diff = toLit(diff_vars[i]);
        lit o1 = toLit(out1);
        lit o2 = toLit(out2);
        
        // If outputs are same, diff is false
        // (o1 ∧ o2) → ¬diff
        lits[0] = lit_neg(o1);
        lits[1] = lit_neg(o2);
        lits[2] = lit_neg(diff);
        solver_addclause(s, lits, lits + 3);
        
        // (¬o1 ∧ ¬o2) → ¬diff
        lits[0] = o1;
        lits[1] = o2;
        lits[2] = lit_neg(diff);
        solver_addclause(s, lits, lits + 3);
        
        // If outputs differ, diff is true
        // (o1 ∧ ¬o2) → diff
        lits[0] = lit_neg(o1);
        lits[1] = o2;
        lits[2] = diff;
        solver_addclause(s, lits, lits + 3);
        
        // (¬o1 ∧ o2) → diff
        lits[0] = o1;
        lits[1] = lit_neg(o2);
        lits[2] = diff;
        solver_addclause(s, lits, lits + 3);
    }
    
    // any_diff_var is true if ANY output differs
    // This is: any_diff_var ↔ (diff[0] ∨ diff[1] ∨ ... ∨ diff[n-1])
    
    // If any diff[i] is true, any_diff_var must be true
    for (size_t i = 0; i < io1->num_outputs; i++) {
        lit lits[2];
        lits[0] = lit_neg(toLit(diff_vars[i]));
        lits[1] = toLit(any_diff_var);
        solver_addclause(s, lits, lits + 2);
    }
    
    // If any_diff_var is true, at least one diff[i] must be true
    lit* big_clause = malloc((io1->num_outputs + 1) * sizeof(lit));
    big_clause[0] = lit_neg(toLit(any_diff_var));
    for (size_t i = 0; i < io1->num_outputs; i++) {
        big_clause[i + 1] = toLit(diff_vars[i]);
    }
    solver_addclause(s, big_clause, big_clause + io1->num_outputs + 1);
    
    // Finally, assert that some output MUST differ
    lit final_lit = toLit(any_diff_var);
    solver_addclause(s, &final_lit, &final_lit + 1);
    
    // Solve
    printf("\nSolving SAT instance...\n");
    printf("Variables: %d\n", solver_nvars(s));
    printf("Clauses: %d\n", solver_nclauses(s));
    
    bool sat_result = solver_solve(s, NULL, NULL);
    
    int equivalent = !sat_result;  // UNSAT means equivalent
    
    if (equivalent) {
        printf("\n✅ PROVEN: Circuits are 100%% EQUIVALENT!\n");
        printf("No input exists where outputs differ.\n");
        printf("∀ input ∈ {0,1}^%zu : Circuit1(input) ≡ Circuit2(input)\n", 
               io1->num_inputs);
    } else {
        printf("\n❌ DISPROVEN: Circuits are NOT equivalent!\n");
        printf("Found input where outputs differ.\n");
        // In a real implementation, we'd extract and print the counterexample
    }
    
    // Cleanup
    free(diff_vars);
    free(big_clause);
    solver_delete(s);
    
    return equivalent;
}

// Example: Prove two adder implementations are equivalent
void example_adder_equivalence() {
    printf("\n=== Example: 4-bit Adder Equivalence ===\n");
    
    // Circuit 1: Ripple carry adder
    riscv_circuit_t* circuit1 = riscv_circuit_create(8, 4);  // 8 inputs (4+4), 4 outputs
    
    uint32_t a1[4], b1[4], sum1[4];
    for (int i = 0; i < 4; i++) {
        a1[i] = riscv_circuit_allocate_wire(circuit1);
        b1[i] = riscv_circuit_allocate_wire(circuit1);
    }
    
    // Build ripple carry adder
    uint32_t carry = CONSTANT_0_WIRE;
    for (int i = 0; i < 4; i++) {
        sum1[i] = riscv_circuit_allocate_wire(circuit1);
        uint32_t ab_xor = riscv_circuit_allocate_wire(circuit1);
        
        riscv_circuit_add_gate(circuit1, a1[i], b1[i], ab_xor, GATE_XOR);
        riscv_circuit_add_gate(circuit1, ab_xor, carry, sum1[i], GATE_XOR);
        
        uint32_t ab_and = riscv_circuit_allocate_wire(circuit1);
        uint32_t carry_and = riscv_circuit_allocate_wire(circuit1);
        
        riscv_circuit_add_gate(circuit1, a1[i], b1[i], ab_and, GATE_AND);
        riscv_circuit_add_gate(circuit1, carry, ab_xor, carry_and, GATE_AND);
        
        uint32_t new_carry = riscv_circuit_allocate_wire(circuit1);
        riscv_circuit_add_gate(circuit1, ab_and, carry_and, new_carry, GATE_XOR);
        carry = new_carry;
    }
    
    circuit_io_t io1 = {
        .input_wires = malloc(8 * sizeof(uint32_t)),
        .output_wires = sum1,
        .num_inputs = 8,
        .num_outputs = 4
    };
    for (int i = 0; i < 4; i++) {
        io1.input_wires[i] = a1[i];
        io1.input_wires[4 + i] = b1[i];
    }
    
    // Circuit 2: Another ripple carry implementation (slightly different)
    riscv_circuit_t* circuit2 = riscv_circuit_create(8, 4);
    
    uint32_t a2[4], b2[4], sum2[4];
    for (int i = 0; i < 4; i++) {
        a2[i] = riscv_circuit_allocate_wire(circuit2);
        b2[i] = riscv_circuit_allocate_wire(circuit2);
    }
    
    // Build with different gate order (but same function)
    carry = CONSTANT_0_WIRE;
    for (int i = 0; i < 4; i++) {
        sum2[i] = riscv_circuit_allocate_wire(circuit2);
        
        // Same logic, different intermediate wire allocation
        uint32_t temp1 = riscv_circuit_allocate_wire(circuit2);
        uint32_t temp2 = riscv_circuit_allocate_wire(circuit2);
        uint32_t temp3 = riscv_circuit_allocate_wire(circuit2);
        
        riscv_circuit_add_gate(circuit2, a2[i], b2[i], temp1, GATE_XOR);
        riscv_circuit_add_gate(circuit2, a2[i], b2[i], temp2, GATE_AND);
        riscv_circuit_add_gate(circuit2, temp1, carry, sum2[i], GATE_XOR);
        riscv_circuit_add_gate(circuit2, temp1, carry, temp3, GATE_AND);
        
        carry = riscv_circuit_allocate_wire(circuit2);
        riscv_circuit_add_gate(circuit2, temp2, temp3, carry, GATE_XOR);
    }
    
    circuit_io_t io2 = {
        .input_wires = malloc(8 * sizeof(uint32_t)),
        .output_wires = sum2,
        .num_inputs = 8,
        .num_outputs = 4
    };
    for (int i = 0; i < 4; i++) {
        io2.input_wires[i] = a2[i];
        io2.input_wires[4 + i] = b2[i];
    }
    
    // Prove equivalence
    int result = prove_circuit_equivalence(circuit1, &io1, circuit2, &io2);
    
    printf("\nResult: %s\n", result ? "EQUIVALENT" : "NOT EQUIVALENT");
    
    // Cleanup
    free(circuit1->gates);
    free(circuit1);
    free(circuit2->gates);
    free(circuit2);
    free(io1.input_wires);
    free(io2.input_wires);
}

int main() {
    printf("Complete Circuit Equivalence Prover\n");
    printf("===================================\n\n");
    
    printf("This tool proves 100%% equivalence between circuits.\n");
    printf("It verifies: ∀ input : Circuit1(input) ≡ Circuit2(input)\n\n");
    
    // Run example
    example_adder_equivalence();
    
    printf("\nApplications:\n");
    printf("1. Verify Rust SHA3 ≡ Our SHA3 implementation\n");
    printf("2. Verify optimized circuits ≡ reference circuits\n");
    printf("3. Verify cross-compiled code produces same results\n");
    printf("4. Enable proof-of-code by binding proofs to source\n");
    
    return 0;
}