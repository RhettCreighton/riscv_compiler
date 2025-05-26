#ifndef TEST_PROGRAMS_H
#define TEST_PROGRAMS_H

#include <stdint.h>
#include <stddef.h>

// Test program definitions for differential testing

// Simple arithmetic test
extern uint32_t simple_arithmetic_program[];
extern size_t simple_arithmetic_program_size;

// Fibonacci calculation (iterative)
extern uint32_t fibonacci_program[];
extern size_t fibonacci_program_size;

// Bit manipulation test
extern uint32_t bitwise_program[];
extern size_t bitwise_program_size;

// Shift operations test
extern uint32_t shift_program[];
extern size_t shift_program_size;

// Comparison operations test
extern uint32_t comparison_program[];
extern size_t comparison_program_size;

// Complex arithmetic (multiple operations)
extern uint32_t complex_arithmetic_program[];
extern size_t complex_arithmetic_program_size;

#endif // TEST_PROGRAMS_H