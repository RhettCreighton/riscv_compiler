# Troubleshooting Guide - RISC-V zkVM Compiler

## Overview

This guide provides solutions to common issues, error messages, and debugging techniques for the RISC-V zkVM compiler. It covers compilation errors, runtime issues, performance problems, and development workflow challenges.

## 🚨 Common Error Messages

### 1. Compilation Errors

#### "Error: Circuit gate limit exceeded"
```
Error: Circuit gate limit exceeded (2000000)
```

**Cause**: Program compilation requires more than 2M gates (memory limit).

**Solutions**:
```bash
# Check program size
wc -l your_program.c

# Compile smaller test programs first
./examples/simple_riscv_demo

# Increase limit in include/riscv_compiler.h
#define MAX_GATES 4000000  // Double the limit

# Use streaming compilation for large programs
./examples/optimized_arithmetic_demo
```

#### "Error: Circuit wire limit would be exceeded"
```
Error: Circuit wire limit would be exceeded
```

**Cause**: Circuit requires more than 4M wires.

**Solutions**:
```c
// In include/riscv_compiler.h, increase limit:
#define MAX_WIRES 8000000  // Double wire limit

// Or implement wire reuse optimization:
void optimize_wire_usage(riscv_circuit_t* circuit) {
    compact_wire_numbering(circuit);
    reuse_temporary_wires(circuit);
}
```

#### "Unknown instruction opcode: 0xXXXXXXXX"
```
Unknown instruction opcode: 0x12345678
```

**Cause**: Unsupported instruction or malformed binary.

**Solutions**:
```bash
# Check if instruction is in RV32I base set
riscv32-unknown-elf-objdump -d your_program.elf

# Verify supported instructions
grep -r "case.*:" src/riscv_compiler.c

# Check for compressed instructions (not supported)
riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 # No 'c' extension
```

### 2. Memory Management Errors

#### "Failed to allocate memory for circuit"
```
Failed to allocate memory for circuit (requested: 10485760 bytes)
```

**Cause**: System out of memory or hitting process limits.

**Solutions**:
```bash
# Check available memory
free -h

# Increase process memory limit
ulimit -v unlimited

# Use smaller test programs
./examples/simple.s  # Start with minimal programs

# Enable memory debugging
valgrind --tool=memcheck ./your_program
```

#### "Segmentation fault in riscv_circuit_add_gate"
```
Segmentation fault (core dumped) at riscv_circuit_add_gate:127
```

**Cause**: Uninitialized circuit or invalid wire references.

**Debug Steps**:
```c
// Add validation in your code:
void safe_add_gate(riscv_circuit_t* circuit, uint32_t left, uint32_t right, 
                  uint32_t output, gate_type_t type) {
    if (!circuit) {
        fprintf(stderr, "Error: Circuit is NULL\n");
        return;
    }
    if (circuit->num_gates >= circuit->max_gates) {
        fprintf(stderr, "Error: Gate limit exceeded\n");
        return;
    }
    if (left >= circuit->next_wire_id || right >= circuit->next_wire_id) {
        fprintf(stderr, "Error: Invalid wire reference\n");
        return;
    }
    riscv_circuit_add_gate(circuit, left, right, output, type);
}
```

### 3. ELF Loading Issues

#### "Failed to load ELF file"
```
Error: Failed to load ELF file 'program.elf'
```

**Cause**: Invalid ELF format or unsupported architecture.

**Solutions**:
```bash
# Verify ELF file format
file program.elf
# Should show: ELF 32-bit LSB executable, UCB RISC-V

# Check compilation flags
riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib program.c

# Verify required sections exist
riscv32-unknown-elf-objdump -h program.elf | grep -E "(text|data)"

# Check entry point
riscv32-unknown-elf-objdump -f program.elf
```

#### "ELF file has no .text section"
```
Error: ELF file has no .text section
```

**Solutions**:
```bash
# Use proper linker script
riscv32-unknown-elf-gcc -T examples/link.ld program.c

# Check section creation
riscv32-unknown-elf-objdump -h program.elf

# Ensure code generation
riscv32-unknown-elf-gcc -S program.c  # Check assembly output
```

## 🔧 Performance Issues

### 1. Slow Compilation

#### "Compilation taking too long (>10 seconds)"

**Diagnosis**:
```bash
# Profile compilation
time ./your_program

# Check instruction count
riscv32-unknown-elf-objdump -d program.elf | grep -c "^\s*[0-9a-f]\+:"

# Monitor memory usage
top -p $(pgrep your_program)
```

**Solutions**:
```c
// Enable compiler optimizations
#define ENABLE_KOGGE_STONE_ADDER 1
#define ENABLE_BOOTH_MULTIPLIER 1

// Use bounded compilation
void compile_with_limits(riscv_compiler_t* compiler, uint32_t* program, 
                        size_t length, size_t max_instructions) {
    for (size_t i = 0; i < length && i < max_instructions; i++) {
        riscv_compile_instruction(compiler, program[i]);
    }
}

// Parallel compilation (future)
#pragma omp parallel for
for (int i = 0; i < independent_instruction_count; i++) {
    compile_instruction_thread_safe(compiler, instructions[i]);
}
```

