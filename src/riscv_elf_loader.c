/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#define _GNU_SOURCE  // for strdup
#include "riscv_elf_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Validate ELF header
bool riscv_elf_validate_header(const Elf32_Ehdr* header) {
    // Check ELF magic
    if (header->e_ident[0] != 0x7F ||
        header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' ||
        header->e_ident[3] != 'F') {
        fprintf(stderr, "Invalid ELF magic\n");
        return false;
    }
    
    // Check 32-bit
    if (header->e_ident[4] != ELF_CLASS_32) {
        fprintf(stderr, "Only 32-bit ELF supported\n");
        return false;
    }
    
    // Check little-endian
    if (header->e_ident[5] != ELF_DATA_LSB) {
        fprintf(stderr, "Only little-endian ELF supported\n");
        return false;
    }
    
    // Check version
    if (header->e_ident[6] != ELF_VERSION_CURRENT) {
        fprintf(stderr, "Invalid ELF version\n");
        return false;
    }
    
    // Check RISC-V
    if (header->e_machine != ELF_MACHINE_RISCV) {
        fprintf(stderr, "Not a RISC-V ELF file (machine=%d)\n", header->e_machine);
        return false;
    }
    
    // Check executable
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        fprintf(stderr, "Not an executable ELF file\n");
        return false;
    }
    
    return true;
}

// Load ELF segments into program structure
int riscv_elf_load_segments(FILE* file, const Elf32_Ehdr* header, riscv_program_t* program) {
    // Seek to program header table
    fseek(file, header->e_phoff, SEEK_SET);
    
    // Read program headers
    Elf32_Phdr* phdrs = malloc(header->e_phnum * sizeof(Elf32_Phdr));
    if (!phdrs) {
        return -1;
    }
    
    if (fread(phdrs, sizeof(Elf32_Phdr), header->e_phnum, file) != header->e_phnum) {
        free(phdrs);
        return -1;
    }
    
    // Find loadable segments
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) {
            continue;
        }
        
        // Check if this is text or data segment
        if (phdrs[i].p_flags & 0x1) {  // Execute permission
            // Text segment
            program->text_start = phdrs[i].p_vaddr;
            program->text_size = phdrs[i].p_memsz;
            
            // Allocate instruction array
            program->num_instructions = phdrs[i].p_filesz / 4;  // 4 bytes per instruction
            program->instructions = malloc(program->num_instructions * sizeof(uint32_t));
            if (!program->instructions) {
                free(phdrs);
                return -1;
            }
            
            // Read instructions
            fseek(file, phdrs[i].p_offset, SEEK_SET);
            if (fread(program->instructions, 4, program->num_instructions, file) != program->num_instructions) {
                free(phdrs);
                return -1;
            }
            
        } else {
            // Data segment
            program->data_start = phdrs[i].p_vaddr;
            program->data_size = phdrs[i].p_memsz;
            
            // Allocate and read data
            program->data = malloc(program->data_size);
            if (!program->data) {
                free(phdrs);
                return -1;
            }
            
            // Initialize with zeros
            memset(program->data, 0, program->data_size);
            
            // Read initialized data
            if (phdrs[i].p_filesz > 0) {
                fseek(file, phdrs[i].p_offset, SEEK_SET);
                if (fread(program->data, 1, phdrs[i].p_filesz, file) != phdrs[i].p_filesz) {
                    fprintf(stderr, "Warning: partial read of data segment\n");
                }
            }
        }
    }
    
    // Set entry point
    program->entry_point = header->e_entry;
    
    free(phdrs);
    return 0;
}

// Load RISC-V ELF file
riscv_program_t* riscv_load_elf(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return NULL;
    }
    
    // Read ELF header
    Elf32_Ehdr header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Failed to read ELF header\n");
        fclose(file);
        return NULL;
    }
    
    // Validate header
    if (!riscv_elf_validate_header(&header)) {
        fclose(file);
        return NULL;
    }
    
    // Allocate program structure
    riscv_program_t* program = calloc(1, sizeof(riscv_program_t));
    if (!program) {
        fclose(file);
        return NULL;
    }
    
    // Load segments
    if (riscv_elf_load_segments(file, &header, program) != 0) {
        fprintf(stderr, "Failed to load ELF segments\n");
        riscv_program_free(program);
        fclose(file);
        return NULL;
    }
    
    // Save filename
    program->filename = strdup(filename);
    program->is_loaded = true;
    
    fclose(file);
    return program;
}

// Free program structure
void riscv_program_free(riscv_program_t* program) {
    if (!program) return;
    
    free(program->instructions);
    free(program->data);
    free(program->filename);
    free(program);
}

