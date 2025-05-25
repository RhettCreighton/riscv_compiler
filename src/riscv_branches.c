#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Extract branch immediate from instruction
static int32_t get_branch_immediate(uint32_t instruction) {
    // B-type immediate reconstruction
    int32_t imm = 0;
    imm |= ((instruction >> 31) & 0x1) << 12;  // imm[12]
    imm |= ((instruction >> 7) & 0x1) << 11;   // imm[11]
    imm |= ((instruction >> 25) & 0x3F) << 5;  // imm[10:5]
    imm |= ((instruction >> 8) & 0xF) << 1;    // imm[4:1]
    // imm[0] is always 0
    
    // Sign extend
    if (imm & 0x1000) {
        imm |= 0xFFFFE000;
    }
    
    return imm;
}

// Helper: Build less-than comparator
static uint32_t build_less_than(riscv_circuit_t* circuit, 
                               uint32_t* a_bits, uint32_t* b_bits, 
                               size_t num_bits, bool is_signed) {
    // For unsigned: a < b if a - b produces a borrow
    uint32_t* diff_bits = riscv_circuit_allocate_wire_array(circuit, num_bits);
    uint32_t borrow_out = build_subtractor(circuit, a_bits, b_bits, diff_bits, num_bits);
    
    if (!is_signed) {
        // Unsigned: less_than = NOT borrow_out (borrow means a >= b)
        uint32_t not_borrow = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, borrow_out, 2, not_borrow, GATE_XOR);  // NOT
        free(diff_bits);
        return not_borrow;
    } else {
        // Signed comparison
        uint32_t a_sign = a_bits[num_bits - 1];
        uint32_t b_sign = b_bits[num_bits - 1];
        uint32_t diff_sign = diff_bits[num_bits - 1];
        
        // If signs differ, a < b if a is negative
        // If signs same, a < b if difference is negative
        uint32_t signs_differ = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a_sign, b_sign, signs_differ, GATE_XOR);
        
        // MUX: if signs differ, use a_sign, else use diff_sign
        uint32_t not_signs_differ = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, signs_differ, 2, not_signs_differ, GATE_XOR);
        
        uint32_t case1 = riscv_circuit_allocate_wire(circuit);
        uint32_t case2 = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, signs_differ, a_sign, case1, GATE_AND);
        riscv_circuit_add_gate(circuit, not_signs_differ, diff_sign, case2, GATE_AND);
        
        // OR the cases
        uint32_t case1_xor_case2 = riscv_circuit_allocate_wire(circuit);
        uint32_t case1_and_case2 = riscv_circuit_allocate_wire(circuit);
        uint32_t result = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, case1, case2, case1_xor_case2, GATE_XOR);
        riscv_circuit_add_gate(circuit, case1, case2, case1_and_case2, GATE_AND);
        riscv_circuit_add_gate(circuit, case1_xor_case2, case1_and_case2, result, GATE_XOR);
        
        free(diff_bits);
        return result;
    }
}

// Helper: Build equality checker
static uint32_t build_equal(riscv_circuit_t* circuit, 
                           uint32_t* a_bits, uint32_t* b_bits, 
                           size_t num_bits) {
    // Check if all bits are equal
    uint32_t all_equal = 2;  // Start with 1
    
    for (size_t i = 0; i < num_bits; i++) {
        // Check if bits are equal: NOT (a XOR b)
        uint32_t bit_xor = riscv_circuit_allocate_wire(circuit);
        uint32_t bit_equal = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, a_bits[i], b_bits[i], bit_xor, GATE_XOR);
        riscv_circuit_add_gate(circuit, bit_xor, 2, bit_equal, GATE_XOR);  // NOT
        
        // AND with running result
        uint32_t new_all_equal = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, all_equal, bit_equal, new_all_equal, GATE_AND);
        all_equal = new_all_equal;
    }
    
    return all_equal;
}

