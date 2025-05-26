#include "riscv_compiler.h"
#include "riscv_elf_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Memory constraint management for 10MB input/output limits
// This ensures zkVM circuits stay within gate_computer bounds

// Calculate the overhead from PC and registers
#define STATE_OVERHEAD_BITS (2 + PC_BITS + REGS_BITS)  // 2 + 32 + 1024 = 1058 bits
#define STATE_OVERHEAD_BYTES ((STATE_OVERHEAD_BITS + 7) / 8)  // 133 bytes

// Maximum memory allowed given 10MB total limit
#define MAX_MEMORY_BYTES (MAX_INPUT_BITS / 8 - STATE_OVERHEAD_BYTES)  // ~10,485,627 bytes

// Memory usage analysis - defined in riscv_compiler.h

// Analyze memory requirements of a program
memory_analysis_t* analyze_memory_requirements(const riscv_program_t* program) {
    memory_analysis_t* analysis = calloc(1, sizeof(memory_analysis_t));
    if (!analysis) return NULL;
    
    // Code size from loaded program
    analysis->code_size = program->num_instructions * 4;  // 4 bytes per instruction
    analysis->code_start = program->entry_point & ~0xFFF;  // Align to page
    analysis->code_end = analysis->code_start + analysis->code_size;
    
    // Data sections from ELF
    analysis->data_size = program->data_size;
    analysis->data_start = program->data_start;
    analysis->data_end = analysis->data_start + analysis->data_size;
    
    // BSS (uninitialized data) - not currently extracted from ELF
    analysis->bss_size = 0;
    
    // Estimate heap and stack requirements
    // These are heuristics - real values depend on program behavior
    analysis->heap_size = 1024 * 1024;   // 1MB default heap
    analysis->stack_size = 64 * 1024;    // 64KB default stack
    
    // Calculate total
    analysis->total_memory = analysis->code_size + 
                            analysis->data_size + 
                            analysis->bss_size +
                            analysis->heap_size +
                            analysis->stack_size;
    
    // Layout memory regions
    if (analysis->data_end > 0) {
        analysis->heap_start = (analysis->data_end + 0xFFF) & ~0xFFF;  // Page align
    } else {
        analysis->heap_start = (analysis->code_end + 0xFFF) & ~0xFFF;
    }
    analysis->heap_end = analysis->heap_start + analysis->heap_size;
    
    // Stack grows down from high memory
    analysis->stack_end = 0x80000000;  // Common RISC-V stack top
    analysis->stack_start = analysis->stack_end - analysis->stack_size;
    
    return analysis;
}

// Check if memory requirements fit within constraints
bool check_memory_constraints(const memory_analysis_t* analysis, 
                             char* error_msg, size_t error_msg_size) {
    if (analysis->total_memory > MAX_MEMORY_BYTES) {
        snprintf(error_msg, error_msg_size,
                "Program requires %.2f MB of memory, but zkVM limit is %.2f MB\n"
                "  Code:  %.2f MB\n"
                "  Data:  %.2f MB\n"
                "  Heap:  %.2f MB\n"
                "  Stack: %.2f MB\n"
                "  Total: %.2f MB\n"
                "\n"
                "Suggestions to reduce memory usage:\n"
                "  • Reduce heap allocation (current: %.2f MB)\n"
                "  • Optimize data structures\n"
                "  • Use smaller stack size\n"
                "  • Split program into smaller chunks",
                analysis->total_memory / (1024.0 * 1024.0),
                MAX_MEMORY_BYTES / (1024.0 * 1024.0),
                analysis->code_size / (1024.0 * 1024.0),
                analysis->data_size / (1024.0 * 1024.0),
                analysis->heap_size / (1024.0 * 1024.0),
                analysis->stack_size / (1024.0 * 1024.0),
                analysis->total_memory / (1024.0 * 1024.0),
                analysis->heap_size / (1024.0 * 1024.0));
        return false;
    }
    
    return true;
}

