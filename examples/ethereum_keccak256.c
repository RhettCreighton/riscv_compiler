/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ethereum_keccak256.c - Ethereum's Keccak-256 hash for circuits
 * 
 * Ethereum uses Keccak-256 (NOT SHA3-256) for hashing.
 * The difference is in the padding: Keccak uses 0x01, SHA3 uses 0x06.
 * 
 * This implements Keccak-256 optimized for gate circuits.
 */

#include "../include/zkvm.h"
#include <string.h>

// Keccak parameters
#define KECCAK_ROUNDS 24
#define KECCAK_RATE 1088  // Rate in bits for 256-bit output

// Round constants
static const uint64_t KECCAK_ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

// Rotation offsets
static const unsigned int ROTATION_OFFSETS[25] = {
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14
};

// Helper: Rotate left (for 64-bit values split into 32-bit pairs)
ZKVM_INLINE static void rotate_left_64(uint32_t* hi, uint32_t* lo, int n) GATES(640) {
    n &= 63;  // Ensure n is in range [0, 63]
    
    if (n == 0) return;
    
    if (n < 32) {
        uint32_t new_hi = (*hi << n) | (*lo >> (32 - n));
        uint32_t new_lo = (*lo << n) | (*hi >> (32 - n));
        *hi = new_hi;
        *lo = new_lo;
    } else {
        n -= 32;
        uint32_t new_hi = (*lo << n) | (*hi >> (32 - n));
        uint32_t new_lo = (*hi << n) | (*lo >> (32 - n));
        *hi = new_hi;
        *lo = new_lo;
    }
}

// Keccak-f[1600] permutation
static void keccak_f(uint32_t state[50]) {  // 25 * 2 = 50 (64-bit values as 32-bit pairs)
    uint32_t C[10];    // 5 * 2 = 10
    uint32_t D[10];    // 5 * 2 = 10
    uint32_t B[50];    // 25 * 2 = 50
    
    for (int round = 0; round < KECCAK_ROUNDS; round++) {
        // Theta step
        for (int x = 0; x < 5; x++) {
            C[x*2] = state[x*10] ^ state[x*10+2] ^ state[x*10+4] ^ state[x*10+6] ^ state[x*10+8];
            C[x*2+1] = state[x*10+1] ^ state[x*10+3] ^ state[x*10+5] ^ state[x*10+7] ^ state[x*10+9];
        }
        
        for (int x = 0; x < 5; x++) {
            uint32_t t_hi = C[((x+1)%5)*2+1];
            uint32_t t_lo = C[((x+1)%5)*2];
            rotate_left_64(&t_hi, &t_lo, 1);
            
            D[x*2] = C[((x+4)%5)*2] ^ t_lo;
            D[x*2+1] = C[((x+4)%5)*2+1] ^ t_hi;
        }
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                state[(x*5+y)*2] ^= D[x*2];
                state[(x*5+y)*2+1] ^= D[x*2+1];
            }
        }
        
        // Rho and Pi steps
        for (int i = 0; i < 50; i++) {
            B[i] = state[i];
        }
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                int src_idx = (x*5 + y) * 2;
                int dst_x = y;
                int dst_y = (2*x + 3*y) % 5;
                int dst_idx = (dst_x*5 + dst_y) * 2;
                
                uint32_t hi = B[src_idx+1];
                uint32_t lo = B[src_idx];
                rotate_left_64(&hi, &lo, ROTATION_OFFSETS[x*5+y]);
                
                state[dst_idx] = lo;
                state[dst_idx+1] = hi;
            }
        }
        
        // Chi step
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                B[x*2] = state[(x*5+y)*2];
                B[x*2+1] = state[(x*5+y)*2+1];
            }
            
            for (int x = 0; x < 5; x++) {
                int idx = (x*5 + y) * 2;
                state[idx] = B[x*2] ^ ((~B[((x+1)%5)*2]) & B[((x+2)%5)*2]);
                state[idx+1] = B[x*2+1] ^ ((~B[((x+1)%5)*2+1]) & B[((x+2)%5)*2+1]);
            }
        }
        
        // Iota step
        uint64_t rc = KECCAK_ROUND_CONSTANTS[round];
        state[0] ^= (uint32_t)rc;
        state[1] ^= (uint32_t)(rc >> 32);
    }
}

// Main Keccak-256 function
void zkvm_keccak256(const uint8_t* message, size_t length, uint8_t output[32]) {
    // Initialize state (1600 bits = 200 bytes = 50 uint32_t)
    uint32_t state[50] = {0};
    
    // Absorb phase
    size_t rate_bytes = KECCAK_RATE / 8;  // 136 bytes
    size_t offset = 0;
    
    // Process full blocks
    while (length >= rate_bytes) {
        // XOR message block into state
        for (size_t i = 0; i < rate_bytes; i++) {
            size_t state_idx = i / 4;
            size_t byte_idx = i % 4;
            state[state_idx] ^= ((uint32_t)message[offset + i]) << (byte_idx * 8);
        }
        
        keccak_f(state);
        offset += rate_bytes;
        length -= rate_bytes;
    }
    
    // Process final block with padding
    uint8_t final_block[200] = {0};  // Max size needed
    
    // Copy remaining message bytes
    for (size_t i = 0; i < length; i++) {
        final_block[i] = message[offset + i];
    }
    
    // Keccak padding: append 0x01, pad with zeros, end with 0x80
    final_block[length] = 0x01;
    final_block[rate_bytes - 1] |= 0x80;
    
    // XOR final block into state
    for (size_t i = 0; i < rate_bytes; i++) {
        size_t state_idx = i / 4;
        size_t byte_idx = i % 4;
        state[state_idx] ^= ((uint32_t)final_block[i]) << (byte_idx * 8);
    }
    
    keccak_f(state);
    
    // Squeeze phase - extract 256 bits (32 bytes)
    for (size_t i = 0; i < 32; i++) {
        size_t state_idx = i / 4;
        size_t byte_idx = i % 4;
        output[i] = (state[state_idx] >> (byte_idx * 8)) & 0xFF;
    }
}

// Example: Hash an Ethereum address
int main() {
    // Example: Hash an Ethereum address (without 0x prefix)
    const char* address = "742d35Cc6634C0532925a3b844Bc9e7595f8A49b";
    size_t len = 40;  // 20 bytes as hex = 40 chars
    
    // Convert hex string to bytes
    uint8_t addr_bytes[20];
    for (int i = 0; i < 20; i++) {
        uint8_t hi = (address[i*2] >= 'a') ? (address[i*2] - 'a' + 10) : 
                     (address[i*2] >= 'A') ? (address[i*2] - 'A' + 10) : 
                     (address[i*2] - '0');
        uint8_t lo = (address[i*2+1] >= 'a') ? (address[i*2+1] - 'a' + 10) : 
                     (address[i*2+1] >= 'A') ? (address[i*2+1] - 'A' + 10) : 
                     (address[i*2+1] - '0');
        addr_bytes[i] = (hi << 4) | lo;
    }
    
    // Hash the address
    uint8_t hash[32];
    zkvm_keccak256(addr_bytes, 20, hash);
    
    // Output the hash
    zkvm_output((uint32_t*)hash, 8);  // 32 bytes = 8 words
    
    // Expected gate count:
    // - Keccak-f[1600]: ~192K gates per round × 24 rounds = ~4.6M gates
    // - Padding and setup: ~10K gates
    // - Total: ~4.6M gates per block
    
    zkvm_checkpoint("Keccak-256 complete");
    
    return 0;
}