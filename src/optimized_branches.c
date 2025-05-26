#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Optimized branch operations - reduces gates through better comparators
// Current branches use ~500-700 gates, target ~300-400 gates

// Extract instruction fields
#define GET_OPCODE(instr) ((instr) & 0x7F)
#define GET_FUNCT3(instr) (((instr) >> 12) & 0x7)
#define GET_RS1(instr)    (((instr) >> 15) & 0x1F)
#define GET_RS2(instr)    (((instr) >> 20) & 0x1F)

// Extract branch immediate (12-bit signed)
static int32_t get_branch_immediate_optimized(uint32_t instruction) {
    int32_t imm = 0;
    imm |= ((instruction >> 31) & 0x1) << 12;  // bit 12
    imm |= ((instruction >> 7) & 0x1) << 11;   // bit 11
    imm |= ((instruction >> 25) & 0x3F) << 5;  // bits 10:5
    imm |= ((instruction >> 8) & 0xF) << 1;    // bits 4:1
    // bit 0 is always 0 for branches
    
    // Sign extend
    if (imm & 0x1000) {
        imm |= 0xFFFFE000;
    }
    
    return imm;
}

// Optimized equality checker - fewer gates than full comparator
static uint32_t build_equality_optimized(riscv_circuit_t* circuit, 
                                        uint32_t* a_bits, uint32_t* b_bits, 
                                        size_t num_bits) {
    // XOR all bit pairs, then NOR the results
    uint32_t result = CONSTANT_1_WIRE;  // Start with 1 (equal)
    
    for (size_t i = 0; i < num_bits; i++) {
        // a[i] XOR b[i] gives 1 if different, 0 if same
        uint32_t diff = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], diff, GATE_XOR);
        
        // result = result AND (NOT diff) = result AND (diff XOR 1)
        uint32_t not_diff = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, diff, CONSTANT_1_WIRE, not_diff, GATE_XOR);
        
        uint32_t new_result = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, result, not_diff, new_result, GATE_AND);
        result = new_result;
    }
    
    return result;
}

// Optimized less-than comparator using ripple comparison
static uint32_t build_less_than_optimized(riscv_circuit_t* circuit,
                                         uint32_t* a_bits, uint32_t* b_bits,
                                         size_t num_bits, bool is_signed) {
    // For signed comparison, handle sign bits specially
    if (is_signed) {
        uint32_t a_sign = a_bits[num_bits - 1];
        uint32_t b_sign = b_bits[num_bits - 1];
        
        // If signs differ: a < b iff a is negative and b is positive
        uint32_t signs_differ = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_sign, b_sign, signs_differ, GATE_XOR);
        
        uint32_t a_neg_b_pos = riscv_circuit_allocate_wire(circuit);
        uint32_t not_b_sign = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, b_sign, CONSTANT_1_WIRE, not_b_sign, GATE_XOR);
        riscv_circuit_add_gate(circuit, a_sign, not_b_sign, a_neg_b_pos, GATE_AND);
        
        // If signs same, compare magnitudes (unsigned comparison)
        uint32_t magnitude_lt = build_less_than_optimized(circuit, a_bits, b_bits, num_bits - 1, false);
        
        // Result: (signs_differ AND a_neg_b_pos) OR (NOT signs_differ AND magnitude_lt)
        uint32_t case1 = riscv_circuit_allocate_wire(circuit);
        uint32_t not_signs_differ = riscv_circuit_allocate_wire(circuit);
        uint32_t case2 = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, signs_differ, a_neg_b_pos, case1, GATE_AND);
        riscv_circuit_add_gate(circuit, signs_differ, CONSTANT_1_WIRE, not_signs_differ, GATE_XOR);
        riscv_circuit_add_gate(circuit, not_signs_differ, magnitude_lt, case2, GATE_AND);
        
        uint32_t result = riscv_circuit_allocate_wire(circuit);
        uint32_t case_xor = riscv_circuit_allocate_wire(circuit);
        uint32_t case_and = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, case1, case2, case_xor, GATE_XOR);
        riscv_circuit_add_gate(circuit, case1, case2, case_and, GATE_AND);
        riscv_circuit_add_gate(circuit, case_xor, case_and, result, GATE_XOR);
        
        return result;
    }
    
    // Unsigned comparison: ripple from MSB to LSB
    uint32_t result = CONSTANT_0_WIRE;  // Default: not less than
    
    for (int i = (int)num_bits - 1; i >= 0; i--) {
        // At bit i: a < b if (a[i] < b[i]) OR (a[i] == b[i] AND previous_result)
        uint32_t a_lt_b_at_i = riscv_circuit_allocate_wire(circuit);
        uint32_t not_a_i = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, a_bits[i], CONSTANT_1_WIRE, not_a_i, GATE_XOR);
        riscv_circuit_add_gate(circuit, not_a_i, b_bits[i], a_lt_b_at_i, GATE_AND);
        
        uint32_t a_eq_b_at_i = riscv_circuit_allocate_wire(circuit);
        uint32_t diff_at_i = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], diff_at_i, GATE_XOR);
        riscv_circuit_add_gate(circuit, diff_at_i, CONSTANT_1_WIRE, a_eq_b_at_i, GATE_XOR);
        
        uint32_t continue_prev = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_eq_b_at_i, result, continue_prev, GATE_AND);
        
        uint32_t new_result = riscv_circuit_allocate_wire(circuit);
        uint32_t or_xor = riscv_circuit_allocate_wire(circuit);
        uint32_t or_and = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, a_lt_b_at_i, continue_prev, or_xor, GATE_XOR);
        riscv_circuit_add_gate(circuit, a_lt_b_at_i, continue_prev, or_and, GATE_AND);
        riscv_circuit_add_gate(circuit, or_xor, or_and, new_result, GATE_XOR);
        
        result = new_result;
    }
    
    return result;
}

