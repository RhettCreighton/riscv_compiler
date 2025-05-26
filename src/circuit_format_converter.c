/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#include "riscv_compiler.h"
#include <stdio.h>
#include <stdlib.h>

// Convert RISC-V circuit to gate_computer format
// Format: layers of gates that can be evaluated in parallel
int riscv_circuit_to_gate_format(const riscv_circuit_t* circuit, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return -1;
    
    // Calculate layer assignment for each gate
    // A gate is in layer L if all its inputs are from layers < L
    size_t* gate_layers = calloc(circuit->num_gates, sizeof(size_t));
    size_t* wire_layers = calloc(circuit->next_wire_id, sizeof(size_t));
    size_t max_layer = 0;
    
    // Initialize input wires to layer 0
    for (uint32_t i = 0; i < circuit->next_wire_id; i++) {
        wire_layers[i] = 0;
    }
    
    // Calculate layer for each gate
    for (size_t i = 0; i < circuit->num_gates; i++) {
        const gate_t* gate = &circuit->gates[i];
        
        // Gate layer is max(input layers) + 1
        size_t left_layer = wire_layers[gate->left_input];
        size_t right_layer = wire_layers[gate->right_input];
        size_t gate_layer = (left_layer > right_layer ? left_layer : right_layer) + 1;
        
        gate_layers[i] = gate_layer;
        wire_layers[gate->output] = gate_layer;
        
        if (gate_layer > max_layer) {
            max_layer = gate_layer;
        }
    }
    
    // Count gates per layer
    size_t* gates_per_layer = calloc(max_layer + 1, sizeof(size_t));
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gates_per_layer[gate_layers[i]]++;
    }
    
    // Write circuit file header
    fprintf(f, "# RISC-V zkVM Circuit\n");
    fprintf(f, "# Generated from RISC-V instructions\n");
    fprintf(f, "\n");
    
    // Count actual inputs (constants + register bits + memory bits)
    size_t num_inputs = 2 + 32 * 32;  // 2 constants + 32 registers * 32 bits
    size_t num_outputs = 32 * 32;     // 32 registers * 32 bits
    
    fprintf(f, "input %zu\n", num_inputs);
    fprintf(f, "output %zu\n", num_outputs);
    fprintf(f, "gate %zu\n", circuit->num_gates);
    fprintf(f, "\n");
    
    // Write layers
    for (size_t layer = 1; layer <= max_layer; layer++) {
        if (gates_per_layer[layer] == 0) continue;
        
        fprintf(f, "layer %zu %zu\n", layer - 1, gates_per_layer[layer]);
        
        // Write gates in this layer
        for (size_t i = 0; i < circuit->num_gates; i++) {
            if (gate_layers[i] != layer) continue;
            
            const gate_t* gate = &circuit->gates[i];
            fprintf(f, "%u %u %u %d\n",
                    gate->left_input,
                    gate->right_input,
                    gate->output,
                    gate->type == GATE_AND ? 0 : 1);
        }
        fprintf(f, "\n");
    }
    
    free(gate_layers);
    free(wire_layers);
    free(gates_per_layer);
    fclose(f);
    
    printf("Converted circuit to gate_computer format:\n");
    printf("  Layers: %zu\n", max_layer);
    printf("  Total gates: %zu\n", circuit->num_gates);
    printf("  Inputs: %zu\n", num_inputs);
    printf("  Outputs: %zu\n", num_outputs);
    
    return 0;
}