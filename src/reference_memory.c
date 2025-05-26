/*
 * Reference Implementations for RISC-V Memory Instructions
 * 
 * These implementations model memory access at the bit level,
 * following the RISC-V specification exactly.
 */

#include "../include/formal_verification.h"
#include <string.h>
#include <stdlib.h>

// Simple bit-level memory model for verification
typedef struct {
    bool* bits;              // Memory as array of bits
    size_t size_bytes;       // Size in bytes
    size_t size_bits;        // Size in bits
} bit_memory_t;

// ============================================================================
// Memory Model
// ============================================================================

// Create bit-level memory
bit_memory_t* ref_memory_create(size_t size_bytes) {
    bit_memory_t* mem = malloc(sizeof(bit_memory_t));
    mem->size_bytes = size_bytes;
    mem->size_bits = size_bytes * 8;
    mem->bits = calloc(mem->size_bits, sizeof(bool));
    return mem;
}

// Destroy memory
void ref_memory_destroy(bit_memory_t* mem) {
    free(mem->bits);
    free(mem);
}

// Read a single bit from memory
bool ref_memory_read_bit(bit_memory_t* mem, size_t bit_addr) {
    if (bit_addr < mem->size_bits) {
        return mem->bits[bit_addr];
    }
    return false; // Out of bounds reads return 0
}

// Write a single bit to memory
void ref_memory_write_bit(bit_memory_t* mem, size_t bit_addr, bool value) {
    if (bit_addr < mem->size_bits) {
        mem->bits[bit_addr] = value;
    }
    // Out of bounds writes are ignored
}

// ============================================================================
// Load Instructions (RV32I Chapter 2.6)
// ============================================================================

// LW: Load Word (32 bits)
// Reference: RISC-V Spec Section 2.6 - "LW loads a 32-bit value from memory"
word32_t ref_load_word(bit_memory_t* mem, word32_t addr) {
    word32_t result;
    
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Load 32 bits (4 bytes) from memory, little-endian
    for (int byte = 0; byte < 4; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            size_t bit_addr = (byte_addr + byte) * 8 + bit;
            result.bits[byte * 8 + bit] = ref_memory_read_bit(mem, bit_addr);
        }
    }
    
    return result;
}

// LH: Load Halfword (16 bits, sign-extended)
// Reference: RISC-V Spec Section 2.6 - "LH loads a 16-bit value and sign-extends"
word32_t ref_load_halfword(bit_memory_t* mem, word32_t addr) {
    word32_t result;
    
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Load 16 bits (2 bytes) from memory
    for (int byte = 0; byte < 2; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            size_t bit_addr = (byte_addr + byte) * 8 + bit;
            result.bits[byte * 8 + bit] = ref_memory_read_bit(mem, bit_addr);
        }
    }
    
    // Sign extend from bit 15
    bool sign_bit = result.bits[15];
    for (int i = 16; i < 32; i++) {
        result.bits[i] = sign_bit;
    }
    
    return result;
}

// LHU: Load Halfword Unsigned (16 bits, zero-extended)
// Reference: RISC-V Spec Section 2.6 - "LHU loads a 16-bit value and zero-extends"
word32_t ref_load_halfword_unsigned(bit_memory_t* mem, word32_t addr) {
    word32_t result = word32_fill(false); // Initialize to zeros
    
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Load 16 bits (2 bytes) from memory
    for (int byte = 0; byte < 2; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            size_t bit_addr = (byte_addr + byte) * 8 + bit;
            result.bits[byte * 8 + bit] = ref_memory_read_bit(mem, bit_addr);
        }
    }
    
    // Upper bits remain zero (zero extension)
    return result;
}

// LB: Load Byte (8 bits, sign-extended)
// Reference: RISC-V Spec Section 2.6 - "LB loads an 8-bit value and sign-extends"
word32_t ref_load_byte(bit_memory_t* mem, word32_t addr) {
    word32_t result;
    
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Load 8 bits (1 byte) from memory
    for (int bit = 0; bit < 8; bit++) {
        size_t bit_addr = byte_addr * 8 + bit;
        result.bits[bit] = ref_memory_read_bit(mem, bit_addr);
    }
    
    // Sign extend from bit 7
    bool sign_bit = result.bits[7];
    for (int i = 8; i < 32; i++) {
        result.bits[i] = sign_bit;
    }
    
    return result;
}