### 2. High Gate Count

#### "Gate count exceeding targets (>1000 gates/instruction)"

**Analysis**:
```bash
# Run benchmarks to identify bottlenecks
./tests/benchmark_instructions

# Check per-instruction metrics
grep "gates per instruction" build_output.log

# Identify expensive instructions
./tests/test_comprehensive | grep "High gate count"
```

**Optimizations**:
```c
// Enable all optimizations
#define USE_KOGGE_STONE_ADDER 1
#define USE_BOOTH_MULTIPLIER 1
#define ENABLE_CONSTANT_PROPAGATION 1

// Check optimization status
void print_optimization_status(void) {
    printf("Kogge-Stone Adder: %s\n", 
           KOGGE_STONE_ENABLED ? "ENABLED" : "DISABLED");
    printf("Booth Multiplier: %s\n", 
           BOOTH_MULTIPLIER_ENABLED ? "ENABLED" : "DISABLED");
}
```

## 🐛 Debugging Techniques

### 1. Circuit Validation

#### Enable Debug Output
```c
// In your code, add debugging:
#define DEBUG_GATES 1

void debug_print_circuit(riscv_circuit_t* circuit) {
    printf("Circuit Stats:\n");
    printf("  Gates: %zu/%zu\n", circuit->num_gates, circuit->max_gates);
    printf("  Wires: %u\n", circuit->next_wire_id);
    printf("  Inputs: %zu\n", circuit->num_inputs);
    printf("  Outputs: %zu\n", circuit->num_outputs);
    
    #if DEBUG_GATES
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gate_t* gate = &circuit->gates[i];
        printf("  Gate %zu: %u %s %u -> %u\n", i,
               gate->left_input, 
               (gate->type == GATE_AND) ? "AND" : "XOR",
               gate->right_input, gate->output);
    }
    #endif
}
```

#### Validate Circuit Integrity
```c
bool validate_circuit(riscv_circuit_t* circuit) {
    // Check for invalid wire references
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gate_t* gate = &circuit->gates[i];
        
        if (gate->left_input >= circuit->next_wire_id) {
            printf("Error: Gate %zu references invalid left wire %u\n", 
                   i, gate->left_input);
            return false;
        }
        
        if (gate->right_input >= circuit->next_wire_id) {
            printf("Error: Gate %zu references invalid right wire %u\n", 
                   i, gate->right_input);
            return false;
        }
        
        if (gate->output >= circuit->next_wire_id) {
            printf("Error: Gate %zu produces invalid output wire %u\n", 
                   i, gate->output);
            return false;
        }
    }
    
    // Check for constant wire consistency
    if (circuit->num_inputs < 2) {
        printf("Error: Circuit missing constant wires\n");
        return false;
    }
    
    return true;
}
```

### 2. Instruction Debugging

#### Trace Instruction Compilation
```c
void trace_instruction_compilation(riscv_compiler_t* compiler, uint32_t instruction) {
    printf("Compiling instruction: 0x%08x\n", instruction);
    
    // Decode instruction
    uint32_t opcode = instruction & 0x7F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t funct7 = (instruction >> 25) & 0x7F;
    
    printf("  Opcode: 0x%02x, Funct3: 0x%x, Funct7: 0x%02x\n", 
           opcode, funct3, funct7);
    
    size_t gates_before = compiler->circuit->num_gates;
    
    // Compile instruction
    riscv_compile_instruction(compiler, instruction);
    
    size_t gates_after = compiler->circuit->num_gates;
    printf("  Generated %zu gates\n", gates_after - gates_before);
}
```

### 3. Memory Debugging

#### Check for Memory Leaks
```bash
# Use Valgrind for memory debugging
valgrind --leak-check=full --show-leak-kinds=all ./your_program

# Use AddressSanitizer during compilation
gcc -fsanitize=address -g your_program.c

# Monitor memory usage over time
while true; do
    ps aux | grep your_program | grep -v grep
    sleep 1
done
```

#### Memory Pool Debugging
```c
void debug_memory_usage(riscv_compiler_t* compiler) {
    printf("Memory Usage Report:\n");
    printf("  Circuit: %zu bytes\n", sizeof(riscv_circuit_t));
    printf("  Gates: %zu bytes (%zu gates)\n", 
           compiler->circuit->num_gates * sizeof(gate_t),
           compiler->circuit->num_gates);
    printf("  Register wires: %zu bytes\n", 
           32 * 32 * sizeof(uint32_t));
    printf("  Memory system: %zu bytes\n", 
           sizeof(riscv_memory_t));
    
    // Check for memory fragmentation
    size_t total_allocated = estimate_total_allocation(compiler);
    printf("  Estimated total: %zu bytes (%.2f MB)\n", 
           total_allocated, total_allocated / (1024.0 * 1024.0));
}
```

## ⚙️ Development Workflow Issues

### 1. Build System Problems

#### "CMake configuration failed"
```bash
# Clean and reconfigure
rm -rf build/
mkdir build && cd build
cmake ..

# Check CMake version
cmake --version  # Require >= 3.10

# Install missing dependencies
sudo apt-get install build-essential cmake

# Debug CMake issues
cmake .. --debug-output
```

