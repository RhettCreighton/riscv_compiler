# RISC-V zkVM Memory Constraints Guide

## 🎯 Understanding the 10MB Limit

The zkVM has a fundamental constraint: **10MB total for input and output data**. This is not a limitation of our compiler, but a requirement of the underlying proof system to ensure efficient zero-knowledge proof generation.

### Why This Limit Exists

1. **Proof Generation Speed**: Larger circuits exponentially increase proving time
2. **Verifier Efficiency**: Verification must remain constant-time (~13ms)
3. **Memory Requirements**: Prover memory scales with circuit size
4. **Practical Deployment**: Ensures zkVM can run on commodity hardware

## 📊 Memory Budget Breakdown

```
Total Available: 10,485,760 bytes (10 MB)

Fixed Overhead:
├── Constants:      2 bits
├── Program Counter: 32 bits  
├── Registers (x32): 1,024 bits
└── Total Overhead: ~133 bytes

Available for Program Memory: ~10,485,627 bytes (~9.99 MB)
```

### Memory Layout

```
┌─────────────────┐ 0x00000000
│   Code (.text)  │ ← Your program instructions
├─────────────────┤ 
│   Data (.data)  │ ← Initialized variables
├─────────────────┤
│   BSS (.bss)    │ ← Uninitialized variables  
├─────────────────┤
│      Heap       │ ← malloc() allocations (grows ↑)
├ ─ ─ ─ ─ ─ ─ ─ ─┤
│   Free Space    │
├ ─ ─ ─ ─ ─ ─ ─ ─┤
│      Stack      │ ← Function calls (grows ↓)
└─────────────────┘ 10MB limit
```

## ✅ Working Within Constraints

### 1. Check Memory Requirements Early

```c
// Before compilation, analyze your program:
memory_analysis_t* analysis = analyze_memory_requirements(program);
print_memory_analysis(analysis);

// This will show:
// - Code size
// - Data requirements  
// - Estimated heap usage
// - Stack requirements
// - Total memory needed
```

### 2. Use Memory-Aware Compilation

```c
// Instead of:
riscv_compiler_t* compiler = riscv_compiler_create();

// Use:
riscv_compiler_t* compiler = riscv_compiler_create_constrained(memory_size);
// This enforces limits and provides clear errors
```

### 3. Design Patterns for Large Data

#### Pattern 1: Chunked Processing
```c
#define CHUNK_SIZE (1024 * 1024)  // 1MB chunks

void process_large_array(uint32_t* data, size_t total_size) {
    for (size_t offset = 0; offset < total_size; offset += CHUNK_SIZE) {
        size_t chunk_size = MIN(CHUNK_SIZE, total_size - offset);
        
        // Process this chunk in zkVM
        load_chunk(data + offset, chunk_size);
        process_chunk();
        generate_proof();
    }
}
```

#### Pattern 2: Streaming with State
```c
typedef struct {
    uint32_t hash_state[8];  // Running hash
    size_t bytes_processed;
} streaming_state_t;

void process_streaming(streaming_state_t* state, uint8_t* data, size_t size) {
    // Update state with new data
    update_hash(state->hash_state, data, size);
    state->bytes_processed += size;
    
    // Generate proof for this segment
    generate_incremental_proof(state);
}
```

#### Pattern 3: Merkle Tree Verification
```c
// Instead of loading entire dataset, verify merkle proofs
typedef struct {
    uint8_t root[32];
    uint8_t leaf_data[1024];
    uint8_t proof[32 * 10];  // Path to root
} merkle_verification_t;

bool verify_data_element(merkle_verification_t* mv) {
    // Only ~33KB instead of entire dataset
    return verify_merkle_proof(mv->leaf_data, mv->proof, mv->root);
}
```

## 🚫 Common Pitfalls

### 1. Large Static Arrays
```c
// ❌ BAD: Uses 4MB of memory
int huge_lookup_table[1024 * 1024] = { /* ... */ };

// ✅ GOOD: Generate dynamically or use smaller table
int* create_lookup_table(size_t size) {
    int* table = malloc(size * sizeof(int));
    for (size_t i = 0; i < size; i++) {
        table[i] = compute_value(i);
    }
    return table;
}
```

### 2. Deep Recursion
```c
// ❌ BAD: Each call uses stack space
int recursive_factorial(int n) {
    if (n <= 1) return 1;
    return n * recursive_factorial(n - 1);
}

// ✅ GOOD: Iterative approach
int iterative_factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}
```

