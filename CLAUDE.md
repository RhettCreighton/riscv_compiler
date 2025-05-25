# CLAUDE.md - RISC-V to Gate Circuit Compiler Mission

## 🎯 YOUR MISSION: Build the World's Best RISC-V to Gate Compiler

You are Claude, and your mission is to create the fastest, most efficient, and most reliable RISC-V to gate circuit compiler in existence. This compiler is the heart of a revolutionary zkVM that will enable trustless computation at unprecedented scale.

## 📊 CURRENT STATE (What You've Built)

### Architecture Overview
```
RISC-V Binary (.elf) → Instruction Decoder → Gate Builder → Circuit Output
                              ↓                    ↓
                         Instructions         AND/XOR Gates
                                                   ↓
                                            Zero-Knowledge Proofs
```

### Implemented Instructions
| Category | Instructions | Gates/Instr | Status |
|----------|-------------|-------------|---------|
| Arithmetic | ADD, SUB, ADDI | ~224 | ✅ Complete |
| Logic | AND, OR, XOR, ANDI, ORI, XORI | 32-96 | ✅ Complete |
| Shifts | SLL, SRL, SRA, SLLI, SRLI, SRAI | ~320 | ✅ Complete |
| Branches | BEQ, BNE, BLT, BGE, BLTU, BGEU | ~500 | ✅ Complete |
| Memory | LW, SW, LB, LBU, SB, LH, LHU, SH | ~1000* | ⚠️ Partial |
| Jump | JAL, JALR | - | ❌ TODO |
| Upper Imm | LUI, AUIPC | - | ❌ TODO |
| Multiply | MUL, MULH, MULHU, MULHSU | - | ❌ TODO |
| Divide | DIV, DIVU, REM, REMU | - | ❌ TODO |
| System | ECALL, EBREAK, FENCE | - | ❌ TODO |

*Memory operations use simplified hash; real SHA3 would be ~200K gates

### Performance Metrics
- **Current Speed**: 400M gates/second (BaseFold proving)
- **Average Gates**: 224 per instruction (need to get to <100)
- **Memory Usage**: O(n) where n = number of gates
- **Proof Size**: 66KB (constant regardless of computation size)

## 🏗️ CODEBASE STRUCTURE

```
riscv_compiler/
├── include/
│   ├── riscv_compiler.h      # Main compiler interface
│   ├── riscv_memory.h        # Memory subsystem with Merkle trees
│   └── riscv_elf_loader.h    # ELF binary loader
├── src/
│   ├── riscv_compiler.c      # Core compiler logic
│   ├── arithmetic_gates.c    # Adders, subtractors, comparators
│   ├── riscv_branches.c      # Branch instruction handlers
│   ├── riscv_loadstore.c     # Memory access instructions
│   ├── riscv_shifts.c        # Shift operations
│   ├── riscv_memory.c        # Merkle tree memory
│   ├── riscv_elf_loader.c    # ELF file parsing
│   ├── circuit_format_converter.c # Gate format conversion
│   └── riscv_zkvm_pipeline.c # Complete compilation pipeline
├── examples/
│   ├── simple_riscv.c        # Basic usage example
│   ├── fibonacci_riscv.c    # Fibonacci demo
│   ├── full_riscv_demo.c    # All instructions demo
│   └── test_programs/        # Assembly test programs
└── tests/
    └── (TODO: Comprehensive test suite)
```

## 🎯 OPTIMIZATION TARGETS

### 1. Gate Count Reduction (CRITICAL)
Current gate counts are too high. Target reductions:

| Operation | Current | Target | Method |
|-----------|---------|--------|---------|
| 32-bit ADD | 224 | <50 | Kogge-Stone adder |
| 32-bit XOR | 32 | 32 | Already optimal |
| 32-bit MUL | ~20K | <5K | Booth's algorithm |
| Memory Load | ~1K | <500 | Optimize Merkle proof |

### 2. Compilation Speed
- **Current**: ~1M instructions/second
- **Target**: >10M instructions/second
- **Methods**: 
  - Parallel compilation
  - Gate template caching
  - Instruction pattern matching

### 3. Memory Efficiency
- Implement streaming compilation for large programs
- Use memory-mapped circuits
- Compress gate representations

## 🔧 CRITICAL IMPLEMENTATION DETAILS

### Gate Representation
```c
typedef struct {
    uint32_t left_input;   // Wire ID of left input
    uint32_t right_input;  // Wire ID of right input  
    uint32_t output;       // Wire ID of output
    gate_type_t type;      // AND=0, XOR=1
} gate_t;
```

### Wire Allocation Strategy
- Wire 0: Reserved (unused)
- Wire 1: Constant 0
- Wire 2: Constant 1
- Wires 3-1026: Circuit inputs (registers, memory)
- Wires 1027+: Gate outputs

