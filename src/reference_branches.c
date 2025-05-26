/*
 * Reference Implementations for RISC-V Branch and Jump Instructions
 * 
 * These implementations follow the RISC-V specification exactly,
 * with no optimizations, to serve as ground truth for verification.
 */

#include "../include/formal_verification.h"
#include <string.h>

// ============================================================================
// Branch Target Calculation
// ============================================================================

// Calculate branch target address
// Reference: RISC-V Spec Section 2.5 - "Branch offset is sign-extended and added to PC"
word32_t ref_branch_target(word32_t pc, word32_t offset) {
    // Branch offset is already sign-extended 13-bit value (bit 12 is sign)
    // Stored as: imm[12|10:5|4:1|11] but we assume it's already decoded
    
    // Add offset to PC
    return ref_add(pc, offset);
}

// ============================================================================
// Branch Conditions (RV32I Chapter 2.5)
// ============================================================================

// BEQ: Branch if Equal
// Reference: RISC-V Spec Section 2.5 - "BEQ branches if rs1 equals rs2"
bool ref_branch_eq(word32_t rs1, word32_t rs2) {
    return ref_eq(rs1, rs2);
}

// BNE: Branch if Not Equal
// Reference: RISC-V Spec Section 2.5 - "BNE branches if rs1 not equals rs2"
bool ref_branch_ne(word32_t rs1, word32_t rs2) {
    return !ref_eq(rs1, rs2);
}

// BLT: Branch if Less Than (signed)
// Reference: RISC-V Spec Section 2.5 - "BLT branches if rs1 < rs2 (signed)"
bool ref_branch_lt(word32_t rs1, word32_t rs2) {
    return ref_lt_signed(rs1, rs2);
}

// BGE: Branch if Greater or Equal (signed)
// Reference: RISC-V Spec Section 2.5 - "BGE branches if rs1 >= rs2 (signed)"
bool ref_branch_ge(word32_t rs1, word32_t rs2) {
    return !ref_lt_signed(rs1, rs2);
}

// BLTU: Branch if Less Than Unsigned
// Reference: RISC-V Spec Section 2.5 - "BLTU branches if rs1 < rs2 (unsigned)"
bool ref_branch_ltu(word32_t rs1, word32_t rs2) {
    return ref_lt_unsigned(rs1, rs2);
}

// BGEU: Branch if Greater or Equal Unsigned
// Reference: RISC-V Spec Section 2.5 - "BGEU branches if rs1 >= rs2 (unsigned)"
bool ref_branch_geu(word32_t rs1, word32_t rs2) {
    return !ref_lt_unsigned(rs1, rs2);
}

// ============================================================================
// Jump Instructions (RV32I Chapter 2.5)
// ============================================================================

// JAL: Jump and Link
// Reference: RISC-V Spec Section 2.5 - "JAL stores PC+4 in rd and jumps to PC+offset"
typedef struct {
    word32_t new_pc;    // Target address
    word32_t link;      // Return address (PC + 4)
} jal_result_t;

jal_result_t ref_jal(word32_t pc, word32_t offset) {
    jal_result_t result;
    
    // Calculate return address (PC + 4)
    word32_t four;
    uint32_to_word32(4, &four);
    result.link = ref_add(pc, four);
    
    // Calculate target address (PC + sign-extended offset)
    result.new_pc = ref_add(pc, offset);
    
    // Ensure target is 4-byte aligned (lower 2 bits cleared)
    result.new_pc.bits[0] = false;
    result.new_pc.bits[1] = false;
    
    return result;
}

// JALR: Jump and Link Register
// Reference: RISC-V Spec Section 2.5 - "JALR stores PC+4 in rd and jumps to (rs1+offset)&~1"
jal_result_t ref_jalr(word32_t pc, word32_t rs1, word32_t offset) {
    jal_result_t result;
    
    // Calculate return address (PC + 4)
    word32_t four;
    uint32_to_word32(4, &four);
    result.link = ref_add(pc, four);
    
    // Calculate target address: (rs1 + sign-extended offset) & ~1
    result.new_pc = ref_add(rs1, offset);
    
    // Clear least significant bit to ensure even address
    result.new_pc.bits[0] = false;
    
    return result;
}

// ============================================================================
// Complete Branch Instruction Execution
// ============================================================================

// Execute a branch instruction and return new PC
// Reference: RISC-V Spec Section 2.5
word32_t ref_execute_branch(word32_t pc, word32_t rs1, word32_t rs2, 
                           word32_t offset, int branch_type) {
    bool take_branch = false;
    
    switch (branch_type) {
        case 0: // BEQ
            take_branch = ref_branch_eq(rs1, rs2);
            break;
        case 1: // BNE
            take_branch = ref_branch_ne(rs1, rs2);
            break;
        case 2: // BLT
            take_branch = ref_branch_lt(rs1, rs2);
            break;
        case 3: // BGE
            take_branch = ref_branch_ge(rs1, rs2);
            break;
        case 4: // BLTU
            take_branch = ref_branch_ltu(rs1, rs2);
            break;
        case 5: // BGEU
            take_branch = ref_branch_geu(rs1, rs2);
            break;
    }
    
    if (take_branch) {
        // Branch taken: PC = PC + offset
        return ref_add(pc, offset);
    } else {
        // Branch not taken: PC = PC + 4
        word32_t four;
        uint32_to_word32(4, &four);
        return ref_add(pc, four);
    }
}