// Print memory analysis report
void print_memory_analysis(const memory_analysis_t* analysis) {
    printf("\n");
    printf("=== RISC-V Program Memory Analysis ===\n");
    printf("\n");
    
    printf("Memory Layout:\n");
    printf("  0x%08x - 0x%08x  Code   (.text)     %8zu bytes  (%.2f MB)\n",
           analysis->code_start, analysis->code_end,
           analysis->code_size, analysis->code_size / (1024.0 * 1024.0));
    
    if (analysis->data_size > 0) {
        printf("  0x%08x - 0x%08x  Data   (.data)     %8zu bytes  (%.2f MB)\n",
               analysis->data_start, analysis->data_end,
               analysis->data_size, analysis->data_size / (1024.0 * 1024.0));
    }
    
    printf("  0x%08x - 0x%08x  Heap              %8zu bytes  (%.2f MB)\n",
           analysis->heap_start, analysis->heap_end,
           analysis->heap_size, analysis->heap_size / (1024.0 * 1024.0));
    
    printf("  0x%08x - 0x%08x  Stack             %8zu bytes  (%.2f MB)\n",
           analysis->stack_start, analysis->stack_end,
           analysis->stack_size, analysis->stack_size / (1024.0 * 1024.0));
    
    printf("\n");
    printf("Total Memory Required: %.2f MB\n", 
           analysis->total_memory / (1024.0 * 1024.0));
    printf("zkVM Memory Limit:     %.2f MB\n", 
           MAX_MEMORY_BYTES / (1024.0 * 1024.0));
    
    double usage_pct = 100.0 * analysis->total_memory / MAX_MEMORY_BYTES;
    printf("Memory Usage:          %.1f%%\n", usage_pct);
    
    if (usage_pct > 80) {
        printf("\n⚠️  WARNING: High memory usage (>80%%)\n");
        printf("   Consider optimizing memory allocation\n");
    }
}

// Create a memory-constrained compiler
riscv_compiler_t* riscv_compiler_create_constrained(size_t max_memory_bytes) {
    // Validate memory size
    if (max_memory_bytes > MAX_MEMORY_BYTES) {
        fprintf(stderr, "ERROR: Requested memory size %.2f MB exceeds zkVM limit of %.2f MB\n",
                max_memory_bytes / (1024.0 * 1024.0),
                MAX_MEMORY_BYTES / (1024.0 * 1024.0));
        fprintf(stderr, "\nThe zkVM has a hard limit of 10MB for combined input and output.\n");
        fprintf(stderr, "After accounting for registers and PC, this leaves ~%.1f MB for program memory.\n",
                MAX_MEMORY_BYTES / (1024.0 * 1024.0));
        return NULL;
    }
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return NULL;
    
    // Calculate circuit sizes
    size_t input_bits = calculate_riscv_input_size_with_memory(max_memory_bytes);
    size_t output_bits = calculate_riscv_output_size_with_memory(max_memory_bytes);
    
    // Validate against gate_computer limits
    if (input_bits > MAX_INPUT_BITS || output_bits > MAX_OUTPUT_BITS) {
        fprintf(stderr, "ERROR: Circuit would exceed gate_computer limits\n");
        fprintf(stderr, "  Input bits:  %zu (limit: %d)\n", input_bits, MAX_INPUT_BITS);
        fprintf(stderr, "  Output bits: %zu (limit: %d)\n", output_bits, MAX_OUTPUT_BITS);
        riscv_compiler_destroy(compiler);
        return NULL;
    }
    
    // Create properly sized circuit
    compiler->circuit = riscv_circuit_create(input_bits, output_bits);
    if (!compiler->circuit) {
        riscv_compiler_destroy(compiler);
        return NULL;
    }
    
    return compiler;
}

// Helper to calculate input size with specific memory
size_t calculate_riscv_input_size_with_memory(size_t memory_bytes) {
    return 2 +                    // Constants
           PC_BITS +              // PC
           REGS_BITS +            // Registers  
           (memory_bytes * 8);    // Memory bits
}

// Helper to calculate output size with specific memory
size_t calculate_riscv_output_size_with_memory(size_t memory_bytes) {
    return PC_BITS +              // PC
           REGS_BITS +            // Registers
           (memory_bytes * 8);    // Memory bits
}