#### "Make failed with errors"
```bash
# Clean build
make clean
make -j4

# Verbose build output
make VERBOSE=1

# Check specific file compilation
make src/riscv_compiler.c.o

# Fix common issues
# - Missing headers: check #include paths
# - Linking errors: verify library dependencies
# - Warning as errors: fix all warnings
```

### 2. Test Failures

#### "Tests failing unexpectedly"
```bash
# Run individual test suites
./tests/test_arithmetic
./tests/test_comprehensive

# Increase test verbosity
VERBOSE=1 ./run_all_tests.sh

# Debug specific test
gdb ./tests/test_arithmetic
(gdb) run
(gdb) bt  # When it fails
```

#### "Differential tests failing"
```bash
# Check emulator implementation
./tests/test_differential --verbose

# Compare against known-good results
diff expected_output.txt actual_output.txt

# Validate test programs
riscv32-unknown-elf-objdump -d tests/test_programs.elf
```

## 🔍 Advanced Debugging

### 1. Circuit Visualization

#### Generate Circuit Graphs
```c
void export_circuit_dot(riscv_circuit_t* circuit, const char* filename) {
    FILE* f = fopen(filename, "w");
    fprintf(f, "digraph circuit {\n");
    
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gate_t* gate = &circuit->gates[i];
        fprintf(f, "  gate_%zu [label=\"%s_%zu\"];\n", i, 
                (gate->type == GATE_AND) ? "AND" : "XOR", i);
        fprintf(f, "  wire_%u -> gate_%zu;\n", gate->left_input, i);
        fprintf(f, "  wire_%u -> gate_%zu;\n", gate->right_input, i);
        fprintf(f, "  gate_%zu -> wire_%u;\n", i, gate->output);
    }
    
    fprintf(f, "}\n");
    fclose(f);
}

// Usage:
export_circuit_dot(compiler->circuit, "debug_circuit.dot");
// Then: dot -Tpng debug_circuit.dot -o circuit.png
```

### 2. Performance Profiling

#### Profile with gprof
```bash
# Compile with profiling
gcc -pg -O2 your_program.c

# Run program
./your_program

# Generate profile
gprof ./your_program gmon.out > profile.txt

# Analyze hotspots
grep -A 5 -B 5 "riscv_compile" profile.txt
```

#### Profile with perf
```bash
# Record performance data
perf record -g ./your_program

# Analyze results
perf report

# Focus on specific functions
perf report --symbol=riscv_compile_instruction
```

## 📋 Diagnostic Checklist

### Before Reporting Issues

1. **Environment Check**:
   ```bash
   gcc --version
   cmake --version
   uname -a
   free -h
   ```

2. **Build Verification**:
   ```bash
   make clean && make
   ./run_all_tests.sh
   ./examples/simple_riscv_demo
   ```

3. **Minimal Reproduction**:
   ```bash
   # Create minimal failing case
   echo "Simple test program" > minimal_test.c
   # Reproduce with smallest possible input
   ```

4. **Log Collection**:
   ```bash
   # Collect relevant logs
   make 2>&1 | tee build.log
   ./your_program --verbose 2>&1 | tee runtime.log
   dmesg | tail -50 > system.log
   ```

### Issue Report Template

```markdown
## Issue Description
Brief description of the problem.

## Environment
- OS: Linux/macOS/Windows
- GCC Version: X.X.X
- CMake Version: X.X.X
- RAM: XXX GB

## Steps to Reproduce
1. Compile with: `gcc ...`
2. Run with: `./program ...`
3. Error occurs at: ...

## Expected Behavior
What should happen.

## Actual Behavior
What actually happens.

## Logs
```bash
# Build log
...

# Runtime log
...
```

## Additional Context
Any other relevant information.
```

## 🎯 Quick Fixes

### Common Quick Solutions

1. **Build Issues**: `rm -rf build && ./build.sh`
2. **Memory Issues**: Increase limits in `riscv_compiler.h`
3. **Test Failures**: `./run_all_tests.sh --clean`
4. **Performance**: Enable optimizations in compiler flags
5. **ELF Loading**: Check compilation target (`rv32i`)

### Emergency Debugging Commands

```bash
# Quick circuit validation
./tests/test_comprehensive --validate-only

# Memory usage check
ps aux | grep riscv_compiler

# Quick build test
echo "int main(){return 0;}" | gcc -x c - -o test && ./test

# Verify installation
./examples/simple_riscv_demo && echo "Installation OK"
```

## 📞 Getting Help

### Resources
- **Documentation**: Check all `*.md` files in project root
- **Examples**: Review `examples/` directory
- **Tests**: Study `tests/` for usage patterns
- **Code Comments**: Search for `TODO` and `FIXME` in source

### Community Support
- **Issues**: Report bugs with full diagnostic info
- **Discussions**: Ask questions with minimal reproduction
- **Contributions**: Follow coding standards and test coverage

Remember: **Measure first, optimize second**. Always verify that fixes actually solve the problem without breaking existing functionality.