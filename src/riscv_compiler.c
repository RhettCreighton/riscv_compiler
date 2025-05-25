#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Extract instruction fields
#define GET_OPCODE(instr) ((instr) & 0x7F)
#define GET_RD(instr)     (((instr) >> 7) & 0x1F)
#define GET_FUNCT3(instr) (((instr) >> 12) & 0x7)
#define GET_RS1(instr)    (((instr) >> 15) & 0x1F)
#define GET_RS2(instr)    (((instr) >> 20) & 0x1F)
#define GET_FUNCT7(instr) (((instr) >> 25) & 0x7F)
#define GET_IMM_I(instr)  ((int32_t)(instr) >> 20)
#define GET_IMM_S(instr)  ((((int32_t)(instr) >> 20) & ~0x1F) | (((instr) >> 7) & 0x1F))
#define GET_IMM_B(instr)  ((((int32_t)(instr) >> 19) & ~0xFFF) | \
                           (((instr) << 4) & 0x800) | \
                           (((instr) >> 20) & 0x7E0) | \
                           (((instr) >> 7) & 0x1E))
#define GET_IMM_U(instr)  ((instr) & ~0xFFF)
#define GET_IMM_J(instr)  ((((int32_t)(instr) >> 11) & ~0xFFFFF) | \
                           ((instr) & 0xFF000) | \
                           (((instr) >> 9) & 0x800) | \
                           (((instr) >> 20) & 0x7FE))

riscv_compiler_t* riscv_compiler_create(void) {
    riscv_compiler_t* compiler = calloc(1, sizeof(riscv_compiler_t));
    if (!compiler) return NULL;
    
    compiler->circuit = calloc(1, sizeof(riscv_circuit_t));
    if (!compiler->circuit) {
        free(compiler);
        return NULL;
    }
    
    // Initial circuit capacity
    compiler->circuit->capacity = 1000000;  // Start with 1M gates
    compiler->circuit->gates = calloc(compiler->circuit->capacity, sizeof(gate_t));
    if (!compiler->circuit->gates) {
        free(compiler->circuit);
        free(compiler);
        return NULL;
    }
    
    // Initialize wire counter - wires 0,1 are reserved for constants by convention
    // All circuits use this same standard: input bit 0=constant 0, input bit 1=constant 1
    compiler->circuit->next_wire_id = 2;  // Start allocating from wire 2+
    
    // Constants are handled by circuit input convention:
    // - Every circuit's input bit 0 = constant 0 (false)  
    // - Every circuit's input bit 1 = constant 1 (true)
    // No gate generation needed - these are inputs by definition
    
    // Initialize register wire arrays
    for (int i = 0; i < 32; i++) {
        compiler->reg_wires[i] = calloc(32, sizeof(uint32_t));
        if (!compiler->reg_wires[i]) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free(compiler->reg_wires[j]);
            }
            free(compiler->circuit->gates);
            free(compiler->circuit);
            free(compiler);
            return NULL;
        }
        
        // Initialize register wires to point to input bits
        for (int bit = 0; bit < 32; bit++) {
            compiler->reg_wires[i][bit] = get_register_wire(i, bit);
        }
    }
    
    // Initialize PC wire array
    compiler->pc_wires = calloc(32, sizeof(uint32_t));
    if (!compiler->pc_wires) {
        for (int i = 0; i < 32; i++) {
            free(compiler->reg_wires[i]);
        }
        free(compiler->circuit->gates);
        free(compiler->circuit);
        free(compiler);
        return NULL;
    }
    
    // Initialize PC wires to point to input bits
    for (int bit = 0; bit < 32; bit++) {
        compiler->pc_wires[bit] = get_pc_wire(bit);
    }
    
    return compiler;
}

void riscv_compiler_destroy(riscv_compiler_t* compiler) {
    if (!compiler) return;
    
    if (compiler->circuit) {
        free(compiler->circuit->gates);
        free(compiler->circuit->input_bits);
        free(compiler->circuit->output_bits);
        free(compiler->circuit);
    }
    
    if (compiler->initial_state) {
        free(compiler->initial_state->memory);
        free(compiler->initial_state);
    }
    
    if (compiler->final_state) {
        free(compiler->final_state->memory);
        free(compiler->final_state);
    }
    
    // Free register wire arrays
    for (int i = 0; i < 32; i++) {
        free(compiler->reg_wires[i]);
    }
    
    // Free PC wire array
    free(compiler->pc_wires);
    
    free(compiler);
}

