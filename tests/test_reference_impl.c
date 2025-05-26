/*
 * Test Reference Implementations
 * 
 * Verifies that our reference implementations produce expected results
 * for known test cases from the RISC-V specification.
 */

#include <stdio.h>
#include <assert.h>
#include "../include/formal_verification.h"

// Test utilities
static void print_word32(const char* name, word32_t word) {
    printf("%s = 0x%08X (", name, word32_to_uint32(&word));
    for (int i = 31; i >= 0; i--) {
        printf("%d", word.bits[i] ? 1 : 0);
        if (i % 4 == 0 && i > 0) printf(" ");
    }
    printf(")\n");
}

static void test_case(const char* test_name) {
    printf("\n=== Testing %s ===\n", test_name);
}

static void test_passed(void) {
    printf("✓ PASSED\n");
}

// ============================================================================
// Test Arithmetic Operations
// ============================================================================

void test_add() {
    test_case("ADD instruction");
    
    // Test 1: Simple addition
    word32_t a, b, result;
    uint32_to_word32(0x00000005, &a);  // 5
    uint32_to_word32(0x00000003, &b);  // 3
    result = ref_add(a, b);
    assert(word32_to_uint32(&result) == 0x00000008);  // 5 + 3 = 8
    printf("  5 + 3 = 8 ✓\n");
    
    // Test 2: Overflow wrapping
    uint32_to_word32(0xFFFFFFFF, &a);  // -1 in two's complement
    uint32_to_word32(0x00000001, &b);  // 1
    result = ref_add(a, b);
    assert(word32_to_uint32(&result) == 0x00000000);  // -1 + 1 = 0
    printf("  -1 + 1 = 0 (overflow wrap) ✓\n");
    
    // Test 3: Large numbers
    uint32_to_word32(0x7FFFFFFF, &a);  // Max positive
    uint32_to_word32(0x00000001, &b);  // 1
    result = ref_add(a, b);
    assert(word32_to_uint32(&result) == 0x80000000);  // Overflow to min negative
    printf("  MAX_INT + 1 = MIN_INT ✓\n");
    
    test_passed();
}

void test_sub() {
    test_case("SUB instruction");
    
    word32_t a, b, result;
    
    // Test 1: Simple subtraction
    uint32_to_word32(0x00000008, &a);  // 8
    uint32_to_word32(0x00000003, &b);  // 3
    result = ref_sub(a, b);
    assert(word32_to_uint32(&result) == 0x00000005);  // 8 - 3 = 5
    printf("  8 - 3 = 5 ✓\n");
    
    // Test 2: Negative result
    uint32_to_word32(0x00000003, &a);  // 3
    uint32_to_word32(0x00000008, &b);  // 8
    result = ref_sub(a, b);
    assert(word32_to_uint32(&result) == 0xFFFFFFFB);  // 3 - 8 = -5
    printf("  3 - 8 = -5 ✓\n");
    
    // Test 3: Zero result
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x12345678, &b);
    result = ref_sub(a, b);
    assert(word32_to_uint32(&result) == 0x00000000);
    printf("  x - x = 0 ✓\n");
    
    test_passed();
}

// ============================================================================
// Test Logical Operations
// ============================================================================

void test_logical() {
    test_case("Logical operations (AND, OR, XOR)");
    
    word32_t a, b, result;
    uint32_to_word32(0xAAAAAAAA, &a);  // 1010...1010
    uint32_to_word32(0x55555555, &b);  // 0101...0101
    
    // Test AND
    result = ref_and(a, b);
    assert(word32_to_uint32(&result) == 0x00000000);  // No common bits
    printf("  0xAAAAAAAA & 0x55555555 = 0x00000000 ✓\n");
    
    // Test OR
    result = ref_or(a, b);
    assert(word32_to_uint32(&result) == 0xFFFFFFFF);  // All bits set
    printf("  0xAAAAAAAA | 0x55555555 = 0xFFFFFFFF ✓\n");
    
    // Test XOR
    result = ref_xor(a, b);
    assert(word32_to_uint32(&result) == 0xFFFFFFFF);  // All bits different
    printf("  0xAAAAAAAA ^ 0x55555555 = 0xFFFFFFFF ✓\n");
    
    test_passed();
}

// ============================================================================
// Test Shift Operations
// ============================================================================

