/*
 * zkvm.h - Zero-Knowledge Virtual Machine C Library
 * 
 * Provides efficient primitives for writing C programs that compile
 * to compact gate circuits. These functions map to optimized
 * circuit implementations.
 */

#ifndef ZKVM_H
#define ZKVM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants - These are FREE (hardwired inputs)
// ============================================================================

// These map to input bits 0 and 1, which are hardwired
#define ZERO ((uint32_t)0)  // Maps to input bit 0 (constant false)
#define ONE  ((uint32_t)1)  // Maps to input bit 1 (constant true)

// Common bit patterns (constructed from constants at compile time)
#define ALL_ONES  ((uint32_t)0xFFFFFFFF)
#define HIGH_MASK ((uint32_t)0xFFFF0000)
#define LOW_MASK  ((uint32_t)0x0000FFFF)

// ============================================================================
// Efficient Primitives - Optimized Gate Implementations
// ============================================================================

// SHA3-256 - Optimized circuit (~2M gates total)
void zkvm_sha3_256(const uint32_t* input, size_t words, uint32_t* output);

// Memory operations - Use these instead of loops
void zkvm_memcpy(uint32_t* dst, const uint32_t* src, size_t words);
void zkvm_memset(uint32_t* dst, uint32_t value, size_t words);
int zkvm_memcmp(const uint32_t* a, const uint32_t* b, size_t words);

// Bit manipulation - Optimized circuits
uint32_t zkvm_popcnt(uint32_t x);      // Population count (# of 1 bits)
uint32_t zkvm_clz(uint32_t x);         // Count leading zeros
uint32_t zkvm_ctz(uint32_t x);         // Count trailing zeros
uint32_t zkvm_parity(uint32_t x);      // Parity (XOR of all bits)
uint32_t zkvm_reverse_bits(uint32_t x); // Reverse bit order

// Arithmetic helpers - Gate-aware implementations
uint32_t zkvm_abs(int32_t x);          // Absolute value without branching
uint32_t zkvm_min(uint32_t a, uint32_t b);
uint32_t zkvm_max(uint32_t a, uint32_t b);
uint32_t zkvm_select(bool cond, uint32_t a, uint32_t b); // Branchless select

// ============================================================================
// Memory Management - Constraint Aware
// ============================================================================

// Memory allocation within 10MB constraint
void* zkvm_malloc(size_t bytes);
void zkvm_free(void* ptr);
size_t zkvm_memory_used(void);         // Current memory usage
size_t zkvm_memory_available(void);    // Remaining memory budget

// ============================================================================
// Assertions & Constraints
// ============================================================================

// These become circuit constraints (not runtime checks)
void zkvm_assert(bool condition);
void zkvm_assert_eq(uint32_t a, uint32_t b);
void zkvm_assert_ne(uint32_t a, uint32_t b);
void zkvm_assert_lt(uint32_t a, uint32_t b);
void zkvm_assert_le(uint32_t a, uint32_t b);

// Range constraints
void zkvm_assert_range(uint32_t value, uint32_t min, uint32_t max);
void zkvm_assert_bit(uint32_t value, int bit_position);

// ============================================================================
// Input/Output - Circuit Interface
// ============================================================================

// Read inputs (from circuit input bits)
void zkvm_input(uint32_t* buffer, size_t words);
uint32_t zkvm_input_u32(void);

// Write outputs (to circuit output bits)  
void zkvm_output(const uint32_t* buffer, size_t words);
void zkvm_output_u32(uint32_t value);

// ============================================================================
// Performance Hints - Compiler Directives
// ============================================================================

// Hints to the circuit compiler for optimization
#define ZKVM_UNROLL(n)      __attribute__((zkvm_unroll(n)))
#define ZKVM_INLINE         __attribute__((always_inline))
#define ZKVM_PURE           __attribute__((pure))
#define ZKVM_CONST          __attribute__((const))
#define ZKVM_HOT            __attribute__((hot))
#define ZKVM_COLD           __attribute__((cold))

// Memory tier selection
#define ZKVM_ULTRA_MEMORY   __attribute__((zkvm_memory("ultra")))   // 2.2K gates
#define ZKVM_SIMPLE_MEMORY  __attribute__((zkvm_memory("simple")))  // 101K gates
#define ZKVM_SECURE_MEMORY  __attribute__((zkvm_memory("secure")))  // 3.9M gates/access

// ============================================================================
// Gate Cost Annotations - For Documentation
// ============================================================================

// Use these in comments to document gate costs
#define GATES(n) /* ~n gates */

// Example:
// uint32_t xor_all(uint32_t a, uint32_t b) GATES(32) {
//     return a ^ b;  
// }

// ============================================================================
// Debugging Support
// ============================================================================

#ifdef ZKVM_DEBUG
    // In debug mode, these print gate counts
    void zkvm_trace(const char* label, uint32_t value);
    void zkvm_checkpoint(const char* label);
    void zkvm_report_gates(void);
#else
    // In release mode, these compile to nothing
    #define zkvm_trace(label, value) ((void)0)
    #define zkvm_checkpoint(label)   ((void)0)
    #define zkvm_report_gates()      ((void)0)
#endif

// ============================================================================
// Common Patterns - Macros for Efficiency
// ============================================================================

// Branchless conditional assignment (32-96 gates)
#define ZKVM_COND_ASSIGN(cond, true_val, false_val) \
    (((cond) & (true_val)) | ((~(cond)) & (false_val)))

// Swap without temporary (96 gates)
#define ZKVM_SWAP(a, b) do { \
    (a) ^= (b); \
    (b) ^= (a); \
    (a) ^= (b); \
} while(0)

// Check if power of 2 (33 gates)
#define ZKVM_IS_POWER_OF_2(x) \
    (((x) != 0) && (((x) & ((x) - 1)) == 0))

// Round up to power of 2 (~200 gates)
#define ZKVM_ROUND_UP_POWER_2(x) \
    zkvm_round_up_power_2(x)

static inline uint32_t zkvm_round_up_power_2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

#ifdef __cplusplus
}
#endif

#endif // ZKVM_H