// Create circuit with specified input/output sizes and bounds checking
riscv_circuit_t* riscv_circuit_create(size_t num_inputs, size_t num_outputs) {
    // Bounds checking with helpful error messages
    if (num_inputs > MAX_INPUT_BITS) {
        fprintf(stderr, "\n❌ ERROR: Circuit input size exceeds zkVM limits\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Requested input:  %zu bits (%.2f MB)\n", 
                num_inputs, num_inputs / (8.0 * 1024 * 1024));
        fprintf(stderr, "Maximum allowed:  %d bits (%.2f MB)\n", 
                MAX_INPUT_BITS, MAX_INPUT_BITS / (8.0 * 1024 * 1024));
        fprintf(stderr, "\n");
        fprintf(stderr, "This happens when your program needs too much memory.\n");
        fprintf(stderr, "The zkVM has a hard limit of 10MB for input data.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "To fix this:\n");
        fprintf(stderr, "  1. Reduce memory allocation in your program\n");
        fprintf(stderr, "  2. Process data in smaller chunks\n");
        fprintf(stderr, "  3. Use the memory_aware_example for guidance\n");
        return NULL;
    }
    if (num_outputs > MAX_OUTPUT_BITS) {
        fprintf(stderr, "\n❌ ERROR: Circuit output size exceeds zkVM limits\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Requested output: %zu bits (%.2f MB)\n", 
                num_outputs, num_outputs / (8.0 * 1024 * 1024));
        fprintf(stderr, "Maximum allowed:  %d bits (%.2f MB)\n", 
                MAX_OUTPUT_BITS, MAX_OUTPUT_BITS / (8.0 * 1024 * 1024));
        fprintf(stderr, "\n");
        fprintf(stderr, "The zkVM output is limited to 10MB to ensure\n");
        fprintf(stderr, "efficient proof generation and verification.\n");
        return NULL;
    }
    
    riscv_circuit_t* circuit = calloc(1, sizeof(riscv_circuit_t));
    if (!circuit) return NULL;
    
    // Allocate input/output arrays to exact sizes needed
    circuit->input_bits = calloc(num_inputs, sizeof(bool));
    circuit->output_bits = calloc(num_outputs, sizeof(bool));
    if (!circuit->input_bits || !circuit->output_bits) {
        free(circuit->input_bits);
        free(circuit->output_bits);
        free(circuit);
        return NULL;
    }
    
    circuit->num_inputs = num_inputs;
    circuit->num_outputs = num_outputs;
    circuit->max_inputs = MAX_INPUT_BITS;
    circuit->max_outputs = MAX_OUTPUT_BITS;
    
    // Initialize constants
    if (num_inputs >= 2) {
        circuit->input_bits[CONSTANT_0_WIRE] = false;  // Bit 0: constant 0
        circuit->input_bits[CONSTANT_1_WIRE] = true;   // Bit 1: constant 1
    }
    
    // Initialize gate array
    circuit->capacity = 1000000;  // Start with 1M gates
    circuit->gates = calloc(circuit->capacity, sizeof(gate_t));
    if (!circuit->gates) {
        free(circuit->input_bits);
        free(circuit->output_bits);
        free(circuit);
        return NULL;
    }
    
    // Wire IDs start after input bits
    circuit->next_wire_id = num_inputs;
    circuit->max_wire_id = num_inputs;
    
    return circuit;
}

// Calculate required input size for RISC-V state
size_t calculate_riscv_input_size(const riscv_state_t* state) {
    return 2 +                    // Constants (0, 1)
           PC_BITS +              // PC (32 bits)
           REGS_BITS +            // Registers (32 * 32 = 1024 bits)
           (state->memory_size * 8); // Memory (8 bits per byte)
}

// Calculate required output size for RISC-V state
size_t calculate_riscv_output_size(const riscv_state_t* state) {
    return PC_BITS +              // PC (32 bits)
           REGS_BITS +            // Registers (32 * 32 = 1024 bits)
           (state->memory_size * 8); // Memory (8 bits per byte)
}

// Encode RISC-V state into input bits
void encode_riscv_state_to_input(const riscv_state_t* state, bool* input_bits) {
    size_t bit_idx = 0;
    
    // Constants are already set during circuit creation
    bit_idx = 2;
    
    // Encode PC (32 bits, LSB first)
    for (int i = 0; i < PC_BITS; i++) {
        input_bits[bit_idx++] = (state->pc >> i) & 1;
    }
    
    // Encode registers (32 registers × 32 bits each, LSB first)
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            input_bits[bit_idx++] = (state->regs[reg] >> bit) & 1;
        }
    }
    
    // Encode memory (8 bits per byte, LSB first)
    for (size_t addr = 0; addr < state->memory_size; addr++) {
        for (int bit = 0; bit < 8; bit++) {
            input_bits[bit_idx++] = (state->memory[addr] >> bit) & 1;
        }
    }
}

