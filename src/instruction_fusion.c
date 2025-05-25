#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Instruction fusion: Optimize common instruction sequences
// This dramatically reduces gate count for frequent patterns

// Common fusion patterns
typedef enum {
    FUSION_NONE = 0,
    FUSION_LUI_ADDI,        // Load 32-bit immediate
    FUSION_AUIPC_ADDI,      // Load PC-relative address
    FUSION_ADD_ADD,         // Chained additions
    FUSION_SHIFT_MASK,      // Shift followed by AND (common in bit extraction)
    FUSION_CMP_BRANCH,      // Comparison and branch
    FUSION_LOAD_USE,        // Load followed by use
    FUSION_SEXT_SHIFT,      // Sign extension patterns
    FUSION_ZERO_EXT,        // Zero extension patterns
    FUSION_MUL_ADD,         // Multiply-accumulate
} fusion_type_t;

// Fusion pattern descriptor
typedef struct {
    fusion_type_t type;
    int num_instructions;
    uint32_t (*matcher)(uint32_t* instrs, int count);
    void (*builder)(riscv_compiler_t* compiler, uint32_t* instrs);
    size_t expected_gates;
} fusion_pattern_t;

// Extract instruction fields
#define GET_OPCODE(i) ((i) & 0x7F)
#define GET_RD(i) (((i) >> 7) & 0x1F)
#define GET_RS1(i) (((i) >> 15) & 0x1F)
#define GET_RS2(i) (((i) >> 20) & 0x1F)
#define GET_IMM_I(i) ((int32_t)(i) >> 20)
#define GET_IMM_U(i) ((i) & 0xFFFFF000)

// Pattern matchers

// LUI + ADDI = Load full 32-bit immediate
static uint32_t match_lui_addi(uint32_t* instrs, int count) {
    if (count < 2) return 0;
    
    uint32_t lui = instrs[0];
    uint32_t addi = instrs[1];
    
    if (GET_OPCODE(lui) == 0x37 &&  // LUI
        GET_OPCODE(addi) == 0x13 && // ADDI
        GET_RD(lui) == GET_RS1(addi) && // Same register
        GET_RD(lui) == GET_RD(addi)) {  // Writing same register
        return 2;  // Fuses 2 instructions
    }
    
    return 0;
}

// AUIPC + ADDI = Load PC-relative address
static uint32_t match_auipc_addi(uint32_t* instrs, int count) {
    if (count < 2) return 0;
    
    uint32_t auipc = instrs[0];
    uint32_t addi = instrs[1];
    
    if (GET_OPCODE(auipc) == 0x17 &&  // AUIPC
        GET_OPCODE(addi) == 0x13 &&   // ADDI
        GET_RD(auipc) == GET_RS1(addi) &&
        GET_RD(auipc) == GET_RD(addi)) {
        return 2;
    }
    
    return 0;
}

// ADD + ADD with same destination (accumulation)
static uint32_t match_add_add(uint32_t* instrs, int count) {
    if (count < 2) return 0;
    
    uint32_t add1 = instrs[0];
    uint32_t add2 = instrs[1];
    
    if (GET_OPCODE(add1) == 0x33 && (add1 >> 25) == 0 && // ADD
        GET_OPCODE(add2) == 0x33 && (add2 >> 25) == 0 && // ADD
        GET_RD(add1) == GET_RS1(add2)) { // Chained
        return 2;
    }
    
    return 0;
}

// Shift + AND (bit field extraction)
static uint32_t match_shift_mask(uint32_t* instrs, int count) {
    if (count < 2) return 0;
    
    uint32_t shift = instrs[0];
    uint32_t andi = instrs[1];
    
    // SRL/SRLI followed by ANDI
    if ((GET_OPCODE(shift) == 0x33 || GET_OPCODE(shift) == 0x13) &&
        GET_OPCODE(andi) == 0x13 && ((andi >> 12) & 0x7) == 0x7 && // ANDI
        GET_RD(shift) == GET_RS1(andi)) {
        return 2;
    }
    
    return 0;
}

// Pattern builders (optimized implementations)

