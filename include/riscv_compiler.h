#ifndef RISCV_COMPILER_H
#define RISCV_COMPILER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "riscv_elf_loader.h"

// Bounded circuit model constants
#define MAX_INPUT_SIZE_MB 10
#define MAX_OUTPUT_SIZE_MB 10
#define MAX_INPUT_BITS (MAX_INPUT_SIZE_MB * 1024 * 1024 * 8)
#define MAX_OUTPUT_BITS (MAX_OUTPUT_SIZE_MB * 1024 * 1024 * 8)

// Circuit Input Convention: ALL circuits follow this standard layout
// Input bit 0: ALWAYS 0 (constant false) - available to every gate as CONSTANT_0_WIRE
// Input bit 1: ALWAYS 1 (constant true)  - available to every gate as CONSTANT_1_WIRE  
// Input bits 2+: User data (PC, registers, memory, etc.)
//
// This standardized approach ensures every circuit can easily access constants
// without needing special constant generation gates.
#define CONSTANT_0_WIRE 0  // Input bit 0: always 0 by circuit convention
#define CONSTANT_1_WIRE 1  // Input bit 1: always 1 by circuit convention

// State encoding layout in input bits
#define PC_START_BIT 2
#define PC_BITS 32
#define REGS_START_BIT (PC_START_BIT + PC_BITS)
#define REGS_BITS (32 * 32)  // 32 registers × 32 bits each
#define MEMORY_START_BIT (REGS_START_BIT + REGS_BITS)
#define MAX_MEMORY_BITS (MAX_INPUT_BITS - MEMORY_START_BIT)

// RISC-V RV32I Base Integer Instruction Set
typedef enum {
    // R-type
    RISCV_ADD  = 0x33,
    RISCV_SUB  = 0x33,  // Same opcode, different funct7
    RISCV_XOR  = 0x33,
    RISCV_OR   = 0x33,
    RISCV_AND  = 0x33,
    RISCV_SLL  = 0x33,
    RISCV_SRL  = 0x33,
    RISCV_SRA  = 0x33,
    RISCV_SLT  = 0x33,
    RISCV_SLTU = 0x33,
    
    // I-type
    RISCV_ADDI  = 0x13,
    RISCV_XORI  = 0x13,
    RISCV_ORI   = 0x13,
    RISCV_ANDI  = 0x13,
    RISCV_SLLI  = 0x13,
    RISCV_SRLI  = 0x13,
    RISCV_SRAI  = 0x13,
    RISCV_SLTI  = 0x13,
    RISCV_SLTIU = 0x13,
    
    // Load/Store
    RISCV_LB   = 0x03,
    RISCV_LH   = 0x03,
    RISCV_LW   = 0x03,
    RISCV_LBU  = 0x03,
    RISCV_LHU  = 0x03,
    RISCV_SB   = 0x23,
    RISCV_SH   = 0x23,
    RISCV_SW   = 0x23,
    
    // Branch
    RISCV_BEQ  = 0x63,
    RISCV_BNE  = 0x63,
    RISCV_BLT  = 0x63,
    RISCV_BGE  = 0x63,
    RISCV_BLTU = 0x63,
    RISCV_BGEU = 0x63,
    
    // Jump
    RISCV_JAL  = 0x6F,
    RISCV_JALR = 0x67,
    
    // Upper immediate
    RISCV_LUI   = 0x37,
    RISCV_AUIPC = 0x17,
    
    // System
    RISCV_ECALL  = 0x73,
    RISCV_EBREAK = 0x73,
} riscv_opcode_t;

// Gate types supported by our circuit
typedef enum {
    GATE_AND = 0,
    GATE_XOR = 1,
} gate_type_t;

// Single gate in the circuit
typedef struct {
    uint32_t left_input;   // Index of left input wire
    uint32_t right_input;  // Index of right input wire
    uint32_t output;       // Index of output wire
    gate_type_t type;      // AND or XOR
} gate_t;

