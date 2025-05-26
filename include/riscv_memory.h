#ifndef RISCV_MEMORY_H
#define RISCV_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "riscv_compiler.h"

// Memory configuration
#define MEMORY_SIZE (1 << 20)  // 1MB of memory
#define MEMORY_BITS 20         // log2(MEMORY_SIZE)
#define WORD_SIZE 32          // 32-bit words

// Memory operation types
typedef enum {
    MEM_READ,
    MEM_WRITE
} mem_op_t;

// Memory subsystem interface
typedef struct riscv_memory_t riscv_memory_t;

// Memory access function pointer type
typedef void (*memory_access_fn)(riscv_memory_t* memory,
                                uint32_t* address_bits,
                                uint32_t* write_data_bits,
                                uint32_t write_enable,
                                uint32_t* read_data_bits);

// Memory subsystem using Merkle tree
struct riscv_memory_t {
    // Function pointer for memory access (allows different implementations)
    memory_access_fn access;
    
    // Merkle tree root (represents entire memory state)
    uint32_t* merkle_root_wires;  // 256 wires for SHA3-256 hash
    
    // Memory access interface
    uint32_t* address_wires;      // 32 wires for address
    uint32_t* data_in_wires;      // 32 wires for write data
    uint32_t* data_out_wires;     // 32 wires for read data
    uint32_t* write_enable_wire;  // 1 wire for write enable
    
    // Merkle proof wires
    uint32_t** sibling_hashes;    // MEMORY_BITS levels of 256-bit hashes
    uint32_t* leaf_data_wires;    // Current leaf data (before update)
    
    // Circuit context
    riscv_circuit_t* circuit;
};

// Memory API
riscv_memory_t* riscv_memory_create(riscv_circuit_t* circuit);
void riscv_memory_destroy(riscv_memory_t* memory);

// Simple memory API (no cryptographic proofs, ~2K gates instead of ~3.9M)
riscv_memory_t* riscv_memory_create_simple(riscv_circuit_t* circuit);
void riscv_memory_destroy_simple(riscv_memory_t* memory);
void riscv_memory_access_simple(riscv_memory_t* memory, 
                               uint32_t* address_bits,
                               uint32_t* write_data_bits,
                               uint32_t write_enable,
                               uint32_t* read_data_bits);

// Build memory access circuit
// This creates gates that:
// 1. Verify Merkle proof for the accessed address
// 2. Read the current value
// 3. Optionally write new value and update Merkle root
void riscv_memory_access(riscv_memory_t* memory, 
                        uint32_t* address_bits,
                        uint32_t* write_data_bits,
                        uint32_t write_enable,
                        uint32_t* read_data_bits);

// Helper: Build SHA3-256 circuit for hashing
// Input: 512 bits (2 x 256-bit children or leaf data)
// Output: 256 bits (hash)
void build_sha3_256_circuit(riscv_circuit_t* circuit,
                           uint32_t* input_bits,
                           uint32_t* output_bits);

// Helper: Build equality checker
// Returns 1 if all bits are equal, 0 otherwise
uint32_t build_equality_checker(riscv_circuit_t* circuit,
                               uint32_t* a_bits,
                               uint32_t* b_bits,
                               size_t num_bits);

// Helper: Build conditional update
// If condition is 1, output = new_value, else output = old_value
void build_conditional_update(riscv_circuit_t* circuit,
                             uint32_t condition,
                             uint32_t* old_value,
                             uint32_t* new_value,
                             uint32_t* output,
                             size_t num_bits);

#endif // RISCV_MEMORY_H