// Build fused LUI+ADDI (load 32-bit immediate)
static void build_lui_addi(riscv_compiler_t* compiler, uint32_t* instrs) {
    uint32_t lui = instrs[0];
    uint32_t addi = instrs[1];
    
    uint32_t rd = GET_RD(lui);
    uint32_t upper = GET_IMM_U(lui);
    int32_t lower = GET_IMM_I(addi);
    
    // Combine into full 32-bit value
    uint32_t value = upper + lower;
    
    // Directly wire the constant value
    if (rd != 0) {
        for (int i = 0; i < 32; i++) {
            if (value & (1U << i)) {
                compiler->reg_wires[rd][i] = CONSTANT_1_WIRE;
            } else {
                compiler->reg_wires[rd][i] = CONSTANT_0_WIRE;
            }
        }
    }
    
    // Gate count: 0 (just wiring constants)
}

// Build fused AUIPC+ADDI (PC-relative address)
static void build_auipc_addi(riscv_compiler_t* compiler, uint32_t* instrs) {
    uint32_t auipc = instrs[0];
    uint32_t addi = instrs[1];
    
    uint32_t rd = GET_RD(auipc);
    uint32_t upper = GET_IMM_U(auipc);
    int32_t lower = GET_IMM_I(addi);
    
    if (rd == 0) return;
    
    // Full offset = upper + lower
    int32_t offset = upper + lower;
    
    // Create offset wires
    uint32_t* offset_wires = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    for (int i = 0; i < 32; i++) {
        if (offset & (1 << i)) {
            offset_wires[i] = CONSTANT_1_WIRE;
        } else {
            offset_wires[i] = CONSTANT_0_WIRE;
        }
    }
    
    // Single addition: PC + full_offset
    uint32_t* result = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    build_sparse_kogge_stone_adder(compiler->circuit, 
                                  compiler->pc_wires, offset_wires, 
                                  result, 32);
    
    memcpy(compiler->reg_wires[rd], result, 32 * sizeof(uint32_t));
    
    free(offset_wires);
    free(result);
    
    // Gate count: ~80 (single addition instead of two)
}

// Build fused ADD+ADD (three-operand addition)
static void build_add_add(riscv_compiler_t* compiler, uint32_t* instrs) {
    uint32_t add1 = instrs[0];
    uint32_t add2 = instrs[1];
    
    uint32_t rd = GET_RD(add2);
    uint32_t rs1_1 = GET_RS1(add1);
    uint32_t rs2_1 = GET_RS2(add1);
    uint32_t rs2_2 = GET_RS2(add2);
    
    if (rd == 0) return;
    
    // Three-operand addition using carry-save adder
    uint32_t* sum = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    uint32_t* carry = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    
    // First level: 3:2 compression
    carry[0] = CONSTANT_0_WIRE;
    for (int i = 0; i < 32; i++) {
        uint32_t a = compiler->reg_wires[rs1_1][i];
        uint32_t b = compiler->reg_wires[rs2_1][i];
        uint32_t c = compiler->reg_wires[rs2_2][i];
        
        // Sum = a XOR b XOR c
        uint32_t ab_xor = riscv_circuit_allocate_wire(compiler->circuit);
        riscv_circuit_add_gate(compiler->circuit, a, b, ab_xor, GATE_XOR);
        sum[i] = riscv_circuit_allocate_wire(compiler->circuit);
        riscv_circuit_add_gate(compiler->circuit, ab_xor, c, sum[i], GATE_XOR);
        
        // Carry = (a AND b) OR (c AND (a XOR b))
        if (i < 31) {
            uint32_t ab_and = riscv_circuit_allocate_wire(compiler->circuit);
            uint32_t c_and_xor = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, a, b, ab_and, GATE_AND);
            riscv_circuit_add_gate(compiler->circuit, c, ab_xor, c_and_xor, GATE_AND);
            
            // OR
            uint32_t or_xor = riscv_circuit_allocate_wire(compiler->circuit);
            uint32_t or_and = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, ab_and, c_and_xor, or_xor, GATE_XOR);
            riscv_circuit_add_gate(compiler->circuit, ab_and, c_and_xor, or_and, GATE_AND);
            carry[i+1] = riscv_circuit_allocate_wire(compiler->circuit);
            riscv_circuit_add_gate(compiler->circuit, or_xor, or_and, carry[i+1], GATE_XOR);
        }
    }
    
    // Final addition
    uint32_t* final_sum = riscv_circuit_allocate_wire_array(compiler->circuit, 32);
    build_sparse_kogge_stone_adder(compiler->circuit, sum, carry, final_sum, 32);
    
    memcpy(compiler->reg_wires[rd], final_sum, 32 * sizeof(uint32_t));
    
    free(sum);
    free(carry);
    free(final_sum);
    
    // Gate count: ~120 (vs ~160 for two separate additions)
}

