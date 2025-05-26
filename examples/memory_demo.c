/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include "riscv_memory.h"
#include <stdio.h>
#include <stdlib.h>

// Simple demonstration of memory instructions
// Shows the proper way to initialize memory subsystem

int main() {
    printf("RISC-V Memory Instructions Demo\n");
    printf("===============================\n\n");
    
    // Step 1: Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Step 2: Create and attach memory subsystem - CRITICAL!
    // Without this, memory instructions will fail with "Unsupported opcode"
    riscv_memory_t* memory = riscv_memory_create(compiler->circuit);
    compiler->memory = memory;
    
    printf("Initial state:\n");
    riscv_circuit_print_stats(compiler->circuit);
    printf("\n");
    
    // Simple program: Load value from memory, add 1, store back
    uint32_t program[] = {
        0x00002183,  // lw x3, 0(x0)    - Load word from address 0
        0x00318193,  // addi x3, x3, 3  - Add 3 to loaded value  
        0x00302023,  // sw x3, 0(x0)    - Store result back to address 0
    };
    
    printf("Compiling program:\n");
    printf("  lw x3, 0(x0)    # Load from memory[0]\n");
    printf("  addi x3, x3, 3  # Add 3\n");
    printf("  sw x3, 0(x0)    # Store back to memory[0]\n\n");
    
    // Compile each instruction
    for (int i = 0; i < 3; i++) {
        printf("Instruction %d: ", i+1);
        if (riscv_compile_instruction(compiler, program[i]) == 0) {
            printf("✅ Success (total gates: %zu)\n", compiler->circuit->num_gates);
        } else {
            printf("❌ Failed\n");
        }
    }
    
    printf("\nFinal statistics:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    printf("\n📝 Key Points:\n");
    printf("• Memory subsystem MUST be created and attached before use\n");
    printf("• Each memory access uses SHA3 for Merkle proof (high gate count)\n");
    printf("• Gate count is high (~4M per access) due to cryptographic security\n");
    printf("• This is expected - memory security is expensive but necessary\n");
    
    // Cleanup
    riscv_memory_destroy(memory);
    riscv_compiler_destroy(compiler);
    
    return 0;
}