/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * SHA3-256 RISC-V Implementation
 * 
 * A simplified SHA3-256 implementation in RISC-V assembly
 * designed for verification of the compiled circuit.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// RISC-V assembly implementation of core SHA3 operations
// This would normally be in .S file, but we'll use inline assembly

// Rotate left 64-bit value (C implementation)
uint64_t rotl64_riscv(uint64_t x, unsigned int n) {
    n &= 63;
    return (x << n) | (x >> (64 - n));
}

// XOR two 64-bit values
uint64_t xor64_riscv(uint64_t a, uint64_t b) {
    return a ^ b;
}

// Simplified SHA3 round function for RISC-V
// This demonstrates the structure but is greatly simplified
void sha3_round_riscv(uint32_t* state) {
    // Theta step (simplified)
    // Compute column parities
    uint32_t col0_parity = state[0] ^ state[5] ^ state[10] ^ state[15] ^ state[20];
    
    // Apply theta effect (simplified - just to first element)
    state[0] ^= col0_parity;
    
    // Chi step (simplified)
    // For first row, apply chi
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    
    state[0] = a ^ ((~b) & c);
    
    // Iota step (simplified)
    // XOR round constant into state[0]
    state[0] ^= 0x0001;
}

// Simplified SHA3-256 for RISC-V verification
// This computes a simplified hash suitable for circuit verification
void sha3_256_riscv_simple(const uint32_t* input, size_t words, uint32_t* output) {
    // State array (25 * 2 = 50 words for 64-bit lanes)
    uint32_t state[50] = {0};
    
    // Absorb phase (simplified - just XOR input into state)
    for (size_t i = 0; i < words && i < 34; i++) {  // Rate = 136 bytes = 34 words
        state[i] ^= input[i];
    }
    
    // Permutation (simplified - just one round)
    sha3_round_riscv(state);
    
    // Squeeze phase (output first 8 words = 256 bits)
    for (int i = 0; i < 8; i++) {
        output[i] = state[i];
    }
}

// Generate RISC-V assembly for SHA3
void generate_sha3_assembly() {
    printf("; SHA3-256 RISC-V Assembly (Simplified)\n");
    printf("; Input: a0 = input pointer, a1 = length\n");
    printf("; Output: a2 = output pointer\n");
    printf("\n");
    
    printf("sha3_256:\n");
    printf("    ; Initialize state (50 words)\n");
    printf("    addi    sp, sp, -200    ; Allocate state on stack\n");
    printf("    mv      s0, sp          ; s0 = state pointer\n");
    printf("\n");
    
    printf("    ; Clear state\n");
    printf("    li      t0, 0\n");
    printf("    li      t1, 50\n");
    printf("clear_loop:\n");
    printf("    sw      t0, 0(s0)\n");
    printf("    addi    s0, s0, 4\n");
    printf("    addi    t1, t1, -1\n");
    printf("    bnez    t1, clear_loop\n");
    printf("\n");
    
    printf("    ; Absorb input (simplified)\n");
    printf("    mv      s0, sp          ; Reset state pointer\n");
    printf("    mv      t0, a0          ; Input pointer\n");
    printf("    mv      t1, a1          ; Length in words\n");
    printf("absorb_loop:\n");
    printf("    beqz    t1, absorb_done\n");
    printf("    lw      t2, 0(t0)       ; Load input word\n");
    printf("    lw      t3, 0(s0)       ; Load state word\n");
    printf("    xor     t3, t3, t2      ; XOR into state\n");
    printf("    sw      t3, 0(s0)       ; Store back\n");
    printf("    addi    t0, t0, 4\n");
    printf("    addi    s0, s0, 4\n");
    printf("    addi    t1, t1, -1\n");
    printf("    j       absorb_loop\n");
    printf("absorb_done:\n");
    printf("\n");
    
    printf("    ; Permutation (call round function)\n");
    printf("    mv      a0, sp\n");
    printf("    call    sha3_round_riscv\n");
    printf("\n");
    
    printf("    ; Squeeze output (8 words)\n");
    printf("    mv      s0, sp          ; State pointer\n");
    printf("    mv      t0, a2          ; Output pointer\n");
    printf("    li      t1, 8\n");
    printf("squeeze_loop:\n");
    printf("    lw      t2, 0(s0)\n");
    printf("    sw      t2, 0(t0)\n");
    printf("    addi    s0, s0, 4\n");
    printf("    addi    t0, t0, 4\n");
    printf("    addi    t1, t1, -1\n");
    printf("    bnez    t1, squeeze_loop\n");
    printf("\n");
    
    printf("    ; Cleanup and return\n");
    printf("    addi    sp, sp, 200\n");
    printf("    ret\n");
}

// Create test program that computes SHA3
void create_sha3_test_program(uint32_t* program, size_t* size) {
    // This creates a sequence of RISC-V instructions that compute
    // a simplified SHA3 hash of a small input
    
    uint32_t sha3_program[] = {
        // Setup: Load input "abc" into registers
        0x00061023,  // sb x0, 'a'(x12)    ; Store 'a' at address 0
        0x00161223,  // sb x1, 'b'(x12)    ; Store 'b' at address 1  
        0x00261423,  // sb x2, 'c'(x12)    ; Store 'c' at address 2
        
        // Initialize state array (simplified - just first few words)
        0x00000093,  // addi x1, x0, 0     ; x1 = 0
        0x00008113,  // addi x2, x1, 0     ; x2 = 0
        0x00010193,  // addi x3, x2, 0     ; x3 = 0
        
        // Load input into x4-x6
        0x00002203,  // lw x4, 0(x0)       ; Load input word
        
        // Theta step (very simplified)
        0x00424233,  // xor x4, x4, x4     ; (This would be column parity)
        
        // Chi step (simplified)
        0xFFF24213,  // not x4, x4         ; ~a
        0x00527233,  // and x4, x4, x5     ; ~a & b
        0x00426233,  // xor x4, x4, x6     ; result
        
        // Store result
        0x00402023,  // sw x4, 0(x0)       ; Store first output word
        
        // Return
        0x00008067   // jalr x0, 0(x1)     ; Return
    };
    
    memcpy(program, sha3_program, sizeof(sha3_program));
    *size = sizeof(sha3_program) / sizeof(uint32_t);
}

#ifdef TEST_SHA3_RISCV

int main() {
    printf("SHA3-256 RISC-V Implementation\n");
    printf("==============================\n\n");
    
    // Generate assembly listing
    generate_sha3_assembly();
    
    // Test simplified SHA3
    printf("\n\nTesting Simplified SHA3:\n");
    
    uint32_t input[3] = {0x616263, 0, 0};  // "abc" in little-endian
    uint32_t output[8];
    
    sha3_256_riscv_simple(input, 1, output);
    
    printf("Input: \"abc\" (0x%08x)\n", input[0]);
    printf("Output: ");
    for (int i = 0; i < 8; i++) {
        printf("%08x ", output[i]);
    }
    printf("\n");
    
    // Create test program
    uint32_t program[100];
    size_t program_size;
    create_sha3_test_program(program, &program_size);
    
    printf("\nGenerated %zu RISC-V instructions for SHA3 test\n", program_size);
    
    return 0;
}

#endif