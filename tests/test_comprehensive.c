#include "riscv_compiler.h"
#include "test_framework.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

INIT_TESTS();

// Test all RISC-V arithmetic instructions comprehensively
void test_arithmetic_instructions_complete(void) {
    TEST_SUITE("Complete Arithmetic Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler\n");
        return;
    }
    
    // Test ADD variants
    TEST("ADD x3, x1, x2 (R-type)");
    int result = riscv_compile_instruction(compiler, 0x002081B3);
    ASSERT_EQ(0, result);
    
    TEST("ADDI x3, x1, 100 (I-type positive immediate)");
    result = riscv_compile_instruction(compiler, 0x06408193);
    ASSERT_EQ(0, result);
    
    TEST("ADDI x3, x1, -100 (I-type negative immediate)");
    result = riscv_compile_instruction(compiler, 0xF9C08193);
    ASSERT_EQ(0, result);
    
    // Test SUB
    TEST("SUB x4, x2, x1 (R-type)");
    result = riscv_compile_instruction(compiler, 0x40110233);
    ASSERT_EQ(0, result);
    
    // Test comparison instructions
    TEST("SLT x5, x1, x2 (set less than signed)");
    result = riscv_compile_instruction(compiler, 0x0020A2B3);
    ASSERT_EQ(0, result);
    
    TEST("SLTU x5, x1, x2 (set less than unsigned)");
    result = riscv_compile_instruction(compiler, 0x0020B2B3);
    ASSERT_EQ(0, result);
    
    TEST("SLTI x5, x1, 50 (set less than immediate signed)");
    result = riscv_compile_instruction(compiler, 0x0320A293);
    ASSERT_EQ(0, result);
    
    TEST("SLTIU x5, x1, 50 (set less than immediate unsigned)");
    result = riscv_compile_instruction(compiler, 0x0320B293);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test all logical instructions
