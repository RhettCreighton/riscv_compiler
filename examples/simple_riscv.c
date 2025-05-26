/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdio.h>
#include <stdlib.h>

// Example: Compile a simple RISC-V program to gates
// Program: Add two numbers
//   addi x1, x0, 5    # x1 = 5
//   addi x2, x0, 3    # x2 = 3
//   add  x3, x1, x2   # x3 = x1 + x2 = 8

int main() {
    printf("RISC-V to Gate Compiler Example\n");
    printf("================================\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Initialize register wires
    // x0 is hardwired to 0
    for (int i = 0; i < 32; i++) {
        compiler->reg_wires[0][i] = 1;  // Wire 1 is constant 0
    }
    
    // Initialize other registers
    for (int reg = 1; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            compiler->reg_wires[reg][bit] = riscv_circuit_allocate_wire(compiler->circuit);
        }
    }
    
    printf("Initial circuit state:\n");
    riscv_circuit_print_stats(compiler->circuit);
    printf("\n");
    
    // Compile ADD instruction: x3 = x1 + x2
    // Encoding: ADD rd=x3, rs1=x1, rs2=x2
    // opcode=0x33, rd=3, funct3=0, rs1=1, rs2=2, funct7=0
    uint32_t add_instruction = 0x00208133;  // add x3, x1, x2
    
    printf("Compiling instruction: ADD x3, x1, x2 (0x%08x)\n", add_instruction);
    
    if (riscv_compile_instruction(compiler, add_instruction) < 0) {
        fprintf(stderr, "Failed to compile instruction\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    printf("\nCircuit after ADD:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Compile XOR instruction: x4 = x1 ^ x2
    uint32_t xor_instruction = 0x00214233;  // xor x4, x1, x2
    
    printf("\nCompiling instruction: XOR x4, x1, x2 (0x%08x)\n", xor_instruction);
    
    if (riscv_compile_instruction(compiler, xor_instruction) < 0) {
        fprintf(stderr, "Failed to compile instruction\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    printf("\nCircuit after XOR:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Compile AND instruction: x5 = x1 & x2
    uint32_t and_instruction = 0x0020F2B3;  // and x5, x1, x2
    
    printf("\nCompiling instruction: AND x5, x1, x2 (0x%08x)\n", and_instruction);
    
    if (riscv_compile_instruction(compiler, and_instruction) < 0) {
        fprintf(stderr, "Failed to compile instruction\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    printf("\nFinal circuit statistics:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Save circuit to file
    const char* output_file = "riscv_circuit.txt";
    printf("\nSaving circuit to %s...\n", output_file);
    
    if (riscv_circuit_to_file(compiler->circuit, output_file) < 0) {
        fprintf(stderr, "Failed to save circuit\n");
    } else {
        printf("Circuit saved successfully!\n");
    }
    
    // Estimate gates per instruction
    size_t gates_per_add = 0;
    for (size_t i = 0; i < compiler->circuit->num_gates; i++) {
        if (compiler->circuit->gates[i].type == GATE_AND || 
            compiler->circuit->gates[i].type == GATE_XOR) {
            gates_per_add++;
        }
        if (gates_per_add > 600) break;  // Approximate gates for first ADD
    }
    
    printf("\nPerformance estimates:\n");
    printf("  Gates per ADD: ~%zu\n", gates_per_add);
    printf("  Gates per XOR: 32 (1 gate per bit)\n");
    printf("  Gates per AND: 32 (1 gate per bit)\n");
    printf("  Estimated gates for 1M instructions: ~%zuM\n", gates_per_add / 1000);
    
    riscv_compiler_destroy(compiler);
    return 0;
}