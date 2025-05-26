/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include "riscv_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example: Compile a complete RISC-V program with all instruction types
// This demonstrates:
// - Arithmetic: ADD, SUB, AND, OR, XOR
// - Immediate: ADDI
// - Shifts: SLL, SRL, SRA
// - Branches: BEQ, BNE, BLT
// - Memory: LW, SW

int main() {
    printf("RISC-V Full Instruction Set Demo\n");
    printf("================================\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Create memory subsystem
    riscv_memory_t* memory = riscv_memory_create(compiler->circuit);
    compiler->memory = memory;
    
    // Initialize all registers and PC
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            if (reg == 0) {
                compiler->reg_wires[reg][bit] = 1;  // x0 is always 0
            } else {
                compiler->reg_wires[reg][bit] = riscv_circuit_allocate_wire(compiler->circuit);
            }
        }
    }
    
    // Initialize PC
    for (int bit = 0; bit < 32; bit++) {
        compiler->pc_wires[bit] = riscv_circuit_allocate_wire(compiler->circuit);
    }
    
    printf("Initial circuit state:\n");
    riscv_circuit_print_stats(compiler->circuit);
    printf("\n");
    
    // Demonstrate each instruction type
    
    // 1. Arithmetic instructions
    printf("=== Arithmetic Instructions ===\n");
    
    printf("ADD x3, x1, x2\n");
    riscv_compile_instruction(compiler, 0x002080B3);  // add x1, x1, x2
    
    printf("SUB x4, x1, x2\n");
    riscv_compile_instruction(compiler, 0x40208233);  // sub x4, x1, x2
    
    printf("XOR x5, x1, x2\n");
    riscv_compile_instruction(compiler, 0x0020C2B3);  // xor x5, x1, x2
    
    printf("OR x6, x1, x2\n");
    riscv_compile_instruction(compiler, 0x0020E333);  // or x6, x1, x2
    
    printf("AND x7, x1, x2\n");
    riscv_compile_instruction(compiler, 0x0020F3B3);  // and x7, x1, x2
    
    printf("\nAfter arithmetic: %zu gates\n", compiler->circuit->num_gates);
    
    // 2. Immediate instruction
    printf("\n=== Immediate Instructions ===\n");
    
    printf("ADDI x8, x1, 100\n");
    riscv_compile_instruction(compiler, 0x06408413);  // addi x8, x1, 100
    
    printf("\nAfter immediate: %zu gates\n", compiler->circuit->num_gates);
    
    // 3. Shift instructions
    printf("\n=== Shift Instructions ===\n");
    
    printf("SLL x9, x1, x2\n");
    riscv_compile_instruction(compiler, 0x002094B3);  // sll x9, x1, x2
    
    printf("SRL x10, x1, x2\n");
    riscv_compile_instruction(compiler, 0x0020D533);  // srl x10, x1, x2
    
    printf("SRA x11, x1, x2\n");
    riscv_compile_instruction(compiler, 0x4020D5B3);  // sra x11, x1, x2
    
    printf("SLLI x12, x1, 5\n");
    riscv_compile_instruction(compiler, 0x00509613);  // slli x12, x1, 5
    
    printf("\nAfter shifts: %zu gates\n", compiler->circuit->num_gates);
    
    // 4. Branch instructions
    printf("\n=== Branch Instructions ===\n");
    
    printf("BEQ x1, x2, +16\n");
    riscv_compile_instruction(compiler, 0x00208863);  // beq x1, x2, 16
    
    printf("BNE x1, x2, +16\n");
    riscv_compile_instruction(compiler, 0x00209863);  // bne x1, x2, 16
    
    printf("BLT x1, x2, +16\n");
    riscv_compile_instruction(compiler, 0x0020C863);  // blt x1, x2, 16
    
    printf("\nAfter branches: %zu gates\n", compiler->circuit->num_gates);
    
    // 5. Memory instructions
    printf("\n=== Memory Instructions ===\n");
    
    printf("LW x13, 0(x1)\n");
    riscv_compile_instruction(compiler, 0x0000A683);  // lw x13, 0(x1)
    
    printf("SW x2, 0(x1)\n");
    riscv_compile_instruction(compiler, 0x0020A023);  // sw x2, 0(x1)
    
    printf("\nAfter memory ops: %zu gates\n", compiler->circuit->num_gates);
    
    // Final statistics
    printf("\n=== Final Circuit Statistics ===\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Estimate gate counts per instruction type
    printf("\n=== Gate Count Estimates ===\n");
    printf("ADD/SUB: ~224 gates\n");
    printf("AND/OR/XOR: ~32-96 gates\n");
    printf("ADDI: ~224 gates\n");
    printf("Shifts: ~320 gates\n");
    printf("Branches: ~500 gates\n");
    printf("Memory ops: ~1000 gates (simplified)\n");
    
    // Convert to gate_computer format
    const char* output_file = "full_riscv_demo.txt";
    printf("\nConverting to gate_computer format...\n");
    riscv_circuit_to_gate_format(compiler->circuit, output_file);
    
    printf("\n=== Performance Projections ===\n");
    size_t avg_gates_per_instr = compiler->circuit->num_gates / 18;  // 18 instructions compiled
    printf("Average gates per instruction: %zu\n", avg_gates_per_instr);
    printf("Estimated gates for 1K instructions: %zuK\n", avg_gates_per_instr * 1 / 1000);
    printf("Estimated gates for 1M instructions: %zuM\n", avg_gates_per_instr * 1 / 1000);
    
    printf("\nWith BaseFold at 400M gates/sec:\n");
    printf("  1K instructions: %.1f ms\n", (avg_gates_per_instr * 1000.0) / 400000000 * 1000);
    printf("  1M instructions: %.1f s\n", (avg_gates_per_instr * 1000000.0) / 400000000);
    
    riscv_memory_destroy(memory);
    riscv_compiler_destroy(compiler);
    
    printf("\n✅ Successfully demonstrated RISC-V to gate compilation!\n");
    printf("Next steps:\n");
    printf("1. Implement multiplication (MUL) - ~20K gates\n");
    printf("2. Add division support - ~50K gates\n");
    printf("3. Create ELF loader for real programs\n");
    printf("4. Optimize gate counts with better algorithms\n");
    printf("5. Integrate with gate_computer for zkVM proofs\n");
    
    return 0;
}