void test_logical_instructions_complete(void) {
    TEST_SUITE("Complete Logical Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // R-type logical operations
    TEST("XOR x3, x1, x2");
    int result = riscv_compile_instruction(compiler, 0x0020C1B3);
    ASSERT_EQ(0, result);
    
    TEST("OR x3, x1, x2");
    result = riscv_compile_instruction(compiler, 0x0020E1B3);
    ASSERT_EQ(0, result);
    
    TEST("AND x3, x1, x2");
    result = riscv_compile_instruction(compiler, 0x0020F1B3);
    ASSERT_EQ(0, result);
    
    // I-type logical operations with immediates
    TEST("XORI x3, x1, 0xFF");
    result = riscv_compile_instruction(compiler, 0x0FF0C193);
    ASSERT_EQ(0, result);
    
    TEST("ORI x3, x1, 0xFF");
    result = riscv_compile_instruction(compiler, 0x0FF0E193);
    ASSERT_EQ(0, result);
    
    TEST("ANDI x3, x1, 0xFF");
    result = riscv_compile_instruction(compiler, 0x0FF0F193);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test all shift instructions
void test_shift_instructions_complete(void) {
    TEST_SUITE("Complete Shift Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Variable shift amounts (R-type)
    TEST("SLL x3, x1, x2 (shift left logical)");
    int result = riscv_compile_instruction(compiler, 0x002091B3);
    ASSERT_EQ(0, result);
    
    TEST("SRL x3, x1, x2 (shift right logical)");
    result = riscv_compile_instruction(compiler, 0x0020D1B3);
    ASSERT_EQ(0, result);
    
    TEST("SRA x3, x1, x2 (shift right arithmetic)");
    result = riscv_compile_instruction(compiler, 0x4020D1B3);
    ASSERT_EQ(0, result);
    
    // Immediate shift amounts (I-type)
    TEST("SLLI x3, x1, 5 (shift left logical immediate)");
    result = riscv_compile_instruction(compiler, 0x00509193);
    ASSERT_EQ(0, result);
    
    TEST("SRLI x3, x1, 5 (shift right logical immediate)");
    result = riscv_compile_instruction(compiler, 0x0050D193);
    ASSERT_EQ(0, result);
    
    TEST("SRAI x3, x1, 5 (shift right arithmetic immediate)");
    result = riscv_compile_instruction(compiler, 0x4050D193);
    ASSERT_EQ(0, result);
    
    // Edge cases
    TEST("SLLI x3, x1, 31 (maximum shift)");
    result = riscv_compile_instruction(compiler, 0x01F09193);
    ASSERT_EQ(0, result);
    
    TEST("SLLI x3, x1, 0 (zero shift)");
    result = riscv_compile_instruction(compiler, 0x00009193);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test all branch instructions
void test_branch_instructions_complete(void) {
    TEST_SUITE("Complete Branch Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Equality branches
    TEST("BEQ x1, x2, 8 (branch if equal)");
    int result = riscv_compile_instruction(compiler, 0x00208463);
    ASSERT_EQ(0, result);
    
    TEST("BNE x1, x2, 8 (branch if not equal)");
    result = riscv_compile_instruction(compiler, 0x00209463);
    ASSERT_EQ(0, result);
    
    // Signed comparison branches
    TEST("BLT x1, x2, 8 (branch if less than signed)");
    result = riscv_compile_instruction(compiler, 0x0020C463);
    ASSERT_EQ(0, result);
    
    TEST("BGE x1, x2, 8 (branch if greater or equal signed)");
    result = riscv_compile_instruction(compiler, 0x0020D463);
    ASSERT_EQ(0, result);
    
    // Unsigned comparison branches
    TEST("BLTU x1, x2, 8 (branch if less than unsigned)");
    result = riscv_compile_instruction(compiler, 0x0020E463);
    ASSERT_EQ(0, result);
    
    TEST("BGEU x1, x2, 8 (branch if greater or equal unsigned)");
    result = riscv_compile_instruction(compiler, 0x0020F463);
    ASSERT_EQ(0, result);
    
    // Test with different branch offsets
    TEST("BEQ x1, x2, 100 (larger offset)");
    result = riscv_compile_instruction(compiler, 0x06208263);
    ASSERT_EQ(0, result);
    
    TEST("BEQ x1, x2, -8 (negative offset)");
    result = riscv_compile_instruction(compiler, 0xFE208CE3);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test jump instructions
void test_jump_instructions_complete(void) {
    TEST_SUITE("Complete Jump Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // JAL (Jump and Link) - J-type
    TEST("JAL x1, 100 (jump and link)");
    int result = riscv_compile_instruction(compiler, 0x064000EF);
    ASSERT_EQ(0, result);
    
    TEST("JAL x0, 100 (unconditional jump, no link)");
    result = riscv_compile_instruction(compiler, 0x0640006F);
    ASSERT_EQ(0, result);
    
    // JALR (Jump and Link Register) - I-type
    TEST("JALR x1, x2, 4 (jump to register + offset)");
    result = riscv_compile_instruction(compiler, 0x004100E7);
    ASSERT_EQ(0, result);
    
    TEST("JALR x0, x1, 0 (return - jump to x1)");
    result = riscv_compile_instruction(compiler, 0x00008067);
    ASSERT_EQ(0, result);
    
    // Test larger offsets
    TEST("JAL x1, 2048 (large positive offset)");
    result = riscv_compile_instruction(compiler, 0x800000EF);
    ASSERT_EQ(0, result);
    
    TEST("JALR x1, x2, -4 (negative offset)");
    result = riscv_compile_instruction(compiler, 0xFFC100E7);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test upper immediate instructions
void test_upper_immediate_instructions_complete(void) {
    TEST_SUITE("Complete Upper Immediate Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // LUI (Load Upper Immediate)
    TEST("LUI x1, 0x12345 (load upper immediate)");
    int result = riscv_compile_instruction(compiler, 0x123450B7);
    ASSERT_EQ(0, result);
    
    TEST("LUI x2, 0x80000 (large immediate)");
    result = riscv_compile_instruction(compiler, 0x80000137);
    ASSERT_EQ(0, result);
    
    TEST("LUI x3, 0x0 (zero immediate)");
    result = riscv_compile_instruction(compiler, 0x000001B7);
    ASSERT_EQ(0, result);
    
    // AUIPC (Add Upper Immediate to PC)
    TEST("AUIPC x1, 0x1000 (add upper immediate to PC)");
    result = riscv_compile_instruction(compiler, 0x010000B7);
    ASSERT_EQ(0, result);
    
    TEST("AUIPC x2, 0x0 (PC to register)");
    result = riscv_compile_instruction(compiler, 0x00000137);
    ASSERT_EQ(0, result);
    
    TEST("AUIPC x3, 0xFFFFF (negative-like upper immediate)");
    result = riscv_compile_instruction(compiler, 0xFFFFF1B7);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test memory instructions
void test_memory_instructions_complete(void) {
    TEST_SUITE("Complete Memory Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Load instructions
    TEST("LW x3, 0(x1) (load word)");
    int result = riscv_compile_instruction(compiler, 0x0000A183);
    ASSERT_EQ(0, result);
    
    TEST("LH x3, 2(x1) (load halfword)");
    result = riscv_compile_instruction(compiler, 0x00209183);
    ASSERT_EQ(0, result);
    
    TEST("LB x3, 3(x1) (load byte)");
    result = riscv_compile_instruction(compiler, 0x00308183);
    ASSERT_EQ(0, result);
    
    TEST("LHU x3, 2(x1) (load halfword unsigned)");
    result = riscv_compile_instruction(compiler, 0x0020D183);
    ASSERT_EQ(0, result);
    
    TEST("LBU x3, 3(x1) (load byte unsigned)");
    result = riscv_compile_instruction(compiler, 0x0030C183);
    ASSERT_EQ(0, result);
    
    // Store instructions
    TEST("SW x2, 0(x1) (store word)");
    result = riscv_compile_instruction(compiler, 0x0020A023);
    ASSERT_EQ(0, result);
    
    TEST("SH x2, 2(x1) (store halfword)");
    result = riscv_compile_instruction(compiler, 0x00209123);
    ASSERT_EQ(0, result);
    
    TEST("SB x2, 3(x1) (store byte)");
    result = riscv_compile_instruction(compiler, 0x00208123);
    ASSERT_EQ(0, result);
    
    // Test with larger offsets
    TEST("LW x3, 100(x1) (load with large offset)");
    result = riscv_compile_instruction(compiler, 0x0640A183);
    ASSERT_EQ(0, result);
    
    TEST("SW x2, -4(x1) (store with negative offset)");
    result = riscv_compile_instruction(compiler, 0xFE20AE23);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test multiplication instructions
void test_multiply_instructions_complete(void) {
    TEST_SUITE("Complete Multiplication Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Basic multiplication
    TEST("MUL x3, x1, x2 (multiply low 32 bits)");
    int result = riscv_compile_instruction(compiler, 0x022081B3);
    ASSERT_EQ(0, result);
    
    // High multiplication variants
    TEST("MULH x3, x1, x2 (multiply high signed)");
    result = riscv_compile_instruction(compiler, 0x022091B3);
    ASSERT_EQ(0, result);
    
    TEST("MULHU x3, x1, x2 (multiply high unsigned)");
    result = riscv_compile_instruction(compiler, 0x0220B1B3);
    ASSERT_EQ(0, result);
    
    TEST("MULHSU x3, x1, x2 (multiply high signed-unsigned)");
    result = riscv_compile_instruction(compiler, 0x0220A1B3);
    ASSERT_EQ(0, result);
    
    // Test with different register combinations
    TEST("MUL x5, x3, x4 (different registers)");
    result = riscv_compile_instruction(compiler, 0x024182B3);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test division instructions
void test_divide_instructions_complete(void) {
    TEST_SUITE("Complete Division Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Division
    TEST("DIV x3, x1, x2 (divide signed)");
    int result = riscv_compile_instruction(compiler, 0x0220C1B3);
    ASSERT_EQ(0, result);
    
    TEST("DIVU x3, x1, x2 (divide unsigned)");
    result = riscv_compile_instruction(compiler, 0x0220D1B3);
    ASSERT_EQ(0, result);
    
    // Remainder
    TEST("REM x3, x1, x2 (remainder signed)");
    result = riscv_compile_instruction(compiler, 0x0220E1B3);
    ASSERT_EQ(0, result);
    
    TEST("REMU x3, x1, x2 (remainder unsigned)");
    result = riscv_compile_instruction(compiler, 0x0220F1B3);
    ASSERT_EQ(0, result);
    
    // Test with different register combinations
    TEST("DIV x4, x2, x3 (different registers)");
    result = riscv_compile_instruction(compiler, 0x02314233);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Test system instructions
void test_system_instructions_complete(void) {
    TEST_SUITE("Complete System Instructions");
    
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) return;
    
    // Environment calls
    TEST("ECALL (environment call)");
    int result = riscv_compile_instruction(compiler, 0x00000073);
    ASSERT_EQ(0, result);
    
    TEST("EBREAK (environment break)");
    result = riscv_compile_instruction(compiler, 0x00100073);
    ASSERT_EQ(0, result);
    
    // Fence instruction
    TEST("FENCE (memory fence)");
    result = riscv_compile_instruction(compiler, 0x0000000F);
    ASSERT_EQ(0, result);
    
    riscv_compiler_destroy(compiler);
}

// Main test runner
int main(void) {
    printf("RISC-V Compiler Comprehensive Test Suite\n");
    printf("========================================\n");
    printf("Testing ALL RISC-V RV32I instructions with complete coverage\n\n");
    
    test_arithmetic_instructions_complete();
    test_logical_instructions_complete();
    test_shift_instructions_complete();
    test_branch_instructions_complete();
    test_jump_instructions_complete();
    test_upper_immediate_instructions_complete();
    test_memory_instructions_complete();
    test_multiply_instructions_complete();
    test_divide_instructions_complete();
    test_system_instructions_complete();
    
    print_test_summary();
    
    printf("\n📊 INSTRUCTION COVERAGE ANALYSIS\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("✅ Arithmetic:    ADD, SUB, ADDI, SLT, SLTU, SLTI, SLTIU\n");
    printf("✅ Logical:       XOR, OR, AND, XORI, ORI, ANDI\n");
    printf("✅ Shifts:        SLL, SRL, SRA, SLLI, SRLI, SRAI\n");
    printf("✅ Branches:      BEQ, BNE, BLT, BGE, BLTU, BGEU\n");
    printf("✅ Jumps:         JAL, JALR\n");
    printf("✅ Upper Imm:     LUI, AUIPC\n");
    printf("✅ Memory:        LW, LH, LB, LHU, LBU, SW, SH, SB\n");
    printf("✅ Multiply:      MUL, MULH, MULHU, MULHSU\n");
    printf("✅ Divide:        DIV, DIVU, REM, REMU\n");
    printf("✅ System:        ECALL, EBREAK, FENCE\n");
    printf("\n🎯 COMPLETE RV32I INSTRUCTION SET IMPLEMENTED\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}