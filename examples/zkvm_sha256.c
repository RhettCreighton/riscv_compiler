/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * zkvm_sha256.c - Example of efficient SHA-256 for circuits
 * 
 * This demonstrates how to write C code that compiles to an
 * efficient gate circuit. We implement SHA-256 using operations
 * that map well to gates.
 */

#include "../include/zkvm.h"

// SHA-256 constants (these compile to wiring, not gates!)
static const uint32_t K[64] = {
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

// Initial hash values
static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

// Rotate right - optimized for gates (640 gates for variable shift)
ZKVM_INLINE static uint32_t rotr(uint32_t x, int n) {
    // For constant shifts, compiler optimizes this
    return (x >> n) | (x << (32 - n));
}

// SHA-256 functions - these compile to efficient gates
ZKVM_INLINE static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) GATES(96) {
    // (x & y) ^ (~x & z)
    // Optimized version using only XOR and AND
    return (x & y) ^ ((~x) & z);
}

ZKVM_INLINE static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) GATES(160) {
    // (x & y) ^ (x & z) ^ (y & z)
    // This is the majority function
    return (x & y) ^ (x & z) ^ (y & z);
}

ZKVM_INLINE static uint32_t sigma0(uint32_t x) GATES(1920) {
    // rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22)
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

ZKVM_INLINE static uint32_t sigma1(uint32_t x) GATES(1920) {
    // rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25)
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

ZKVM_INLINE static uint32_t gamma0(uint32_t x) GATES(1920) {
    // rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3)
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

ZKVM_INLINE static uint32_t gamma1(uint32_t x) GATES(1920) {
    // rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10)
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// Process one 512-bit block
static void sha256_block(uint32_t state[8], const uint32_t block[16]) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    
    // Initialize working variables
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];
    
    // Prepare message schedule
    // First 16 words come from the block
    for (int i = 0; i < 16; i++) {
        W[i] = block[i];
    }
    
    // Extend to 64 words
    for (int i = 16; i < 64; i++) {
        // W[i] = gamma1(W[i-2]) + W[i-7] + gamma0(W[i-15]) + W[i-16]
        // Each iteration: ~1920*2 + 224*3 = 4512 gates
        W[i] = gamma1(W[i-2]) + W[i-7] + gamma0(W[i-15]) + W[i-16];
    }
    
    // Main compression loop
    for (int i = 0; i < 64; i++) {
        // T1 = h + sigma1(e) + ch(e,f,g) + K[i] + W[i]
        // Gates: 224 + 1920 + 96 + 224 + 224 = 2688 gates
        uint32_t T1 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];
        
        // T2 = sigma0(a) + maj(a,b,c)  
        // Gates: 1920 + 160 = 2080 gates
        uint32_t T2 = sigma0(a) + maj(a, b, c);
        
        // Update working variables (register moves, ~0 gates)
        h = g;
        g = f;
        f = e;
        e = d + T1;      // 224 gates
        d = c;
        c = b;
        b = a;
        a = T1 + T2;     // 224 gates
        
        // Total per iteration: ~5216 gates
    }
    
    // Add to hash state (8 * 224 = 1792 gates)
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
    
    // Total for one block: ~340,000 gates
}

// Public interface - hash a message
void zkvm_sha256(const uint8_t* message, size_t length, uint8_t output[32]) {
    uint32_t state[8];
    uint32_t block[16];
    size_t i;
    
    // Initialize state
    for (i = 0; i < 8; i++) {
        state[i] = H0[i];
    }
    
    // Process complete 512-bit blocks
    while (length >= 64) {
        // Convert bytes to words (big-endian)
        for (i = 0; i < 16; i++) {
            block[i] = ((uint32_t)message[i*4] << 24) |
                       ((uint32_t)message[i*4+1] << 16) |
                       ((uint32_t)message[i*4+2] << 8) |
                       ((uint32_t)message[i*4+3]);
        }
        
        sha256_block(state, block);
        message += 64;
        length -= 64;
    }
    
    // Handle final block with padding
    uint8_t final_block[64] = {0};
    for (i = 0; i < length; i++) {
        final_block[i] = message[i];
    }
    
    // Add padding
    final_block[length] = 0x80;
    
    // If no room for length, need extra block
    if (length >= 56) {
        // Convert and process this block
        for (i = 0; i < 16; i++) {
            block[i] = ((uint32_t)final_block[i*4] << 24) |
                       ((uint32_t)final_block[i*4+1] << 16) |
                       ((uint32_t)final_block[i*4+2] << 8) |
                       ((uint32_t)final_block[i*4+3]);
        }
        sha256_block(state, block);
        
        // Clear for next block
        for (i = 0; i < 64; i++) {
            final_block[i] = 0;
        }
    }
    
    // Add length in bits to final block
    uint64_t bit_length = length * 8;
    final_block[56] = (bit_length >> 56) & 0xFF;
    final_block[57] = (bit_length >> 48) & 0xFF;
    final_block[58] = (bit_length >> 40) & 0xFF;
    final_block[59] = (bit_length >> 32) & 0xFF;
    final_block[60] = (bit_length >> 24) & 0xFF;
    final_block[61] = (bit_length >> 16) & 0xFF;
    final_block[62] = (bit_length >> 8) & 0xFF;
    final_block[63] = bit_length & 0xFF;
    
    // Process final block
    for (i = 0; i < 16; i++) {
        block[i] = ((uint32_t)final_block[i*4] << 24) |
                   ((uint32_t)final_block[i*4+1] << 16) |
                   ((uint32_t)final_block[i*4+2] << 8) |
                   ((uint32_t)final_block[i*4+3]);
    }
    sha256_block(state, block);
    
    // Convert state to output bytes
    for (i = 0; i < 8; i++) {
        output[i*4] = (state[i] >> 24) & 0xFF;
        output[i*4+1] = (state[i] >> 16) & 0xFF;
        output[i*4+2] = (state[i] >> 8) & 0xFF;
        output[i*4+3] = state[i] & 0xFF;
    }
}

// Example usage
int main() {
    // Input message
    const char* message = "Hello, zkVM!";
    size_t len = 12;
    
    // Output buffer
    uint8_t hash[32];
    
    // Compute hash
    zkvm_sha256((const uint8_t*)message, len, hash);
    
    // Output the hash
    zkvm_output((uint32_t*)hash, 8);  // 32 bytes = 8 words
    
    // Total gates estimate:
    // - One block: ~340K gates
    // - Padding logic: ~10K gates  
    // - Total: ~350K gates for this example
    
    zkvm_checkpoint("SHA-256 complete");
    
    return 0;
}