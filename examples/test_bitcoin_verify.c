/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * test_bitcoin_verify.c - Test the Bitcoin block verification circuit
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../include/riscv_compiler.h"

void zkvm_sha256(const uint8_t* message, size_t length, uint8_t output[32]) {
    // Stub implementation
    (void)message;
    (void)length;
    memset(output, 0, 32);
}

uint32_t verify_bitcoin_block_header(const uint8_t header[80]) {
    // This will be replaced by the circuit version
    (void)header;
    return 1;
}

int main() {
    printf("Bitcoin Block Verification Circuit Test\n");
    printf("======================================\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        printf("Failed to create compiler\n");
        return 1;
    }
    
    // Get the circuit
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Allocate input wires for 80-byte header (640 bits)
    uint32_t* header_bits = malloc(640 * sizeof(uint32_t));
    for (int i = 0; i < 640; i++) {
        header_bits[i] = riscv_circuit_allocate_wire(circuit);
    }
    
    // Allocate output wire
    uint32_t output_wire = riscv_circuit_allocate_wire(circuit);
    
    // Build a simple verification circuit
    // For now, just test that we can build gates
    
    // Example: XOR some input bits (testing basic functionality)
    uint32_t temp1 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, header_bits[0], header_bits[1], temp1, GATE_XOR);
    
    uint32_t temp2 = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, temp1, header_bits[2], temp2, GATE_XOR);
    
    // Connect to output (for testing)
    riscv_circuit_add_gate(circuit, temp2, CONSTANT_0_WIRE, output_wire, GATE_XOR);
    
    printf("Circuit Statistics:\n");
    printf("  Total gates: %zu\n", riscv_circuit_get_num_gates(circuit));
    printf("  Total wires: %u\n", circuit->next_wire_id);
    
    // In a full implementation, we would:
    // 1. Implement the full SHA-256 circuit
    // 2. Implement the difficulty comparison circuit
    // 3. Connect everything properly
    
    printf("\nNote: This is a simplified test. Full implementation would include:\n");
    printf("  - Complete SHA-256 circuit (~340K gates per hash)\n");
    printf("  - Double SHA-256 for Bitcoin (~680K gates)\n");
    printf("  - Difficulty target decoding (~1K gates)\n");
    printf("  - 256-bit comparison (~8K gates)\n");
    printf("  - Total: ~690K gates\n");
    
    // Cleanup
    free(header_bits);
    riscv_compiler_destroy(compiler);
    
    printf("\nTest completed successfully!\n");
    return 0;
}