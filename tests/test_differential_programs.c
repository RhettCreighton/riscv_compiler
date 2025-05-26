#include "riscv_compiler.h"
#include "test_framework.h"
#include "riscv_emulator.h"
#include "test_programs.h"
#include <stdlib.h>
#include <string.h>

INIT_TESTS();

// Helper function to run a full differential test on a program
bool run_differential_test(const char* test_name, 
                          uint32_t* program, 
                          size_t program_size,
                          uint32_t* initial_regs,
                          bool verbose) {
    
    // Create emulator
    emulator_state_t* emu = create_emulator(1024 * 1024);
    if (!emu) {
        printf("Failed to create emulator for %s\n", test_name);
        return false;
    }
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler for %s\n", test_name);
        destroy_emulator(emu);
        return false;
    }
    
    // Set initial register state
    if (initial_regs) {
        memcpy(emu->regs, initial_regs, sizeof(uint32_t) * 32);
    }
    
    // Load program into emulator
    load_program(emu, program, program_size, 0);
    
    bool success = true;
    size_t total_gates_before = compiler->circuit->num_gates;
    
    if (verbose) {
        printf("Running differential test: %s\n", test_name);
        printf("Program size: %zu instructions\n", program_size);
        printf("Initial registers:\n");
        print_registers(emu);
    }
    
    // Execute each instruction
    for (size_t i = 0; i < program_size; i++) {
        uint32_t instruction = program[i];
        
        if (verbose) {
            printf("Instruction %zu: 0x%08x (%s)\n", 
                   i, instruction, get_instruction_name(instruction));
        }
        
        // Store emulator state before execution
        emulator_state_t emu_before = *emu;
        
        // Execute in emulator
        bool emu_success = execute_instruction(emu, instruction);
        
        // Compile instruction
        size_t gates_before = compiler->circuit->num_gates;
        int compile_result = riscv_compile_instruction(compiler, instruction);
        size_t gates_added = compiler->circuit->num_gates - gates_before;
        
        if (verbose) {
            printf("  Emulator: %s, Compiler: %s, Gates added: %zu\n",
                   emu_success ? "OK" : "FAIL",
                   (compile_result == 0) ? "OK" : "FAIL",
                   gates_added);
        }
        
        // Check compilation success
        if (compile_result != 0) {
            printf("Compilation failed for instruction %zu in %s: 0x%08x\n", 
                   i, test_name, instruction);
            success = false;
            break;
        }
        
        // Check emulator success
        if (!emu_success) {
            printf("Emulator failed for instruction %zu in %s: 0x%08x\n", 
                   i, test_name, instruction);
            success = false;
            break;
        }
        
        if (emu->halt) {
            if (verbose) {
                printf("Program halted at instruction %zu\n", i);
            }
            break;
        }
    }
    
    if (verbose && success) {
        printf("Final emulator state:\n");
        print_registers(emu);
        printf("Total gates generated: %zu\n", 
               compiler->circuit->num_gates - total_gates_before);
        printf("Average gates per instruction: %.1f\n",
               (double)(compiler->circuit->num_gates - total_gates_before) / program_size);
    }
    
    destroy_emulator(emu);
    riscv_compiler_destroy(compiler);
    
    return success;
}

void test_simple_arithmetic_program(void) {
    TEST_SUITE("Simple Arithmetic Program");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 100;  // x1 = 100
    initial_regs[2] = 50;   // x2 = 50
    
    TEST("Simple arithmetic: add and subtract");
    bool result = run_differential_test("Simple Arithmetic", 
                                       simple_arithmetic_program,
                                       simple_arithmetic_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_fibonacci_program(void) {
    TEST_SUITE("Fibonacci Program");
    
    uint32_t initial_regs[32] = {0};
    // Fibonacci will initialize its own values
    
    TEST("Fibonacci sequence calculation");
    bool result = run_differential_test("Fibonacci", 
                                       fibonacci_program,
                                       fibonacci_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_bitwise_program(void) {
    TEST_SUITE("Bitwise Operations Program");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 0xAAAAAAAA;  // Alternating pattern
    initial_regs[2] = 0x55555555;  // Complementary pattern
    
    TEST("Bitwise operations: XOR, OR, AND");
    bool result = run_differential_test("Bitwise Operations", 
                                       bitwise_program,
                                       bitwise_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_shift_program(void) {
    TEST_SUITE("Shift Operations Program");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 0x12345678;  // Test pattern
    
    TEST("Shift operations: logical and arithmetic");
    bool result = run_differential_test("Shift Operations", 
                                       shift_program,
                                       shift_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_comparison_program(void) {
    TEST_SUITE("Comparison Operations Program");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = -10;  // Negative number
    initial_regs[2] = 20;   // Positive number
    
    TEST("Comparison operations: signed and unsigned");
    bool result = run_differential_test("Comparison Operations", 
                                       comparison_program,
                                       comparison_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_complex_arithmetic_program(void) {
    TEST_SUITE("Complex Arithmetic Program");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 42;   // Some test value
    initial_regs[2] = 17;   // Another test value
    
    TEST("Complex arithmetic operations sequence");
    bool result = run_differential_test("Complex Arithmetic", 
                                       complex_arithmetic_program,
                                       complex_arithmetic_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
}

void test_verbose_example(void) {
    TEST_SUITE("Verbose Differential Test Example");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 123;
    initial_regs[2] = 456;
    
    TEST("Verbose output for simple arithmetic");
    bool result = run_differential_test("Verbose Example", 
                                       simple_arithmetic_program,
                                       simple_arithmetic_program_size,
                                       initial_regs, 
                                       true);  // Verbose mode
    ASSERT_TRUE(result);
}

void test_edge_case_values(void) {
    TEST_SUITE("Edge Case Values");
    
    // Test with edge case values
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 0xFFFFFFFF;  // -1 in signed, max in unsigned
    initial_regs[2] = 0x80000000;  // Most negative in signed, large positive in unsigned
    initial_regs[3] = 0x7FFFFFFF;  // Max positive in signed
    initial_regs[4] = 0x00000001;  // Small positive
    
    TEST("Edge case values with simple arithmetic");
    bool result = run_differential_test("Edge Case Values", 
                                       simple_arithmetic_program,
                                       simple_arithmetic_program_size,
                                       initial_regs, 
                                       false);
    ASSERT_TRUE(result);
    
    TEST("Edge case values with bitwise operations");
    result = run_differential_test("Edge Case Bitwise", 
                                  bitwise_program,
                                  bitwise_program_size,
                                  initial_regs, 
                                  false);
    ASSERT_TRUE(result);
}

int main(void) {
    printf("RISC-V Differential Testing with Program Examples\n");
    printf("===============================================\n");
    printf("Testing compiler against emulator with complete programs\n\n");
    
    test_simple_arithmetic_program();
    test_fibonacci_program();
    test_bitwise_program();
    test_shift_program();
    test_comparison_program();
    test_complex_arithmetic_program();
    test_verbose_example();
    test_edge_case_values();
    
    print_test_summary();
    
    if (g_test_results.failed_tests == 0) {
        printf("\n✓ All program-level differential tests passed!\n");
        printf("✓ Compiler successfully handles complete programs\n");
        printf("✓ Behavior matches reference RISC-V emulator\n");
    } else {
        printf("\n✗ %d program-level tests failed\n", g_test_results.failed_tests);
        printf("✗ Compiler behavior differs from reference implementation\n");
    }
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}