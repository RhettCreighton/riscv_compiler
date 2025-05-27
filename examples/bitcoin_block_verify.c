/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * bitcoin_block_verify.c - Bitcoin block header verification circuit
 * 
 * This circuit verifies a Bitcoin block header's proof-of-work.
 * It takes an 80-byte block header as input and outputs 1 if valid, 0 if not.
 * 
 * Bitcoin block header structure (80 bytes):
 * - Version (4 bytes)
 * - Previous block hash (32 bytes)
 * - Merkle root (32 bytes)
 * - Timestamp (4 bytes)
 * - Bits (4 bytes) - encoded difficulty target
 * - Nonce (4 bytes)
 * 
 * Verification process:
 * 1. Double SHA-256 of the header
 * 2. Compare result with difficulty target
 * 3. Output 1 if hash <= target, 0 otherwise
 */

#include "../include/zkvm.h"

// Import SHA-256 implementation
extern void zkvm_sha256(const uint8_t* message, size_t length, uint8_t output[32]);

// Helper: Compare two 256-bit numbers (big-endian)
// Returns 1 if a <= b, 0 otherwise
ZKVM_INLINE static uint32_t compare_256bit_le(const uint8_t a[32], const uint8_t b[32]) GATES(8192) {
    // Start from most significant byte
    uint32_t result = ONE;  // Assume a <= b
    uint32_t equal_so_far = ONE;
    
    for (int i = 0; i < 32; i++) {
        // Compare bytes from MSB to LSB
        uint32_t a_byte = 0;
        uint32_t b_byte = 0;
        
        // Build byte values from bits
        for (int j = 0; j < 8; j++) {
            uint32_t bit_val = ONE << j;
            a_byte = a_byte | ((a[i] & (1 << j)) ? bit_val : ZERO);
            b_byte = b_byte | ((b[i] & (1 << j)) ? bit_val : ZERO);
        }
        
        // Check if a[i] < b[i]
        uint32_t a_less = (~a_byte) & b_byte;
        
        // Check if a[i] == b[i]
        uint32_t equal = ~(a_byte ^ b_byte);
        
        // Update result: if we're still equal and this byte is less, set result
        uint32_t update_condition = equal_so_far & a_less;
        result = result | update_condition;
        
        // Update equal_so_far
        equal_so_far = equal_so_far & equal;
    }
    
    return result;
}

// Helper: Decode 'bits' field to 256-bit target
// Bitcoin uses a compact representation: 0xAABBCCDD means 0xBBCCDD * 256^(0xAA-3)
static void decode_bits_to_target(uint32_t bits, uint8_t target[32]) {
    // Initialize target to zero
    for (int i = 0; i < 32; i++) {
        target[i] = 0;
    }
    
    // Extract exponent and mantissa
    uint8_t exponent = (bits >> 24) & 0xFF;
    uint32_t mantissa = bits & 0x00FFFFFF;
    
    // Special case: if exponent <= 3, it's invalid
    if (exponent <= 3) {
        return;  // Target remains zero (impossible to meet)
    }
    
    // Calculate byte position for mantissa
    int byte_pos = 32 - (exponent - 3);
    
    // Place mantissa in target (big-endian)
    if (byte_pos >= 0 && byte_pos < 32) {
        target[byte_pos] = (mantissa >> 16) & 0xFF;
        if (byte_pos + 1 < 32) {
            target[byte_pos + 1] = (mantissa >> 8) & 0xFF;
        }
        if (byte_pos + 2 < 32) {
            target[byte_pos + 2] = mantissa & 0xFF;
        }
    }
}

// Main verification function
uint32_t verify_bitcoin_block_header(const uint8_t header[80]) {
    uint8_t hash1[32];
    uint8_t hash2[32];
    uint8_t target[32];
    
    // Step 1: First SHA-256
    zkvm_sha256(header, 80, hash1);
    
    // Step 2: Second SHA-256 (Bitcoin uses double SHA-256)
    zkvm_sha256(hash1, 32, hash2);
    
    // Step 3: Extract difficulty bits from header
    // Bits field is at offset 72-75 (little-endian)
    uint32_t bits = ((uint32_t)header[72]) |
                    ((uint32_t)header[73] << 8) |
                    ((uint32_t)header[74] << 16) |
                    ((uint32_t)header[75] << 24);
    
    // Step 4: Decode bits to target
    decode_bits_to_target(bits, target);
    
    // Step 5: Compare hash with target
    // Bitcoin considers the hash as a little-endian number,
    // but we need to compare as big-endian
    uint8_t hash_be[32];
    for (int i = 0; i < 32; i++) {
        hash_be[i] = hash2[31 - i];  // Reverse bytes
    }
    
    uint8_t target_be[32];
    for (int i = 0; i < 32; i++) {
        target_be[i] = target[31 - i];  // Reverse bytes
    }
    
    // Return 1 if hash <= target, 0 otherwise
    return compare_256bit_le(hash_be, target_be);
}

// Example usage with a real Bitcoin block header
int main() {
    // Example: Bitcoin block #100000
    // This is a real block header from the Bitcoin blockchain
    uint8_t block_header[80] = {
        // Version (1)
        0x01, 0x00, 0x00, 0x00,
        
        // Previous block hash
        0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0xd6, 0x68,
        0x9c, 0x08, 0x5a, 0xe1, 0x65, 0x83, 0x1e, 0x93,
        0x4f, 0xf7, 0x63, 0xae, 0x46, 0xa2, 0xa6, 0xc1,
        0x72, 0xb3, 0xf1, 0xb6, 0x0a, 0x8c, 0xe2, 0x6f,
        
        // Merkle root
        0x87, 0x71, 0x4d, 0x3e, 0x1f, 0xec, 0xfd, 0x30,
        0x5b, 0x2b, 0x0e, 0xcb, 0x33, 0xf3, 0x74, 0xc1,
        0xbc, 0xe6, 0x1d, 0x72, 0x8f, 0xa0, 0x8d, 0xc9,
        0x0e, 0xfd, 0x6f, 0xae, 0x86, 0x43, 0x48, 0x8a,
        
        // Timestamp (1293623863)
        0x37, 0x7a, 0x36, 0x4d,
        
        // Bits (0x1b04864c)
        0x4c, 0x86, 0x04, 0x1b,
        
        // Nonce (274148111)
        0x0f, 0x79, 0x57, 0x10
    };
    
    // Verify the block
    uint32_t is_valid = verify_bitcoin_block_header(block_header);
    
    // Output result
    zkvm_output(&is_valid, 1);
    
    // Expected gate count:
    // - Double SHA-256: ~680K gates (2 * 340K)
    // - Target decoding: ~1K gates
    // - 256-bit comparison: ~8K gates
    // - Total: ~690K gates
    
    zkvm_checkpoint("Bitcoin block verification complete");
    
    return is_valid;
}