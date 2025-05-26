# SAT-Based Circuit Verification Guide

## Overview

We have three options for SAT-based verification of our RISC-V circuits:

### 1. Built-in Simple SAT Solver
- **Location**: `src/simple_sat_solver.c`
- **Pros**: No dependencies, tailored for circuits, educational
- **Cons**: Limited to ~1000 variables, basic DPLL algorithm
- **Use for**: Small circuits (<100 gates), unit tests, learning

### 2. MiniSAT Integration
- **Website**: http://minisat.se/
- **Pros**: Highly optimized, handles millions of variables
- **Cons**: External dependency, C++ (need wrapper)
- **Use for**: Production verification, large circuits

### 3. CryptoMiniSat
- **Website**: https://github.com/msoos/cryptominisat
- **Pros**: Native XOR support (perfect for our XOR gates!)
- **Cons**: Larger dependency, more complex
- **Use for**: Cryptographic circuits, when XOR performance matters

## Quick Start with Built-in Solver

```c
// 1. Convert circuit to SAT
sat_problem_t* sat = circuit_to_sat(circuit);

// 2. Add constraints for equivalence checking
// (inputs must be equal, outputs must differ)

// 3. Solve
if (sat_solve(sat)) {
    printf("Circuits differ!\n");
} else {
    printf("Circuits are equivalent!\n");
}
```

## Installing MiniSAT

```bash
# Option 1: From package manager
sudo apt-get install minisat2  # Debian/Ubuntu
brew install minisat           # macOS

# Option 2: Build from source
git clone https://github.com/niklasso/minisat.git
cd minisat
make
sudo make install
```

## MiniSAT Integration Example

```c
// Write circuit to DIMACS CNF format
void write_dimacs(circuit_t* circuit, const char* filename) {
    FILE* f = fopen(filename, "w");
    
    // Header: p cnf num_variables num_clauses
    fprintf(f, "p cnf %zu %zu\n", 
            circuit->num_wires, 
            circuit->num_gates * 3);  // Approx 3 clauses per gate
    
    // Write clauses for each gate
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gate_t* g = &circuit->gates[i];
        
        if (g->type == GATE_AND) {
            // AND gate: c = a ∧ b
            fprintf(f, "-%d -%d %d 0\n", g->left, g->right, g->output);
            fprintf(f, "%d -%d 0\n", g->left, g->output);
            fprintf(f, "%d -%d 0\n", g->right, g->output);
        } else if (g->type == GATE_XOR) {
            // XOR gate: c = a ⊕ b  
            fprintf(f, "-%d -%d -%d 0\n", g->left, g->right, g->output);
            fprintf(f, "%d %d -%d 0\n", g->left, g->right, g->output);
            fprintf(f, "%d -%d %d 0\n", g->left, g->right, g->output);
            fprintf(f, "-%d %d %d 0\n", g->left, g->right, g->output);
        }
    }
    
    fclose(f);
}

// Call MiniSAT and parse result
bool check_with_minisat(const char* cnf_file) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "minisat %s result.txt", cnf_file);
    
    int ret = system(cmd);
    if (ret != 0) return false;
    
    // Parse result
    FILE* f = fopen("result.txt", "r");
    char line[100];
    fgets(line, sizeof(line), f);
    fclose(f);
    
    return strncmp(line, "SAT", 3) == 0;
}
```

## CryptoMiniSat for XOR-heavy Circuits

```bash
# Install
git clone https://github.com/msoos/cryptominisat.git
cd cryptominisat
mkdir build && cd build
cmake ..
make
sudo make install
```

CryptoMiniSat has native XOR clause support:
```
c XOR clause example: a ⊕ b ⊕ c = true
x 1 2 3 0
```

## Performance Comparison

| Method | Circuit Size | Time | Memory |
|--------|-------------|------|--------|
| Built-in SAT | 100 gates | 10ms | 1MB |
| Built-in SAT | 1000 gates | 30s | 10MB |
| MiniSAT | 100 gates | 1ms | 1MB |
| MiniSAT | 10K gates | 100ms | 50MB |
| MiniSAT | 1M gates | 30s | 2GB |
| CryptoMiniSat | 10K XOR gates | 50ms | 30MB |

## Verification Strategy

### Small Circuits (<100 gates)
1. Use exhaustive testing if <16 inputs
2. Use built-in SAT for quick checks
3. Random testing for confidence

### Medium Circuits (100-10K gates)
1. MiniSAT for complete verification
2. Random testing for quick checks
3. Focus verification on critical paths

### Large Circuits (>10K gates)
1. Compositional verification (verify components)
2. MiniSAT on abstracted versions
3. Extensive random testing
4. Focus on specific properties

## Common Pitfalls

1. **Wire Numbering**: Ensure consistent wire numbering between reference and circuit
2. **Input Constraints**: Must constrain inputs to be equal when checking equivalence
3. **Output Constraints**: Must assert at least one output differs to find counterexamples
4. **Memory**: SAT solvers can use lots of memory for large circuits
5. **Timeout**: Set reasonable timeouts for large problems

## Advanced Techniques

### Incremental SAT Solving
```c
// Check multiple similar circuits efficiently
sat_solver* solver = sat_solver_new();

// Add base circuit clauses
add_circuit_clauses(solver, base_circuit);

// For each variant
for (int i = 0; i < num_variants; i++) {
    sat_solver_push(solver);  // Save state
    
    add_variant_clauses(solver, variants[i]);
    bool result = sat_solver_solve(solver);
    
    sat_solver_pop(solver);   // Restore state
}
```

### Bounded Model Checking
```c
// Verify circuit behavior over multiple cycles
for (int cycle = 0; cycle < max_cycles; cycle++) {
    // Add clauses for this cycle
    add_cycle_constraints(sat, circuit, cycle);
    
    // Check property
    if (!property_holds(sat, cycle)) {
        printf("Property violated at cycle %d\n", cycle);
        break;
    }
}
```

## Conclusion

- Start with the built-in solver for learning and small circuits
- Move to MiniSAT for production use
- Consider CryptoMiniSat if you have many XOR gates
- Remember: SAT solving is NP-complete, so be realistic about circuit sizes