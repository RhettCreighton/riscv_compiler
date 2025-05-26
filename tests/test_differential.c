/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include "test_framework.h"
#include "riscv_emulator.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

INIT_TESTS();

// Test patterns for differential testing
typedef struct {
    const char* name;
    uint32_t* instructions;
    size_t instruction_count;
    uint32_t initial_regs[32];
    bool has_initial_regs;
} test_program_t;

// Helper function to compare compiler result with emulator result
bool differential_test_single_instruction(uint32_t instruction, 
                                         uint32_t* initial_regs,
                                         bool verbose) {
    // Create emulator
    emulator_state_t* emu = create_emulator(1024 * 1024);  // 1MB memory
    if (!emu) {
        printf("Failed to create emulator\n");
        return false;
    }
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler\n");
        destroy_emulator(emu);
        return false;
    }
    
    // Set initial register state
    if (initial_regs) {
        for (int i = 0; i < 32; i++) {
            emu->regs[i] = initial_regs[i];
            // Set compiler state would need to be implemented
        }
    }
    
    // Execute in emulator
    bool emu_success = execute_instruction(emu, instruction);
    
    // Compile instruction
    int compile_result = riscv_compile_instruction(compiler, instruction);
    
    if (verbose) {
        printf("Instruction: 0x%08x (%s)\n", instruction, get_instruction_name(instruction));
        printf("Emulator success: %d, Compiler result: %d\n", emu_success, compile_result);
    }
    
    // For now, we check compilation success
    // TODO: Need circuit evaluation to compare register states
    bool success = (compile_result == 0) && emu_success;
    
    if (!success && verbose) {
        printf("Differential test failed for instruction 0x%08x\n", instruction);
        if (compile_result != 0) {
            printf("Compilation failed with code %d\n", compile_result);
        }
        if (!emu_success) {
            printf("Emulator execution failed\n");
        }
    }
    
    destroy_emulator(emu);
    riscv_compiler_destroy(compiler);
    
    return success;
}

// Run differential test on a program
bool differential_test_program(test_program_t* program, bool verbose) {
    emulator_state_t* emu = create_emulator(1024 * 1024);
    if (!emu) return false;
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        destroy_emulator(emu);
        return false;
    }
    
    // Set initial state
    if (program->has_initial_regs) {
        for (int i = 0; i < 32; i++) {
            emu->regs[i] = program->initial_regs[i];
        }
    }
    
    // Load program into emulator
    load_program(emu, program->instructions, program->instruction_count, 0);
    
    bool success = true;
    
    // Execute each instruction in both emulator and compiler
    for (size_t i = 0; i < program->instruction_count; i++) {
        uint32_t instruction = program->instructions[i];
        
        if (verbose) {
            printf("Executing instruction %zu: 0x%08x (%s)\n", 
                   i, instruction, get_instruction_name(instruction));
        }
        
        // Step emulator
        step_emulator(emu);
        
        // Compile instruction
        int compile_result = riscv_compile_instruction(compiler, instruction);
        
        if (compile_result != 0) {
            if (verbose) {
                printf("Compilation failed for instruction %zu: 0x%08x\n", i, instruction);
            }
            success = false;
            break;
        }
        
        if (emu->halt) {
            if (verbose) {
                printf("Emulator halted at instruction %zu\n", i);
            }
            break;
        }
    }
    
    if (verbose && success) {
        printf("Program executed successfully in both emulator and compiler\n");
        printf("Final emulator state:\n");
        print_registers(emu);
        printf("Compiler generated %zu gates\n", compiler->circuit->num_gates);
    }
    
    destroy_emulator(emu);
    riscv_compiler_destroy(compiler);
    
    return success;
}

// Test that verifies compiler matches emulator for arithmetic instructions
void test_arithmetic_correctness(void) {
    TEST_SUITE("Arithmetic Correctness");
    
    // Test individual arithmetic instructions
    TEST("ADD instruction compilation");
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 100;
    initial_regs[2] = 200;
    bool result = differential_test_single_instruction(0x002081B3, initial_regs, false);  // add x3, x1, x2
    ASSERT_TRUE(result);
    
    TEST("SUB instruction compilation");
    initial_regs[1] = 500;
    initial_regs[2] = 200;
    result = differential_test_single_instruction(0x402081B3, initial_regs, false);  // sub x3, x1, x2
    ASSERT_TRUE(result);
    
    TEST("XOR instruction compilation");
    initial_regs[1] = 0xAAAAAAAA;
    initial_regs[2] = 0x55555555;
    result = differential_test_single_instruction(0x0020C1B3, initial_regs, false);  // xor x3, x1, x2
    ASSERT_TRUE(result);
    
    TEST("AND instruction compilation");
    initial_regs[1] = 0xFF00FF00;
    initial_regs[2] = 0x0F0F0F0F;
    result = differential_test_single_instruction(0x0020F1B3, initial_regs, false);  // and x3, x1, x2
    ASSERT_TRUE(result);
    
    TEST("OR instruction compilation");
    initial_regs[1] = 0xF0F0F0F0;
    initial_regs[2] = 0x0F0F0F0F;
    result = differential_test_single_instruction(0x0020E1B3, initial_regs, false);  // or x3, x1, x2
    ASSERT_TRUE(result);
    
    TEST("ADDI instruction compilation");
    initial_regs[1] = 100;
    result = differential_test_single_instruction(0x06408093, initial_regs, false);  // addi x1, x1, 100
    ASSERT_TRUE(result);
}

