#include "test_programs.h"

// Simple arithmetic: compute a + b, store in x3, then x3 - x1, store in x4
uint32_t simple_arithmetic_program[] = {
    0x002081B3,  // add x3, x1, x2    // x3 = x1 + x2 (a + b)
    0x40118233   // sub x4, x3, x1    // x4 = x3 - x1 = x2
};
size_t simple_arithmetic_program_size = sizeof(simple_arithmetic_program) / sizeof(uint32_t);

// Fibonacci: compute first few fibonacci numbers
// x1 = 0, x2 = 1, compute next numbers in sequence
uint32_t fibonacci_program[] = {
    0x00100093,  // addi x1, x0, 1    // x1 = 1 (fib(1))
    0x00100113,  // addi x2, x0, 1    // x2 = 1 (fib(2))  
    0x002081B3,  // add x3, x1, x2    // x3 = fib(3) = 2
    0x00210213,  // add x4, x2, x3    // x4 = fib(4) = 3  
    0x003182B3,  // add x5, x3, x4    // x5 = fib(5) = 5
    0x00420333   // add x6, x4, x5    // x6 = fib(6) = 8
};
size_t fibonacci_program_size = sizeof(fibonacci_program) / sizeof(uint32_t);

// Bitwise operations test
uint32_t bitwise_program[] = {
    0x0020C1B3,  // xor x3, x1, x2    // x3 = x1 ^ x2
    0x0020E213,  // or x4, x1, x2     // x4 = x1 | x2
    0x0020F293,  // and x5, x1, x2    // x5 = x1 & x2
    0x0031C313,  // xor x6, x3, x4    // x6 = x3 ^ x4
    0x00536393   // or x7, x6, x5     // x7 = x6 | x5
};
size_t bitwise_program_size = sizeof(bitwise_program) / sizeof(uint32_t);

// Shift operations test  
uint32_t shift_program[] = {
    0x00109093,  // slli x1, x1, 1    // x1 = x1 << 1
    0x0010D113,  // srli x2, x1, 1    // x2 = x1 >> 1 (logical)
    0x4010D193,  // srai x3, x1, 1    // x3 = x1 >> 1 (arithmetic)
    0x00209213,  // slli x4, x1, 2    // x4 = x1 << 2
    0x00409293   // slli x5, x1, 4    // x5 = x1 << 4
};
size_t shift_program_size = sizeof(shift_program) / sizeof(uint32_t);

// Comparison operations test
uint32_t comparison_program[] = {
    0x0020A1B3,  // slt x3, x1, x2    // x3 = (x1 < x2) ? 1 : 0 (signed)
    0x0020B213,  // sltu x4, x1, x2   // x4 = (x1 < x2) ? 1 : 0 (unsigned)
    0x00000213,  // addi x4, x0, 0    // x4 = 0 (clear for test)
    0x0020A233,  // slt x4, x1, x2    // x4 = (x1 < x2) signed
    0x002032B3   // sltu x5, x0, x2   // x5 = (0 < x2) ? 1 : 0
};
size_t comparison_program_size = sizeof(comparison_program) / sizeof(uint32_t);

// Complex arithmetic using only available instructions
uint32_t complex_arithmetic_program[] = {
    0x002081B3,  // add x3, x1, x2    // x3 = x1 + x2
    0x40318233,  // sub x4, x3, x1    // x4 = x3 - x1 = x2
    0x004201B3,  // add x3, x4, x4    // x3 = x4 + x4 = 2*x2
    0x0031C293,  // xor x5, x3, x1    // x5 = x3 ^ x1 = 2*x2 ^ x1
    0x00529313,  // add x6, x5, x5    // x6 = x5 + x5 = 2*x5
    0x40630393,  // sub x7, x6, x3    // x7 = x6 - x3 = 2*x5 - 2*x2
    0x0073E413   // or x8, x7, x3     // x8 = x7 | x3
};
size_t complex_arithmetic_program_size = sizeof(complex_arithmetic_program) / sizeof(uint32_t);