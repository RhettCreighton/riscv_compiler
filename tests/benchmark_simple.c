/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

// Simple timing function
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Benchmark instruction compilation speed
void benchmark_instruction_speed(void) {
    printf("RISC-V Compiler Performance Benchmark\n");
    printf("=====================================\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return;
    }
    
    // Test instructions
    uint32_t test_instructions[] = {
        0x00208133,  // ADD x2, x1, x2
        0x00214233,  // XOR x4, x1, x2
        0x0020F2B3,  // AND x5, x1, x2
        0x40208333,  // SUB x6, x1, x2
        0x00509393,  // SLLI x7, x1, 5
        0x0050D413,  // SRLI x8, x1, 5
        0x06408493,  // ADDI x9, x1, 100
    };
    
    const char* instruction_names[] = {
        "ADD", "XOR", "AND", "SUB", "SLLI", "SRLI", "ADDI"
    };
    
    size_t num_instructions = sizeof(test_instructions) / sizeof(test_instructions[0]);
    
    printf("Individual Instruction Performance:\n");
    printf("%-10s %10s %15s %15s\n", "Instruction", "Gates", "Time (ms)", "Gates/ms");
    printf("%-10s %10s %15s %15s\n", "----------", "-----", "---------", "--------");
    
    // Benchmark each instruction type
    for (size_t i = 0; i < num_instructions; i++) {
        // Reset circuit
        compiler->circuit->num_gates = 0;
        
        double start_time = get_time_ms();
        size_t start_gates = compiler->circuit->num_gates;
        
        // Compile instruction
        if (riscv_compile_instruction(compiler, test_instructions[i]) < 0) {
            printf("%-10s %10s %15s %15s\n", instruction_names[i], "FAILED", "-", "-");
            continue;
        }
        
        double end_time = get_time_ms();
        size_t gates_added = compiler->circuit->num_gates - start_gates;
        double time_ms = end_time - start_time;
        
        printf("%-10s %10zu %15.3f %15.1f\n", 
               instruction_names[i], gates_added, time_ms, 
               time_ms > 0 ? gates_added / time_ms : 0);
    }
    
    // Benchmark bulk compilation
    printf("\nBulk Compilation Performance:\n");
    printf("%-20s %10s %15s %20s\n", "Test", "Instructions", "Time (ms)", "Instructions/sec");
    printf("%-20s %10s %15s %20s\n", "--------------------", "------------", "---------", "----------------");
    
    // Test different batch sizes
    size_t batch_sizes[] = {100, 1000, 10000};
    
    for (size_t b = 0; b < sizeof(batch_sizes)/sizeof(batch_sizes[0]); b++) {
        size_t batch_size = batch_sizes[b];
        
        // Reset compiler
        riscv_compiler_destroy(compiler);
        compiler = riscv_compiler_create();
        if (!compiler) break;
        
        double start_time = get_time_ms();
        
        // Compile many instructions
        for (size_t i = 0; i < batch_size; i++) {
            uint32_t instr = test_instructions[i % num_instructions];
            if (riscv_compile_instruction(compiler, instr) < 0) {
                break;
            }
        }
        
        double end_time = get_time_ms();
        double time_ms = end_time - start_time;
        double instructions_per_sec = time_ms > 0 ? (batch_size * 1000.0 / time_ms) : 0;
        
        printf("%-20s %10zu %15.1f %20.0f\n", 
               "Mixed instructions", batch_size, time_ms, instructions_per_sec);
    }
    
    // Summary statistics
    printf("\nCircuit Statistics:\n");
    printf("Total gates: %zu\n", compiler->circuit->num_gates);
    printf("Total wires: %u\n", compiler->circuit->next_wire_id);
    
    size_t and_gates = 0, xor_gates = 0;
    for (size_t i = 0; i < compiler->circuit->num_gates; i++) {
        if (compiler->circuit->gates[i].type == GATE_AND) and_gates++;
        else xor_gates++;
    }
    printf("AND gates: %zu (%.1f%%)\n", and_gates, 100.0 * and_gates / compiler->circuit->num_gates);
    printf("XOR gates: %zu (%.1f%%)\n", xor_gates, 100.0 * xor_gates / compiler->circuit->num_gates);
    
    riscv_compiler_destroy(compiler);
}

// Benchmark memory impact
void benchmark_memory_usage(void) {
    printf("\n\nMemory Usage Benchmark\n");
    printf("======================\n\n");
    
    size_t circuit_sizes[] = {1000, 10000, 100000};
    
    printf("%-15s %15s %20s\n", "Circuit Size", "Memory (KB)", "Gates/KB");
    printf("%-15s %15s %20s\n", "---------------", "----------", "--------");
    
    for (size_t s = 0; s < sizeof(circuit_sizes)/sizeof(circuit_sizes[0]); s++) {
        size_t target_gates = circuit_sizes[s];
        
        riscv_compiler_t* compiler = riscv_compiler_create();
        if (!compiler) continue;
        
        // Fill circuit with gates
        while (compiler->circuit->num_gates < target_gates) {
            // Add a simple XOR instruction
            riscv_compile_instruction(compiler, 0x00214233);
        }
        
        // Estimate memory usage
        size_t gate_memory = compiler->circuit->num_gates * sizeof(gate_t);
        size_t wire_memory = compiler->circuit->next_wire_id * sizeof(uint32_t);
        size_t total_memory = gate_memory + wire_memory + sizeof(riscv_circuit_t) + sizeof(riscv_compiler_t);
        
        double memory_kb = total_memory / 1024.0;
        double gates_per_kb = compiler->circuit->num_gates / memory_kb;
        
        printf("%-15zu %15.1f %20.1f\n", compiler->circuit->num_gates, memory_kb, gates_per_kb);
        
        riscv_compiler_destroy(compiler);
    }
}

int main(void) {
    // Run benchmarks
    benchmark_instruction_speed();
    benchmark_memory_usage();
    
    printf("\n✅ Benchmark completed successfully!\n");
    return 0;
}