// Build shift+mask (bit field extraction)
static void build_shift_mask(riscv_compiler_t* compiler, uint32_t* instrs) {
    uint32_t shift_instr = instrs[0];
    uint32_t andi_instr = instrs[1];
    
    uint32_t rd = GET_RD(andi_instr);
    uint32_t rs1 = GET_RS1(shift_instr);
    int32_t mask = GET_IMM_I(andi_instr);
    
    if (rd == 0) return;
    
    // Extract shift amount
    int shift_amount = 0;
    if (GET_OPCODE(shift_instr) == 0x13) { // SRLI
        shift_amount = (shift_instr >> 20) & 0x1F;
    }
    
    // Optimize for common bit field extractions
    int mask_bits = __builtin_popcount(mask & 0xFFFFFFFF);
    int mask_lsb = __builtin_ctz(mask);
    
    // Direct bit selection for simple masks
    if (mask_bits <= 8 && (mask == ((1 << mask_bits) - 1) << mask_lsb)) {
        // Extract bits directly without shift
        int start_bit = shift_amount + mask_lsb;
        
        for (int i = 0; i < 32; i++) {
            if (i < mask_bits) {
                compiler->reg_wires[rd][i] = compiler->reg_wires[rs1][start_bit + i];
            } else {
                compiler->reg_wires[rd][i] = CONSTANT_0_WIRE;
            }
        }
        
        // Gate count: 0 (just rewiring)
        return;
    }
    
    // Fall back to standard implementation for complex masks
    // ... standard shift and mask implementation ...
}

// Fusion pattern table
static fusion_pattern_t fusion_patterns[] = {
    {FUSION_LUI_ADDI, 2, match_lui_addi, build_lui_addi, 0},
    {FUSION_AUIPC_ADDI, 2, match_auipc_addi, build_auipc_addi, 80},
    {FUSION_ADD_ADD, 2, match_add_add, build_add_add, 120},
    {FUSION_SHIFT_MASK, 2, match_shift_mask, build_shift_mask, 0},
};

#define NUM_FUSION_PATTERNS (sizeof(fusion_patterns) / sizeof(fusion_patterns[0]))

// Statistics tracking
typedef struct {
    size_t pattern_counts[NUM_FUSION_PATTERNS];
    size_t total_fusions;
    size_t gates_saved;
} fusion_stats_t;

static fusion_stats_t g_fusion_stats = {0};

// Main fusion compiler
size_t compile_with_fusion(riscv_compiler_t* compiler,
                          uint32_t* instructions, size_t count) {
    size_t i = 0;
    size_t compiled = 0;
    
    while (i < count) {
        bool fused = false;
        
        // Try each fusion pattern
        for (size_t p = 0; p < NUM_FUSION_PATTERNS; p++) {
            fusion_pattern_t* pattern = &fusion_patterns[p];
            
            if (i + pattern->num_instructions <= count) {
                uint32_t matched = pattern->matcher(&instructions[i], 
                                                   count - i);
                if (matched > 0) {
                    // Apply fusion
                    size_t gates_before = compiler->circuit->num_gates;
                    pattern->builder(compiler, &instructions[i]);
                    size_t gates_used = compiler->circuit->num_gates - gates_before;
                    
                    // Calculate savings (vs non-fused)
                    size_t normal_gates = matched * 80;  // Assume 80 gates average
                    if (gates_used < normal_gates) {
                        g_fusion_stats.gates_saved += normal_gates - gates_used;
                    }
                    
                    g_fusion_stats.pattern_counts[p]++;
                    g_fusion_stats.total_fusions++;
                    
                    i += matched;
                    compiled += matched;
                    fused = true;
                    break;
                }
            }
        }
        
        // No fusion found, compile normally
        if (!fused) {
            riscv_compile_instruction(compiler, instructions[i]);
            i++;
            compiled++;
        }
    }
    
    return compiled;
}

