/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * bitcoin_merkle_verify.c - Bitcoin Merkle tree verification circuit
 * 
 * This circuit verifies that a transaction is included in a Bitcoin block
 * by checking the Merkle proof path from the transaction to the Merkle root.
 * 
 * Input structure:
 * - Transaction hash (32 bytes)
 * - Merkle root (32 bytes) 
 * - Merkle proof path (array of 32-byte hashes)
 * - Proof path directions (bit array - 0=left, 1=right)
 * - Proof depth (typically 1-20 for Bitcoin blocks)
 * 
 * Output: 1 if transaction is in the block, 0 otherwise
 */

#include "../include/zkvm.h"
#include <string.h>

// Import SHA-256 implementation
extern void zkvm_sha256(const uint8_t* message, size_t length, uint8_t output[32]);

// Helper: Double SHA-256 (Bitcoin's hash function)
ZKVM_INLINE static void double_sha256(const uint8_t* data, size_t len, uint8_t hash[32]) GATES(680000) {
    uint8_t temp[32];
    zkvm_sha256(data, len, temp);
    zkvm_sha256(temp, 32, hash);
}

// Helper: Combine two hashes with double SHA-256
// Bitcoin concatenates hashes and applies double SHA-256
ZKVM_INLINE static void merkle_combine(const uint8_t left[32], const uint8_t right[32], uint8_t output[32]) GATES(680000) {
    uint8_t combined[64];
    
    // Concatenate left and right hashes
    for (int i = 0; i < 32; i++) {
        combined[i] = left[i];
        combined[32 + i] = right[i];
    }
    
    // Double SHA-256 of concatenated hashes
    double_sha256(combined, 64, output);
}

// Main Merkle proof verification
// Returns 1 if proof is valid, 0 otherwise
uint32_t verify_merkle_proof(
    const uint8_t tx_hash[32],        // Transaction hash to verify
    const uint8_t merkle_root[32],    // Expected Merkle root
    const uint8_t proof[][32],        // Array of proof hashes
    const uint32_t* proof_directions, // Bit array of directions (0=left, 1=right)
    uint32_t proof_depth              // Number of levels in proof
) {
    uint8_t current_hash[32];
    uint8_t temp_hash[32];
    
    // Start with the transaction hash
    for (int i = 0; i < 32; i++) {
        current_hash[i] = tx_hash[i];
    }
    
    // Walk up the Merkle tree using the proof
    for (uint32_t level = 0; level < proof_depth; level++) {
        // Check direction bit
        uint32_t word_idx = level / 32;
        uint32_t bit_idx = level % 32;
        uint32_t is_right = (proof_directions[word_idx] >> bit_idx) & 1;
        
        // Combine current hash with proof hash
        // Order matters: if we're on the right, proof goes left
        if (is_right) {
            merkle_combine(proof[level], current_hash, temp_hash);
        } else {
            merkle_combine(current_hash, proof[level], temp_hash);
        }
        
        // Update current hash
        for (int i = 0; i < 32; i++) {
            current_hash[i] = temp_hash[i];
        }
    }
    
    // Compare final hash with expected Merkle root
    uint32_t is_equal = ONE;
    for (int i = 0; i < 32; i++) {
        // Check if bytes are equal
        uint32_t byte_equal = ~(current_hash[i] ^ merkle_root[i]);
        // Convert to single bit (all bits must be 1 for equality)
        for (int j = 0; j < 8; j++) {
            uint32_t bit = (byte_equal >> j) & 1;
            is_equal = is_equal & bit;
        }
    }
    
    return is_equal;
}

