/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


// Create a minimal ELF file for testing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "riscv_elf_loader.h"

// Simple RISC-V program:
// li x1, 5     -> addi x1, x0, 5   -> 0x00500093
// li x2, 7     -> addi x2, x0, 7   -> 0x00700113
// add x3, x1, x2                    -> 0x002081B3
// j _start     -> jal x0, -12       -> 0xFF5FF06F

uint32_t program_code[] = {
    0x00500093,  // addi x1, x0, 5
    0x00700113,  // addi x2, x0, 7
    0x002081B3,  // add x3, x1, x2
    0xFF5FF06F   // jal x0, -12 (jump back to start)
};

int main(int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : "test_program.elf";
    
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create file: %s\n", filename);
        return 1;
    }
    
    // Create minimal ELF header
    Elf32_Ehdr header = {0};
    
    // Magic and identification
    header.e_ident[0] = 0x7F;
    header.e_ident[1] = 'E';
    header.e_ident[2] = 'L';
    header.e_ident[3] = 'F';
    header.e_ident[4] = ELF_CLASS_32;      // 32-bit
    header.e_ident[5] = ELF_DATA_LSB;      // Little endian
    header.e_ident[6] = ELF_VERSION_CURRENT;
    
    // ELF header fields
    header.e_type = ET_EXEC;
    header.e_machine = ELF_MACHINE_RISCV;
    header.e_version = ELF_VERSION_CURRENT;
    header.e_entry = 0x0;                  // Entry point
    header.e_phoff = sizeof(Elf32_Ehdr);   // Program header offset
    header.e_shoff = 0;                    // No section headers
    header.e_flags = 0;
    header.e_ehsize = sizeof(Elf32_Ehdr);
    header.e_phentsize = sizeof(Elf32_Phdr);
    header.e_phnum = 1;                    // One program header
    header.e_shentsize = 0;
    header.e_shnum = 0;
    header.e_shstrndx = 0;
    
    // Write ELF header
    fwrite(&header, sizeof(header), 1, f);
    
    // Create program header for code segment
    Elf32_Phdr phdr = {0};
    phdr.p_type = PT_LOAD;
    phdr.p_offset = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);
    phdr.p_vaddr = 0x0;
    phdr.p_paddr = 0x0;
    phdr.p_filesz = sizeof(program_code);
    phdr.p_memsz = sizeof(program_code);
    phdr.p_flags = 0x5;  // R-X (readable and executable)
    phdr.p_align = 4;
    
    // Write program header
    fwrite(&phdr, sizeof(phdr), 1, f);
    
    // Write program code
    fwrite(program_code, sizeof(program_code), 1, f);
    
    fclose(f);
    
    printf("Created minimal ELF file: %s\n", filename);
    printf("Program:\n");
    printf("  addi x1, x0, 5   # x1 = 5\n");
    printf("  addi x2, x0, 7   # x2 = 7\n");
    printf("  add x3, x1, x2   # x3 = 12\n");
    printf("  jal x0, -12      # jump to start\n");
    
    return 0;
}