/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>

// SHA3-256 circuit implementation
// This implements the full Keccak-f[1600] permutation used in SHA3
// Expected gate count: ~192,000 gates

// SHA3 parameters for 256-bit output
#define SHA3_256_RATE 1088  // Rate in bits (136 bytes)
#define SHA3_256_CAPACITY 512  // Capacity in bits
#define SHA3_STATE_SIZE 1600  // Total state size in bits (25 * 64)
#define SHA3_ROUNDS 24  // Number of Keccak rounds

// Helper: Create rotation circuit for 64-bit values
static void build_rotation_64(riscv_circuit_t* circuit, 
                             uint32_t* input_bits, 
                             uint32_t* output_bits,
                             int rotation_amount) {
    // Rotate a 64-bit value left by rotation_amount
    for (int i = 0; i < 64; i++) {
        int src_bit = (i - rotation_amount + 64) % 64;
        output_bits[i] = input_bits[src_bit];
    }
}

// Helper: Create XOR for 64-bit values
static void build_xor_64(riscv_circuit_t* circuit,
                        uint32_t* a_bits,
                        uint32_t* b_bits, 
                        uint32_t* output_bits) {
    for (int i = 0; i < 64; i++) {
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], output_bits[i], GATE_XOR);
    }
}

// Helper: Create AND for 64-bit values
static void build_and_64(riscv_circuit_t* circuit,
                        uint32_t* a_bits,
                        uint32_t* b_bits,
                        uint32_t* output_bits) {
    for (int i = 0; i < 64; i++) {
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], output_bits[i], GATE_AND);
    }
}

// Helper: Create NOT for 64-bit values
static void build_not_64(riscv_circuit_t* circuit,
                        uint32_t* input_bits,
                        uint32_t* output_bits) {
    for (int i = 0; i < 64; i++) {
        riscv_circuit_add_gate(circuit, input_bits[i], CONSTANT_1_WIRE, output_bits[i], GATE_XOR);
    }
}

// Keccak θ (theta) step: Column parity and XOR
static void build_keccak_theta(riscv_circuit_t* circuit, uint32_t** state) {
    // Allocate temporary storage for column parities
    uint32_t** C = malloc(5 * sizeof(uint32_t*));
    uint32_t** D = malloc(5 * sizeof(uint32_t*));
    
    for (int i = 0; i < 5; i++) {
        C[i] = riscv_circuit_allocate_wire_array(circuit, 64);
        D[i] = riscv_circuit_allocate_wire_array(circuit, 64);
    }
    
    // Step 1: Compute column parities C[x] = state[x,0] ⊕ state[x,1] ⊕ ... ⊕ state[x,4]
    for (int x = 0; x < 5; x++) {
        // Start with first row
        memcpy(C[x], state[x * 5 + 0], 64 * sizeof(uint32_t));
        
        // XOR with remaining rows
        for (int y = 1; y < 5; y++) {
            uint32_t* temp = riscv_circuit_allocate_wire_array(circuit, 64);
            build_xor_64(circuit, C[x], state[x * 5 + y], temp);
            memcpy(C[x], temp, 64 * sizeof(uint32_t));
            free(temp);
        }
    }
    
    // Step 2: Compute D[x] = C[(x+4)%5] ⊕ ROT(C[(x+1)%5], 1)
    for (int x = 0; x < 5; x++) {
        uint32_t* rotated = riscv_circuit_allocate_wire_array(circuit, 64);
        build_rotation_64(circuit, C[(x + 1) % 5], rotated, 1);
        build_xor_64(circuit, C[(x + 4) % 5], rotated, D[x]);
        free(rotated);
    }
    
    // Step 3: Apply θ transformation: state[x,y] = state[x,y] ⊕ D[x]
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            uint32_t* new_lane = riscv_circuit_allocate_wire_array(circuit, 64);
            build_xor_64(circuit, state[x * 5 + y], D[x], new_lane);
            memcpy(state[x * 5 + y], new_lane, 64 * sizeof(uint32_t));
            free(new_lane);
        }
    }
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        free(C[i]);
        free(D[i]);
    }
    free(C);
    free(D);
}

// Keccak ρ (rho) and π (pi) steps: Rotation and permutation
static void build_keccak_rho_pi(riscv_circuit_t* circuit, uint32_t** state) {
    // Rotation amounts for each position (from Keccak specification)
    static const int rho_offsets[25] = {
         0,  1, 62, 28, 27,
        36, 44,  6, 55, 20,
         3, 10, 43, 25, 39,
        41, 45, 15, 21,  8,
        18,  2, 61, 56, 14
    };
    
    uint32_t** new_state = malloc(25 * sizeof(uint32_t*));
    for (int i = 0; i < 25; i++) {
        new_state[i] = riscv_circuit_allocate_wire_array(circuit, 64);
    }
    
    // Apply ρ (rotation) and π (permutation) simultaneously
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            int src_index = x * 5 + y;
            int dst_x = y;
            int dst_y = (2 * x + 3 * y) % 5;
            int dst_index = dst_x * 5 + dst_y;
            
            build_rotation_64(circuit, state[src_index], new_state[dst_index], rho_offsets[src_index]);
        }
    }
    
    // Copy back to original state
    for (int i = 0; i < 25; i++) {
        memcpy(state[i], new_state[i], 64 * sizeof(uint32_t));
        free(new_state[i]);
    }
    free(new_state);
}