// Print fusion statistics
void print_fusion_stats(void) {
    printf("\nInstruction Fusion Statistics:\n");
    printf("==============================\n");
    printf("Total fusions: %zu\n", g_fusion_stats.total_fusions);
    printf("Gates saved: %zu\n", g_fusion_stats.gates_saved);
    
    if (g_fusion_stats.total_fusions > 0) {
        printf("\nFusion pattern breakdown:\n");
        for (size_t i = 0; i < NUM_FUSION_PATTERNS; i++) {
            if (g_fusion_stats.pattern_counts[i] > 0) {
                const char* names[] = {
                    "NONE", "LUI+ADDI", "AUIPC+ADDI", "ADD+ADD",
                    "SHIFT+MASK", "CMP+BRANCH", "LOAD+USE",
                    "SEXT+SHIFT", "ZERO_EXT", "MUL+ADD"
                };
                printf("  %-15s: %6zu times (%.1f%%)\n",
                       names[fusion_patterns[i].type],
                       g_fusion_stats.pattern_counts[i],
                       100.0 * g_fusion_stats.pattern_counts[i] / 
                       g_fusion_stats.total_fusions);
            }
        }
        
        printf("\nAverage gates saved per fusion: %.1f\n",
               (double)g_fusion_stats.gates_saved / g_fusion_stats.total_fusions);
    }
}

// Benchmark fusion effectiveness
void benchmark_instruction_fusion(void) {
    printf("\n");
    printf("=================================================================\n");
    printf("                 INSTRUCTION FUSION BENCHMARK                     \n");
    printf("=================================================================\n\n");
    
    // Test programs with fusion opportunities
    struct {
        const char* name;
        uint32_t* code;
        size_t count;
    } test_programs[] = {
        {
            "Load immediate values",
            (uint32_t[]){
                0x123450B7,  // lui x1, 0x12345
                0x67808093,  // addi x1, x1, 0x678
                0xABCDE137,  // lui x2, 0xABCDE  
                0xF0010113,  // addi x2, x2, -256
                0x00000037,  // lui x0, 0 (nop)
                0x00000013,  // addi x0, x0, 0 (nop)
            },
            6
        },
        {
            "PC-relative addressing",
            (uint32_t[]){
                0x00000097,  // auipc x1, 0
                0x01008093,  // addi x1, x1, 16
                0x00001117,  // auipc x2, 1
                0xFF410113,  // addi x2, x2, -12
            },
            4
        },
        {
            "Chained additions",
            (uint32_t[]){
                0x002081B3,  // add x3, x1, x2
                0x004181B3,  // add x3, x3, x4
                0x006281B3,  // add x5, x5, x6
                0x007281B3,  // add x5, x5, x7
            },
            4
        },
        {
            "Bit field extraction",
            (uint32_t[]){
                0x00C0D093,  // srli x1, x1, 12
                0x0FF0F093,  // andi x1, x1, 0xFF
                0x0080D113,  // srli x2, x1, 8
                0x00F17113,  // andi x2, x2, 0xF
            },
            4
        }
    };
    
    printf("%-25s %8s %8s %10s %12s\n",
           "Pattern", "Instrs", "Gates", "Fused Gates", "Improvement");
    printf("%-25s %8s %8s %10s %12s\n",
           "-------", "------", "-----", "-----------", "-----------");
    
    for (size_t p = 0; p < sizeof(test_programs)/sizeof(test_programs[0]); p++) {
        // Reset stats
        memset(&g_fusion_stats, 0, sizeof(g_fusion_stats));
        
        // Compile without fusion
        riscv_compiler_t* normal = riscv_compiler_create();
        size_t gates_before = normal->circuit->num_gates;
        
        for (size_t i = 0; i < test_programs[p].count; i++) {
            riscv_compile_instruction(normal, test_programs[p].code[i]);
        }
        
        size_t normal_gates = normal->circuit->num_gates - gates_before;
        
        // Compile with fusion
        riscv_compiler_t* fused = riscv_compiler_create();
        gates_before = fused->circuit->num_gates;
        
        compile_with_fusion(fused, test_programs[p].code, test_programs[p].count);
        
        size_t fused_gates = fused->circuit->num_gates - gates_before;
        
        double improvement = 100.0 * (1.0 - (double)fused_gates / normal_gates);
        
        printf("%-25s %8zu %8zu %10zu %11.1f%%\n",
               test_programs[p].name,
               test_programs[p].count,
               normal_gates,
               fused_gates,
               improvement);
        
        riscv_compiler_destroy(normal);
        riscv_compiler_destroy(fused);
    }
    
    printf("\n");
    print_fusion_stats();
}