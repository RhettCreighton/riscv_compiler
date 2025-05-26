/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * Reference Implementations for RISC-V Instructions
 * 
 * These implementations are designed to be "obviously correct" by:
 * 1. Following mathematical definitions directly
 * 2. No optimizations or tricks
 * 3. Clear correspondence to RISC-V specification
 * 4. Extensive comments linking to spec sections
 * 
 * These serve as the ground truth for formal verification.
 */

#include "../include/formal_verification.h"
#include <string.h>
#include <assert.h>

// ============================================================================
// Utility Functions
// ============================================================================

// Convert uint32_t to bit array representation
void uint32_to_word32(uint32_t value, word32_t* word) {
    for (int i = 0; i < 32; i++) {
        word->bits[i] = (value >> i) & 1;
    }
}

// Convert bit array to uint32_t
uint32_t word32_to_uint32(const word32_t* word) {
    uint32_t value = 0;
    for (int i = 0; i < 32; i++) {
        if (word->bits[i]) {
            value |= (1U << i);
        }
    }
    return value;
}

// Create a word32_t with all bits set to a value
word32_t word32_fill(bool value) {
    word32_t result;
    for (int i = 0; i < 32; i++) {
        result.bits[i] = value;
    }
    return result;
}

// ============================================================================
// Arithmetic Operations (RV32I Chapter 2.4)
// ============================================================================

// ADD: rd = rs1 + rs2
// Reference: RISC-V Spec Section 2.4 - "ADD adds the 32-bit values in rs1 and rs2"
// Mathematical definition: result[i] = a[i] ⊕ b[i] ⊕ carry[i-1]
word32_t ref_add(word32_t a, word32_t b) {
    word32_t result;
    bool carry = false;
    
    // Process each bit from LSB to MSB
    for (int i = 0; i < 32; i++) {
        // Full adder logic: sum = a ⊕ b ⊕ carry_in
        result.bits[i] = a.bits[i] ^ b.bits[i] ^ carry;
        
        // Carry out: carry = (a ∧ b) ∨ (carry_in ∧ (a ⊕ b))
        carry = (a.bits[i] & b.bits[i]) | (carry & (a.bits[i] ^ b.bits[i]));
    }
    
    // Overflow is ignored in RISC-V (wraps around)
    return result;
}

// SUB: rd = rs1 - rs2
// Reference: RISC-V Spec Section 2.4 - "SUB subtracts rs2 from rs1"
// Mathematical definition: a - b = a + (~b) + 1 (two's complement)
word32_t ref_sub(word32_t a, word32_t b) {
    word32_t result;
    bool borrow = false;
    
    // Subtraction using borrow propagation
    for (int i = 0; i < 32; i++) {
        // Compute a[i] - b[i] - borrow
        bool diff = a.bits[i] ^ b.bits[i] ^ borrow;
        
        // New borrow occurs when:
        // - b[i] = 1 and a[i] = 0
        // - b[i] = a[i] and previous borrow = 1
        borrow = (!a.bits[i] & b.bits[i]) | (borrow & !(a.bits[i] ^ b.bits[i]));
        
        result.bits[i] = diff;
    }
    
    return result;
}

// ============================================================================
// Logical Operations (RV32I Chapter 2.4)
// ============================================================================

// AND: rd = rs1 & rs2
// Reference: RISC-V Spec Section 2.4 - "AND is a bitwise logical AND"
// Mathematical definition: result[i] = a[i] ∧ b[i]
word32_t ref_and(word32_t a, word32_t b) {
    word32_t result;
    
    // Bitwise AND: each bit independently
    for (int i = 0; i < 32; i++) {
        result.bits[i] = a.bits[i] & b.bits[i];
    }
    
    return result;
}

// OR: rd = rs1 | rs2
// Reference: RISC-V Spec Section 2.4 - "OR is a bitwise logical OR"
// Mathematical definition: result[i] = a[i] ∨ b[i]
word32_t ref_or(word32_t a, word32_t b) {
    word32_t result;
    
    // Bitwise OR: each bit independently
    for (int i = 0; i < 32; i++) {
        result.bits[i] = a.bits[i] | b.bits[i];
    }
    
    return result;
}