// Bounded circuit representation - allocates only what's needed
typedef struct {
    gate_t* gates;
    size_t num_gates;
    size_t capacity;
    
    // Dynamic input/output arrays (allocated to actual needs)
    bool* input_bits;      // e.g., 1090 bits for SHA3, or PC+regs+memory for RISC-V
    bool* output_bits;     // e.g., 256 bits for SHA3, or final state for RISC-V
    size_t num_inputs;     // Actual number of input bits used
    size_t num_outputs;    // Actual number of output bits used
    
    // Bounds checking (10MB limits)
    size_t max_inputs;     // MAX_INPUT_BITS (83.8M)
    size_t max_outputs;    // MAX_OUTPUT_BITS (83.8M)
    
    // Wire management (for intermediate computations)
    uint32_t next_wire_id;
    uint32_t max_wire_id;  // Track highest wire used
} riscv_circuit_t;

// RISC-V machine state (bounded within 10MB)
typedef struct {
    uint32_t pc;           // Program counter
    uint32_t regs[32];     // 32 general purpose registers (x0-x31)
    uint8_t* memory;       // Memory (bounded to fit within input/output)
    size_t memory_size;    // Actual memory size used
} riscv_state_t;

// Compiler context
typedef struct {
    riscv_circuit_t* circuit;
    riscv_state_t* initial_state;  // Input state
    riscv_state_t* final_state;    // Output state
    
    // Register wire tracking
    uint32_t* reg_wires[32];       // Wire IDs for each register's bits
    uint32_t* pc_wires;            // Wire IDs for PC bits
    
    // Memory subsystem
    struct riscv_memory_t* memory;  // Forward declaration
} riscv_compiler_t;

// Main API functions
riscv_compiler_t* riscv_compiler_create(void);
void riscv_compiler_destroy(riscv_compiler_t* compiler);

// Compile a single RISC-V instruction to gates
int riscv_compile_instruction(riscv_compiler_t* compiler, uint32_t instruction);

// Compile a full RISC-V program
riscv_circuit_t* riscv_compile_program(const uint32_t* program, size_t num_instructions);

// Helper functions for building arithmetic circuits
uint32_t build_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                     uint32_t* sum_bits, size_t num_bits);
uint32_t build_kogge_stone_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                                 uint32_t* sum_bits, size_t num_bits);
uint32_t build_ripple_carry_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                                  uint32_t* sum_bits, size_t num_bits);
uint32_t build_subtractor(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                          uint32_t* diff_bits, size_t num_bits);
uint32_t build_comparator(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                          size_t num_bits, bool is_signed);
uint32_t build_shifter(riscv_circuit_t* circuit, uint32_t* value_bits, uint32_t* shift_bits,
                       uint32_t* result_bits, size_t num_bits, bool is_left, bool is_arithmetic);
uint32_t build_shifter_optimized(riscv_circuit_t* circuit, uint32_t* value_bits, uint32_t* shift_bits,
                                uint32_t* result_bits, size_t num_bits, bool is_left, bool is_arithmetic);
int compile_shift_instruction_optimized(riscv_compiler_t* compiler, uint32_t instruction);
uint32_t* build_multiplier(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                           size_t num_bits);

// Circuit management
void riscv_circuit_add_gate(riscv_circuit_t* circuit, uint32_t left, uint32_t right, 
                            uint32_t output, gate_type_t type);
uint32_t riscv_circuit_allocate_wire(riscv_circuit_t* circuit);
uint32_t* riscv_circuit_allocate_wire_array(riscv_circuit_t* circuit, size_t count);

// Circuit creation with bounds checking
riscv_circuit_t* riscv_circuit_create(size_t num_inputs, size_t num_outputs);

// State encoding/decoding functions
void encode_riscv_state_to_input(const riscv_state_t* state, bool* input_bits);
void decode_riscv_state_from_output(const bool* output_bits, riscv_state_t* state);

// Wire mapping helper functions
uint32_t get_pc_wire(int bit);
uint32_t get_register_wire(int reg, int bit);
uint32_t get_memory_wire(uint32_t addr, int bit);

// Calculate required input/output sizes for RISC-V state
size_t calculate_riscv_input_size(const riscv_state_t* state);
size_t calculate_riscv_output_size(const riscv_state_t* state);