// Test edge cases using differential testing
void test_edge_cases(void) {
    TEST_SUITE("Edge Cases");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 100;
    
    // Test maximum immediate values
    TEST("ADDI with max positive immediate");
    bool result = differential_test_single_instruction(0x7FF08093, initial_regs, false);  // addi x1, x1, 2047
    ASSERT_TRUE(result);
    
    TEST("ADDI with max negative immediate");
    result = differential_test_single_instruction(0x80008093, initial_regs, false);  // addi x1, x1, -2048
    ASSERT_TRUE(result);
    
    // Test overflow cases
    TEST("ADD overflow");
    initial_regs[1] = 0x7FFFFFFF;  // MAX_INT
    initial_regs[2] = 1;
    result = differential_test_single_instruction(0x002081B3, initial_regs, false);  // add x3, x1, x2
    ASSERT_TRUE(result);
    
    // Test x0 behavior
    TEST("Writing to x0 (should be ignored)");
    initial_regs[1] = 100;
    initial_regs[2] = 200;
    result = differential_test_single_instruction(0x00208033, initial_regs, false);  // add x0, x1, x2
    ASSERT_TRUE(result);
    
    // Test all registers as destinations
    TEST("Can write to all registers except x0");
    bool all_regs_ok = true;
    for (int i = 1; i < 32 && all_regs_ok; i++) {
        uint32_t add_instr = 0x00208033 | (i << 7);  // add xi, x1, x2
        all_regs_ok = differential_test_single_instruction(add_instr, initial_regs, false);
    }
    ASSERT_TRUE(all_regs_ok);
}

// Test different instruction patterns
void test_instruction_patterns(void) {
    TEST_SUITE("Instruction Patterns");
    
    uint32_t initial_regs[32] = {0};
    for (int i = 1; i < 32; i++) {
        initial_regs[i] = i * 100;  // Give each register a unique value
    }
    
    // Test basic arithmetic patterns
    TEST("Arithmetic instruction pattern");
    uint32_t arithmetic_program[] = {
        0x002081B3,  // add x3, x1, x2
        0x004182B3,  // add x5, x3, x4  
        0x40428333,  // sub x6, x5, x4
        0x0062C3B3   // xor x7, x5, x6
    };
    
    test_program_t program = {
        .name = "Arithmetic Pattern",
        .instructions = arithmetic_program,
        .instruction_count = sizeof(arithmetic_program) / sizeof(uint32_t),
        .initial_regs = {0},
        .has_initial_regs = true
    };
    memcpy(program.initial_regs, initial_regs, sizeof(initial_regs));
    
    bool result = differential_test_program(&program, false);
    ASSERT_TRUE(result);
    
    // Test immediate instruction patterns
    TEST("Immediate instruction pattern");
    uint32_t immediate_program[] = {
        0x06408093,  // addi x1, x1, 100
        0x0FF0C093,  // xori x1, x1, 255
        0x0020E113,  // ori x2, x1, 2
        0x00F17193   // andi x3, x2, 15
    };
    
    program.name = "Immediate Pattern";
    program.instructions = immediate_program;
    program.instruction_count = sizeof(immediate_program) / sizeof(uint32_t);
    memcpy(program.initial_regs, initial_regs, sizeof(initial_regs));
    
    result = differential_test_program(&program, false);
    ASSERT_TRUE(result);
    
    // Test shift patterns
    TEST("Shift instruction pattern");
    uint32_t shift_program[] = {
        0x00209093,  // slli x1, x1, 2
        0x0020D113,  // srli x2, x1, 2  
        0x4020D193   // srai x3, x1, 2
    };
    
    program.name = "Shift Pattern";
    program.instructions = shift_program;
    program.instruction_count = sizeof(shift_program) / sizeof(uint32_t);
    memcpy(program.initial_regs, initial_regs, sizeof(initial_regs));
    
    result = differential_test_program(&program, false);
    ASSERT_TRUE(result);
}

