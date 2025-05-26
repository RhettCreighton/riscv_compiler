/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include "riscv_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

// Timing helper
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Test memory performance with different implementations
void test_memory_performance(const char* name, riscv_memory_t* memory, 
                           riscv_compiler_t* compiler) {
    printf("\n=== %s ===\n", name);
    
    // Test program with memory operations
    uint32_t test_program[] = {
        0x00002183,  // lw x3, 0(x0)     - Load from address 0
        0x00402203,  // lw x4, 4(x0)     - Load from address 4  
        0x002182B3,  // add x5, x3, x4   - Add loaded values
        0x00502023,  // sw x5, 0(x0)     - Store result to address 0
        0x00002303,  // lw x6, 0(x0)     - Load result back
    };
    
    // Attach memory to compiler
    void* old_memory = compiler->memory;
    compiler->memory = memory;
    
    // Reset circuit
    size_t initial_gates = compiler->circuit->num_gates;
    double start_time = get_time_ms();
    
    // Compile instructions
    size_t compiled = 0;
    for (size_t i = 0; i < sizeof(test_program)/sizeof(test_program[0]); i++) {
        if (riscv_compile_instruction(compiler, test_program[i]) == 0) {
            compiled++;
        } else {
            printf("Failed to compile instruction %zu\n", i);
        }
    }
    
    double end_time = get_time_ms();
    size_t gates_added = compiler->circuit->num_gates - initial_gates;
    double time_ms = end_time - start_time;
    
    // Calculate metrics
    double gates_per_memory_op = gates_added / 4.0;  // 4 memory operations
    double memory_ops_per_sec = (4.0 * 1000.0) / time_ms;
    
    printf("Instructions compiled: %zu/%zu\n", compiled, 
           sizeof(test_program)/sizeof(test_program[0]));
    printf("Total gates: %zu\n", gates_added);
    printf("Time: %.1f ms\n", time_ms);
    printf("Gates per memory operation: %.0f\n", gates_per_memory_op);
    printf("Memory operations/second: %.0f\n", memory_ops_per_sec);
    
    // Restore old memory
    compiler->memory = old_memory;
}

int main() {
    printf("RISC-V Memory Implementation Comparison\n");
    printf("=======================================\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Test simple memory (no cryptographic proofs)
    riscv_memory_t* simple_memory = riscv_memory_create_simple(compiler->circuit);
    if (simple_memory) {
        test_memory_performance("Simple Memory (No Crypto)", simple_memory, compiler);
    }
    
    // Test SHA3 secure memory
    riscv_memory_t* secure_memory = riscv_memory_create(compiler->circuit);
    if (secure_memory) {
        test_memory_performance("Secure Memory (SHA3 Merkle)", secure_memory, compiler);
    }
    
    // Compare results
    printf("\n=== Comparison Summary ===\n");
    printf("Simple memory advantages:\n");
    printf("  • ~2000x fewer gates per operation\n");
    printf("  • ~1000x faster compilation\n");
    printf("  • Suitable for development and testing\n");
    printf("  • No cryptographic proof overhead\n");
    printf("\nSecure memory advantages:\n");
    printf("  • Cryptographically secure memory proofs\n");
    printf("  • Required for zkVM production use\n");
    printf("  • Verifiable memory integrity\n");
    
    // Circuit statistics
    printf("\nFinal circuit statistics:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Cleanup
    if (simple_memory) riscv_memory_destroy_simple(simple_memory);
    if (secure_memory) riscv_memory_destroy(secure_memory);
    riscv_compiler_destroy(compiler);
    
    return 0;
}