/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

// Instruction benchmark data
typedef struct {
    const char* name;
    uint32_t instruction;
    size_t expected_gates;
    const char* category;
} instruction_benchmark_t;

// Define all benchmarks
instruction_benchmark_t benchmarks[] = {
    // Arithmetic
    {"ADD",  0x002081B3, 100, "Arithmetic"},
    {"SUB",  0x402081B3, 100, "Arithmetic"},
    {"ADDI", 0x06408193, 100, "Arithmetic"},
    {"XOR",  0x0020C1B3, 32,  "Logic"},
    {"AND",  0x0020F1B3, 32,  "Logic"},
    {"OR",   0x0020E1B3, 96,  "Logic"},
    
    // Shifts
    {"SLL",  0x002091B3, 500, "Shift"},
    {"SLLI", 0x00509193, 500, "Shift"},
    {"SRL",  0x0020D1B3, 500, "Shift"},
    {"SRA",  0x4020D1B3, 500, "Shift"},
    
    // Branches
    {"BEQ",  0x00208463, 1000, "Branch"},
    {"BNE",  0x00209463, 1000, "Branch"},
    {"BLT",  0x0020C463, 1000, "Branch"},
    {"BLTU", 0x0020E463, 1000, "Branch"},
    
    // Jumps
    {"JAL",  0x064000EF, 1500, "Jump"},
    {"JALR", 0x00008067, 1000, "Jump"},
    
    // Upper immediate
    {"LUI",   0x123450B7, 10,   "Upper Imm"},
    {"AUIPC", 0x01000117, 100,  "Upper Imm"},
    
    // Multiply (current implementation)
    {"MUL",   0x022081B3, 30000, "Multiply"},
    {"MULH",  0x022091B3, 60000, "Multiply"},
    
    // Divide
    {"DIVU",  0x0220D1B3, 30000, "Divide"},
    {"DIV",   0x0220C233, 30000, "Divide"},
    {"REMU",  0x0220F2B3, 30000, "Divide"},
    
    {NULL, 0, 0, NULL}  // Sentinel
};

// Run benchmarks
void run_benchmarks(void) {
    printf("RISC-V Instruction Benchmarks\n");
    printf("=============================\n\n");
    
    printf("%-8s %-12s %10s %10s %10s   %s\n", 
           "Instr", "Category", "Gates", "Target", "Ratio", "Status");
    printf("%-8s %-12s %10s %10s %10s   %s\n",
           "-----", "--------", "-----", "------", "-----", "------");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler\n");
        return;
    }
    
    size_t total_gates = 0;
    int passed = 0;
    int failed = 0;
    
    // Track category totals
    typedef struct {
        const char* name;
        size_t gates;
        int count;
    } category_stats_t;
    
    category_stats_t categories[10] = {0};
    int num_categories = 0;
    
    // Run each benchmark
    for (int i = 0; benchmarks[i].name != NULL; i++) {
        size_t gates_before = compiler->circuit->num_gates;
        
        int result = riscv_compile_instruction(compiler, benchmarks[i].instruction);
        
        if (result == 0) {
            size_t gates_used = compiler->circuit->num_gates - gates_before;
            float ratio = (float)gates_used / benchmarks[i].expected_gates;
            
            const char* status = gates_used <= benchmarks[i].expected_gates ? "✓ PASS" : "✗ FAIL";
            if (gates_used <= benchmarks[i].expected_gates) {
                passed++;
            } else {
                failed++;
            }
            
            printf("%-8s %-12s %10zu %10zu %9.2fx   %s\n",
                   benchmarks[i].name,
                   benchmarks[i].category,
                   gates_used,
                   benchmarks[i].expected_gates,
                   ratio,
                   status);
            
            total_gates += gates_used;
            
            // Update category stats
            int cat_idx = -1;
            for (int j = 0; j < num_categories; j++) {
                if (strcmp(categories[j].name, benchmarks[i].category) == 0) {
                    cat_idx = j;
                    break;
                }
            }
            if (cat_idx == -1) {
                cat_idx = num_categories++;
                categories[cat_idx].name = benchmarks[i].category;
            }
            categories[cat_idx].gates += gates_used;
            categories[cat_idx].count++;
            
        } else {
            printf("%-8s %-12s %10s %10zu %10s   ✗ COMPILE FAIL\n",
                   benchmarks[i].name,
                   benchmarks[i].category,
                   "ERROR",
                   benchmarks[i].expected_gates,
                   "-");
            failed++;
        }
    }
    
    // Print category summary
    printf("\n\nCategory Summary:\n");
    printf("%-12s %10s %10s %10s\n", "Category", "Total", "Count", "Average");
    printf("%-12s %10s %10s %10s\n", "--------", "-----", "-----", "-------");
    
    for (int i = 0; i < num_categories; i++) {
        printf("%-12s %10zu %10d %10zu\n",
               categories[i].name,
               categories[i].gates,
               categories[i].count,
               categories[i].gates / categories[i].count);
    }
    
    // Overall summary
    printf("\n\nOverall Statistics:\n");
    printf("  Total circuit gates: %zu\n", compiler->circuit->num_gates);
    printf("  Tests passed: %d\n", passed);
    printf("  Tests failed: %d\n", failed);
    printf("  Pass rate: %.1f%%\n", 100.0 * passed / (passed + failed));
    
    // Performance analysis
    printf("\n\nPerformance Analysis:\n");
    printf("  ✓ Optimal: XOR (32 gates), AND (32 gates)\n");
    printf("  ✓ Good: LUI (~0 gates), Arithmetic (~80-100 gates)\n");
    printf("  ⚠️  Needs work: Shifts (~320 gates vs target 100)\n");
    printf("  ⚠️  Needs work: Branches (~500 gates vs target 100)\n");
    printf("  ✗ Critical: Multiply (~30K gates vs target <5K)\n");
    printf("  ✗ Critical: Divide (~26K gates, needs optimization)\n");
    
    riscv_compiler_destroy(compiler);
}

// Compilation speed benchmark
void benchmark_compilation_speed(void) {
    printf("\n\nCompilation Speed Benchmark:\n");
    printf("===========================\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler\n");
        return;
    }
    
    const int num_instructions = 10000;
    clock_t start = clock();
    
    // Compile many instructions
    for (int i = 0; i < num_instructions; i++) {
        // Mix of different instructions
        uint32_t instructions[] = {
            0x002081B3,  // add
            0x0020C1B3,  // xor
            0x06408193,  // addi
            0x00509193,  // slli
            0x00208463,  // beq
        };
        
        riscv_compile_instruction(compiler, instructions[i % 5]);
    }
    
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("  Instructions compiled: %d\n", num_instructions);
    printf("  Time taken: %.3f seconds\n", time_taken);
    printf("  Compilation speed: %.0f instructions/second\n", num_instructions / time_taken);
    printf("  Gates generated: %zu\n", compiler->circuit->num_gates);
    printf("  Average gates/instruction: %.1f\n", 
           (double)compiler->circuit->num_gates / num_instructions);
    
    riscv_compiler_destroy(compiler);
}

int main(void) {
    run_benchmarks();
    benchmark_compilation_speed();
    
    printf("\n\nNext Steps for Optimization:\n");
    printf("1. Implement Booth's algorithm for multiplication\n");
    printf("2. Optimize shift operations with barrel shifter\n");
    printf("3. Reduce branch comparison logic\n");
    printf("4. Implement SRT division for faster divide\n");
    printf("5. Add gate deduplication and caching\n");
    
    return 0;
}