// LBU: Load Byte Unsigned (8 bits, zero-extended)
// Reference: RISC-V Spec Section 2.6 - "LBU loads an 8-bit value and zero-extends"
word32_t ref_load_byte_unsigned(bit_memory_t* mem, word32_t addr) {
    word32_t result = word32_fill(false); // Initialize to zeros
    
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Load 8 bits (1 byte) from memory
    for (int bit = 0; bit < 8; bit++) {
        size_t bit_addr = byte_addr * 8 + bit;
        result.bits[bit] = ref_memory_read_bit(mem, bit_addr);
    }
    
    // Upper bits remain zero (zero extension)
    return result;
}

// ============================================================================
// Store Instructions (RV32I Chapter 2.6)
// ============================================================================

// SW: Store Word (32 bits)
// Reference: RISC-V Spec Section 2.6 - "SW stores a 32-bit value to memory"
void ref_store_word(bit_memory_t* mem, word32_t addr, word32_t data) {
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Store 32 bits (4 bytes) to memory, little-endian
    for (int byte = 0; byte < 4; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            size_t bit_addr = (byte_addr + byte) * 8 + bit;
            ref_memory_write_bit(mem, bit_addr, data.bits[byte * 8 + bit]);
        }
    }
}

// SH: Store Halfword (16 bits)
// Reference: RISC-V Spec Section 2.6 - "SH stores lower 16 bits to memory"
void ref_store_halfword(bit_memory_t* mem, word32_t addr, word32_t data) {
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Store lower 16 bits (2 bytes) to memory
    for (int byte = 0; byte < 2; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            size_t bit_addr = (byte_addr + byte) * 8 + bit;
            ref_memory_write_bit(mem, bit_addr, data.bits[byte * 8 + bit]);
        }
    }
}

// SB: Store Byte (8 bits)
// Reference: RISC-V Spec Section 2.6 - "SB stores lower 8 bits to memory"
void ref_store_byte(bit_memory_t* mem, word32_t addr, word32_t data) {
    // Convert address to byte address
    uint32_t byte_addr = word32_to_uint32(&addr);
    
    // Store lower 8 bits (1 byte) to memory
    for (int bit = 0; bit < 8; bit++) {
        size_t bit_addr = byte_addr * 8 + bit;
        ref_memory_write_bit(mem, bit_addr, data.bits[bit]);
    }
}

// ============================================================================
// Memory Access with Alignment Checking
// ============================================================================

// Check if address is aligned for given access size
bool ref_is_aligned(word32_t addr, int access_size) {
    uint32_t addr_val = word32_to_uint32(&addr);
    
    switch (access_size) {
        case 4: // Word access requires 4-byte alignment
            return (addr_val & 0x3) == 0;
        case 2: // Halfword access requires 2-byte alignment
            return (addr_val & 0x1) == 0;
        case 1: // Byte access has no alignment requirement
            return true;
        default:
            return false;
    }
}

// Memory access result (includes alignment check)
typedef struct {
    word32_t data;
    bool alignment_fault;
} memory_result_t;

// Safe load with alignment checking
memory_result_t ref_load_aligned(bit_memory_t* mem, word32_t addr, int size) {
    memory_result_t result;
    result.alignment_fault = !ref_is_aligned(addr, size);
    
    if (result.alignment_fault) {
        result.data = word32_fill(false); // Return zeros on fault
        return result;
    }
    
    switch (size) {
        case 4:
            result.data = ref_load_word(mem, addr);
            break;
        case 2:
            result.data = ref_load_halfword(mem, addr);
            break;
        case 1:
            result.data = ref_load_byte(mem, addr);
            break;
        default:
            result.data = word32_fill(false);
            result.alignment_fault = true;
    }
    
    return result;
}