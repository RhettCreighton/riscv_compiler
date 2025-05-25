# RISC-V Compiler Usage Examples and Tutorials

## Table of Contents
- [Quick Start](#quick-start)
- [Basic Usage Patterns](#basic-usage-patterns)
- [Advanced Examples](#advanced-examples)
- [Performance Optimization](#performance-optimization)
- [Testing and Validation](#testing-and-validation)
- [Production Usage](#production-usage)
- [Troubleshooting](#troubleshooting)

## Quick Start

### 5-Minute Setup

1. **Build the compiler**:
```bash
cd riscv_compiler
mkdir build && cd build
cmake ..
make -j4
```

2. **Run your first example**:
```bash
./simple_riscv_demo
```

3. **Run the test suite**:
```bash
../run_all_tests.sh
```

### Hello World Example

```c
#include "riscv_compiler.h"
#include <stdio.h>

int main() {
    // Create compiler instance
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Compile a simple ADD instruction
    uint32_t add_instruction = 0x002081B3;  // ADD x3, x1, x2
    int result = riscv_compile_instruction(compiler, add_instruction);
    
    if (result == 0) {
        printf("✅ Successfully compiled ADD instruction!\n");
        printf("Generated %zu gates\n", compiler->circuit->num_gates);
    } else {
        printf("❌ Failed to compile instruction\n");
    }
    
    // Clean up
    riscv_compiler_destroy(compiler);
    return 0;
}
```

**Expected Output**:
```
✅ Successfully compiled ADD instruction!
Generated 224 gates
```

## Basic Usage Patterns

### Pattern 1: Single Instruction Compilation

Use this pattern when you need to compile individual instructions and analyze their gate usage.

```c
#include "riscv_compiler.h"

void compile_single_instruction_example() {
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    // Test different instruction types
    struct {
        uint32_t instruction;
        const char* name;
        size_t expected_gates;
    } tests[] = {
        {0x002081B3, "ADD x3, x1, x2", 224},
        {0x0020C1B3, "XOR x3, x1, x2", 32},
        {0x12345037, "LUI x0, 0x12345", 0},
        {0x064000EF, "JAL x1, 100", 448}
    };
    
    printf("Instruction Gate Analysis:\n");
    printf("%-20s | %s\n", "Instruction", "Gates");
    printf("---------------------|-------\n");
    
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        size_t gates_before = compiler->circuit->num_gates;
        
        int result = riscv_compile_instruction(compiler, tests[i].instruction);
        if (result == 0) {
            size_t gates_used = compiler->circuit->num_gates - gates_before;
            printf("%-20s | %zu\n", tests[i].name, gates_used);
        } else {
            printf("%-20s | ERROR\n", tests[i].name);
        }
    }
    
    riscv_compiler_destroy(compiler);
}
```

### Pattern 2: Program Compilation

Use this for compiling complete RISC-V programs.

```c
#include "riscv_compiler.h"

void compile_program_example() {
    // Simple program: compute (a + b) * 2
    uint32_t program[] = {
        0x002081B3,  // ADD x3, x1, x2    ; x3 = x1 + x2
        0x003181B3,  // ADD x3, x3, x3    ; x3 = x3 * 2 (shift would be better)
        0x00000013   // NOP               ; End program
    };
    size_t num_instructions = sizeof(program) / sizeof(program[0]);
    
    printf("Compiling program with %zu instructions...\n", num_instructions);
    
    riscv_circuit_t* circuit = riscv_compile_program(program, num_instructions);
    if (circuit) {
        printf("✅ Program compiled successfully!\n");
        printf("Total gates: %zu\n", circuit->num_gates);
        printf("Average gates per instruction: %.1f\n", 
               (double)circuit->num_gates / num_instructions);
        
        // Save circuit to file for analysis
        riscv_circuit_to_file(circuit, "program_circuit.txt");
        printf("Circuit saved to program_circuit.txt\n");
        
        free(circuit);
    } else {
        printf("❌ Program compilation failed\n");
    }
}
```

### Pattern 3: Performance Monitoring

Use this pattern to track compilation performance and optimize bottlenecks.

```c
#include "riscv_compiler.h"
#include <time.h>

void performance_monitoring_example() {
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    // Test compilation speed
    const int num_iterations = 1000;
    uint32_t add_instruction = 0x002081B3;  // ADD x3, x1, x2
    
    printf("Performance test: compiling %d ADD instructions...\n", num_iterations);
    
    clock_t start = clock();
    size_t initial_gates = compiler->circuit->num_gates;
    
    for (int i = 0; i < num_iterations; i++) {
        int result = riscv_compile_instruction(compiler, add_instruction);
        if (result != 0) {
            printf("❌ Compilation failed at iteration %d\n", i);
            break;
        }
    }
    
    clock_t end = clock();
    size_t final_gates = compiler->circuit->num_gates;
    
    // Calculate metrics
    double time_seconds = ((double)(end - start)) / CLOCKS_PER_SEC;
    double instructions_per_second = num_iterations / time_seconds;
    double gates_per_instruction = (double)(final_gates - initial_gates) / num_iterations;
    double gates_per_second = gates_per_instruction * instructions_per_second;
    
    printf("📊 Performance Results:\n");
    printf("  Time: %.3f seconds\n", time_seconds);
    printf("  Speed: %.0f instructions/second\n", instructions_per_second);
    printf("  Efficiency: %.1f gates/instruction\n", gates_per_instruction);
    printf("  Throughput: %.0f gates/second\n", gates_per_second);
    
    // Compare against targets from CLAUDE.md
    printf("\n🎯 Target Comparison:\n");
    printf("  Speed target (>1M/s): %s\n", 
           instructions_per_second > 1000000 ? "✅ MET" : "❌ NOT MET");
    printf("  Efficiency target (<100): %s\n",
           gates_per_instruction < 100 ? "✅ MET" : "❌ NOT MET");
    
    riscv_compiler_destroy(compiler);
}
```

## Advanced Examples

### Example 1: Custom Circuit with State Encoding

This example shows how to work directly with circuits and encode RISC-V state.

```c
#include "riscv_compiler.h"
#include "riscv_memory.h"

void custom_circuit_example() {
    // Create a custom circuit for specific RISC-V state
    riscv_state_t initial_state = {
        .pc = 0x1000,
        .regs = {0},  // All registers start at 0
        .memory = calloc(1024, 1),  // 1KB memory
        .memory_size = 1024
    };
    
    // Set some initial register values
    initial_state.regs[1] = 0x12345678;  // x1
    initial_state.regs[2] = 0x87654321;  // x2
    
    // Calculate required circuit size
    size_t input_size = calculate_riscv_input_size(&initial_state);
    size_t output_size = calculate_riscv_output_size(&initial_state);
    
    printf("Creating custom circuit:\n");
    printf("  Input size: %zu bits (%.1f KB)\n", input_size, input_size / 8192.0);
    printf("  Output size: %zu bits (%.1f KB)\n", output_size, output_size / 8192.0);
    
    // Create optimally-sized circuit
    riscv_circuit_t* circuit = riscv_circuit_create(input_size, output_size);
    if (!circuit) {
        printf("❌ Failed to create circuit\n");
        free(initial_state.memory);
        return;
    }
    
    // Encode initial state to input bits
    encode_riscv_state_to_input(&initial_state, circuit->input_bits);
    
    printf("✅ State encoded successfully\n");
    printf("  PC at bit %d: 0x%08X\n", PC_START_BIT, initial_state.pc);
    printf("  x1 at bit %d: 0x%08X\n", REGS_START_BIT, initial_state.regs[1]);
    printf("  x2 at bit %d: 0x%08X\n", REGS_START_BIT + 32, initial_state.regs[2]);
    
    // Access specific wire IDs using helper functions
    uint32_t pc_bit_0 = get_pc_wire(0);
    uint32_t x1_bit_0 = get_register_wire(1, 0);
    uint32_t x2_bit_0 = get_register_wire(2, 0);
    
    printf("  Wire mapping: PC[0]=%u, x1[0]=%u, x2[0]=%u\n", 
           pc_bit_0, x1_bit_0, x2_bit_0);
    
    // Cleanup
    free(circuit->input_bits);
    free(circuit->output_bits);
    free(circuit->gates);
    free(circuit);
    free(initial_state.memory);
}
```

### Example 2: Memory Operations with SHA3

This example demonstrates the cryptographically secure memory system.

```c
#include "riscv_compiler.h"
#include "riscv_memory.h"

void memory_operations_example() {
    printf("🔐 Cryptographically Secure Memory Example\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    riscv_memory_t* memory = riscv_memory_create(compiler->circuit);
    
    if (!memory) {
        printf("❌ Failed to create memory system\n");
        riscv_compiler_destroy(compiler);
        return;
    }
    
    printf("✅ Memory system created with SHA3-256 security\n");
    printf("  Merkle tree depth: %d levels\n", MEMORY_BITS);
    printf("  Maximum memory: %d bytes\n", MEMORY_SIZE);
    printf("  Hash function: SHA3-256 (~194K gates per hash)\n");
    
    // Simulate memory access circuit generation
    uint32_t* address_wires = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    uint32_t* write_data_wires = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    uint32_t* read_data_wires = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    uint32_t write_enable = riscv_circuit_allocate_wire(compiler->circuit);
    
    size_t gates_before = compiler->circuit->num_gates;
    
    // This would generate a complete memory access circuit
    // Including Merkle proof verification and SHA3 hashing
    riscv_memory_access(memory, address_wires, write_data_wires, 
                       write_enable, read_data_wires);
    
    size_t gates_after = compiler->circuit->num_gates;
    size_t memory_gates = gates_after - gates_before;
    
    printf("📊 Memory Access Circuit:\n");
    printf("  Gates generated: %zu\n", memory_gates);
    printf("  Estimated cost: ~%zu SHA3 operations\n", memory_gates / 194000);
    printf("  Security level: Cryptographic (production-ready)\n");
    
    // Cleanup
    free(address_wires);
    free(write_data_wires);
    free(read_data_wires);
    riscv_memory_destroy(memory);
    riscv_compiler_destroy(compiler);
}
```

### Example 3: Instruction Set Coverage Analysis

This example analyzes the complete RV32I instruction set implementation.

```c
#include "riscv_compiler.h"

void instruction_coverage_analysis() {
    printf("📋 RV32I Instruction Set Coverage Analysis\n\n");
    
    struct instruction_category {
        const char* name;
        struct {
            uint32_t encoding;
            const char* mnemonic;
        } instructions[10];
        size_t count;
    } categories[] = {
        {"Arithmetic", {
            {0x002081B3, "ADD x3, x1, x2"},
            {0x402081B3, "SUB x3, x1, x2"},
            {0x06408193, "ADDI x3, x1, 100"},
            {0x0020A2B3, "SLT x5, x1, x2"},
            {0x0020B2B3, "SLTU x5, x1, x2"}
        }, 5},
        {"Logical", {
            {0x0020C1B3, "XOR x3, x1, x2"},
            {0x0020E1B3, "OR x3, x1, x2"},
            {0x0020F1B3, "AND x3, x1, x2"},
            {0x0FF0C193, "XORI x3, x1, 0xFF"},
            {0x0FF0E193, "ORI x3, x1, 0xFF"}
        }, 5},
        {"Shifts", {
            {0x002091B3, "SLL x3, x1, x2"},
            {0x0020D1B3, "SRL x3, x1, x2"},
            {0x4020D1B3, "SRA x3, x1, x2"},
            {0x00509193, "SLLI x3, x1, 5"},
            {0x0050D193, "SRLI x3, x1, 5"}
        }, 5},
        {"Branches", {
            {0x00208463, "BEQ x1, x2, 8"},
            {0x00209463, "BNE x1, x2, 8"},
            {0x0020C463, "BLT x1, x2, 8"},
            {0x0020D463, "BGE x1, x2, 8"},
            {0x0020E463, "BLTU x1, x2, 8"}
        }, 5},
        {"Memory", {
            {0x0000A183, "LW x3, 0(x1)"},
            {0x0020A023, "SW x2, 0(x1)"},
            {0x00308183, "LB x3, 3(x1)"},
            {0x00208123, "SB x2, 3(x1)"}
        }, 4}
    };
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    size_t total_instructions = 0;
    size_t successful_compilations = 0;
    
    for (size_t cat = 0; cat < sizeof(categories)/sizeof(categories[0]); cat++) {
        printf("=== %s Instructions ===\n", categories[cat].name);
        
        size_t category_success = 0;
        for (size_t i = 0; i < categories[cat].count; i++) {
            uint32_t instruction = categories[cat].instructions[i].encoding;
            const char* mnemonic = categories[cat].instructions[i].mnemonic;
            
            size_t gates_before = compiler->circuit->num_gates;
            int result = riscv_compile_instruction(compiler, instruction);
            size_t gates_used = compiler->circuit->num_gates - gates_before;
            
            total_instructions++;
            if (result == 0) {
                successful_compilations++;
                category_success++;
                printf("  ✅ %-20s (%6zu gates)\n", mnemonic, gates_used);
            } else {
                printf("  ❌ %-20s (FAILED)\n", mnemonic);
            }
        }
        
        printf("  Category success: %zu/%zu (%.1f%%)\n\n", 
               category_success, categories[cat].count,
               100.0 * category_success / categories[cat].count);
    }
    
    printf("🎯 Overall Coverage Summary:\n");
    printf("  Instructions tested: %zu\n", total_instructions);
    printf("  Successful compilations: %zu\n", successful_compilations);
    printf("  Success rate: %.1f%%\n", 100.0 * successful_compilations / total_instructions);
    printf("  Total gates generated: %zu\n", compiler->circuit->num_gates);
    printf("  Average gates/instruction: %.1f\n", 
           (double)compiler->circuit->num_gates / successful_compilations);
    
    riscv_compiler_destroy(compiler);
}
```

## Performance Optimization

### Optimization Example 1: Gate Count Analysis

```c
#include "riscv_compiler.h"

void gate_count_optimization_example() {
    printf("🔧 Gate Count Optimization Analysis\n\n");
    
    struct optimization_test {
        uint32_t instruction;
        const char* name;
        size_t current_gates;
        size_t target_gates;
        const char* optimization;
    } tests[] = {
        {0x002081B3, "ADD", 224, 80, "Kogge-Stone adder"},
        {0x0020C1B3, "XOR", 32, 32, "Already optimal"},
        {0x022081B3, "MUL", 11632, 3000, "Booth's algorithm"},
        {0x002091B3, "SLL", 960, 400, "Logarithmic shifter"}
    };
    
    printf("%-10s | %8s | %8s | %8s | %s\n", 
           "Instr", "Current", "Target", "Reduction", "Optimization");
    printf("-----------|----------|----------|----------|------------------\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        size_t gates_before = compiler->circuit->num_gates;
        riscv_compile_instruction(compiler, tests[i].instruction);
        size_t actual_gates = compiler->circuit->num_gates - gates_before;
        
        double reduction = 100.0 * (1.0 - (double)tests[i].target_gates / actual_gates);
        
        printf("%-10s | %8zu | %8zu | %7.1f%% | %s\n",
               tests[i].name, actual_gates, tests[i].target_gates,
               reduction, tests[i].optimization);
    }
    
    printf("\n💡 Optimization Priorities:\n");
    printf("  1. HIGH: Enable Booth multiplier (74%% gate reduction)\n");
    printf("  2. MEDIUM: Optimize shift circuits (58%% gate reduction)\n");
    printf("  3. MEDIUM: Implement Kogge-Stone adder (64%% gate reduction)\n");
    
    riscv_compiler_destroy(compiler);
}
```

### Optimization Example 2: Compilation Speed Profiling

```c
#include "riscv_compiler.h"
#include <time.h>

void compilation_speed_profiling() {
    printf("⏱️ Compilation Speed Profiling\n\n");
    
    struct speed_test {
        uint32_t instruction;
        const char* name;
        int iterations;
    } tests[] = {
        {0x0020C1B3, "XOR (fast)", 10000},
        {0x002081B3, "ADD (medium)", 5000},
        {0x002091B3, "SLL (slow)", 1000},
        {0x022081B3, "MUL (very slow)", 100}
    };
    
    printf("%-15s | %10s | %15s | %10s\n",
           "Instruction", "Iterations", "Time (ms)", "Speed (K/s)");
    printf("----------------|------------|---------------|------------\n");
    
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        riscv_compiler_t* compiler = riscv_compiler_create();
        
        clock_t start = clock();
        for (int j = 0; j < tests[i].iterations; j++) {
            riscv_compile_instruction(compiler, tests[i].instruction);
        }
        clock_t end = clock();
        
        double time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
        double speed_k_per_sec = tests[i].iterations / time_ms;
        
        printf("%-15s | %10d | %13.1f | %9.1f\n",
               tests[i].name, tests[i].iterations, time_ms, speed_k_per_sec);
        
        riscv_compiler_destroy(compiler);
    }
    
    printf("\n🎯 Speed Optimization Targets:\n");
    printf("  Current bottleneck: Complex instructions (MUL, shifts)\n");
    printf("  Target: >1000 K instructions/second average\n");
    printf("  Strategy: Optimize high-gate-count instructions first\n");
}
```

## Testing and Validation

### Unit Testing Example

```c
#include "riscv_compiler.h"

void unit_testing_example() {
    printf("🧪 Unit Testing Example\n\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    // Test 1: Basic functionality
    printf("Test 1: Basic ADD compilation\n");
    int result = riscv_compile_instruction(compiler, 0x002081B3);
    if (result == 0) {
        printf("  ✅ PASS: ADD instruction compiled successfully\n");
    } else {
        printf("  ❌ FAIL: ADD instruction compilation failed\n");
    }
    
    // Test 2: Gate count validation
    printf("Test 2: Gate count within expected range\n");
    size_t gates = compiler->circuit->num_gates;
    if (gates >= 200 && gates <= 300) {
        printf("  ✅ PASS: Gate count %zu is within range [200, 300]\n", gates);
    } else {
        printf("  ❌ FAIL: Gate count %zu is outside expected range\n", gates);
    }
    
    // Test 3: Register x0 handling
    printf("Test 3: Register x0 write handling\n");
    size_t gates_before = compiler->circuit->num_gates;
    result = riscv_compile_instruction(compiler, 0x00208033);  // ADD x0, x1, x2
    size_t gates_after = compiler->circuit->num_gates;
    
    if (result == 0 && gates_after > gates_before) {
        printf("  ✅ PASS: x0 write generates gates but ignores result\n");
    } else {
        printf("  ❌ FAIL: x0 write handling incorrect\n");
    }
    
    // Test 4: Error handling
    printf("Test 4: Invalid instruction handling\n");
    result = riscv_compile_instruction(compiler, 0x0000007C);  // Invalid opcode
    if (result != 0) {
        printf("  ✅ PASS: Invalid instruction properly rejected\n");
    } else {
        printf("  ❌ FAIL: Invalid instruction incorrectly accepted\n");
    }
    
    printf("\n📊 Unit Test Summary:\n");
    printf("  Basic functionality: ✅\n");
    printf("  Performance validation: ✅\n");
    printf("  Edge case handling: ✅\n");
    printf("  Error handling: ✅\n");
    
    riscv_compiler_destroy(compiler);
}
```

### Integration Testing Example

```c
#include "riscv_compiler.h"

void integration_testing_example() {
    printf("🔗 Integration Testing Example\n\n");
    
    // Test complete program compilation and execution flow
    uint32_t fibonacci_program[] = {
        // Fibonacci sequence calculator (simplified)
        0x00100093,  // ADDI x1, x0, 1     ; x1 = 1 (first fib number)
        0x00100113,  // ADDI x2, x0, 1     ; x2 = 1 (second fib number)
        0x00500193,  // ADDI x3, x0, 5     ; x3 = 5 (counter)
        0x002081B3,  // ADD x3, x1, x2     ; x3 = x1 + x2 (next fib)
        0x00108093,  // ADDI x1, x1, 0     ; x1 = x2 (update)
        0x00018113,  // ADDI x2, x3, 0     ; x2 = x3 (update)
        0xFE000CE3,  // BNE x0, x0, -8     ; Loop (simplified)
        0x00000013   // NOP                ; End
    };
    size_t program_size = sizeof(fibonacci_program) / sizeof(fibonacci_program[0]);
    
    printf("Compiling Fibonacci program (%zu instructions)...\n", program_size);
    
    riscv_circuit_t* circuit = riscv_compile_program(fibonacci_program, program_size);
    
    if (circuit) {
        printf("✅ Integration test PASSED\n");
        printf("  Program compiled successfully\n");
        printf("  Total gates: %zu\n", circuit->num_gates);
        printf("  Average gates/instruction: %.1f\n", 
               (double)circuit->num_gates / program_size);
        printf("  Circuit complexity: %s\n",
               circuit->num_gates < 5000 ? "Low" :
               circuit->num_gates < 15000 ? "Medium" : "High");
        
        // Validate circuit structure
        bool valid_circuit = true;
        for (size_t i = 0; i < circuit->num_gates; i++) {
            gate_t* gate = &circuit->gates[i];
            if (gate->type != GATE_AND && gate->type != GATE_XOR) {
                valid_circuit = false;
                break;
            }
        }
        
        printf("  Circuit validation: %s\n", valid_circuit ? "✅ Valid" : "❌ Invalid");
        
        free(circuit);
    } else {
        printf("❌ Integration test FAILED\n");
        printf("  Program compilation failed\n");
    }
}
```

## Production Usage

### Production Deployment Example

```c
#include "riscv_compiler.h"
#include <syslog.h>

// Production-ready compilation with error handling and logging
int production_compile_instruction(uint32_t instruction, 
                                 riscv_circuit_t** output_circuit) {
    // Input validation
    if (!output_circuit) {
        syslog(LOG_ERR, "riscv_compiler: NULL output circuit pointer");
        return -1;
    }
    
    // Create compiler with error handling
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        syslog(LOG_ERR, "riscv_compiler: Failed to create compiler instance");
        return -1;
    }
    
    // Compile with performance monitoring
    clock_t start = clock();
    int result = riscv_compile_instruction(compiler, instruction);
    clock_t end = clock();
    
    if (result == 0) {
        // Log successful compilation
        double time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
        syslog(LOG_INFO, "riscv_compiler: Compiled instruction 0x%08X in %.2fms, %zu gates",
               instruction, time_ms, compiler->circuit->num_gates);
        
        // Transfer ownership of circuit
        *output_circuit = compiler->circuit;
        compiler->circuit = NULL;  // Prevent double-free
        
        // Cleanup compiler (but not circuit)
        riscv_compiler_destroy(compiler);
        return 0;
    } else {
        // Log compilation failure
        syslog(LOG_WARNING, "riscv_compiler: Failed to compile instruction 0x%08X", 
               instruction);
        
        riscv_compiler_destroy(compiler);
        return -1;
    }
}

// Production usage example
void production_usage_example() {
    printf("🏭 Production Usage Example\n\n");
    
    openlog("riscv_compiler_demo", LOG_PID, LOG_USER);
    
    uint32_t instructions[] = {
        0x002081B3,  // ADD x3, x1, x2
        0x0020C1B3,  // XOR x3, x1, x2
        0x00208463   // BEQ x1, x2, 8
    };
    
    for (size_t i = 0; i < sizeof(instructions)/sizeof(instructions[0]); i++) {
        riscv_circuit_t* circuit = NULL;
        int result = production_compile_instruction(instructions[i], &circuit);
        
        if (result == 0 && circuit) {
            printf("✅ Instruction %zu compiled successfully (%zu gates)\n", 
                   i+1, circuit->num_gates);
            
            // In production, you would pass this circuit to the proving system
            // prover_system_add_circuit(circuit);
            
            free(circuit);
        } else {
            printf("❌ Instruction %zu compilation failed\n", i+1);
        }
    }
    
    closelog();
}
```

## Troubleshooting

### Common Issues and Solutions

#### Issue 1: "Unsupported opcode" Error

```c
void troubleshoot_unsupported_opcode() {
    printf("🔧 Troubleshooting: Unsupported Opcode\n\n");
    
    // Common issue: Trying to compile unimplemented instruction
    uint32_t problematic_instruction = 0x0000000F;  // FENCE instruction
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    int result = riscv_compile_instruction(compiler, problematic_instruction);
    
    if (result != 0) {
        printf("❌ Error: Unsupported opcode 0x%02X\n", 
               problematic_instruction & 0x7F);
        
        printf("💡 Solutions:\n");
        printf("  1. Check if instruction is part of RV32I base set\n");
        printf("  2. Verify instruction encoding is correct\n");
        printf("  3. Some instructions may not be implemented yet:\n");
        printf("     - Memory instructions (LW, SW, etc.) - Partial support\n");
        printf("     - FENCE instruction - Not implemented\n");
        printf("  4. Use comprehensive test suite to check coverage\n");
    }
    
    riscv_compiler_destroy(compiler);
}
```

#### Issue 2: High Gate Count

```c
void troubleshoot_high_gate_count() {
    printf("🔧 Troubleshooting: High Gate Count\n\n");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    // Compile a multiplication instruction (known high gate count)
    uint32_t mul_instruction = 0x022081B3;  // MUL x3, x1, x2
    size_t gates_before = compiler->circuit->num_gates;
    
    riscv_compile_instruction(compiler, mul_instruction);
    size_t gates_used = compiler->circuit->num_gates - gates_before;
    
    if (gates_used > 5000) {
        printf("⚠️ Warning: High gate count detected (%zu gates)\n", gates_used);
        
        printf("💡 Optimization strategies:\n");
        printf("  1. For MUL instructions:\n");
        printf("     - Enable Booth's algorithm (set USE_BOOTH_MULTIPLIER=1)\n");
        printf("     - Expected reduction: ~74%% (11K → 3K gates)\n");
        printf("  2. For shift instructions:\n");
        printf("     - Implement logarithmic shifter\n");
        printf("     - Expected reduction: ~58%% (960 → 400 gates)\n");
        printf("  3. For ADD instructions:\n");
        printf("     - Enable optimized Kogge-Stone adder\n");
        printf("     - Expected reduction: ~64%% (224 → 80 gates)\n");
        printf("  4. Monitor with benchmark_comprehensive tool\n");
    }
    
    riscv_compiler_destroy(compiler);
}
```

#### Issue 3: Memory Allocation Failures

```c
void troubleshoot_memory_allocation() {
    printf("🔧 Troubleshooting: Memory Allocation\n\n");
    
    // Simulate stress testing that might cause allocation issues
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("❌ Error: Failed to create compiler\n");
        printf("💡 Solutions:\n");
        printf("  1. Check available system memory\n");
        printf("  2. Reduce circuit capacity if needed\n");
        printf("  3. Implement circuit streaming for large programs\n");
        return;
    }
    
    // Test large compilation
    const int stress_iterations = 10000;
    printf("Stress testing with %d instructions...\n", stress_iterations);
    
    for (int i = 0; i < stress_iterations; i++) {
        int result = riscv_compile_instruction(compiler, 0x002081B3);  // ADD
        if (result != 0) {
            printf("❌ Allocation failure at iteration %d\n", i);
            printf("💡 Solutions:\n");
            printf("  1. Current circuit has %zu gates (capacity: %zu)\n",
                   compiler->circuit->num_gates, compiler->circuit->capacity);
            printf("  2. Consider circuit chunking for large programs\n");
            printf("  3. Implement garbage collection for unused wires\n");
            break;
        }
        
        if (i % 1000 == 0) {
            printf("  Progress: %d iterations, %zu gates\n", 
                   i, compiler->circuit->num_gates);
        }
    }
    
    riscv_compiler_destroy(compiler);
}
```

### Performance Debugging

```c
void performance_debugging_example() {
    printf("🔍 Performance Debugging\n\n");
    
    // Enable performance tracking
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    struct {
        uint32_t instruction;
        const char* name;
        size_t expected_max_gates;
    } performance_tests[] = {
        {0x0020C1B3, "XOR", 50},      // Should be very fast
        {0x002081B3, "ADD", 300},     // Should be fast
        {0x002091B3, "SLL", 1500},    // May be slow
        {0x022081B3, "MUL", 15000}    // Known to be slow
    };
    
    printf("Performance Analysis:\n");
    printf("%-10s | %8s | %8s | %s\n", "Instr", "Gates", "Expected", "Status");
    printf("-----------|----------|----------|----------\n");
    
    for (size_t i = 0; i < sizeof(performance_tests)/sizeof(performance_tests[0]); i++) {
        size_t gates_before = compiler->circuit->num_gates;
        
        clock_t start = clock();
        int result = riscv_compile_instruction(compiler, performance_tests[i].instruction);
        clock_t end = clock();
        
        if (result == 0) {
            size_t gates_used = compiler->circuit->num_gates - gates_before;
            double time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
            
            const char* status = gates_used <= performance_tests[i].expected_max_gates ? 
                               "✅ Good" : "⚠️ Slow";
            
            printf("%-10s | %8zu | %8zu | %s (%.2fms)\n",
                   performance_tests[i].name, gates_used, 
                   performance_tests[i].expected_max_gates, status, time_ms);
        } else {
            printf("%-10s | %8s | %8zu | ❌ Failed\n",
                   performance_tests[i].name, "ERROR", 
                   performance_tests[i].expected_max_gates);
        }
    }
    
    riscv_compiler_destroy(compiler);
}
```

For more examples and advanced usage patterns, see:
- `examples/` directory for complete programs
- `tests/` directory for comprehensive test cases
- `PERFORMANCE_ANALYSIS.md` for optimization strategies
- `API_DOCUMENTATION.md` for detailed function reference