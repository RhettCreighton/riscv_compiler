/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 * efficient_sum.c - Example of writing efficient code for circuits
 * 
 * This program demonstrates good practices for minimizing gate count
 * while computing the sum of an array with some conditions.
 */

#include "../include/zkvm.h"

// Compute sum of array elements that pass a condition
// Good example: uses bit operations instead of comparisons
uint32_t conditional_sum(const uint32_t* data, size_t len) {
    uint32_t sum = ZERO;  // Use constant 0 (free!)
    
    zkvm_checkpoint("Starting conditional sum");
    
    for (size_t i = 0; i < len; i++) {
        uint32_t value = data[i];
        
        // BAD: Using modulo (expensive!)
        // if (value % 2 == 0) { sum += value; }
        
        // GOOD: Using bit operations (cheap!)
        // Check if even: last bit is 0
        uint32_t is_even = ~(value & ONE);  // 33 gates
        
        // Branchless conditional add
        // If even: add value, else add 0
        uint32_t to_add = value & is_even;  // 32 gates
        sum += to_add;  // 224 gates
        
        // Total per iteration: 33 + 32 + 224 = 289 gates
        // vs. thousands of gates for modulo!
    }
    
    zkvm_checkpoint("Conditional sum complete");
    return sum;
}

// Example of efficient bit counting
uint32_t count_bits(uint32_t x) {
    // BAD: Loop through each bit
    // uint32_t count = 0;
    // for (int i = 0; i < 32; i++) {
    //     count += (x >> i) & 1;  // 640 + 224 gates per iteration!
    // }
    
    // GOOD: Use parallel bit counting
    // This is the "population count" algorithm
    x = x - ((x >> 1) & 0x55555555);  // 2-bit counts
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);  // 4-bit counts
    x = (x + (x >> 4)) & 0x0f0f0f0f;  // 8-bit counts
    x = x + (x >> 8);  // 16-bit counts
    x = x + (x >> 16);  // 32-bit count
    return x & 0x3f;  // Max 32 bits set
    
    // Total: ~500 gates vs 27,000+ for naive approach!
}

// Efficient array processing with chunking
void process_large_array(const uint32_t* input, size_t total_len, uint32_t* output) {
    // Process in chunks to stay within memory constraints
    const size_t CHUNK_SIZE = 256;  // Fits in simple memory tier
    
    uint32_t chunk_results[8];  // Store intermediate results
    size_t num_chunks = (total_len + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    for (size_t chunk = 0; chunk < num_chunks && chunk < 8; chunk++) {
        size_t start = chunk * CHUNK_SIZE;
        size_t end = start + CHUNK_SIZE;
        if (end > total_len) end = total_len;
        
        // Process this chunk
        uint32_t chunk_sum = ZERO;
        for (size_t i = start; i < end; i++) {
            chunk_sum ^= input[i];  // XOR is cheap (32 gates)
        }
        
        chunk_results[chunk] = chunk_sum;
    }
    
    // Combine chunk results
    uint32_t final_result = ZERO;
    for (size_t i = 0; i < num_chunks && i < 8; i++) {
        final_result ^= chunk_results[i];
    }
    
    *output = final_result;
}

int main() {
    // Example input data
    uint32_t data[8] = {
        0x12345678, 0x9ABCDEF0, 0x11111111, 0x22222222,
        0x33333333, 0x44444444, 0x55555555, 0x66666666
    };
    
    // Compute conditional sum
    uint32_t sum = conditional_sum(data, 8);
    zkvm_output_u32(sum);
    
    // Count bits in a value
    uint32_t bit_count = count_bits(0xAAAAAAAA);  // Pattern: 1010...
    zkvm_output_u32(bit_count);  // Should output 16
    
    // Process array efficiently
    uint32_t result;
    process_large_array(data, 8, &result);
    zkvm_output_u32(result);
    
    zkvm_report_gates();  // In debug mode, shows gate counts
    
    return 0;
}

/*
 * Compilation:
 * ./compile_to_circuit.sh efficient_sum.c -m simple --stats
 * 
 * Expected gate counts:
 * - conditional_sum: ~2,312 gates (8 iterations * 289 gates)
 * - count_bits: ~500 gates
 * - process_large_array: ~256 gates (8 XORs)
 * - Total: ~3,068 gates (very efficient!)
 */