void test_shifts() {
    test_case("Shift operations (SLL, SRL, SRA)");
    
    word32_t a, b, result;
    
    // Test SLL (shift left logical)
    uint32_to_word32(0x00000001, &a);  // 1
    uint32_to_word32(0x00000004, &b);  // Shift by 4
    result = ref_sll(a, b);
    assert(word32_to_uint32(&result) == 0x00000010);  // 1 << 4 = 16
    printf("  1 << 4 = 16 ✓\n");
    
    // Test SRL (shift right logical)
    uint32_to_word32(0x80000000, &a);  // MSB set
    uint32_to_word32(0x00000004, &b);  // Shift by 4
    result = ref_srl(a, b);
    assert(word32_to_uint32(&result) == 0x08000000);  // Logical shift (zeros)
    printf("  0x80000000 >> 4 = 0x08000000 (logical) ✓\n");
    
    // Test SRA (shift right arithmetic)
    uint32_to_word32(0x80000000, &a);  // Negative number
    uint32_to_word32(0x00000004, &b);  // Shift by 4
    result = ref_sra(a, b);
    assert(word32_to_uint32(&result) == 0xF8000000);  // Sign extension
    printf("  0x80000000 >>> 4 = 0xF8000000 (arithmetic) ✓\n");
    
    // Test shift by 0
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x00000000, &b);
    result = ref_sll(a, b);
    assert(word32_to_uint32(&result) == 0x12345678);  // No change
    printf("  x << 0 = x ✓\n");
    
    test_passed();
}

// ============================================================================
// Test Comparison Operations
// ============================================================================

void test_comparisons() {
    test_case("Comparison operations");
    
    word32_t a, b;
    
    // Test equality
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x12345678, &b);
    assert(ref_eq(a, b) == true);
    printf("  0x12345678 == 0x12345678 ✓\n");
    
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x87654321, &b);
    assert(ref_eq(a, b) == false);
    printf("  0x12345678 != 0x87654321 ✓\n");
    
    // Test signed comparison
    uint32_to_word32(0xFFFFFFFF, &a);  // -1
    uint32_to_word32(0x00000001, &b);  // 1
    assert(ref_lt_signed(a, b) == true);  // -1 < 1
    printf("  -1 < 1 (signed) ✓\n");
    
    // Test unsigned comparison
    uint32_to_word32(0xFFFFFFFF, &a);  // Large positive
    uint32_to_word32(0x00000001, &b);  // 1
    assert(ref_lt_unsigned(a, b) == false);  // 0xFFFFFFFF > 1
    printf("  0xFFFFFFFF > 1 (unsigned) ✓\n");
    
    test_passed();
}

// ============================================================================
// Test Multiplication
// ============================================================================

void test_multiplication() {
    test_case("MUL instruction");
    
    word32_t a, b, result;
    
    // Test 1: Simple multiplication
    uint32_to_word32(0x00000005, &a);  // 5
    uint32_to_word32(0x00000007, &b);  // 7
    result = ref_mul(a, b);
    assert(word32_to_uint32(&result) == 0x00000023);  // 5 * 7 = 35
    printf("  5 * 7 = 35 ✓\n");
    
    // Test 2: Multiplication by zero
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x00000000, &b);
    result = ref_mul(a, b);
    assert(word32_to_uint32(&result) == 0x00000000);
    printf("  x * 0 = 0 ✓\n");
    
    // Test 3: Multiplication by one
    uint32_to_word32(0x12345678, &a);
    uint32_to_word32(0x00000001, &b);
    result = ref_mul(a, b);
    assert(word32_to_uint32(&result) == 0x12345678);
    printf("  x * 1 = x ✓\n");
    
    // Test 4: Overflow (only lower 32 bits kept)
    uint32_to_word32(0x10000000, &a);
    uint32_to_word32(0x00000010, &b);
    result = ref_mul(a, b);
    assert(word32_to_uint32(&result) == 0x00000000);  // Overflow, lower bits = 0
    printf("  Large * 16 = overflow (lower 32 bits) ✓\n");
    
    test_passed();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("===========================================\n");
    printf("Testing RISC-V Reference Implementations\n");
    printf("===========================================\n");
    
    // Test all operations
    test_add();
    test_sub();
    test_logical();
    test_shifts();
    test_comparisons();
    test_multiplication();
    
    printf("\n===========================================\n");
    printf("All reference implementation tests PASSED! ✓\n");
    printf("===========================================\n");
    
    return 0;
}