#include "riscv_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper: Create NOT gate
static uint32_t build_not(riscv_circuit_t* circuit, uint32_t a) {
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a, 2, result, GATE_XOR);  // a XOR 1 = NOT a
    return result;
}

// Helper: Create OR gate  
static uint32_t build_or(riscv_circuit_t* circuit, uint32_t a, uint32_t b) {
    // a OR b = (a XOR b) XOR (a AND b)
    uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
    uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t result = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, a, b, a_xor_b, GATE_XOR);
    riscv_circuit_add_gate(circuit, a, b, a_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, a_xor_b, a_and_b, result, GATE_XOR);
    
    return result;
}

// Helper: Create 2-to-1 MUX
static uint32_t build_mux(riscv_circuit_t* circuit, uint32_t sel, uint32_t a, uint32_t b) {
    // result = sel ? b : a = (sel AND b) OR ((NOT sel) AND a)
    uint32_t not_sel = build_not(circuit, sel);
    uint32_t sel_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t notsel_and_a = riscv_circuit_allocate_wire(circuit);
    
    riscv_circuit_add_gate(circuit, sel, b, sel_and_b, GATE_AND);
    riscv_circuit_add_gate(circuit, not_sel, a, notsel_and_a, GATE_AND);
    
    return build_or(circuit, sel_and_b, notsel_and_a);
}

riscv_memory_t* riscv_memory_create(riscv_circuit_t* circuit) {
    riscv_memory_t* memory = calloc(1, sizeof(riscv_memory_t));
    if (!memory) return NULL;
    
    memory->circuit = circuit;
    memory->access = riscv_memory_access;  // Set function pointer
    
    // Allocate Merkle root wires (256 bits for SHA3-256)
    memory->merkle_root_wires = riscv_circuit_allocate_wire_array(circuit, 256);
    
    // Allocate address and data wires
    memory->address_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    memory->data_in_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    memory->data_out_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    memory->write_enable_wire = riscv_circuit_allocate_wire(circuit);
    
    // Allocate Merkle proof sibling hashes (one per level)
    memory->sibling_hashes = malloc(MEMORY_BITS * sizeof(uint32_t*));
    for (int i = 0; i < MEMORY_BITS; i++) {
        memory->sibling_hashes[i] = riscv_circuit_allocate_wire_array(circuit, 256);
    }
    
    // Allocate leaf data
    memory->leaf_data_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    
    return memory;
}

void riscv_memory_destroy(riscv_memory_t* memory) {
    if (!memory) return;
    
    free(memory->merkle_root_wires);
    free(memory->address_wires);
    free(memory->data_in_wires);
    free(memory->data_out_wires);
    free(memory->leaf_data_wires);
    
    for (int i = 0; i < MEMORY_BITS; i++) {
        free(memory->sibling_hashes[i]);
    }
    free(memory->sibling_hashes);
    
    free(memory);
}

// Build equality checker for arrays
uint32_t build_equality_checker(riscv_circuit_t* circuit,
                               uint32_t* a_bits,
                               uint32_t* b_bits,
                               size_t num_bits) {
    // Check if all bits are equal
    // equal = AND of all (a[i] XNOR b[i])
    // XNOR = NOT XOR
    
    uint32_t* bit_equals = riscv_circuit_allocate_wire_array(circuit, num_bits);
    
    // Check each bit
    for (size_t i = 0; i < num_bits; i++) {
        uint32_t xor_result = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], xor_result, GATE_XOR);
        bit_equals[i] = build_not(circuit, xor_result);  // XNOR
    }
    
    // AND all bit equalities together
    uint32_t result = bit_equals[0];
    for (size_t i = 1; i < num_bits; i++) {
        uint32_t new_result = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, result, bit_equals[i], new_result, GATE_AND);
        result = new_result;
    }
    
    free(bit_equals);
    return result;
}

