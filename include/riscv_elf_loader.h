#ifndef RISCV_ELF_LOADER_H
#define RISCV_ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

// ELF header constants
#define ELF_MAGIC 0x464C457F  // "\x7FELF"
#define ELF_CLASS_32 1
#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_DATA_MSB 2
#define ELF_VERSION_CURRENT 1
#define ELF_MACHINE_RISCV 243

// ELF types
#define ET_EXEC 2  // Executable file
#define ET_DYN 3   // Shared object file

// Program header types
#define PT_LOAD 1  // Loadable segment

// ELF32 structures
typedef struct {
    uint8_t  e_ident[16];    // Magic number and other info
    uint16_t e_type;         // Object file type
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // Object file version
    uint32_t e_entry;        // Entry point virtual address
    uint32_t e_phoff;        // Program header table file offset
    uint32_t e_shoff;        // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;       // ELF header size in bytes
    uint16_t e_phentsize;    // Program header table entry size
    uint16_t e_phnum;        // Program header table entry count
    uint16_t e_shentsize;    // Section header table entry size
    uint16_t e_shnum;        // Section header table entry count
    uint16_t e_shstrndx;     // Section header string table index
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;         // Segment type
    uint32_t p_offset;       // Segment file offset
    uint32_t p_vaddr;        // Segment virtual address
    uint32_t p_paddr;        // Segment physical address
    uint32_t p_filesz;       // Segment size in file
    uint32_t p_memsz;        // Segment size in memory
    uint32_t p_flags;        // Segment flags
    uint32_t p_align;        // Segment alignment
} Elf32_Phdr;

// Section header for getting symbol information
typedef struct {
    uint32_t sh_name;        // Section name (string tbl index)
    uint32_t sh_type;        // Section type
    uint32_t sh_flags;       // Section flags
    uint32_t sh_addr;        // Section virtual addr at execution
    uint32_t sh_offset;      // Section file offset
    uint32_t sh_size;        // Section size in bytes
    uint32_t sh_link;        // Link to another section
    uint32_t sh_info;        // Additional section information
    uint32_t sh_addralign;   // Section alignment
    uint32_t sh_entsize;     // Entry size if section holds table
} Elf32_Shdr;

// Symbol table entry
typedef struct {
    uint32_t st_name;        // Symbol name (string tbl index)
    uint32_t st_value;       // Symbol value
    uint32_t st_size;        // Symbol size
    uint8_t  st_info;        // Symbol type and binding
    uint8_t  st_other;       // Symbol visibility
    uint16_t st_shndx;       // Section index
} Elf32_Sym;

// RISC-V program representation
typedef struct {
    uint32_t* instructions;   // Array of RISC-V instructions
    size_t num_instructions;  // Number of instructions
    uint32_t entry_point;     // Entry point address
    
    // Memory layout
    uint32_t text_start;      // Start of code segment
    uint32_t text_size;       // Size of code segment
    uint32_t data_start;      // Start of data segment
    uint32_t data_size;       // Size of data segment
    uint8_t* data;           // Initial data values
    
    // For debugging
    char* filename;           // Source filename
    bool is_loaded;          // Successfully loaded flag
} riscv_program_t;

// Loader API
riscv_program_t* riscv_load_elf(const char* filename);
void riscv_program_free(riscv_program_t* program);

// Helper functions
bool riscv_elf_validate_header(const Elf32_Ehdr* header);
int riscv_elf_load_segments(FILE* file, const Elf32_Ehdr* header, riscv_program_t* program);
void riscv_elf_print_info(const riscv_program_t* program);

// Disassembler helper (for debugging)
void riscv_disassemble_instruction(uint32_t instruction, char* buffer, size_t buffer_size);

#endif // RISCV_ELF_LOADER_H