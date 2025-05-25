#include "riscv_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for circuit format converter
int riscv_circuit_to_gate_format(const riscv_circuit_t* circuit, const char* filename);

// Example: Compile a Fibonacci RISC-V program
// Compute fib(5) = 5
//
// RISC-V Assembly:
//   addi x1, x0, 0    # x1 = 0 (fib[0])
//   addi x2, x0, 1    # x2 = 1 (fib[1])
//   addi x3, x0, 5    # x3 = 5 (counter)
// loop:
//   add  x4, x1, x2   # x4 = x1 + x2
//   add  x1, x0, x2   # x1 = x2 (mv x1, x2)
//   add  x2, x0, x4   # x2 = x4 (mv x2, x4)
//   addi x3, x3, -1   # x3 = x3 - 1
//   bne  x3, x0, loop # if x3 != 0, goto loop
//   # Result in x2

// Helper to compile ADDI instruction
static void compile_addi(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, int32_t imm) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get source register wires
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    
    // Create immediate value wires
    uint32_t* imm_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        if (imm & (1 << i)) {
            imm_wires[i] = 2;  // Wire 2 is constant 1
        } else {
            imm_wires[i] = 1;  // Wire 1 is constant 0
        }
    }
    
    // Add rs1 + imm
    if (rd != 0) {
        uint32_t* result_wires = riscv_circuit_allocate_wire_array(circuit, 32);
        build_adder(circuit, rs1_wires, imm_wires, result_wires, 32);
        memcpy(compiler->reg_wires[rd], result_wires, 32 * sizeof(uint32_t));
        free(result_wires);
    }
    
    free(imm_wires);
}

int main() {
    printf("RISC-V Fibonacci to Gate Compiler\n");
    printf("=================================\n\n");
    
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return 1;
    }
    
    // Initialize all registers
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            if (reg == 0) {
                compiler->reg_wires[reg][bit] = 1;  // x0 is always 0
            } else {
                compiler->reg_wires[reg][bit] = riscv_circuit_allocate_wire(compiler->circuit);
            }
        }
    }
    
    // Set initial input values for registers
    // For simplicity, we'll initialize them as circuit inputs
    compiler->circuit->num_inputs = 2 + 32 * 32;  // 2 constants + 32 registers
    compiler->circuit->input_wires = malloc(compiler->circuit->num_inputs * sizeof(uint32_t));
    
    // Map constants
    compiler->circuit->input_wires[0] = 1;  // Constant 0
    compiler->circuit->input_wires[1] = 2;  // Constant 1
    
    // Map register inputs (for initial state)
    size_t input_idx = 2;
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            if (reg == 0) {
                // x0 is hardwired to 0
                compiler->circuit->input_wires[input_idx++] = 1;
            } else {
                compiler->circuit->input_wires[input_idx++] = compiler->reg_wires[reg][bit];
            }
        }
    }
    
    printf("Compiling Fibonacci program...\n\n");
    
    // Compile: addi x1, x0, 0
    printf("1. addi x1, x0, 0    # x1 = 0\n");
    compile_addi(compiler, 1, 0, 0);
    
    // Compile: addi x2, x0, 1
    printf("2. addi x2, x0, 1    # x2 = 1\n");
    compile_addi(compiler, 2, 0, 1);
    
    // Compile: addi x3, x0, 5
    printf("3. addi x3, x0, 5    # x3 = 5 (loop counter)\n");
    compile_addi(compiler, 3, 0, 5);
    
    // For demonstration, compile one iteration of the loop
    printf("\nLoop iteration:\n");
    
    // Compile: add x4, x1, x2
    printf("4. add x4, x1, x2    # x4 = x1 + x2\n");
    uint32_t add_x4_x1_x2 = 0x00208233;  // add x4, x1, x2
    riscv_compile_instruction(compiler, add_x4_x1_x2);
    
    // Compile: add x1, x0, x2 (move x2 to x1)
    printf("5. add x1, x0, x2    # x1 = x2\n");
    uint32_t add_x1_x0_x2 = 0x002000B3;  // add x1, x0, x2
    riscv_compile_instruction(compiler, add_x1_x0_x2);
    
    // Compile: add x2, x0, x4 (move x4 to x2)
    printf("6. add x2, x0, x4    # x2 = x4\n");
    uint32_t add_x2_x0_x4 = 0x00400133;  // add x2, x0, x4
    riscv_compile_instruction(compiler, add_x2_x0_x4);
    
    // Compile: addi x3, x3, -1
    printf("7. addi x3, x3, -1   # x3 = x3 - 1\n");
    compile_addi(compiler, 3, 3, -1);
    
    printf("\nCircuit statistics:\n");
    riscv_circuit_print_stats(compiler->circuit);
    
    // Set output wires (final register state)
    compiler->circuit->num_outputs = 32 * 32;  // All 32 registers
    compiler->circuit->output_wires = malloc(compiler->circuit->num_outputs * sizeof(uint32_t));
    
    size_t output_idx = 0;
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            compiler->circuit->output_wires[output_idx++] = compiler->reg_wires[reg][bit];
        }
    }
    
    // Convert to gate_computer format
    const char* output_file = "fibonacci_circuit.txt";
    printf("\nConverting to gate_computer format...\n");
    riscv_circuit_to_gate_format(compiler->circuit, output_file);
    
    printf("\nEstimates for full Fibonacci computation:\n");
    printf("  Instructions per loop: 5\n");
    printf("  Gates per loop: ~%zu\n", compiler->circuit->num_gates);
    printf("  Total loops for fib(10): 10\n");
    printf("  Estimated total gates: ~%zu\n", compiler->circuit->num_gates * 10);
    
    printf("\nNext steps:\n");
    printf("1. Load the circuit into gate_computer\n");
    printf("2. Set initial register values as inputs\n");
    printf("3. Generate BaseFold proof of execution\n");
    printf("4. Verify the proof and check output registers\n");
    
    riscv_compiler_destroy(compiler);
    return 0;
}