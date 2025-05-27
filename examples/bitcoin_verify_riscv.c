/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * bitcoin_verify_riscv.c - Bitcoin verification via RISC-V compilation
 * 
 * This version compiles to RISC-V instructions first, then to gates.
 * Compare with bitcoin_block_verify.c which uses direct gate generation.
 */

#include <stdint.h>
#include <string.h>

// Standard C implementation of SHA-256 that will compile to RISC-V
static uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t rotr(uint32_t x, int n) {
    // This compiles to RISC-V shift instructions (SRLI, SLLI, OR)
    return (x >> n) | (x << (32 - n));
}

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    
    // This loop compiles to RISC-V load instructions (LW, LBU)
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i*4] << 24) |
               ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) |
               ((uint32_t)block[i*4+3]);
    }
    
    // Message schedule - uses RISC-V arithmetic (ADD, XOR)
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(W[i-15], 7) ^ rotr(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = rotr(W[i-2], 17) ^ rotr(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    
    // Initialize working variables - RISC-V register moves
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    // Main loop - heavy use of RISC-V arithmetic
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        T1 = h + S1 + ch + K[i] + W[i];
        
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        T2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }
    
    // Update state - RISC-V ADD instructions
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha256(const uint8_t* data, size_t len, uint8_t hash[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    uint8_t block[64];
    size_t i;
    
    // Process full blocks
    while (len >= 64) {
        memcpy(block, data, 64);  // Compiles to RISC-V memory ops
        sha256_transform(state, block);
        data += 64;
        len -= 64;
    }
    
    // Handle final block with padding
    memset(block, 0, 64);
    memcpy(block, data, len);
    block[len] = 0x80;
    
    if (len >= 56) {
        sha256_transform(state, block);
        memset(block, 0, 64);
    }
    
    // Append length - RISC-V shift and store operations
    uint64_t bit_len = len * 8;
    for (i = 0; i < 8; i++) {
        block[63 - i] = bit_len >> (i * 8);
    }
    
    sha256_transform(state, block);
    
    // Output hash - RISC-V store operations
    for (i = 0; i < 8; i++) {
        hash[i*4] = state[i] >> 24;
        hash[i*4+1] = state[i] >> 16;
        hash[i*4+2] = state[i] >> 8;
        hash[i*4+3] = state[i];
    }
}

// Simple Bitcoin header verification
int verify_bitcoin_header(const uint8_t header[80]) {
    uint8_t hash1[32], hash2[32];
    
    // Double SHA-256
    sha256(header, 80, hash1);
    sha256(hash1, 32, hash2);
    
    // Extract difficulty target (simplified)
    uint32_t bits = ((uint32_t)header[72]) |
                    ((uint32_t)header[73] << 8) |
                    ((uint32_t)header[74] << 16) |
                    ((uint32_t)header[75] << 24);
    
    // Very simplified check - just verify first few bytes are zero
    // Real implementation would decode full target
    int zeros_needed = (bits >> 24) - 3;
    
    for (int i = 31; i >= 32 - zeros_needed && i >= 0; i--) {
        if (hash2[i] != 0) return 0;
    }
    
    return 1;
}

int main() {
    // Example Bitcoin block header
    uint8_t header[80] = {
        // Version
        0x01, 0x00, 0x00, 0x00,
        // Previous block hash (32 bytes)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Merkle root (32 bytes)
        0x3b, 0xa3, 0xed, 0xfd, 0x7a, 0x7b, 0x12, 0xb2,
        0x7a, 0xc7, 0x2c, 0x3e, 0x67, 0x76, 0x8f, 0x61,
        0x7f, 0xc8, 0x1b, 0xc3, 0x88, 0x8a, 0x51, 0x32,
        0x3a, 0x9f, 0xb8, 0xaa, 0x4b, 0x1e, 0x5e, 0x4a,
        // Timestamp
        0x29, 0xab, 0x5f, 0x49,
        // Bits (difficulty)
        0xff, 0xff, 0x00, 0x1d,
        // Nonce
        0x1d, 0xac, 0x2b, 0x7c
    };
    
    int valid = verify_bitcoin_header(header);
    
    // Return result
    return valid ? 0 : 1;
}

/*
 * RISC-V Compilation Analysis:
 * 
 * This compiles to approximately:
 * - SHA-256 transform: ~5,000 RISC-V instructions per block
 * - Double SHA-256: ~10,000 instructions
 * - Each RISC-V instruction: 50-500 gates (average ~200)
 * - Total: ~2,000,000 gates (vs 690K for direct approach)
 * 
 * The RISC-V path is LESS efficient because:
 * 1. Generic instructions vs optimized gates
 * 2. Register spilling and memory access overhead
 * 3. No instruction fusion opportunities
 * 4. Must maintain full RISC-V state
 */