// Decode RISC-V state from output bits
void decode_riscv_state_from_output(const bool* output_bits, riscv_state_t* state) {
    size_t bit_idx = 0;
    
    // Decode PC (32 bits, LSB first)
    state->pc = 0;
    for (int i = 0; i < PC_BITS; i++) {
        if (output_bits[bit_idx++]) {
            state->pc |= (1U << i);
        }
    }
    
    // Decode registers (32 registers × 32 bits each, LSB first)
    for (int reg = 0; reg < 32; reg++) {
        state->regs[reg] = 0;
        for (int bit = 0; bit < 32; bit++) {
            if (output_bits[bit_idx++]) {
                state->regs[reg] |= (1U << bit);
            }
        }
    }
    
    // Decode memory (8 bits per byte, LSB first)
    for (size_t addr = 0; addr < state->memory_size; addr++) {
        state->memory[addr] = 0;
        for (int bit = 0; bit < 8; bit++) {
            if (output_bits[bit_idx++]) {
                state->memory[addr] |= (1U << bit);
            }
        }
    }
}

// Wire mapping helper functions
uint32_t get_pc_wire(int bit) {
    return PC_START_BIT + bit;
}

uint32_t get_register_wire(int reg, int bit) {
    return REGS_START_BIT + (reg * 32) + bit;
}

uint32_t get_memory_wire(uint32_t addr, int bit) {
    return MEMORY_START_BIT + (addr * 8) + bit;
}

uint32_t riscv_circuit_allocate_wire(riscv_circuit_t* circuit) {
    return circuit->next_wire_id++;
}

uint32_t* riscv_circuit_allocate_wire_array(riscv_circuit_t* circuit, size_t count) {
    uint32_t* wires = malloc(count * sizeof(uint32_t));
    if (!wires) return NULL;
    
    for (size_t i = 0; i < count; i++) {
        wires[i] = riscv_circuit_allocate_wire(circuit);
    }
    
    return wires;
}

void riscv_circuit_add_gate(riscv_circuit_t* circuit, uint32_t left, uint32_t right, 
                            uint32_t output, gate_type_t type) {
    if (circuit->num_gates >= circuit->capacity) {
        // Resize the circuit
        size_t new_capacity = circuit->capacity * 2;
        gate_t* new_gates = realloc(circuit->gates, new_capacity * sizeof(gate_t));
        if (!new_gates) {
            fprintf(stderr, "Failed to resize circuit\n");
            return;
        }
        circuit->gates = new_gates;
        circuit->capacity = new_capacity;
    }
    
    circuit->gates[circuit->num_gates].left_input = left;
    circuit->gates[circuit->num_gates].right_input = right;
    circuit->gates[circuit->num_gates].output = output;
    circuit->gates[circuit->num_gates].type = type;
    circuit->num_gates++;
}

// Build a full adder: sum = a XOR b XOR cin, cout = (a AND b) OR (cin AND (a XOR b))
static void build_full_adder(riscv_circuit_t* circuit, uint32_t a, uint32_t b, uint32_t cin,
                            uint32_t* sum, uint32_t* cout) {
    // Allocate intermediate wires
    uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
    uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
    uint32_t cin_and_axorb = riscv_circuit_allocate_wire(circuit);
    
    // a XOR b
    riscv_circuit_add_gate(circuit, a, b, a_xor_b, GATE_XOR);
    
    // sum = (a XOR b) XOR cin
    *sum = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a_xor_b, cin, *sum, GATE_XOR);
    
    // a AND b
    riscv_circuit_add_gate(circuit, a, b, a_and_b, GATE_AND);
    
    // cin AND (a XOR b)
    riscv_circuit_add_gate(circuit, cin, a_xor_b, cin_and_axorb, GATE_AND);
    
    // cout = (a AND b) OR (cin AND (a XOR b))
    // Since we only have AND/XOR, we use: a OR b = (a XOR b) XOR (a AND b)
    uint32_t or_inputs_xor = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a_and_b, cin_and_axorb, or_inputs_xor, GATE_XOR);
    
    uint32_t or_inputs_and = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, a_and_b, cin_and_axorb, or_inputs_and, GATE_AND);
    
    *cout = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, or_inputs_xor, or_inputs_and, *cout, GATE_XOR);
}

// Forward declaration for optimized implementation
uint32_t build_sparse_kogge_stone_adder(riscv_circuit_t* circuit,
                                       uint32_t* a_bits, uint32_t* b_bits,
                                       uint32_t* sum_bits, size_t num_bits);


