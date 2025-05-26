/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef RISCV_EMULATOR_H
#define RISCV_EMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// RISC-V emulator state
typedef struct {
    uint32_t regs[32];          // 32 general-purpose registers
    uint32_t pc;                // Program counter
    uint8_t* memory;            // Memory space
    size_t memory_size;         // Size of memory
    bool halt;                  // Halt flag
    uint32_t instruction_count; // Number of instructions executed
} emulator_state_t;

// Instruction types
typedef enum {
    R_TYPE,   // Register-register operations
    I_TYPE,   // Immediate operations
    S_TYPE,   // Store operations  
    B_TYPE,   // Branch operations
    U_TYPE,   // Upper immediate operations
    J_TYPE    // Jump operations
} instruction_type_t;

// Decoded instruction
typedef struct {
    uint32_t opcode;
    uint32_t rd, rs1, rs2;
    int32_t imm;
    uint32_t funct3, funct7;
    instruction_type_t type;
} decoded_instruction_t;

// Emulator functions
emulator_state_t* create_emulator(size_t memory_size);
void destroy_emulator(emulator_state_t* emu);
void reset_emulator(emulator_state_t* emu);

// Memory operations
void write_memory_word(emulator_state_t* emu, uint32_t addr, uint32_t value);
uint32_t read_memory_word(emulator_state_t* emu, uint32_t addr);
void write_memory_byte(emulator_state_t* emu, uint32_t addr, uint8_t value);
uint8_t read_memory_byte(emulator_state_t* emu, uint32_t addr);
void write_memory_halfword(emulator_state_t* emu, uint32_t addr, uint16_t value);
uint16_t read_memory_halfword(emulator_state_t* emu, uint32_t addr);

// Program loading
void load_program(emulator_state_t* emu, uint32_t* program, size_t length, uint32_t start_addr);

// Instruction execution
decoded_instruction_t decode_instruction(uint32_t instruction);
bool execute_instruction(emulator_state_t* emu, uint32_t instruction);
void step_emulator(emulator_state_t* emu);
void run_emulator(emulator_state_t* emu, uint32_t max_instructions);

// State inspection
void print_registers(const emulator_state_t* emu);
void print_memory_range(const emulator_state_t* emu, uint32_t start, uint32_t end);
bool compare_states(const emulator_state_t* emu1, const emulator_state_t* emu2);

// Utility functions
const char* get_instruction_name(uint32_t instruction);
uint32_t sign_extend(uint32_t value, int bits);

#endif // RISCV_EMULATOR_H