// Print program information
void riscv_elf_print_info(const riscv_program_t* program) {
    if (!program || !program->is_loaded) {
        printf("No program loaded\n");
        return;
    }
    
    printf("RISC-V ELF Program Info:\n");
    printf("  File: %s\n", program->filename);
    printf("  Entry point: 0x%08x\n", program->entry_point);
    printf("  Text segment: 0x%08x - 0x%08x (%u bytes)\n", 
           program->text_start, 
           program->text_start + program->text_size,
           program->text_size);
    printf("  Instructions: %zu\n", program->num_instructions);
    
    if (program->data_size > 0) {
        printf("  Data segment: 0x%08x - 0x%08x (%u bytes)\n",
               program->data_start,
               program->data_start + program->data_size,
               program->data_size);
    }
    
    // Show first few instructions
    printf("\nFirst 10 instructions:\n");
    for (size_t i = 0; i < program->num_instructions && i < 10; i++) {
        char disasm[128];
        riscv_disassemble_instruction(program->instructions[i], disasm, sizeof(disasm));
        printf("  [%04zu] 0x%08x: %08x  %s\n", 
               i, 
               (unsigned int)(program->text_start + i * 4),
               program->instructions[i],
               disasm);
    }
}

// Simple RISC-V disassembler for common instructions
void riscv_disassemble_instruction(uint32_t instruction, char* buffer, size_t buffer_size) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t funct7 = (instruction >> 25) & 0x7F;
    
    switch (opcode) {
        case 0x33:  // R-type
            if (funct3 == 0 && funct7 == 0) {
                snprintf(buffer, buffer_size, "add x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 0 && funct7 == 0x20) {
                snprintf(buffer, buffer_size, "sub x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 4) {
                snprintf(buffer, buffer_size, "xor x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 6) {
                snprintf(buffer, buffer_size, "or x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 7) {
                snprintf(buffer, buffer_size, "and x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 1) {
                snprintf(buffer, buffer_size, "sll x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 5 && funct7 == 0) {
                snprintf(buffer, buffer_size, "srl x%d, x%d, x%d", rd, rs1, rs2);
            } else if (funct3 == 5 && funct7 == 0x20) {
                snprintf(buffer, buffer_size, "sra x%d, x%d, x%d", rd, rs1, rs2);
            } else {
                snprintf(buffer, buffer_size, "unknown R-type");
            }
            break;
            
        case 0x13:  // I-type
            {
                int32_t imm = (int32_t)instruction >> 20;
                if (funct3 == 0) {
                    snprintf(buffer, buffer_size, "addi x%d, x%d, %d", rd, rs1, imm);
                } else if (funct3 == 4) {
                    snprintf(buffer, buffer_size, "xori x%d, x%d, %d", rd, rs1, imm);
                } else if (funct3 == 6) {
                    snprintf(buffer, buffer_size, "ori x%d, x%d, %d", rd, rs1, imm);
                } else if (funct3 == 7) {
                    snprintf(buffer, buffer_size, "andi x%d, x%d, %d", rd, rs1, imm);
                } else {
                    snprintf(buffer, buffer_size, "unknown I-type");
                }
            }
            break;
            
        case 0x03:  // Load
            {
                int32_t imm = (int32_t)instruction >> 20;
                if (funct3 == 2) {
                    snprintf(buffer, buffer_size, "lw x%d, %d(x%d)", rd, imm, rs1);
                } else {
                    snprintf(buffer, buffer_size, "load");
                }
            }
            break;
            
        case 0x23:  // Store
            {
                int32_t imm = ((instruction >> 25) << 5) | ((instruction >> 7) & 0x1F);
                if (imm & 0x800) imm |= 0xFFFFF000;  // Sign extend
                if (funct3 == 2) {
                    snprintf(buffer, buffer_size, "sw x%d, %d(x%d)", rs2, imm, rs1);
                } else {
                    snprintf(buffer, buffer_size, "store");
                }
            }
            break;
            
        case 0x63:  // Branch
            {
                int32_t imm = 0;
                imm |= ((instruction >> 31) & 0x1) << 12;
                imm |= ((instruction >> 7) & 0x1) << 11;
                imm |= ((instruction >> 25) & 0x3F) << 5;
                imm |= ((instruction >> 8) & 0xF) << 1;
                if (imm & 0x1000) imm |= 0xFFFFE000;  // Sign extend
                
                if (funct3 == 0) {
                    snprintf(buffer, buffer_size, "beq x%d, x%d, %d", rs1, rs2, imm);
                } else if (funct3 == 1) {
                    snprintf(buffer, buffer_size, "bne x%d, x%d, %d", rs1, rs2, imm);
                } else {
                    snprintf(buffer, buffer_size, "branch");
                }
            }
            break;
            
        default:
            snprintf(buffer, buffer_size, "unknown opcode 0x%02x", opcode);
            break;
    }
}