// Keep old ripple-carry adder for comparison/fallback
uint32_t build_ripple_carry_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                                  uint32_t* sum_bits, size_t num_bits) {
    uint32_t carry = CONSTANT_0_WIRE;  // Start with constant 0 (no initial carry)
    
    for (size_t i = 0; i < num_bits; i++) {
        uint32_t new_carry;
        build_full_adder(circuit, a_bits[i], b_bits[i], carry, &sum_bits[i], &new_carry);
        carry = new_carry;
    }
    
    return carry;  // Return final carry out
}

// Main adder function - uses ripple-carry for minimal gate count
uint32_t build_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                     uint32_t* sum_bits, size_t num_bits) {
    // For zkVM, gate count matters more than circuit depth
    // Ripple-carry: 224 gates (7 per bit)
    // Sparse Kogge-Stone: 396 gates (12.4 per bit)
    return build_ripple_carry_adder(circuit, a_bits, b_bits, sum_bits, num_bits);
}

uint32_t build_subtractor(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                          uint32_t* diff_bits, size_t num_bits) {
    // a - b = a + (~b) + 1
    // First, invert b
    uint32_t* b_inverted = riscv_circuit_allocate_wire_array(circuit, num_bits);
    for (size_t i = 0; i < num_bits; i++) {
        // NOT b[i] = b[i] XOR 1
        riscv_circuit_add_gate(circuit, b_bits[i], CONSTANT_1_WIRE, b_inverted[i], GATE_XOR);
    }
    
    // Add with carry-in = 1
    uint32_t carry = CONSTANT_1_WIRE;
    
    for (size_t i = 0; i < num_bits; i++) {
        uint32_t new_carry;
        build_full_adder(circuit, a_bits[i], b_inverted[i], carry, &diff_bits[i], &new_carry);
        carry = new_carry;
    }
    
    free(b_inverted);
    return carry;
}

// Compile ADD instruction: rd = rs1 + rs2
static void compile_add(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get wire arrays for source registers
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    uint32_t* rs2_wires = compiler->reg_wires[rs2];
    uint32_t* rd_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    
    // Build 32-bit adder
    build_adder(circuit, rs1_wires, rs2_wires, rd_wires, 32);
    
    // Update register wire mapping (skip x0 which is always 0)
    if (rd != 0) {
        memcpy(compiler->reg_wires[rd], rd_wires, 32 * sizeof(uint32_t));
    }
    
    free(rd_wires);
}

// Compile XOR instruction: rd = rs1 ^ rs2
static void compile_xor(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get wire arrays for source registers
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    uint32_t* rs2_wires = compiler->reg_wires[rs2];
    
    // XOR each bit
    if (rd != 0) {  // Skip x0
        for (int i = 0; i < 32; i++) {
            uint32_t result_wire = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, rs1_wires[i], rs2_wires[i], result_wire, GATE_XOR);
            compiler->reg_wires[rd][i] = result_wire;
        }
    }
}

// Compile AND instruction: rd = rs1 & rs2
static void compile_and(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get wire arrays for source registers
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    uint32_t* rs2_wires = compiler->reg_wires[rs2];
    
    // AND each bit
    if (rd != 0) {  // Skip x0
        for (int i = 0; i < 32; i++) {
            uint32_t result_wire = riscv_circuit_allocate_wire(circuit);
            riscv_circuit_add_gate(circuit, rs1_wires[i], rs2_wires[i], result_wire, GATE_AND);
            compiler->reg_wires[rd][i] = result_wire;
        }
    }
}

// Compile SUB instruction: rd = rs1 - rs2
static void compile_sub(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get wire arrays for source registers
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    uint32_t* rs2_wires = compiler->reg_wires[rs2];
    uint32_t* rd_wires = riscv_circuit_allocate_wire_array(circuit, 32);
    
    // Build 32-bit subtractor
    build_subtractor(circuit, rs1_wires, rs2_wires, rd_wires, 32);
    
    // Update register wire mapping (skip x0 which is always 0)
    if (rd != 0) {
        memcpy(compiler->reg_wires[rd], rd_wires, 32 * sizeof(uint32_t));
    }
    
    free(rd_wires);
}

