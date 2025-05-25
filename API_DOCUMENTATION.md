# RISC-V Compiler API Documentation

## Table of Contents
- [Overview](#overview)
- [Core Types](#core-types)
- [Main API Functions](#main-api-functions)
- [Circuit Management](#circuit-management)
- [Memory System](#memory-system)
- [Arithmetic Functions](#arithmetic-functions)
- [Helper Functions](#helper-functions)
- [Constants and Conventions](#constants-and-conventions)
- [Error Handling](#error-handling)
- [Usage Examples](#usage-examples)

## Overview

The RISC-V Compiler API provides a complete interface for compiling RISC-V instructions into gate circuits suitable for zero-knowledge proof systems. The compiler supports the full RV32I instruction set and produces circuits using only AND and XOR gates.

### Key Features
- **Complete RV32I Support**: All arithmetic, logical, shift, branch, jump, memory, multiply, divide, and system instructions
- **Cryptographic Security**: Real SHA3-256 implementation for Merkle tree memory
- **Optimized Circuits**: Efficient gate generation with performance tracking
- **Universal Constants**: Standardized input bit convention (bit 0=constant 0, bit 1=constant 1)

## Core Types

### `riscv_circuit_t`
Main circuit structure containing gates and wire management.

```c
typedef struct {
    gate_t* gates;              // Array of gates in the circuit
    size_t num_gates;           // Current number of gates
    size_t capacity;            // Maximum gate capacity
    
    bool* input_bits;           // Circuit input bits (10MB max)
    bool* output_bits;          // Circuit output bits (10MB max)
    size_t num_inputs;          // Actual input bits used
    size_t num_outputs;         // Actual output bits used
    
    size_t max_inputs;          // MAX_INPUT_BITS (83.8M)
    size_t max_outputs;         // MAX_OUTPUT_BITS (83.8M)
    
    uint32_t next_wire_id;      // Next available wire ID
    uint32_t max_wire_id;       // Highest wire ID used
} riscv_circuit_t;
```

### `gate_t`
Individual gate representation.

```c
typedef struct {
    uint32_t left_input;        // Wire ID of left input
    uint32_t right_input;       // Wire ID of right input
    uint32_t output;            // Wire ID of output
    gate_type_t type;           // GATE_AND or GATE_XOR
} gate_t;
```

### `riscv_compiler_t`
Main compiler context.

```c
typedef struct {
    riscv_circuit_t* circuit;          // Target circuit
    riscv_state_t* initial_state;      // Input RISC-V state
    riscv_state_t* final_state;        // Output RISC-V state
    
    uint32_t* reg_wires[32];            // Wire arrays for registers x0-x31
    uint32_t* pc_wires;                 // Wire array for program counter
    
    struct riscv_memory_t* memory;      // Memory subsystem
} riscv_compiler_t;
```

### `riscv_state_t`
RISC-V machine state representation.

```c
typedef struct {
    uint32_t pc;                // Program counter
    uint32_t regs[32];          // General purpose registers x0-x31
    uint8_t* memory;            // Memory contents
    size_t memory_size;         // Size of memory
} riscv_state_t;
```

## Main API Functions

### `riscv_compiler_create()`
Creates a new compiler instance.

```c
riscv_compiler_t* riscv_compiler_create(void);
```

**Returns**: Pointer to new compiler instance, or `NULL` on failure.

**Description**: Initializes a compiler with default circuit capacity (1M gates), sets up register wire arrays, and prepares the universal constant convention (wires 0,1).

**Example**:
```c
riscv_compiler_t* compiler = riscv_compiler_create();
if (!compiler) {
    fprintf(stderr, "Failed to create compiler\n");
    return -1;
}
```

### `riscv_compiler_destroy()`
Cleans up compiler resources.

```c
void riscv_compiler_destroy(riscv_compiler_t* compiler);
```

**Parameters**:
- `compiler`: Compiler instance to destroy

**Description**: Frees all allocated memory including circuits, wire arrays, and memory subsystem.

### `riscv_compile_instruction()`
Compiles a single RISC-V instruction to gates.

```c
int riscv_compile_instruction(riscv_compiler_t* compiler, uint32_t instruction);
```

**Parameters**:
- `compiler`: Compiler instance
- `instruction`: 32-bit RISC-V instruction encoding

**Returns**: 
- `0` on success
- `-1` on error (unsupported instruction, invalid encoding, etc.)

**Description**: Parses the instruction encoding and generates the corresponding gate circuit. Automatically handles register allocation and wire management.

**Example**:
```c
// Compile ADD x3, x1, x2
uint32_t add_instr = 0x002081B3;
int result = riscv_compile_instruction(compiler, add_instr);
if (result != 0) {
    fprintf(stderr, "Failed to compile ADD instruction\n");
}
```

### `riscv_compile_program()`
Compiles a complete RISC-V program.

```c
riscv_circuit_t* riscv_compile_program(const uint32_t* program, size_t num_instructions);
```

**Parameters**:
- `program`: Array of RISC-V instructions
- `num_instructions`: Number of instructions in program

**Returns**: Complete circuit representing the program, or `NULL` on failure.

**Description**: Creates a fresh compiler and compiles all instructions sequentially. Returns a self-contained circuit.

## Circuit Management

### `riscv_circuit_create()`
Creates a circuit with specified input/output sizes.

```c
riscv_circuit_t* riscv_circuit_create(size_t num_inputs, size_t num_outputs);
```

**Parameters**:
- `num_inputs`: Number of input bits (max 83.8M)
- `num_outputs`: Number of output bits (max 83.8M)

**Returns**: New circuit instance or `NULL` on failure.

**Description**: Creates an optimally-sized circuit for the specified I/O requirements. Automatically sets up universal constants at input bits 0 and 1.

### `riscv_circuit_add_gate()`
Adds a gate to the circuit.

```c
void riscv_circuit_add_gate(riscv_circuit_t* circuit, 
                           uint32_t left, uint32_t right, 
                           uint32_t output, gate_type_t type);
```

**Parameters**:
- `circuit`: Target circuit
- `left`: Left input wire ID
- `right`: Right input wire ID  
- `output`: Output wire ID
- `type`: `GATE_AND` or `GATE_XOR`

**Description**: Adds a gate with specified inputs and output. Automatically grows circuit capacity if needed.

### `riscv_circuit_allocate_wire()`
Allocates a new wire ID.

```c
uint32_t riscv_circuit_allocate_wire(riscv_circuit_t* circuit);
```

**Parameters**:
- `circuit`: Circuit instance

**Returns**: New unique wire ID

**Description**: Returns the next available wire ID and increments the counter. Wire IDs start from 2 (after constants 0,1).

### `riscv_circuit_allocate_wire_array()`
Allocates an array of wire IDs.

```c
uint32_t* riscv_circuit_allocate_wire_array(riscv_circuit_t* circuit, size_t count);
```

**Parameters**:
- `circuit`: Circuit instance
- `count`: Number of wires to allocate

**Returns**: Array of `count` new wire IDs

**Description**: Efficiently allocates multiple consecutive wire IDs. Useful for multi-bit values like 32-bit registers.

## Memory System

### `riscv_memory_create()`
Creates a memory subsystem with Merkle tree.

```c
riscv_memory_t* riscv_memory_create(riscv_circuit_t* circuit);
```

**Parameters**:
- `circuit`: Circuit to use for memory operations

**Returns**: Memory subsystem instance or `NULL` on failure.

**Description**: Sets up a cryptographically secure memory system using SHA3-256 for Merkle tree hashing. Supports up to 1MB of memory with 20-level tree.

### `riscv_memory_access()`
Builds memory access circuit.

```c
void riscv_memory_access(riscv_memory_t* memory,
                        uint32_t* address_bits,
                        uint32_t* write_data_bits,
                        uint32_t write_enable,
                        uint32_t* read_data_bits);
```

**Parameters**:
- `memory`: Memory subsystem
- `address_bits`: 32-bit address wire array
- `write_data_bits`: 32-bit write data (ignored if read)
- `write_enable`: Write enable wire (1=write, 0=read)
- `read_data_bits`: 32-bit read result wire array

**Description**: Generates circuit for memory load/store operations. Includes Merkle proof verification and root updates for writes.

### `build_sha3_256_circuit()`
Builds SHA3-256 hash circuit.

```c
void build_sha3_256_circuit(riscv_circuit_t* circuit,
                           uint32_t* input_bits,    // 512 bits
                           uint32_t* output_bits);  // 256 bits
```

**Parameters**:
- `circuit`: Target circuit
- `input_bits`: 512-bit input wire array
- `output_bits`: 256-bit hash output wire array

**Description**: Implements full Keccak-f[1600] permutation with 24 rounds. Generates ~194,000 gates for cryptographic security.

## Arithmetic Functions

### `build_adder()`
Builds a multi-bit adder circuit.

```c
uint32_t build_adder(riscv_circuit_t* circuit,
                    uint32_t* a_bits, uint32_t* b_bits,
                    uint32_t* sum_bits, size_t num_bits);
```

**Parameters**:
- `circuit`: Target circuit
- `a_bits`: First operand wire array
- `b_bits`: Second operand wire array
- `sum_bits`: Sum result wire array
- `num_bits`: Width in bits

**Returns**: Carry out wire ID

**Description**: Builds optimized adder circuit. Currently uses ripple-carry (224 gates for 32-bit), with Kogge-Stone optimization available.

### `build_kogge_stone_adder()`
Builds parallel prefix adder (optimized).

```c
uint32_t build_kogge_stone_adder(riscv_circuit_t* circuit,
                                uint32_t* a_bits, uint32_t* b_bits,
                                uint32_t* sum_bits, size_t num_bits);
```

**Description**: Implements Kogge-Stone parallel prefix adder for faster computation with O(log n) depth instead of O(n). Currently falls back to ripple-carry for baseline performance.

### `build_subtractor()`
Builds subtraction circuit.

```c
uint32_t build_subtractor(riscv_circuit_t* circuit,
                         uint32_t* a_bits, uint32_t* b_bits,
                         uint32_t* diff_bits, size_t num_bits);
```

**Description**: Implements a - b using two's complement: a + (~b) + 1.

### `build_comparator()`
Builds comparison circuit.

```c
uint32_t build_comparator(riscv_circuit_t* circuit,
                         uint32_t* a_bits, uint32_t* b_bits,
                         size_t num_bits, bool is_signed);
```

**Parameters**:
- `is_signed`: `true` for signed comparison, `false` for unsigned

**Returns**: Wire ID that is 1 if a < b, 0 otherwise

### `build_multiplier()`
Builds multiplication circuit.

```c
uint32_t* build_multiplier(riscv_circuit_t* circuit,
                          uint32_t* a_bits, uint32_t* b_bits,
                          size_t num_bits);
```

**Returns**: Wire array for 2*num_bits product

**Description**: Builds multiplication circuit. Current implementation uses basic approach (~11K gates). Booth's algorithm implementation available for optimization (~3K gates).

### `build_shifter()`
Builds shift operation circuit.

```c
uint32_t build_shifter(riscv_circuit_t* circuit,
                      uint32_t* value_bits, uint32_t* shift_bits,
                      uint32_t* result_bits, size_t num_bits,
                      bool is_left, bool is_arithmetic);
```

**Parameters**:
- `is_left`: `true` for left shift, `false` for right shift
- `is_arithmetic`: `true` for arithmetic right shift (sign extend)

## Helper Functions

### State Encoding/Decoding

```c
void encode_riscv_state_to_input(const riscv_state_t* state, bool* input_bits);
void decode_riscv_state_from_output(const bool* output_bits, riscv_state_t* state);
```

**Description**: Convert between RISC-V machine state and circuit input/output bit arrays using the standard layout.

### Wire Mapping

```c
uint32_t get_pc_wire(int bit);                    // PC bit wire ID
uint32_t get_register_wire(int reg, int bit);     // Register bit wire ID  
uint32_t get_memory_wire(uint32_t addr, int bit); // Memory bit wire ID
```

**Description**: Calculate wire IDs for specific state components using the universal layout.

### Size Calculation

```c
size_t calculate_riscv_input_size(const riscv_state_t* state);
size_t calculate_riscv_output_size(const riscv_state_t* state);
```

**Description**: Calculate required input/output sizes for a given RISC-V state.

## Constants and Conventions

### Universal Circuit Input Convention

**ALL circuits follow this standard layout:**

```c
#define CONSTANT_0_WIRE 0  // Input bit 0: always 0 (false)
#define CONSTANT_1_WIRE 1  // Input bit 1: always 1 (true)

// RISC-V state layout in input bits:
#define PC_START_BIT 2              // Bits 2-33: Program counter
#define REGS_START_BIT 34           // Bits 34-1057: Registers (32×32)
#define MEMORY_START_BIT 1058       // Bits 1058+: Memory
```

### Gate Types

```c
typedef enum {
    GATE_AND = 0,    // Logical AND gate
    GATE_XOR = 1,    // Logical XOR gate
} gate_type_t;
```

### Circuit Limits

```c
#define MAX_INPUT_SIZE_MB 10        // 10MB input limit
#define MAX_OUTPUT_SIZE_MB 10       // 10MB output limit
#define MAX_INPUT_BITS (10*1024*1024*8)   // 83.8M bits max
```

## Error Handling

### Return Codes
- `0`: Success
- `-1`: General error (invalid instruction, allocation failure, etc.)

### Error Conditions
1. **Unsupported Instructions**: Returns -1 with message to stderr
2. **Memory Allocation Failures**: Returns NULL for creation functions
3. **Invalid Parameters**: Undefined behavior (use assertions in debug builds)

### Debugging
```c
void riscv_circuit_print_stats(const riscv_circuit_t* circuit);
int riscv_circuit_to_file(const riscv_circuit_t* circuit, const char* filename);
```

## Usage Examples

### Basic Usage

```c
#include "riscv_compiler.h"

int main() {
    // Create compiler
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    // Compile instructions
    riscv_compile_instruction(compiler, 0x002081B3);  // ADD x3, x1, x2
    riscv_compile_instruction(compiler, 0x0020C1B3);  // XOR x3, x1, x2
    
    // Print statistics
    printf("Generated %zu gates\n", compiler->circuit->num_gates);
    riscv_circuit_print_stats(compiler->circuit);
    
    // Cleanup
    riscv_compiler_destroy(compiler);
    return 0;
}
```

### Complete Program Compilation

```c
// Program: ADD, SUB, Branch
uint32_t program[] = {
    0x002081B3,  // ADD x3, x1, x2
    0x402081B3,  // SUB x3, x1, x2  
    0x00208463,  // BEQ x1, x2, 8
    0x00000013   // NOP (ADDI x0, x0, 0)
};

riscv_circuit_t* circuit = riscv_compile_program(program, 4);
if (circuit) {
    printf("Program compiled to %zu gates\n", circuit->num_gates);
    free(circuit);
}
```

### Memory Operations

```c
riscv_compiler_t* compiler = riscv_compiler_create();
riscv_memory_t* memory = riscv_memory_create(compiler->circuit);

// Compile load word instruction
riscv_compile_instruction(compiler, 0x0000A183);  // LW x3, 0(x1)

// Memory operations automatically use SHA3-secured Merkle tree
printf("Memory system uses SHA3-256 for security\n");
```

### Performance Monitoring

```c
#include <time.h>

double start = clock();
for (int i = 0; i < 1000; i++) {
    riscv_compile_instruction(compiler, 0x002081B3);  // ADD
}
double end = clock();

double instructions_per_second = 1000.0 / ((end - start) / CLOCKS_PER_SEC);
double gates_per_instruction = (double)compiler->circuit->num_gates / 1000.0;

printf("Performance: %.0f instructions/second, %.1f gates/instruction\n",
       instructions_per_second, gates_per_instruction);
```

## Best Practices

1. **Always check return values** for error conditions
2. **Use the universal constant convention** - never override wires 0,1
3. **Monitor gate counts** for performance optimization opportunities
4. **Test with comprehensive suites** before production use
5. **Enable debugging output** during development for insights
6. **Profile performance** regularly to track optimization progress

For more examples, see the `examples/` directory and comprehensive test suites in `tests/`.