// Memory optimization suggestions
void suggest_memory_optimizations(const memory_analysis_t* analysis) {
    printf("\n");
    printf("=== Memory Optimization Suggestions ===\n");
    printf("\n");
    
    // Analyze memory usage patterns
    double code_pct = 100.0 * analysis->code_size / analysis->total_memory;
    double data_pct = 100.0 * analysis->data_size / analysis->total_memory;
    double heap_pct = 100.0 * analysis->heap_size / analysis->total_memory;
    double stack_pct = 100.0 * analysis->stack_size / analysis->total_memory;
    
    printf("Memory Distribution:\n");
    printf("  Code:  %5.1f%%  ", code_pct);
    for (int i = 0; i < code_pct/2; i++) printf("█");
    printf("\n");
    
    printf("  Data:  %5.1f%%  ", data_pct);
    for (int i = 0; i < data_pct/2; i++) printf("█");
    printf("\n");
    
    printf("  Heap:  %5.1f%%  ", heap_pct);
    for (int i = 0; i < heap_pct/2; i++) printf("█");
    printf("\n");
    
    printf("  Stack: %5.1f%%  ", stack_pct);
    for (int i = 0; i < stack_pct/2; i++) printf("█");
    printf("\n");
    
    printf("\nOptimization Strategies:\n");
    
    if (heap_pct > 40) {
        printf("  • Large heap usage (%.1f%%) detected\n", heap_pct);
        printf("    - Consider using stack allocation where possible\n");
        printf("    - Implement custom memory pooling\n");
        printf("    - Free memory as soon as possible\n");
    }
    
    if (stack_pct > 20) {
        printf("  • Large stack usage (%.1f%%) detected\n", stack_pct);
        printf("    - Reduce function call depth\n");
        printf("    - Use heap for large local arrays\n");
        printf("    - Optimize recursive algorithms\n");
    }
    
    if (data_pct > 30) {
        printf("  • Large static data (%.1f%%) detected\n", data_pct);
        printf("    - Consider compressing constant data\n");
        printf("    - Load data dynamically if possible\n");
        printf("    - Use more compact data structures\n");
    }
    
    printf("\nzkVM-Specific Optimizations:\n");
    printf("  • Split large programs into smaller proof chunks\n");
    printf("  • Use merkle trees for large data sets\n");
    printf("  • Implement state checkpointing between proofs\n");
    printf("  • Consider off-chain storage with on-chain verification\n");
}

// Example: Memory-aware program loader
int load_program_with_constraints(const char* elf_file, 
                                  riscv_compiler_t** compiler_out,
                                  riscv_program_t** program_out) {
    // Load the program
    riscv_program_t* program = riscv_load_elf(elf_file);
    if (!program) {
        fprintf(stderr, "Failed to load ELF file\n");
        return -1;
    }
    
    // Analyze memory requirements
    memory_analysis_t* analysis = analyze_memory_requirements(program);
    if (!analysis) {
        riscv_program_free(program);
        return -1;
    }
    
    // Print analysis
    print_memory_analysis(analysis);
    
    // Check constraints
    char error_msg[1024];
    if (!check_memory_constraints(analysis, error_msg, sizeof(error_msg))) {
        fprintf(stderr, "\n❌ ERROR: Program exceeds zkVM memory constraints\n\n");
        fprintf(stderr, "%s\n", error_msg);
        suggest_memory_optimizations(analysis);
        
        free(analysis);
        riscv_program_free(program);
        return -1;
    }
    
    // Create constrained compiler
    riscv_compiler_t* compiler = riscv_compiler_create_constrained(analysis->total_memory);
    if (!compiler) {
        free(analysis);
        riscv_program_free(program);
        return -1;
    }
    
    printf("\n✅ Program fits within zkVM constraints\n");
    printf("   Memory usage: %.1f%% of limit\n", 
           100.0 * analysis->total_memory / MAX_MEMORY_BYTES);
    
    free(analysis);
    *compiler_out = compiler;
    *program_out = program;
    return 0;
}