// Keccak χ (chi) step: Non-linear transformation
static void build_keccak_chi(riscv_circuit_t* circuit, uint32_t** state) {
    uint32_t** new_state = malloc(25 * sizeof(uint32_t*));
    for (int i = 0; i < 25; i++) {
        new_state[i] = riscv_circuit_allocate_wire_array(circuit, 64);
    }
    
    // For each lane: new[x,y] = old[x,y] ⊕ ((NOT old[x+1,y]) AND old[x+2,y])
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            int idx = x * 5 + y;
            int idx_x1 = ((x + 1) % 5) * 5 + y;
            int idx_x2 = ((x + 2) % 5) * 5 + y;
            
            uint32_t* not_x1 = riscv_circuit_allocate_wire_array(circuit, 64);
            uint32_t* and_result = riscv_circuit_allocate_wire_array(circuit, 64);
            
            build_not_64(circuit, state[idx_x1], not_x1);
            build_and_64(circuit, not_x1, state[idx_x2], and_result);
            build_xor_64(circuit, state[idx], and_result, new_state[idx]);
            
            free(not_x1);
            free(and_result);
        }
    }
    
    // Copy back to original state
    for (int i = 0; i < 25; i++) {
        memcpy(state[i], new_state[i], 64 * sizeof(uint32_t));
        free(new_state[i]);
    }
    free(new_state);
}

// Keccak ι (iota) step: Add round constant
static void build_keccak_iota(riscv_circuit_t* circuit, uint32_t** state, int round) {
    // Round constants for SHA3 (from specification)
    static const uint64_t round_constants[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
        0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
        0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
        0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
        0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
        0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
    };
    
    uint64_t rc = round_constants[round];
    
    // XOR the round constant with state[0,0] (position 0)
    for (int i = 0; i < 64; i++) {
        uint32_t constant_bit = ((rc >> i) & 1) ? CONSTANT_1_WIRE : CONSTANT_0_WIRE;
        uint32_t new_bit = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, state[0][i], constant_bit, new_bit, GATE_XOR);
        state[0][i] = new_bit;
    }
}

// Complete Keccak-f[1600] permutation
static void build_keccak_f(riscv_circuit_t* circuit, uint32_t** state) {
    for (int round = 0; round < SHA3_ROUNDS; round++) {
        build_keccak_theta(circuit, state);
        build_keccak_rho_pi(circuit, state);
        build_keccak_chi(circuit, state);
        build_keccak_iota(circuit, state, round);
    }
}

// Main SHA3-256 circuit builder
void build_sha3_256_circuit(riscv_circuit_t* circuit,
                           uint32_t* input_bits,  // 512 bits input
                           uint32_t* output_bits) // 256 bits output
{
    // Allocate Keccak state (25 lanes of 64 bits each = 1600 bits)
    uint32_t** state = malloc(25 * sizeof(uint32_t*));
    for (int i = 0; i < 25; i++) {
        state[i] = riscv_circuit_allocate_wire_array(circuit, 64);
        // Initialize to zero
        for (int j = 0; j < 64; j++) {
            state[i][j] = CONSTANT_0_WIRE;
        }
    }
    
    // Absorb input into state (512 bits = 8 lanes of 64 bits)
    for (int lane = 0; lane < 8; lane++) {
        for (int bit = 0; bit < 64; bit++) {
            int input_idx = lane * 64 + bit;
            if (input_idx < 512) {
                uint32_t new_bit = riscv_circuit_allocate_wire(circuit);
                riscv_circuit_add_gate(circuit, state[lane][bit], input_bits[input_idx], new_bit, GATE_XOR);
                state[lane][bit] = new_bit;
            }
        }
    }
    
    // Apply padding (simplified - real SHA3 has more complex padding)
    // Set the first bit after the message to 1
    if (512 < SHA3_256_RATE) {
        uint32_t new_bit = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, state[8][0], CONSTANT_1_WIRE, new_bit, GATE_XOR);
        state[8][0] = new_bit;
    }
    
    // Set the last bit of the rate to 1 (domain separation)
    int last_rate_lane = (SHA3_256_RATE - 1) / 64;
    int last_rate_bit = (SHA3_256_RATE - 1) % 64;
    if (last_rate_lane < 25) {
        uint32_t new_bit = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, state[last_rate_lane][last_rate_bit], CONSTANT_1_WIRE, new_bit, GATE_XOR);
        state[last_rate_lane][last_rate_bit] = new_bit;
    }
    
    // Apply Keccak-f permutation
    build_keccak_f(circuit, state);
    
    // Extract output (first 256 bits = 4 lanes of 64 bits)
    for (int lane = 0; lane < 4; lane++) {
        for (int bit = 0; bit < 64; bit++) {
            int output_idx = lane * 64 + bit;
            if (output_idx < 256) {
                output_bits[output_idx] = state[lane][bit];
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < 25; i++) {
        free(state[i]);
    }
    free(state);
}