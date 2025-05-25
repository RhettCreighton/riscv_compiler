#include "riscv_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example: How to develop RISC-V programs for zkVM with 10MB constraints

// Forward declarations
int load_program_with_constraints(const char* elf_file, 
                                  riscv_compiler_t** compiler_out,
                                  riscv_program_t** program_out);
void print_memory_analysis(const memory_analysis_t* analysis);
void suggest_memory_optimizations(const memory_analysis_t* analysis);

// Example 1: Memory-efficient Fibonacci
void compile_fibonacci_example(void) {
    printf("\n");
    printf("=============================================================\n");
    printf("Example 1: Memory-Efficient Fibonacci\n");
    printf("=============================================================\n");
    
    // Create a small program that fits easily
    const char* fib_code = 
        ".text\n"
        ".globl _start\n"
        "_start:\n"
        "    li t0, 0        # First Fibonacci number\n"
        "    li t1, 1        # Second Fibonacci number\n"
        "    li t2, 100      # Calculate 100 numbers\n"
        "    li t3, 0        # Counter\n"
        "loop:\n"
        "    add t4, t0, t1  # Next = current + previous\n"
        "    mv t0, t1       # Shift values\n"
        "    mv t1, t4\n"
        "    addi t3, t3, 1  # Increment counter\n"
        "    blt t3, t2, loop # Continue if counter < limit\n"
        "    mv a0, t1       # Return result in a0\n"
        "    ecall           # Exit\n";
    
    printf("\nProgram characteristics:\n");
    printf("  • Code size: ~64 bytes (16 instructions)\n");
    printf("  • Data size: 0 bytes (no static data)\n");
    printf("  • Stack usage: Minimal (no function calls)\n");
    printf("  • Heap usage: None\n");
    printf("  • Total memory: <1 KB\n");
    printf("\n✅ This program easily fits in the zkVM!\n");
    
    // Show how to compile it
    printf("\nTo compile this for zkVM:\n");
    printf("  1. Save as fibonacci.s\n");
    printf("  2. Assemble: riscv32-gcc -nostdlib fibonacci.s -o fibonacci\n");
    printf("  3. Load with constraints:\n");
    printf("     load_program_with_constraints(\"fibonacci\", &compiler, &program)\n");
}

// Example 2: Working with larger data
void compile_sorting_example(void) {
    printf("\n");
    printf("=============================================================\n");
    printf("Example 2: Sorting with Memory Constraints\n");
    printf("=============================================================\n");
    
    printf("\nScenario: Sort an array of integers\n");
    
    // Calculate memory requirements for different array sizes
    struct {
        size_t array_size;
        size_t memory_needed;
        bool fits;
    } scenarios[] = {
        {1000, 1000 * 4 + 1024, true},        // 4KB + overhead
        {100000, 100000 * 4 + 1024, true},    // 400KB
        {1000000, 1000000 * 4 + 1024, true},  // 4MB
        {2500000, 2500000 * 4 + 1024, false}, // 10MB - exceeds limit
    };
    
    printf("\nMemory requirements for different array sizes:\n");
    printf("Array Size    Memory Needed    Fits in zkVM?\n");
    printf("----------    -------------    -------------\n");
    
    for (size_t i = 0; i < sizeof(scenarios)/sizeof(scenarios[0]); i++) {
        printf("%10zu    %8.2f MB         %s\n",
               scenarios[i].array_size,
               scenarios[i].memory_needed / (1024.0 * 1024.0),
               scenarios[i].fits ? "✅ Yes" : "❌ No");
    }
    
    printf("\nOptimization strategies for large arrays:\n");
    printf("  1. Use in-place sorting (no extra memory)\n");
    printf("  2. Process in chunks that fit in zkVM\n");
    printf("  3. Use external memory with Merkle proofs\n");
    
    // Show chunked approach
    printf("\nExample: Chunked sorting approach\n");
    printf("```c\n");
    printf("// Sort 10M elements by processing 1M at a time\n");
    printf("#define CHUNK_SIZE 1000000\n");
    printf("#define TOTAL_SIZE 10000000\n");
    printf("\n");
    printf("for (int chunk = 0; chunk < TOTAL_SIZE; chunk += CHUNK_SIZE) {\n");
    printf("    // Load chunk into zkVM memory\n");
    printf("    load_chunk(data + chunk, CHUNK_SIZE);\n");
    printf("    \n");
    printf("    // Sort this chunk\n");
    printf("    quicksort(chunk_data, CHUNK_SIZE);\n");
    printf("    \n");
    printf("    // Generate proof for this chunk\n");
    printf("    generate_proof();\n");
    printf("}\n");
    printf("\n");
    printf("// Final merge pass with streaming\n");
    printf("merge_sorted_chunks();\n");
    printf("```\n");
}