// Optimized branch instruction compiler
int compile_branch_instruction_optimized(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t opcode = GET_OPCODE(instruction);
    if (opcode != 0x63) return -1;  // Not a branch
    
    uint32_t funct3 = GET_FUNCT3(instruction);
    uint32_t rs1 = GET_RS1(instruction);
    uint32_t rs2 = GET_RS2(instruction);
    int32_t imm = get_branch_immediate_optimized(instruction);
    
    // Get operand wires
    uint32_t* rs1_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* rs2_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* pc_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* target_wires = malloc(32 * sizeof(uint32_t));
    uint32_t* next_pc_wires = malloc(32 * sizeof(uint32_t));
    
    for (int i = 0; i < 32; i++) {
        rs1_wires[i] = get_register_wire(rs1, i);
        rs2_wires[i] = get_register_wire(rs2, i);
        pc_wires[i] = get_pc_wire(i);
        target_wires[i] = riscv_circuit_allocate_wire(compiler->circuit);
        next_pc_wires[i] = riscv_circuit_allocate_wire(compiler->circuit);
    }
    
    // Build condition based on branch type
    uint32_t condition;
    switch (funct3) {
        case 0x0: // BEQ
            condition = build_equality_optimized(compiler->circuit, rs1_wires, rs2_wires, 32);
            break;
            
        case 0x1: // BNE
            condition = build_equality_optimized(compiler->circuit, rs1_wires, rs2_wires, 32);
            // Invert result
            uint32_t not_condition = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, condition, CONSTANT_1_WIRE, not_condition, GATE_XOR);
            condition = not_condition;
            break;
            
        case 0x4: // BLT
            condition = build_less_than_optimized(compiler->circuit, rs1_wires, rs2_wires, 32, true);
            break;
            
        case 0x5: // BGE
            condition = build_less_than_optimized(compiler->circuit, rs1_wires, rs2_wires, 32, true);
            // Invert result
            uint32_t not_lt = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, condition, CONSTANT_1_WIRE, not_lt, GATE_XOR);
            condition = not_lt;
            break;
            
        case 0x6: // BLTU
            condition = build_less_than_optimized(compiler->circuit, rs1_wires, rs2_wires, 32, false);
            break;
            
        case 0x7: // BGEU
            condition = build_less_than_optimized(compiler->circuit, rs1_wires, rs2_wires, 32, false);
            // Invert result
            uint32_t not_ltu = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, condition, CONSTANT_1_WIRE, not_ltu, GATE_XOR);
            condition = not_ltu;
            break;
            
        default:
            free(rs1_wires);
            free(rs2_wires);
            free(pc_wires);
            free(target_wires);
            free(next_pc_wires);
            return -1;  // Unsupported branch
    }
    
    // For now, just generate the comparison circuit
    // Full implementation would update PC based on condition
    
    free(rs1_wires);
    free(rs2_wires);
    free(pc_wires);
    free(target_wires);
    free(next_pc_wires);
    
    return 0;
}