### 3. Memory Leaks
```c
// ❌ BAD: Memory never freed
void process_data() {
    int* buffer = malloc(1024 * 1024);
    // ... use buffer ...
    // Forgot to free!
}

// ✅ GOOD: Always free allocated memory
void process_data_safe() {
    int* buffer = malloc(1024 * 1024);
    if (!buffer) return;
    
    // ... use buffer ...
    
    free(buffer);  // Always clean up
}
```

## 📈 Memory Optimization Strategies

### 1. Measure First
```bash
# Use size command to analyze binary
$ riscv32-unknown-elf-size myprogram
   text    data     bss     dec     hex filename
  12456    2048    4096   18600    48a8 myprogram
```

### 2. Optimize Data Structures
```c
// Before: 12 bytes per entry
struct fat_entry {
    int id;          // 4 bytes
    int value;       // 4 bytes  
    int flags;       // 4 bytes
};

// After: 5 bytes per entry
struct slim_entry {
    uint16_t id;     // 2 bytes
    uint16_t value;  // 2 bytes
    uint8_t flags;   // 1 byte
} __attribute__((packed));
```

### 3. Use Compression
```c
// Store repetitive data compressed
uint8_t compressed_data[] = { /* RLE encoded */ };

void decompress_on_demand(uint8_t* output, size_t offset, size_t len) {
    // Decompress only the needed portion
    rle_decode(compressed_data, output, offset, len);
}
```

## 🔧 Debugging Memory Issues

### Enable Memory Tracking
```c
// Add to your program
void print_memory_usage() {
    extern char _end;  // End of BSS
    extern char __heap_start;
    extern char __stack_start;
    
    printf("Memory usage:\n");
    printf("  Program end: %p\n", &_end);
    printf("  Heap start:  %p\n", &__heap_start);
    printf("  Stack start: %p\n", &__stack_start);
}
```

### Use Static Analysis
```bash
# Generate memory map
$ riscv32-unknown-elf-objdump -h myprogram

# Check symbol sizes
$ riscv32-unknown-elf-nm --size-sort myprogram
```

## 💡 Best Practices

1. **Design for Constraints**: Plan memory usage before coding
2. **Test Early**: Check memory requirements during development
3. **Use Tools**: Leverage analysis tools to track usage
4. **Document Limits**: Make memory requirements clear
5. **Provide Alternatives**: Offer chunked/streaming options

## 🎯 Example: Complete Memory-Aware Program

```c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Configuration for zkVM constraints
#define MAX_ARRAY_SIZE (1024 * 1024)  // 1M elements = 4MB
#define STACK_SIZE (64 * 1024)        // 64KB stack
#define HEAP_RESERVE (1024 * 1024)    // 1MB heap

// Compile-time memory budget check
_Static_assert(
    sizeof(uint32_t) * MAX_ARRAY_SIZE + STACK_SIZE + HEAP_RESERVE < 10485760,
    "Program exceeds zkVM memory limit!"
);

// Memory-efficient quicksort
void quicksort(uint32_t* arr, int low, int high) {
    // Iterative implementation to control stack usage
    int stack[64];  // Max 32 levels deep
    int top = -1;
    
    stack[++top] = low;
    stack[++top] = high;
    
    while (top >= 0) {
        high = stack[top--];
        low = stack[top--];
        
        if (low < high) {
            int pivot = partition(arr, low, high);
            
            // Push larger partition first (tail recursion optimization)
            if (pivot - low < high - pivot) {
                stack[++top] = pivot + 1;
                stack[++top] = high;
                stack[++top] = low;
                stack[++top] = pivot - 1;
            } else {
                stack[++top] = low;
                stack[++top] = pivot - 1;
                stack[++top] = pivot + 1;
                stack[++top] = high;
            }
        }
    }
}

int main() {
    // Allocate within constraints
    uint32_t* data = malloc(MAX_ARRAY_SIZE * sizeof(uint32_t));
    if (!data) return -1;
    
    // Process data
    initialize_array(data, MAX_ARRAY_SIZE);
    quicksort(data, 0, MAX_ARRAY_SIZE - 1);
    
    // Clean up
    free(data);
    
    return 0;
}
```

## 📚 Additional Resources

- [RISC-V zkVM Examples](./examples/)
- [Memory Analysis Tools](./tools/memory_analyzer.py)
- [Chunking Library](./lib/chunked_processing.h)
- [Merkle Tree Utils](./lib/merkle_utils.h)

## 🤝 Getting Help

If you encounter memory constraint issues:

1. Run memory analysis first
2. Check the error messages - they provide specific guidance
3. Refer to the examples in this guide
4. Consider alternative approaches (chunking, streaming, etc.)
5. Ask in the community forums with your memory analysis output

Remember: **The 10MB limit ensures your proofs can be generated and verified efficiently**. Working within constraints leads to better, more efficient programs!