// XOR: rd = rs1 ^ rs2
// Reference: RISC-V Spec Section 2.4 - "XOR is a bitwise logical XOR"
// Mathematical definition: result[i] = a[i] ⊕ b[i]
word32_t ref_xor(word32_t a, word32_t b) {
    word32_t result;
    
    // Bitwise XOR: each bit independently
    for (int i = 0; i < 32; i++) {
        result.bits[i] = a.bits[i] ^ b.bits[i];
    }
    
    return result;
}

// ============================================================================
// Shift Operations (RV32I Chapter 2.4)
// ============================================================================

// SLL: rd = rs1 << rs2[4:0]
// Reference: RISC-V Spec Section 2.4 - "SLL shifts rs1 left by the amount in the lower 5 bits of rs2"
// Mathematical definition: result = a × 2^(b mod 32)
word32_t ref_sll(word32_t a, word32_t b) {
    word32_t result = word32_fill(false);  // Initialize to zeros
    
    // Extract shift amount from lower 5 bits of b
    int shift_amount = 0;
    for (int i = 0; i < 5; i++) {
        if (b.bits[i]) {
            shift_amount |= (1 << i);
        }
    }
    
    // Shift left by copying bits to higher positions
    for (int i = 0; i < 32; i++) {
        int source_pos = i - shift_amount;
        if (source_pos >= 0 && source_pos < 32) {
            result.bits[i] = a.bits[source_pos];
        }
        // Else: result.bits[i] remains 0 (shift in zeros)
    }
    
    return result;
}

// SRL: rd = rs1 >> rs2[4:0] (logical shift)
// Reference: RISC-V Spec Section 2.4 - "SRL shifts rs1 right by the amount in the lower 5 bits of rs2"
// Mathematical definition: result = floor(a / 2^(b mod 32))
word32_t ref_srl(word32_t a, word32_t b) {
    word32_t result = word32_fill(false);  // Initialize to zeros
    
    // Extract shift amount from lower 5 bits of b
    int shift_amount = 0;
    for (int i = 0; i < 5; i++) {
        if (b.bits[i]) {
            shift_amount |= (1 << i);
        }
    }
    
    // Shift right by copying bits to lower positions
    for (int i = 0; i < 32; i++) {
        int source_pos = i + shift_amount;
        if (source_pos < 32) {
            result.bits[i] = a.bits[source_pos];
        }
        // Else: result.bits[i] remains 0 (shift in zeros)
    }
    
    return result;
}

// SRA: rd = rs1 >>> rs2[4:0] (arithmetic shift)
// Reference: RISC-V Spec Section 2.4 - "SRA shifts rs1 right, filling with the original sign bit"
// Mathematical definition: sign-extends the result
word32_t ref_sra(word32_t a, word32_t b) {
    word32_t result;
    
    // Extract shift amount from lower 5 bits of b
    int shift_amount = 0;
    for (int i = 0; i < 5; i++) {
        if (b.bits[i]) {
            shift_amount |= (1 << i);
        }
    }
    
    // Get sign bit (MSB)
    bool sign_bit = a.bits[31];
    
    // Initialize result with sign bit (for sign extension)
    result = word32_fill(sign_bit);
    
    // Shift right by copying bits to lower positions
    for (int i = 0; i < 32; i++) {
        int source_pos = i + shift_amount;
        if (source_pos < 32) {
            result.bits[i] = a.bits[source_pos];
        }
        // Else: result.bits[i] remains as sign bit (sign extension)
    }
    
    return result;
}

// ============================================================================
// Comparison Operations (RV32I Chapter 2.5 - Branch Instructions)
// ============================================================================

// EQ: Test if a == b
// Reference: RISC-V Spec Section 2.5 - "BEQ branches if rs1 equals rs2"
// Mathematical definition: ∀i: a[i] = b[i]
bool ref_eq(word32_t a, word32_t b) {
    // Two values are equal if all bits are identical
    for (int i = 0; i < 32; i++) {
        if (a.bits[i] != b.bits[i]) {
            return false;
        }
    }
    return true;
}