// Helper to compile ADDI instruction
void compile_addi(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, int32_t imm) {
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

// Compile OR instruction: rd = rs1 | rs2
static void compile_or(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Get wire arrays for source registers
    uint32_t* rs1_wires = compiler->reg_wires[rs1];
    uint32_t* rs2_wires = compiler->reg_wires[rs2];
    
    // OR each bit: a OR b = (a XOR b) XOR (a AND b)
    if (rd != 0) {  // Skip x0
        for (int i = 0; i < 32; i++) {
            uint32_t a_xor_b = riscv_circuit_allocate_wire(circuit);
            uint32_t a_and_b = riscv_circuit_allocate_wire(circuit);
            uint32_t result_wire = riscv_circuit_allocate_wire(circuit);
            
            riscv_circuit_add_gate(circuit, rs1_wires[i], rs2_wires[i], a_xor_b, GATE_XOR);
            riscv_circuit_add_gate(circuit, rs1_wires[i], rs2_wires[i], a_and_b, GATE_AND);
            riscv_circuit_add_gate(circuit, a_xor_b, a_and_b, result_wire, GATE_XOR);
            
            compiler->reg_wires[rd][i] = result_wire;
        }
    }
}

int riscv_compile_instruction(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t opcode = GET_OPCODE(instruction);
    uint32_t rd = GET_RD(instruction);
    uint32_t funct3 = GET_FUNCT3(instruction);
    uint32_t rs1 = GET_RS1(instruction);
    uint32_t rs2 = GET_RS2(instruction);
    uint32_t funct7 = GET_FUNCT7(instruction);
    
    // Try shift instructions first
    if (compile_shift_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try branch instructions
    if (compile_branch_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try jump instructions
    if (compile_jump_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try upper immediate instructions
    if (compile_upper_immediate_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try multiply instructions
    if (compile_multiply_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try system instructions
    if (compile_system_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try division instructions
    if (compile_divide_instruction(compiler, instruction) == 0) {
        return 0;
    }
    
    // Try memory instructions (if memory subsystem is available)
    if (compiler->memory && compile_memory_instruction(compiler, compiler->memory, instruction) == 0) {
        return 0;
    }
    
    switch (opcode) {
        case 0x33:  // R-type arithmetic
            switch (funct3) {
                case 0x0:  // ADD/SUB
                    if (funct7 == 0x00) {
                        compile_add(compiler, rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        compile_sub(compiler, rd, rs1, rs2);
                    }
                    break;
                case 0x4:  // XOR
                    compile_xor(compiler, rd, rs1, rs2);
                    break;
                case 0x6:  // OR
                    compile_or(compiler, rd, rs1, rs2);
                    break;
                case 0x7:  // AND
                    compile_and(compiler, rd, rs1, rs2);
                    break;
                // Shifts are handled by compile_shift_instruction
            }
            break;
            
        case 0x13:  // I-type arithmetic
            switch (funct3) {
                case 0x0:  // ADDI
                    compile_addi(compiler, rd, rs1, GET_IMM_I(instruction));
                    break;
                // Other I-type instructions can be added here
            }
            break;
            
        // Branches, jumps, memory ops are handled above
        default:
            fprintf(stderr, "Unsupported opcode: 0x%x\n", opcode);
            return -1;
    }
    
    return 0;
}

void riscv_circuit_print_stats(const riscv_circuit_t* circuit) {
    printf("Circuit Statistics:\n");
    printf("  Total gates: %zu\n", circuit->num_gates);
    printf("  Total wires: %u\n", circuit->next_wire_id);
    printf("  Inputs: %zu\n", circuit->num_inputs);
    printf("  Outputs: %zu\n", circuit->num_outputs);
    
    size_t and_count = 0, xor_count = 0;
    for (size_t i = 0; i < circuit->num_gates; i++) {
        if (circuit->gates[i].type == GATE_AND) and_count++;
        else if (circuit->gates[i].type == GATE_XOR) xor_count++;
    }
    
    printf("  AND gates: %zu\n", and_count);
    printf("  XOR gates: %zu\n", xor_count);
}

int riscv_circuit_to_file(const riscv_circuit_t* circuit, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return -1;
    
    // Write header
    fprintf(f, "# RISC-V compiled circuit\n");
    fprintf(f, "# Format: gate_id left_input right_input output_wire gate_type\n");
    fprintf(f, "CIRCUIT_INPUTS %zu\n", circuit->num_inputs);
    fprintf(f, "CIRCUIT_OUTPUTS %zu\n", circuit->num_outputs);
    fprintf(f, "CIRCUIT_GATES %zu\n", circuit->num_gates);
    
    // Write gates
    for (size_t i = 0; i < circuit->num_gates; i++) {
        fprintf(f, "%zu %u %u %u %s\n",
                i,
                circuit->gates[i].left_input,
                circuit->gates[i].right_input,
                circuit->gates[i].output,
                circuit->gates[i].type == GATE_AND ? "AND" : "XOR");
    }
    
    fclose(f);
    return 0;
}