// Example: Verify a transaction in a Bitcoin block
int main() {
    // Example transaction hash (from a real Bitcoin transaction)
    uint8_t tx_hash[32] = {
        0x5f, 0xfd, 0xa5, 0x8e, 0x6d, 0x1a, 0x3b, 0x4f,
        0x8e, 0x2b, 0xd9, 0x7a, 0x12, 0x43, 0x0b, 0x68,
        0x79, 0x61, 0xf6, 0x3d, 0x57, 0x63, 0x6e, 0x9b,
        0x1d, 0x15, 0xc2, 0xba, 0x33, 0x36, 0xe6, 0x69
    };
    
    // Example Merkle root (from block header)
    uint8_t merkle_root[32] = {
        0x8b, 0x30, 0xc5, 0xf0, 0x6f, 0xe9, 0xf9, 0xa0,
        0x3e, 0x0e, 0xb2, 0xe4, 0x50, 0x44, 0x9f, 0x50,
        0x5a, 0xd7, 0xdc, 0x30, 0xc5, 0x5a, 0x1f, 0x0c,
        0xd9, 0x83, 0xf3, 0x78, 0xe7, 0x56, 0x6b, 0x7b
    };
    
    // Example Merkle proof (5 levels deep)
    uint8_t proof[5][32] = {
        // Level 0 - sibling hash
        {0x4e, 0x07, 0x64, 0x8e, 0xd4, 0xc2, 0xdf, 0x33,
         0x4f, 0x49, 0x3f, 0x30, 0x6a, 0x28, 0x19, 0x13,
         0x15, 0xb9, 0x1a, 0x42, 0x00, 0x96, 0x48, 0x4a,
         0xaa, 0x9e, 0xbb, 0xf8, 0x7e, 0x3b, 0x5f, 0xd8},
        
        // Level 1
        {0x12, 0x2e, 0x42, 0x9f, 0x08, 0xb0, 0x1e, 0xb3,
         0xcc, 0x63, 0xf1, 0x3a, 0x2f, 0x93, 0x5d, 0xde,
         0x61, 0x8f, 0x77, 0x51, 0xb4, 0xc9, 0x0a, 0x36,
         0xb5, 0xdc, 0x98, 0xa0, 0xf8, 0x4f, 0x1b, 0x1f},
         
        // Level 2
        {0x76, 0x21, 0xb0, 0x38, 0x4f, 0x3d, 0xd7, 0x0b,
         0x0a, 0xb6, 0x8e, 0x6e, 0xfd, 0x86, 0xb3, 0x7f,
         0x67, 0xad, 0x4a, 0x00, 0xec, 0x3d, 0x2a, 0x67,
         0xfa, 0x7f, 0x52, 0x5f, 0x6b, 0x57, 0x21, 0x5e},
         
        // Level 3
        {0xc3, 0xa5, 0x3f, 0x26, 0xaa, 0x7c, 0x00, 0x2d,
         0x1b, 0x16, 0xb8, 0x6f, 0x0e, 0xaf, 0xfd, 0x74,
         0x80, 0xdc, 0x9f, 0x2f, 0x3f, 0xd2, 0xef, 0x2f,
         0x53, 0xa8, 0xc0, 0x69, 0x05, 0x4e, 0xb5, 0xf5},
         
        // Level 4
        {0xe3, 0xb8, 0x41, 0x15, 0xc0, 0x57, 0x76, 0xdf,
         0xb7, 0x8d, 0x72, 0xd3, 0x5e, 0x1f, 0xab, 0x13,
         0x66, 0x82, 0xdf, 0xed, 0xa9, 0x65, 0xf8, 0xeb,
         0x3f, 0xfa, 0xf0, 0x59, 0xa9, 0xae, 0x95, 0x04}
    };
    
    // Proof directions (packed as bits)
    // For this example: left, right, right, left, right = 0b10110 = 22
    uint32_t proof_directions[1] = {22};
    
    // Verify the proof
    uint32_t is_valid = verify_merkle_proof(
        tx_hash,
        merkle_root,
        proof,
        proof_directions,
        5  // proof depth
    );
    
    // Output result
    zkvm_output(&is_valid, 1);
    
    // Expected gate count:
    // - 5 levels × 680K gates per combine = 3.4M gates
    // - Plus comparison logic ~1K gates
    // - Total: ~3.4M gates
    
    zkvm_checkpoint("Merkle proof verification complete");
    
    return is_valid;
}