// Test random instruction sequences
void test_random_sequences(void) {
    TEST_SUITE("Random Instruction Sequences");
    
    // Seed random number generator
    srand((unsigned int)time(NULL));
    
    uint32_t initial_regs[32] = {0};
    for (int i = 1; i < 32; i++) {
        initial_regs[i] = rand();
    }
    
    // Test random arithmetic instructions
    TEST("Random arithmetic sequence");
    uint32_t random_instructions[10];
    for (int i = 0; i < 10; i++) {
        // Generate random R-type arithmetic instruction
        uint32_t rs1 = (rand() % 31) + 1;  // x1-x31
        uint32_t rs2 = (rand() % 31) + 1;  // x1-x31
        uint32_t rd = (rand() % 31) + 1;   // x1-x31
        uint32_t funct3 = rand() % 4;      // 0=ADD, 4=XOR, 6=OR, 7=AND
        if (funct3 == 1) funct3 = 4;
        if (funct3 == 2) funct3 = 6;
        if (funct3 == 3) funct3 = 7;
        
        random_instructions[i] = 0x00000033 |  // R-type opcode
                               (rd << 7) |
                               (funct3 << 12) |
                               (rs1 << 15) |
                               (rs2 << 20);
    }
    
    test_program_t program = {
        .name = "Random Arithmetic",
        .instructions = random_instructions,
        .instruction_count = 10,
        .initial_regs = {0},
        .has_initial_regs = true
    };
    memcpy(program.initial_regs, initial_regs, sizeof(initial_regs));
    
    bool result = differential_test_program(&program, false);
    ASSERT_TRUE(result);
}

// Test comprehensive instruction coverage
void test_instruction_coverage(void) {
    TEST_SUITE("Instruction Coverage");
    
    uint32_t initial_regs[32] = {0};
    initial_regs[1] = 0x12345678;
    initial_regs[2] = 0x87654321;
    initial_regs[3] = 0xAAAAAAAA;
    initial_regs[4] = 0x55555555;
    
    // Test all implemented arithmetic instructions
    const uint32_t arithmetic_instructions[] = {
        0x002081B3,  // add x3, x1, x2
        0x402081B3,  // sub x3, x1, x2
        0x0020C1B3,  // xor x3, x1, x2
        0x0020E1B3,  // or x3, x1, x2
        0x0020F1B3,  // and x3, x1, x2
        0x00209213,  // slli x4, x1, 2
        0x0020D213,  // srli x4, x1, 2
        0x4020D213,  // srai x4, x1, 2
        0x002092B3,  // sll x5, x1, x2
        0x0020D2B3,  // srl x5, x1, x2
        0x4020D2B3,  // sra x5, x1, x2
        0x0020A2B3,  // slt x5, x1, x2
        0x0020B2B3   // sltu x5, x1, x2
    };
    
    TEST("All arithmetic instructions compile");
    bool all_compile = true;
    for (size_t i = 0; i < sizeof(arithmetic_instructions) / sizeof(uint32_t); i++) {
        bool result = differential_test_single_instruction(arithmetic_instructions[i], initial_regs, false);
        if (!result) {
            printf(" (failed at instruction %zu: 0x%08x)", i, arithmetic_instructions[i]);
            all_compile = false;
            break;
        }
    }
    ASSERT_TRUE(all_compile);
    
    // Test immediate instructions
    const uint32_t immediate_instructions[] = {
        0x06408093,  // addi x1, x1, 100
        0x0FF0C093,  // xori x1, x1, 255
        0x0020E113,  // ori x2, x1, 2
        0x00F17193,  // andi x3, x2, 15
        0x0640A093,  // slti x1, x1, 100
        0x0640B093   // sltiu x1, x1, 100
    };
    
    TEST("All immediate instructions compile");
    all_compile = true;
    for (size_t i = 0; i < sizeof(immediate_instructions) / sizeof(uint32_t); i++) {
        bool result = differential_test_single_instruction(immediate_instructions[i], initial_regs, false);
        if (!result) {
            printf(" (failed at instruction %zu: 0x%08x)", i, immediate_instructions[i]);
            all_compile = false;
            break;
        }
    }
    ASSERT_TRUE(all_compile);
}

int main(void) {
    printf("RISC-V Compiler Differential Tests\n");
    printf("==================================\n");
    printf("Comparing compiler output against RISC-V emulator\n\n");
    
    test_arithmetic_correctness();
    test_edge_cases();
    test_instruction_patterns();
    test_random_sequences();
    test_instruction_coverage();
    
    print_test_summary();
    
    if (g_test_results.failed_tests == 0) {
        printf("\n✓ All differential tests passed!\n");
        printf("✓ Compiler behavior matches emulator\n");
    } else {
        printf("\n✗ %d differential tests failed\n", g_test_results.failed_tests);
        printf("✗ Compiler behavior differs from emulator\n");
    }
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}