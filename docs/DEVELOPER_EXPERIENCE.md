# Developer Experience Design for Circuit Compilation

## Overview

We provide two paths for developers to create efficient gate circuits for zero-knowledge proofs:

1. **C → RISC-V → Circuit** (High-level, Easy)
2. **RISC-V → Circuit** (Low-level, Optimal)

## Path 1: C to Circuit (Recommended for Most Users)

### Toolchain
```bash
# Step 1: Compile C to RISC-V
riscv32-unknown-elf-gcc -O2 -march=rv32im -mabi=ilp32 \
    -nostdlib -nostartfiles \
    -T link.ld \
    -o program.elf program.c

# Step 2: Compile RISC-V to Circuit  
./riscv_to_circuit program.elf -o program.circuit
```

### Developer Workflow

```c
// zkvm_program.c
#include "zkvm.h"  // Provides efficient primitives

// Constants are FREE - use these macros
#define ZERO CONSTANT_0  // Maps to input bit 0
#define ONE  CONSTANT_1  // Maps to input bit 1

// Example: Efficient hash computation
void compute_hash(uint32_t* input, uint32_t* output) {
    // Use zkvm_sha3 instead of loops - compiles to optimized circuit
    zkvm_sha3_256(input, 16, output);  // 16 words input
}

// Example: Efficient arithmetic
uint32_t efficient_multiply(uint32_t a, uint32_t b) {
    // Compiler knows to use Booth multiplier
    return a * b;  // 11,600 gates
}

// Example: Avoiding expensive operations
void process_data(uint32_t* data, size_t len) {
    // BAD: Random memory access is expensive
    // for (int i = 0; i < len; i++) {
    //     result ^= memory[data[i]];  // Each access = 3.9M gates!
    // }
    
    // GOOD: Sequential access with local computation
    uint32_t accumulator = ZERO;
    for (int i = 0; i < len; i++) {
        accumulator ^= data[i];  // Just 32 gates per iteration
    }
    return accumulator;
}
```

### C Library for ZK Circuits (`zkvm.h`)

```c
// Efficient primitives that map to optimized circuits
void zkvm_sha3_256(uint32_t* input, size_t words, uint32_t* output);
void zkvm_memcpy(uint32_t* dst, uint32_t* src, size_t words);
uint32_t zkvm_popcnt(uint32_t x);  // Population count
uint32_t zkvm_clz(uint32_t x);     // Count leading zeros

// Memory allocation within constraints
void* zkvm_malloc(size_t bytes);   // Tracks memory budget
void zkvm_free(void* ptr);

// Assertions that become circuit constraints
void zkvm_assert(bool condition);
void zkvm_assert_eq(uint32_t a, uint32_t b);
```

## Path 2: RISC-V Assembly to Circuit (For Optimization)

### When to Use Assembly
- Cryptographic primitives (SHA3, AES, etc.)
- Inner loops that dominate gate count
- Bit manipulation algorithms
- When you need exact gate counts

### Example: Optimized XOR-Reduce
```asm
# xor_reduce.s - XOR all elements in array
# Input: a0 = array pointer, a1 = length
# Output: a0 = XOR of all elements

xor_reduce:
    li   t0, 0          # accumulator = 0 (uses x0 which should be 0)
    li   t1, 0          # counter = 0
    
loop:
    bge  t1, a1, done   # if counter >= length, done
    lw   t2, 0(a0)      # load array[i]
    xor  t0, t0, t2     # accumulator ^= array[i] (32 gates!)
    addi a0, a0, 4      # array++
    addi t1, t1, 1      # counter++
    j    loop           # continue
    
done:
    mv   a0, t0         # return accumulator
    ret
```

## Cost Model for Developers

### Gate Costs by Operation

| Operation | C Code | Gate Cost | Notes |
|-----------|--------|-----------|-------|
| XOR | `a ^ b` | 32 | Optimal - use freely |
| AND/OR | `a & b` | 32-96 | AND optimal, OR needs conversion |
| ADD/SUB | `a + b` | 224 | Use sparingly in loops |
| Multiply | `a * b` | 11,600 | Avoid in loops! |
| Divide | `a / b` | 11,600 | Very expensive |
| Shift | `a << n` | 640 | Constant shifts preferred |
| Memory Load | `mem[i]` | 3,900,000 | Cache in registers! |
| Comparison | `a < b` | 96-257 | Optimized in latest version |

### Memory Access Patterns