// Compile BEQ instruction: if (rs1 == rs2) PC += imm
void compile_beq(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = get_branch_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Compare rs1 == rs2
    uint32_t equal = build_equal(circuit, 
                                compiler->reg_wires[rs1], 
                                compiler->reg_wires[rs2], 
                                32);
    
    // Calculate branch target: PC + imm
    uint32_t* imm_bits = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        imm_bits[i] = (imm & (1 << i)) ? 2 : 1;  // Constant wires
    }
    
    uint32_t* new_pc = riscv_circuit_allocate_wire_array(circuit, 32);
    build_adder(circuit, compiler->pc_wires, imm_bits, new_pc, 32);
    
    // Calculate PC + 4 (next instruction)
    uint32_t* four_bits = riscv_circuit_allocate_wire_array(circuit, 32);
    for (int i = 0; i < 32; i++) {
        four_bits[i] = (i == 2) ? 2 : 1;  // 4 in binary
    }
    
    uint32_t* pc_plus_4 = riscv_circuit_allocate_wire_array(circuit, 32);
    build_adder(circuit, compiler->pc_wires, four_bits, pc_plus_4, 32);
    
    // MUX: if equal, PC = PC + imm, else PC = PC + 4
    for (int i = 0; i < 32; i++) {
        uint32_t not_equal = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, equal, 2, not_equal, GATE_XOR);
        
        uint32_t case_branch = riscv_circuit_allocate_wire(circuit);
        uint32_t case_no_branch = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, equal, new_pc[i], case_branch, GATE_AND);
        riscv_circuit_add_gate(circuit, not_equal, pc_plus_4[i], case_no_branch, GATE_AND);
        
        // OR the cases
        uint32_t xor_result = riscv_circuit_allocate_wire(circuit);
        uint32_t and_result = riscv_circuit_allocate_wire(circuit);
        
        riscv_circuit_add_gate(circuit, case_branch, case_no_branch, xor_result, GATE_XOR);
        riscv_circuit_add_gate(circuit, case_branch, case_no_branch, and_result, GATE_AND);
        riscv_circuit_add_gate(circuit, xor_result, and_result, compiler->pc_wires[i], GATE_XOR);
    }
    
    free(imm_bits);
    free(new_pc);
    free(four_bits);
    free(pc_plus_4);
}

// Compile BNE instruction: if (rs1 != rs2) PC += imm
void compile_bne(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = get_branch_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Compare rs1 == rs2
    uint32_t equal = build_equal(circuit, 
                                compiler->reg_wires[rs1], 
                                compiler->reg_wires[rs2], 
                                32);
    
    // We want NOT equal
    uint32_t not_equal = riscv_circuit_allocate_wire(circuit);
    riscv_circuit_add_gate(circuit, equal, 2, not_equal, GATE_XOR);
    
    // Rest is similar to BEQ but with not_equal condition
    // ... (similar code to BEQ but using not_equal)
}

// Compile BLT instruction: if (rs1 < rs2) PC += imm (signed)
void compile_blt(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = get_branch_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Compare rs1 < rs2 (signed)
    uint32_t less_than = build_less_than(circuit,
                                        compiler->reg_wires[rs1],
                                        compiler->reg_wires[rs2],
                                        32, true);
    
    // Rest is similar to BEQ but with less_than condition
    // ... (branch logic similar to BEQ)
}

// Compile BLTU instruction: if (rs1 < rs2) PC += imm (unsigned)
void compile_bltu(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = get_branch_immediate(instruction);
    
    riscv_circuit_t* circuit = compiler->circuit;
    
    // Compare rs1 < rs2 (unsigned)
    uint32_t less_than = build_less_than(circuit,
                                        compiler->reg_wires[rs1],
                                        compiler->reg_wires[rs2],
                                        32, false);
    
    // Rest is similar to BEQ but with less_than condition
    // ... (branch logic similar to BEQ)
}

// Add branch instruction support to main compiler
int compile_branch_instruction(riscv_compiler_t* compiler, uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    
    if (opcode != 0x63) {  // Not a branch instruction
        return -1;
    }
    
    uint32_t funct3 = (instruction >> 12) & 0x7;
    
    switch (funct3) {
        case 0x0:  // BEQ
            compile_beq(compiler, instruction);
            break;
        case 0x1:  // BNE
            compile_bne(compiler, instruction);
            break;
        case 0x4:  // BLT
            compile_blt(compiler, instruction);
            break;
        case 0x5:  // BGE
            // Similar to BLT but inverted condition
            break;
        case 0x6:  // BLTU
            compile_bltu(compiler, instruction);
            break;
        case 0x7:  // BGEU
            // Similar to BLTU but inverted condition
            break;
        default:
            fprintf(stderr, "Unknown branch funct3: 0x%x\n", funct3);
            return -1;
    }
    
    return 0;
}