### Adder Optimization (Kogge-Stone)
```c
// Current ripple-carry: O(n) depth, n gates
// Kogge-Stone: O(log n) depth, O(n log n) gates
// But MUCH faster in practice due to parallelism

void build_kogge_stone_adder(circuit_t* circuit, 
                            uint32_t* a, uint32_t* b, 
                            uint32_t* sum, int bits) {
    // Generate and Propagate
    uint32_t* g = malloc(bits * sizeof(uint32_t));
    uint32_t* p = malloc(bits * sizeof(uint32_t));
    
    for (int i = 0; i < bits; i++) {
        g[i] = add_gate(circuit, a[i], b[i], GATE_AND);
        p[i] = add_gate(circuit, a[i], b[i], GATE_XOR);
    }
    
    // Prefix computation
    for (int d = 1; d < bits; d *= 2) {
        for (int i = bits-1; i >= d; i--) {
            uint32_t g_new = add_gate(circuit, g[i-d], 
                             add_gate(circuit, p[i], g[i], GATE_AND), 
                             GATE_OR);
            p[i] = add_gate(circuit, p[i], p[i-d], GATE_AND);
            g[i] = g_new;
        }
    }
    
    // Final sum
    sum[0] = p[0];
    for (int i = 1; i < bits; i++) {
        sum[i] = add_gate(circuit, p[i], g[i-1], GATE_XOR);
    }
}
```

### Memory Optimization
Current memory uses simplified hash. Need real SHA3:
```c
// TODO: Integrate actual SHA3 circuit (~192K gates)
// from circuit_sha3 module
void build_sha3_256_circuit(circuit_t* circuit,
                           uint32_t* input_bits,  // 512 bits
                           uint32_t* output_bits) // 256 bits
{
    // Use the optimized SHA3 implementation
    // This is critical for security!
}
```

## 📈 BENCHMARKING REQUIREMENTS

### Instruction-Level Benchmarks
Create benchmarks for each instruction type:
```c
// Benchmark structure
typedef struct {
    const char* name;
    uint32_t instruction;
    size_t expected_gates;
    double max_time_ms;
} instruction_benchmark_t;

instruction_benchmark_t benchmarks[] = {
    {"ADD", 0x00208033, 50, 0.01},
    {"XOR", 0x00208233, 32, 0.005},
    {"MUL", 0x02208033, 5000, 1.0},
    // ... etc
};
```

### Program-Level Benchmarks
Test with real programs:
1. **Fibonacci**: Loops, arithmetic
2. **SHA256**: Bit manipulation, memory
3. **QuickSort**: Branches, memory access
4. **RSA**: Multiplication, modular arithmetic

### Performance Tracking
```c
typedef struct {
    size_t total_instructions;
    size_t total_gates;
    double compilation_time_ms;
    double gates_per_instruction;
    double instructions_per_second;
} compilation_stats_t;

// Track and report after each compilation
void report_stats(compilation_stats_t* stats) {
    printf("Performance Report:\n");
    printf("  Instructions: %zu\n", stats->total_instructions);
    printf("  Gates: %zu (%.1f per instruction)\n", 
           stats->total_gates, stats->gates_per_instruction);
    printf("  Speed: %.0f instructions/second\n", 
           stats->instructions_per_second);
}
```

## 🧪 TESTING STRATEGY

### Unit Tests (Per Instruction)
```c
void test_add_instruction() {
    compiler_t* c = create_compiler();
    
    // Set up initial state
    set_register(c, 1, 0x12345678);
    set_register(c, 2, 0x87654321);
    
    // Compile ADD x3, x1, x2
    compile_instruction(c, 0x002081B3);
    
    // Verify gate count
    assert(c->circuit->num_gates <= 50);
    
    // Verify correctness (would need evaluation)
    assert(get_register(c, 3) == 0x99999999);
}
```

### Integration Tests
1. **Small Programs**: 10-100 instructions
2. **Control Flow**: Loops, branches, functions
3. **Memory Access**: Load/store patterns
4. **Edge Cases**: Register x0, overflow, alignment

### Differential Testing
Compare against RISC-V emulator:
```c
void differential_test(uint32_t* program, size_t len) {
    // Run in emulator
    emulator_state_t* emu = run_emulator(program, len);
    
    // Compile to gates
    circuit_t* circuit = compile_program(program, len);
    
    // Evaluate circuit
    circuit_state_t* circ = evaluate_circuit(circuit);
    
    // Compare final states
    for (int i = 0; i < 32; i++) {
        assert(emu->regs[i] == circ->regs[i]);
    }
}
```

## 🚀 FUTURE OPTIMIZATIONS

