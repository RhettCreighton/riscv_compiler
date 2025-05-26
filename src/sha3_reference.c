/*
 * SHA3-256 Reference Implementation
 * 
 * A simple, readable implementation of SHA3-256 (Keccak)
 * for verification purposes.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// SHA3-256 parameters
#define SHA3_256_RATE 136      // Rate in bytes (1088 bits)
#define SHA3_256_CAPACITY 64   // Capacity in bytes (512 bits)
#define SHA3_256_OUTPUT 32     // Output size in bytes (256 bits)
#define KECCAK_ROUNDS 24       // Number of rounds

// Keccak state: 5x5 array of 64-bit lanes
typedef uint64_t keccak_state_t[25];

// Rotation constants
static const unsigned int r[25] = {
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14
};

// Round constants
static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

// Rotate left
static inline uint64_t rotl64(uint64_t x, unsigned int n) {
    return (x << n) | (x >> (64 - n));
}

// Keccak theta step
static void theta(keccak_state_t A) {
    uint64_t C[5], D[5];
    
    // Column parity
    for (int x = 0; x < 5; x++) {
        C[x] = A[x] ^ A[x + 5] ^ A[x + 10] ^ A[x + 15] ^ A[x + 20];
    }
    
    // Column theta effect
    for (int x = 0; x < 5; x++) {
        D[x] = C[(x + 4) % 5] ^ rotl64(C[(x + 1) % 5], 1);
    }
    
    // Apply theta
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            A[y * 5 + x] ^= D[x];
        }
    }
}

// Keccak rho and pi steps
static void rho_pi(keccak_state_t A) {
    uint64_t B[25];
    
    // Combined rho and pi
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            int index = y * 5 + x;
            int newX = y;
            int newY = (2 * x + 3 * y) % 5;
            int newIndex = newY * 5 + newX;
            B[newIndex] = rotl64(A[index], r[index]);
        }
    }
    
    memcpy(A, B, sizeof(B));
}

// Keccak chi step
static void chi(keccak_state_t A) {
    uint64_t B[25];
    
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            int index = y * 5 + x;
            B[index] = A[index] ^ ((~A[y * 5 + ((x + 1) % 5)]) & A[y * 5 + ((x + 2) % 5)]);
        }
    }
    
    memcpy(A, B, sizeof(B));
}

// Keccak iota step
static void iota(keccak_state_t A, int round) {
    A[0] ^= RC[round];
}

// Keccak-f[1600] permutation
static void keccak_f(keccak_state_t A) {
    for (int round = 0; round < KECCAK_ROUNDS; round++) {
        theta(A);
        rho_pi(A);
        chi(A);
        iota(A, round);
    }
}

// Absorb block into state
static void keccak_absorb_block(keccak_state_t state, const uint8_t* block) {
    uint64_t* state64 = (uint64_t*)state;
    const uint64_t* block64 = (const uint64_t*)block;
    
    // XOR block into state (little-endian)
    for (int i = 0; i < SHA3_256_RATE / 8; i++) {
        state64[i] ^= block64[i];
    }
    
    keccak_f(state);
}

// SHA3-256 hash function
void sha3_256(const uint8_t* input, size_t input_len, uint8_t* output) {
    keccak_state_t state;
    uint8_t block[SHA3_256_RATE];
    
    // Initialize state to zero
    memset(state, 0, sizeof(state));
    
    // Absorb full blocks
    size_t num_blocks = input_len / SHA3_256_RATE;
    for (size_t i = 0; i < num_blocks; i++) {
        keccak_absorb_block(state, input + i * SHA3_256_RATE);
    }
    
    // Handle last block with padding
    size_t remaining = input_len % SHA3_256_RATE;
    memset(block, 0, SHA3_256_RATE);
    memcpy(block, input + num_blocks * SHA3_256_RATE, remaining);
    
    // SHA3 padding: append 0x06, pad with zeros, end with 0x80
    block[remaining] = 0x06;
    block[SHA3_256_RATE - 1] |= 0x80;
    
    keccak_absorb_block(state, block);
    
    // Squeeze output
    memcpy(output, state, SHA3_256_OUTPUT);
}

// Convert bytes to hex string
void bytes_to_hex(const uint8_t* bytes, size_t len, char* hex) {
    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[len * 2] = '\0';
}

// Test SHA3-256
void test_sha3_256() {
    printf("=== SHA3-256 Reference Tests ===\n");
    
    // Test vectors
    struct {
        const char* input;
        const char* expected;
    } test_vectors[] = {
        // Empty string
        {
            "",
            "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"
        },
        // "abc"
        {
            "abc",
            "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"
        },
        // "The quick brown fox jumps over the lazy dog"
        {
            "The quick brown fox jumps over the lazy dog",
            "69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04"
        }
    };
    
    for (size_t i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++) {
        const char* input = test_vectors[i].input;
        const char* expected = test_vectors[i].expected;
        
        uint8_t output[SHA3_256_OUTPUT];
        char output_hex[SHA3_256_OUTPUT * 2 + 1];
        
        sha3_256((const uint8_t*)input, strlen(input), output);
        bytes_to_hex(output, SHA3_256_OUTPUT, output_hex);
        
        printf("\nTest %zu:\n", i + 1);
        printf("Input: \"%s\"\n", input);
        printf("Expected: %s\n", expected);
        printf("Got:      %s\n", output_hex);
        
        if (strcmp(output_hex, expected) == 0) {
            printf("✓ PASSED\n");
        } else {
            printf("✗ FAILED\n");
        }
    }
}

// Create a simple SHA3 computation for RISC-V
void create_sha3_risc_v_data(uint8_t* input_data, size_t* input_size,
                            uint8_t* expected_output) {
    // Use a simple test case: "abc"
    const char* test_input = "abc";
    *input_size = strlen(test_input);
    memcpy(input_data, test_input, *input_size);
    
    // Compute expected output
    sha3_256((const uint8_t*)test_input, *input_size, expected_output);
}

#ifdef TEST_SHA3_REFERENCE

int main() {
    test_sha3_256();
    return 0;
}

#endif