// Debugging and visualization
void riscv_circuit_print_stats(const riscv_circuit_t* circuit);
int riscv_circuit_to_file(const riscv_circuit_t* circuit, const char* filename);

// Additional instruction compilers
int compile_branch_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_branch_instruction_optimized(riscv_compiler_t* compiler, uint32_t instruction);
int compile_memory_instruction(riscv_compiler_t* compiler, struct riscv_memory_t* memory, uint32_t instruction);
int compile_shift_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_multiply_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_jump_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_upper_immediate_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_system_instruction(riscv_compiler_t* compiler, uint32_t instruction);
int compile_divide_instruction(riscv_compiler_t* compiler, uint32_t instruction);

// Test functions
void test_multiplication_instructions(void);
void test_jump_instructions(void);
void test_upper_immediate_instructions(void);
void test_system_instructions(void);
void test_division_instructions(void);

// Helper to compile ADDI
void compile_addi(riscv_compiler_t* compiler, uint32_t rd, uint32_t rs1, int32_t imm);

// Circuit format converter
int riscv_circuit_to_gate_format(const riscv_circuit_t* circuit, const char* filename);

// Optimized arithmetic operations
uint32_t build_kogge_stone_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits, 
                                 uint32_t* sum_bits, size_t num_bits);
uint32_t build_sparse_kogge_stone_adder(riscv_circuit_t* circuit,
                                       uint32_t* a_bits, uint32_t* b_bits,
                                       uint32_t* sum_bits, size_t num_bits);
uint32_t build_ripple_carry_adder(riscv_circuit_t* circuit, uint32_t* a_bits, uint32_t* b_bits,
                                  uint32_t* sum_bits, size_t num_bits);
void build_booth_multiplier(riscv_circuit_t* circuit,
                           uint32_t* multiplicand, uint32_t* multiplier,
                           uint32_t* product, size_t bits);
void build_booth_multiplier_optimized(riscv_circuit_t* circuit,
                                     uint32_t* multiplicand, uint32_t* multiplier,
                                     uint32_t* product, size_t bits);

// Gate caching and deduplication
void deduplicate_gates(riscv_circuit_t* circuit);
void gate_cache_print_stats(void);

// Advanced gate deduplication functions
void gate_dedup_init(void);
void gate_dedup_cleanup(void);
uint32_t gate_dedup_add(riscv_circuit_t* circuit, uint32_t left, uint32_t right, gate_type_t type);
void gate_dedup_report(void);
uint32_t riscv_circuit_add_gate_dedup(riscv_circuit_t* circuit, uint32_t left, uint32_t right, uint32_t output, gate_type_t type);
void build_adder_dedup(riscv_circuit_t* circuit, uint32_t* a, uint32_t* b, uint32_t* sum, size_t bits);
void riscv_compiler_enable_deduplication(riscv_compiler_t* compiler);
void riscv_compiler_finalize_deduplication(riscv_compiler_t* compiler);

// Memory constraint management
typedef struct {
    size_t code_size;
    size_t data_size;
    size_t bss_size;
    size_t heap_size;
    size_t stack_size;
    size_t total_memory;
    uint32_t code_start;
    uint32_t code_end;
    uint32_t data_start;
    uint32_t data_end;
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t stack_start;
    uint32_t stack_end;
} memory_analysis_t;

memory_analysis_t* analyze_memory_requirements(const riscv_program_t* program);
bool check_memory_constraints(const memory_analysis_t* analysis, char* error_msg, size_t error_msg_size);
void print_memory_analysis(const memory_analysis_t* analysis);
void suggest_memory_optimizations(const memory_analysis_t* analysis);
riscv_compiler_t* riscv_compiler_create_constrained(size_t max_memory_bytes);
size_t calculate_riscv_input_size_with_memory(size_t memory_bytes);
size_t calculate_riscv_output_size_with_memory(size_t memory_bytes);
int load_program_with_constraints(const char* elf_file, 
                                  riscv_compiler_t** compiler_out,
                                  riscv_program_t** program_out);

#endif // RISCV_COMPILER_H