```c
// TERRIBLE: Random access in loop
for (i = 0; i < n; i++) {
    sum += memory[indices[i]];  // 3.9M gates per iteration!
}

// GOOD: Sequential access
for (i = 0; i < n; i++) {
    sum += data[i];  // Just 224 gates per iteration
}

// BEST: Batch operations
zkvm_sha3_256(data, n/4, hash);  // Optimized circuit
```

## Circuit Design Patterns

### Pattern 1: Constant Folding
```c
// Constants are FREE - use them!
uint32_t mask = 0xFF00FF00;  // Compiles to wiring, not gates
uint32_t result = value & mask;  // Only 32 gates
```

### Pattern 2: Bit Manipulation
```c
// Prefer bit operations over arithmetic
uint32_t is_even = (x & 1) ^ 1;     // 33 gates
// NOT: uint32_t is_even = (x % 2) == 0;  // Thousands of gates!
```

### Pattern 3: Loop Unrolling
```c
// Unroll to reduce loop overhead
for (i = 0; i < n; i += 4) {
    acc ^= data[i];     // 32 gates
    acc ^= data[i+1];   // 32 gates  
    acc ^= data[i+2];   // 32 gates
    acc ^= data[i+3];   // 32 gates
    // Total: 128 gates vs 128 + loop overhead
}
```

### Pattern 4: Memory Tiers
```c
// Choose memory tier based on needs
#ifdef ULTRA_FAST_MEMORY
    // 8 words only, 2.2K gates total
    uint32_t cache[8];
#elif defined(SIMPLE_MEMORY)  
    // 256 words, 101K gates total
    uint32_t buffer[256];
#else
    // Full memory with Merkle proofs, 3.9M gates per access
    uint32_t* heap = zkvm_malloc(size);
#endif
```

## Debugging & Profiling

### Gate Count Profiler
```bash
# Compile with profiling
./riscv_to_circuit program.elf --profile -o program.circuit

# Output:
Function         | Instructions | Gates    | Gates/Instr
-----------------|--------------|----------|------------
main             | 245          | 125,680  | 513
compute_hash     | 89           | 2,080    | 23
process_array    | 156          | 35,264   | 226
TOTAL            | 490          | 163,024  | 333
```

### Circuit Visualization
```bash
# Generate visual representation
./riscv_to_circuit program.elf --visualize -o program.svg

# Shows:
# - Gate types by color (AND=blue, XOR=green)
# - Critical paths in red
# - Memory accesses highlighted
```

## Best Practices Summary

### DO:
- ✅ Use constants freely (0 and 1 are wired inputs)
- ✅ Prefer XOR/AND over arithmetic
- ✅ Cache memory values in registers
- ✅ Use zkvm library functions
- ✅ Profile gate counts early and often
- ✅ Batch operations when possible

### DON'T:
- ❌ Use division unless absolutely necessary
- ❌ Access memory in tight loops
- ❌ Use modulo for bit operations
- ❌ Assume C optimizer understands gates
- ❌ Ignore the 10MB input/output limit

## Example: Complete Program

```c
// fibonacci_zkvm.c - Efficient Fibonacci for circuits
#include "zkvm.h"

uint32_t fibonacci(uint32_t n) {
    if (n <= 1) return n;
    
    uint32_t a = 0, b = 1;
    
    // Unroll by 2 for efficiency
    for (uint32_t i = 2; i <= n; i += 2) {
        uint32_t t1 = a + b;  // 224 gates
        uint32_t t2 = b + t1; // 224 gates
        a = t1;
        b = t2;
        
        if (i == n) return t1;
    }
    
    return b;
}

int main() {
    uint32_t n = 10;
    uint32_t result = fibonacci(n);
    
    // Output must fit in circuit outputs
    zkvm_output(&result, 1);
    
    return 0;
}
```

Compile and run:
```bash
riscv32-unknown-elf-gcc -O2 -march=rv32im fibonacci_zkvm.c -o fib.elf
./riscv_to_circuit fib.elf -o fib.circuit --stats

# Stats:
# Total gates: 2,688
# Circuit depth: 67
# Memory usage: 0 (register only)
# Estimated proving time: 0.3s
```

## Getting Started

1. **Install RISC-V toolchain**: 
   ```bash
   # Ubuntu/Debian
   sudo apt install gcc-riscv64-linux-gnu
   
   # Or build from source for rv32
   git clone https://github.com/riscv/riscv-gnu-toolchain
   ```

2. **Write efficient C code** using the patterns above

3. **Compile to circuit** and check gate counts

4. **Optimize hotspots** in assembly if needed

5. **Verify correctness** with our formal verification tools

The key is understanding the cost model and designing algorithms that map efficiently to gates!