// LT (signed): Test if a < b (signed comparison)
// Reference: RISC-V Spec Section 2.5 - "BLT branches if rs1 is less than rs2 (signed)"
// Mathematical definition: Signed two's complement comparison
bool ref_lt_signed(word32_t a, word32_t b) {
    // First check sign bits
    bool a_negative = a.bits[31];
    bool b_negative = b.bits[31];
    
    // If signs differ, negative number is smaller
    if (a_negative && !b_negative) {
        return true;   // a is negative, b is positive
    }
    if (!a_negative && b_negative) {
        return false;  // a is positive, b is negative
    }
    
    // Same sign: compare magnitude from MSB to LSB
    for (int i = 30; i >= 0; i--) {
        if (a.bits[i] < b.bits[i]) {
            return true;
        }
        if (a.bits[i] > b.bits[i]) {
            return false;
        }
    }
    
    // Equal values
    return false;
}

// LT (unsigned): Test if a < b (unsigned comparison)
// Reference: RISC-V Spec Section 2.5 - "BLTU branches if rs1 is less than rs2 (unsigned)"
// Mathematical definition: Unsigned binary comparison
bool ref_lt_unsigned(word32_t a, word32_t b) {
    // Compare from MSB to LSB
    for (int i = 31; i >= 0; i--) {
        if (a.bits[i] < b.bits[i]) {
            return true;
        }
        if (a.bits[i] > b.bits[i]) {
            return false;
        }
    }
    
    // Equal values
    return false;
}

// ============================================================================
// Multiplication (RV32M Extension)
// ============================================================================

// MUL: rd = (rs1 * rs2)[31:0]
// Reference: RISC-V Spec Section 7.1 - "MUL returns the lower 32 bits of the product"
// Mathematical definition: Standard binary multiplication
word32_t ref_mul(word32_t a, word32_t b) {
    word32_t result = word32_fill(false);
    
    // School multiplication algorithm
    // For each bit in b, if set, add shifted a to result
    for (int i = 0; i < 32; i++) {
        if (b.bits[i]) {
            // Add a << i to result
            bool carry = false;
            for (int j = 0; j < 32; j++) {
                if (i + j < 32) {
                    bool sum = result.bits[i + j] ^ a.bits[j] ^ carry;
                    carry = (result.bits[i + j] & a.bits[j]) | 
                           (carry & (result.bits[i + j] ^ a.bits[j]));
                    result.bits[i + j] = sum;
                }
            }
        }
    }
    
    return result;
}

// ============================================================================
// Special Purpose Operations
// ============================================================================

// Set Less Than (for SLT instruction)
// Reference: RISC-V Spec Section 2.4 - "SLT sets rd to 1 if rs1 < rs2 (signed)"
word32_t ref_slt(word32_t a, word32_t b) {
    word32_t result = word32_fill(false);
    result.bits[0] = ref_lt_signed(a, b);
    return result;
}

// Set Less Than Unsigned (for SLTU instruction)
// Reference: RISC-V Spec Section 2.4 - "SLTU sets rd to 1 if rs1 < rs2 (unsigned)"
word32_t ref_sltu(word32_t a, word32_t b) {
    word32_t result = word32_fill(false);
    result.bits[0] = ref_lt_unsigned(a, b);
    return result;
}

// ============================================================================
// Immediate Operations (sign extension)
// ============================================================================

// Sign extend a 12-bit immediate to 32 bits
// Reference: RISC-V Spec Section 2.3 - "Immediates are always sign-extended"
word32_t ref_sign_extend_12(word32_t imm12) {
    word32_t result;
    
    // Copy lower 12 bits
    for (int i = 0; i < 12; i++) {
        result.bits[i] = imm12.bits[i];
    }
    
    // Sign extend from bit 11
    bool sign_bit = imm12.bits[11];
    for (int i = 12; i < 32; i++) {
        result.bits[i] = sign_bit;
    }
    
    return result;
}

// Sign extend a 20-bit immediate to 32 bits
// Reference: RISC-V Spec Section 2.3 - For LUI and AUIPC
word32_t ref_sign_extend_20(word32_t imm20) {
    word32_t result;
    
    // Copy lower 20 bits
    for (int i = 0; i < 20; i++) {
        result.bits[i] = imm20.bits[i];
    }
    
    // Sign extend from bit 19
    bool sign_bit = imm20.bits[19];
    for (int i = 20; i < 32; i++) {
        result.bits[i] = sign_bit;
    }
    
    return result;
}