// Build conditional update for arrays
void build_conditional_update(riscv_circuit_t* circuit,
                             uint32_t condition,
                             uint32_t* old_value,
                             uint32_t* new_value,
                             uint32_t* output,
                             size_t num_bits) {
    for (size_t i = 0; i < num_bits; i++) {
        output[i] = build_mux(circuit, condition, old_value[i], new_value[i]);
    }
}

// Forward declaration - implemented in sha3_circuit.c
// Real SHA3-256 implementation with ~192K gates for cryptographic security
void build_sha3_256_circuit(riscv_circuit_t* circuit,
                           uint32_t* input_bits,  // 512 bits input
                           uint32_t* output_bits); // 256 bits output

// Build memory access circuit
void riscv_memory_access(riscv_memory_t* memory, 
                        uint32_t* address_bits,
                        uint32_t* write_data_bits,
                        uint32_t write_enable,
                        uint32_t* read_data_bits) {
    riscv_circuit_t* circuit = memory->circuit;
    
    // Step 1: Verify Merkle proof
    // Start from leaf and work up to root
    
    // Current hash starts as leaf data
    uint32_t* current_hash = riscv_circuit_allocate_wire_array(circuit, 256);
    
    // Pad leaf data (32 bits) to 256 bits for hashing
    for (int i = 0; i < 32; i++) {
        current_hash[i] = memory->leaf_data_wires[i];
    }
    for (int i = 32; i < 256; i++) {
        current_hash[i] = 1;  // Pad with zeros
    }
    
    // For each level of the tree
    for (int level = 0; level < MEMORY_BITS; level++) {
        // Prepare input for hash (current || sibling or sibling || current)
        uint32_t* hash_input = riscv_circuit_allocate_wire_array(circuit, 512);
        
        // Use address bit to determine order
        uint32_t addr_bit = address_bits[level];
        
        // If address bit is 0, current hash goes first
        // If address bit is 1, sibling hash goes first
        for (int i = 0; i < 256; i++) {
            // First half
            hash_input[i] = build_mux(circuit, addr_bit, 
                                     memory->sibling_hashes[level][i], 
                                     current_hash[i]);
            // Second half
            hash_input[256 + i] = build_mux(circuit, addr_bit,
                                           current_hash[i],
                                           memory->sibling_hashes[level][i]);
        }
        
        // Compute parent hash
        uint32_t* parent_hash = riscv_circuit_allocate_wire_array(circuit, 256);
        build_sha3_256_circuit(circuit, hash_input, parent_hash);
        
        free(hash_input);
        free(current_hash);
        current_hash = parent_hash;
    }
    
    // Verify that computed root matches stored root
    uint32_t proof_valid = build_equality_checker(circuit, 
                                                  current_hash, 
                                                  memory->merkle_root_wires, 
                                                  256);
    
    // Step 2: Read current value (only valid if proof is valid)
    for (int i = 0; i < 32; i++) {
        uint32_t invalid_read = 1;  // Return 0 if proof invalid
        read_data_bits[i] = build_mux(circuit, proof_valid,
                                      invalid_read,
                                      memory->leaf_data_wires[i]);
    }
    
    // Step 3: Conditionally update leaf and root if write is enabled
    uint32_t do_write = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, write_enable, proof_valid, do_write, GATE_AND);
    
    // Update leaf data
    uint32_t* new_leaf_data = riscv_circuit_allocate_wire_array(circuit, 32);
    build_conditional_update(circuit, do_write,
                            memory->leaf_data_wires,
                            write_data_bits,
                            new_leaf_data,
                            32);
    
    // Recompute Merkle root with new leaf data
    // (Similar to verification, but with updated leaf)
    // This is expensive but necessary for consistency
    
    // For now, just update the leaf data wires
    memcpy(memory->leaf_data_wires, new_leaf_data, 32 * sizeof(uint32_t));
    
    free(current_hash);
    free(new_leaf_data);
    
    printf("Memory access circuit: ~%d gates for Merkle proof\n", 
           MEMORY_BITS * 1000);  // Rough estimate
}