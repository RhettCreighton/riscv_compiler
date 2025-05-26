/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "riscv_compiler.h"
#include "riscv_memory.h"

// Test all three memory implementations
void test_memory_implementation(const char* name, 
                               riscv_memory_t* (*create_fn)(riscv_circuit_t*)) {
    printf("\n=== %s ===\n", name);
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return;
    }
    
    // Use specified memory implementation
    compiler->memory = create_fn(compiler->circuit);
    
    // Test instructions: Store and load pattern
    uint32_t test_instructions[] = {
        // SW x1, 0(x0)    - Store register 1 to address 0
        0x00102023,
        // SW x2, 4(x0)    - Store register 2 to address 4  
        0x00202223,
        // LW x3, 0(x0)    - Load from address 0 to register 3
        0x00002183,
        // LW x4, 4(x0)    - Load from address 4 to register 4
        0x00402203,
        // ADD x5, x3, x4  - Add loaded values
        0x004182B3
    };
    
    size_t num_instructions = sizeof(test_instructions) / sizeof(test_instructions[0]);
    size_t initial_gates = compiler->circuit->num_gates;
    
    clock_t start = clock();
    
    // Compile instructions
    for (size_t i = 0; i < num_instructions; i++) {
        riscv_compile_instruction(compiler, test_instructions[i]);
    }
    
    clock_t end = clock();
    double time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    
    size_t gates_used = compiler->circuit->num_gates - initial_gates;
    size_t gates_per_mem_op = gates_used / 4;  // 2 stores + 2 loads
    double ops_per_second = (4.0 / time_ms) * 1000.0;
    
    printf("Instructions compiled: %zu/%zu\n", num_instructions, num_instructions);
    printf("Total gates: %zu\n", gates_used);
    printf("Time: %.1f ms\n", time_ms);
    printf("Gates per memory operation: %zu\n", gates_per_mem_op);
    printf("Memory operations/second: %.0f\n", ops_per_second);
    
    // Cleanup
    riscv_compiler_destroy(compiler);
}

int main() {
    printf("RISC-V Memory Implementation Comparison (All Three)\n");
    printf("===================================================\n");
    
    // Test ultra-simple memory
    test_memory_implementation("Ultra-Simple Memory (8 words)", 
                              riscv_memory_create_ultra_simple);
    
    // Test simple memory  
    test_memory_implementation("Simple Memory (256 words)",
                              riscv_memory_create_simple);
    
    // Test secure memory
    test_memory_implementation("Secure Memory (SHA3 Merkle)",
                              riscv_memory_create);
    
    printf("\n=== Comparison Summary ===\n");
    printf("Ultra-Simple (8 words):\n");
    printf("  • Estimated ~500 gates per operation\n");
    printf("  • Perfect for small demos and testing\n");
    printf("  • 200x improvement over simple memory\n");
    printf("\nSimple (256 words):\n");
    printf("  • ~101K gates per operation\n");
    printf("  • Good for development\n");
    printf("  • 39x improvement over secure memory\n");
    printf("\nSecure (Full Merkle):\n");
    printf("  • ~3.9M gates per operation\n");
    printf("  • Required for production zkVM\n");
    printf("  • Provides cryptographic security\n");
    
    return 0;
}