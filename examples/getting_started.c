/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @file getting_started.c
 * @brief Quick Start Guide for RISC-V Compiler
 * 
 * This is the simplest possible example to get you started with the
 * RISC-V to Gate Circuit Compiler. In just a few lines of code, you'll
 * compile your first RISC-V instruction to a boolean logic circuit!
 * 
 * @author RISC-V Compiler Team
 * @date 2025
 */

#include "riscv_compiler.h"
#include <stdio.h>

int main(void) {
    printf("🚀 RISC-V Compiler - Quick Start\\n");
    printf("=================================\\n\\n");
    
    // Step 1: Create the compiler
    printf("Creating compiler...\\n");
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("❌ Error: Failed to create compiler\\n");
        return 1;
    }
    printf("✅ Compiler created successfully\\n\\n");
    
    // Step 2: Compile a simple instruction
    printf("Compiling: ADD x3, x1, x2\\n");
    uint32_t add_instruction = 0x002081B3;  // ADD x3, x1, x2
    
    int result = riscv_compile_instruction(compiler, add_instruction);
    if (result == 0) {
        printf("✅ Instruction compiled successfully!\\n");
        printf("   Gates generated: %zu\\n", compiler->circuit->num_gates);
        printf("   This creates an optimized 32-bit adder circuit\\n\\n");
    } else {
        printf("❌ Error: Failed to compile instruction\\n");
        riscv_compiler_destroy(compiler);
        return 1;
    }
    
    // Step 3: Try a few more instructions
    printf("Compiling more instructions...\\n");
    
    struct {
        const char* name;
        uint32_t instruction;
    } examples[] = {
        {"XOR x4, x1, x2", 0x0020C233},  // XOR - very efficient (32 gates)
        {"SLLI x5, x1, 5", 0x00509293}, // Shift left - barrel shifter (640 gates)
        {"ADDI x6, x1, 100", 0x06408313} // Add immediate (224 gates)
    };
    
    for (int i = 0; i < 3; i++) {
        size_t gates_before = compiler->circuit->num_gates;
        result = riscv_compile_instruction(compiler, examples[i].instruction);
        size_t gates_after = compiler->circuit->num_gates;
        
        if (result == 0) {
            printf("  ✅ %s: %zu gates\\n", 
                   examples[i].name, gates_after - gates_before);
        } else {
            printf("  ❌ %s: FAILED\\n", examples[i].name);
        }
    }
    
    // Step 4: Show final statistics
    printf("\\n📊 Final Circuit Statistics:\\n");
    printf("   Total gates: %zu\\n", compiler->circuit->num_gates);
    printf("   Total wires: %u\\n", compiler->circuit->next_wire_id);
    printf("   Instructions compiled: 4\\n");
    printf("   Average gates per instruction: %.1f\\n", 
           (double)compiler->circuit->num_gates / 4);
    
    // Step 5: Export the circuit (optional)
    printf("\\nExporting circuit to 'getting_started.circuit'...\\n");
    if (riscv_circuit_to_file(compiler->circuit, "getting_started.circuit") == 0) {
        printf("✅ Circuit exported successfully\\n");
        printf("   You can now use this with Gate Computer for ZK proofs!\\n");
    } else {
        printf("⚠️  Circuit export failed (this is optional)\\n");
    }
    
    // Step 6: Clean up
    printf("\\nCleaning up...\\n");
    riscv_compiler_destroy(compiler);
    printf("✅ Done!\\n\\n");
    
    printf("🎉 SUCCESS! You've compiled your first RISC-V instructions!\\n\\n");
    printf("🚀 NEXT STEPS:\\n");
    printf("   • Run './tutorial_complete' for comprehensive learning\\n");
    printf("   • Try './fibonacci_riscv_demo' for a real program example\\n");
    printf("   • Check './memory_ultra_comparison' for memory optimizations\\n");
    printf("   • Read the API docs in 'include/riscv_compiler.h'\\n\\n");
    printf("💡 TIP: The compiler optimizes each instruction type:\\n");
    printf("   • Logic ops (XOR, AND): 32 gates (optimal)\\n");
    printf("   • Arithmetic (ADD, SUB): 224-256 gates\\n");
    printf("   • Shifts: 640 gates (33%% optimized)\\n");
    printf("   • Branches: 96-257 gates (up to 87%% optimized)\\n");
    printf("   • Memory: 2.2K gates (ultra) to 3.9M gates (secure)\\n\\n");
    printf("📈 Performance: 272K-997K instructions/second compilation speed!\\n");
    
    return 0;
}