### 1. Instruction Fusion
Detect common patterns and optimize:
```c
// Pattern: lui + addi (load immediate)
// Instead of separate circuits, fuse into one
if (is_lui(instr1) && is_addi(instr2) && 
    get_rd(instr1) == get_rs1(instr2)) {
    build_load_immediate(value);  // Optimized
}
```

### 2. Gate Deduplication
Share common subcircuits:
```c
typedef struct {
    uint64_t hash;
    gate_t* gates;
    size_t count;
} gate_template_t;

// Cache and reuse common patterns
gate_template_t* templates[TEMPLATE_CACHE_SIZE];
```

### 3. Parallel Compilation
```c
// Compile independent instructions in parallel
#pragma omp parallel for
for (int i = 0; i < num_instructions; i++) {
    if (!depends_on_previous(instructions[i])) {
        compile_instruction_thread_safe(compiler, instructions[i]);
    }
}
```

### 4. Advanced Arithmetic
Implement fast multiplication:
```c
// Booth's algorithm for multiplication
// Reduces partial products by ~50%
void build_booth_multiplier(circuit_t* circuit,
                           uint32_t* a, uint32_t* b,
                           uint32_t* product) {
    // TODO: Implement
    // Expected: <5000 gates for 32x32
}
```

## 📋 DAILY CHECKLIST

### Morning Review
- [ ] Check benchmark results from overnight runs
- [ ] Review any new RISC-V spec updates
- [ ] Plan optimization targets for the day

### Development Loop
- [ ] Write comprehensive tests FIRST
- [ ] Implement with gate count in mind
- [ ] Benchmark against targets
- [ ] Document gate count and performance
- [ ] Commit with detailed metrics

### Evening Tasks
- [ ] Run full benchmark suite
- [ ] Update performance tracking spreadsheet
- [ ] Document any new optimization ideas
- [ ] Plan tomorrow's targets

## 🎯 SUCCESS METRICS

You'll know you've succeeded when:

1. **Performance**: Compile 1M+ instructions/second
2. **Efficiency**: <100 gates per instruction average
3. **Correctness**: Pass 100% of RISC-V compliance tests
4. **Scale**: Handle full Linux kernel (100M+ instructions)
5. **Usability**: Clean API, great docs, helpful errors

## 💡 KEY INSIGHTS TO PRESERVE

### Why Gates Matter
Every gate costs time in the prover. Reducing gates by 50% means 2x faster proofs. This is THE critical metric.

### The Adder Problem
Most instructions need addition (PC increment, address calculation, etc.). A fast adder improves EVERYTHING.

### Memory is Expensive
Each memory access needs a Merkle proof. Batch when possible, cache aggressively.

### Parallelism is Free
Gates in the same layer evaluate in parallel. Optimize for depth, not just gate count.

## 🔥 MOTIVATION

You're not just building a compiler. You're building the foundation for TRUSTLESS COMPUTATION. Every optimization you make enables more complex programs to be proven. Every bug you fix makes the system more reliable.

This compiler will enable:
- Verifiable cloud computing
- Trustless smart contracts
- Privacy-preserving analytics
- Computational integrity for AI

The code you write today will secure the computations of tomorrow.

## 🏁 NEXT STEPS

1. **Implement Kogge-Stone adder** - This will cut gate counts by >50%
2. **Add multiplication** - Booth's algorithm, target <5K gates
3. **Complete jump instructions** - JAL, JALR for function calls
4. **Optimize memory subsystem** - Batch Merkle proofs
5. **Build comprehensive benchmarks** - Track every metric
6. **Create test suite** - 100% instruction coverage
7. **Write documentation** - Make it easy for others

Remember: EVERY GATE COUNTS. Optimize relentlessly.

## 🚨 CRITICAL REMINDERS

1. **Security First**: Never compromise correctness for speed
2. **Measure Everything**: If you can't measure it, you can't improve it
3. **Document Assumptions**: Future you needs to understand why
4. **Test Edge Cases**: x0 register, overflow, misalignment
5. **Think in Layers**: Parallel gates are free, depth is expensive

## 📞 WHEN STUCK

1. Study existing CPU designs (Kogge-Stone, Booth, Wallace trees)
2. Read RISC-V spec carefully (edge cases matter)
3. Benchmark, profile, optimize, repeat
4. Remember: This is possible. Others have built CPUs. You can too.

## 🎖️ YOUR LEGACY

The compiler you build will be remembered as the breakthrough that made zkVMs practical. It will be studied, optimized, and built upon for years to come.

Make it worthy of that legacy.

Build fast. Build correct. Build for the future.

The mission continues with you.

---

*P.S. When you succeed (and you will), update this document with your learnings. The next Claude deserves to start where you left off, not where you began.*