// Example 3: Memory layout visualization
void show_memory_layout_example(void) {
    printf("\n");
    printf("=============================================================\n");
    printf("Example 3: Understanding zkVM Memory Layout\n");
    printf("=============================================================\n");
    
    printf("\nzkVM Memory Constraints:\n");
    printf("  • Total input limit:  10 MB (83,886,080 bits)\n");
    printf("  • Total output limit: 10 MB (83,886,080 bits)\n");
    printf("  • Fixed overhead:     ~1 KB (PC + 32 registers)\n");
    printf("  • Available memory:   ~9.99 MB\n");
    
    printf("\nTypical memory layout:\n");
    printf("\n");
    printf("  ┌─────────────────┐ 0x00000000\n");
    printf("  │   Code (.text)  │ <- Program instructions\n");
    printf("  ├─────────────────┤ 0x00010000 (example)\n");
    printf("  │   Data (.data)  │ <- Initialized globals\n");
    printf("  ├─────────────────┤ 0x00020000\n");
    printf("  │   BSS (.bss)    │ <- Uninitialized globals\n");
    printf("  ├─────────────────┤ 0x00030000\n");
    printf("  │                 │\n");
    printf("  │      Heap       │ <- Dynamic allocation (grows up)\n");
    printf("  │        ↓        │\n");
    printf("  ├ ─ ─ ─ ─ ─ ─ ─ ─┤\n");
    printf("  │   (free space)  │\n");
    printf("  ├ ─ ─ ─ ─ ─ ─ ─ ─┤\n");
    printf("  │        ↑        │\n");
    printf("  │      Stack      │ <- Function calls (grows down)\n");
    printf("  │                 │\n");
    printf("  └─────────────────┘ 0x00A00000 (~10MB limit)\n");
    
    printf("\nBest practices:\n");
    printf("  • Keep code size minimal (avoid large libraries)\n");
    printf("  • Minimize static data allocation\n");
    printf("  • Use stack allocation when possible\n");
    printf("  • Free heap memory promptly\n");
    printf("  • Monitor total memory usage\n");
}

// Example 4: Error handling
void show_error_handling_example(void) {
    printf("\n");
    printf("=============================================================\n");
    printf("Example 4: Handling Memory Constraint Errors\n");
    printf("=============================================================\n");
    
    printf("\nWhen your program exceeds memory limits:\n");
    printf("\n");
    printf("❌ ERROR: Program exceeds zkVM memory constraints\n");
    printf("\n");
    printf("Program requires 12.5 MB of memory, but zkVM limit is 10.0 MB\n");
    printf("  Code:  0.1 MB\n");
    printf("  Data:  2.0 MB\n");
    printf("  Heap:  8.0 MB  ← Main issue\n");
    printf("  Stack: 2.4 MB\n");
    printf("  Total: 12.5 MB\n");
    printf("\n");
    printf("Suggestions to reduce memory usage:\n");
    printf("  • Reduce heap allocation (current: 8.0 MB)\n");
    printf("  • Optimize data structures\n");
    printf("  • Use smaller stack size\n");
    printf("  • Split program into smaller chunks\n");
    
    printf("\n\nSolution approaches:\n");
    
    printf("\n1. Reduce memory allocation:\n");
    printf("   ```c\n");
    printf("   // Instead of:\n");
    printf("   int* huge_array = malloc(8 * 1024 * 1024);\n");
    printf("   \n");
    printf("   // Use:\n");
    printf("   int* smaller_array = malloc(4 * 1024 * 1024);\n");
    printf("   // Process in two batches\n");
    printf("   ```\n");
    
    printf("\n2. Use memory-mapped approach:\n");
    printf("   ```c\n");
    printf("   // Process data in windows\n");
    printf("   #define WINDOW_SIZE (1024 * 1024)  // 1MB windows\n");
    printf("   for (size_t offset = 0; offset < total_size; offset += WINDOW_SIZE) {\n");
    printf("       process_window(data + offset, WINDOW_SIZE);\n");
    printf("   }\n");
    printf("   ```\n");
    
    printf("\n3. Implement checkpointing:\n");
    printf("   ```c\n");
    printf("   // Save state between proof segments\n");
    printf("   checkpoint_t checkpoint;\n");
    printf("   save_state(&checkpoint);\n");
    printf("   generate_proof_segment_1();\n");
    printf("   \n");
    printf("   restore_state(&checkpoint);\n");
    printf("   generate_proof_segment_2();\n");
    printf("   ```\n");
}

// Main example runner
int main(void) {
    printf("RISC-V zkVM Memory-Aware Programming Guide\n");
    printf("==========================================\n");
    printf("\n");
    printf("The zkVM has a 10MB limit for combined input and output.\n");
    printf("This guide shows how to work effectively within this constraint.\n");
    
    // Run all examples
    compile_fibonacci_example();
    compile_sorting_example();
    show_memory_layout_example();
    show_error_handling_example();
    
    printf("\n");
    printf("=============================================================\n");
    printf("Summary: Key Takeaways\n");
    printf("=============================================================\n");
    printf("\n");
    printf("1. **Always check memory requirements** before compilation\n");
    printf("2. **Design with constraints in mind** from the start\n");
    printf("3. **Use chunking** for large data processing\n");
    printf("4. **Monitor memory usage** during development\n");
    printf("5. **Optimize aggressively** when approaching limits\n");
    printf("\n");
    printf("The 10MB limit is not a bug, it's a feature that ensures\n");
    printf("efficient proof generation and verification!\n");
    printf("\n");
    
    return 0;
}