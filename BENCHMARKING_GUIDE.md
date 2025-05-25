# RISC-V Compiler Benchmarking Guide

## Overview

This guide provides comprehensive documentation for benchmarking the RISC-V to gate circuit compiler. Proper benchmarking is essential for tracking optimization progress, identifying bottlenecks, and validating performance improvements.

## 📊 Benchmarking Tools

### 1. Comprehensive Benchmark Suite (`benchmark_comprehensive`)

**Purpose**: Complete performance analysis of all instruction categories.

**Usage**:
```bash
cd build
./benchmark_comprehensive
```

**Output Format**:
```
┌─────────────────────────┬─────────┬──────────┬─────────┬─────────┐
│ Instruction             │  Gates  │ Expected │  Time   │ Status  │
├─────────────────────────┼─────────┼──────────┼─────────┼─────────┤
│ ADD x3, x1, x2          │     224 │ 200-250  │   0.0ms │ PASS    │
│ XOR x3, x1, x2          │      32 │  32-32   │   0.0ms │ PASS    │
│ MUL x3, x1, x2          │   11632 │ 15000-25000 │   0.0ms │ FAIL    │
└─────────────────────────┴─────────┴──────────┴─────────┴─────────┘
```

**Key Metrics**:
- **Gate Count**: Number of AND/XOR gates generated
- **Compilation Time**: Time to generate circuit (milliseconds)
- **Pass/Fail Status**: Whether performance meets expectations
- **Overall Statistics**: Speed, efficiency, mission progress

### 2. Instruction-Specific Benchmarks (`benchmark_instructions`)

**Purpose**: Detailed analysis of individual instruction performance.

**Usage**:
```bash
cd build
./benchmark_instructions
```

**Focus Areas**:
- Gate count per instruction type
- Compilation speed variations
- Resource usage patterns

### 3. SHA3 Security Benchmarks (`test_sha3`)

**Purpose**: Validate cryptographic security implementation.

**Usage**:
```bash
cd build
./test_sha3
```

**Key Metrics**:
- SHA3-256 gate count (~194K gates)
- Hash generation time
- Security level validation

## 🎯 Performance Metrics

### Primary Metrics

#### 1. Gate Efficiency
**Definition**: Average number of gates per compiled instruction.

**Current**: 1,009 gates/instruction  
**Target**: <100 gates/instruction  
**Formula**: `total_gates / total_instructions`

**Interpretation**:
- **<100**: ✅ Excellent (mission target achieved)
- **100-200**: ✅ Good (acceptable performance)  
- **200-500**: ⚠️ Fair (needs optimization)
- **>500**: ❌ Poor (significant optimization needed)

#### 2. Compilation Speed
**Definition**: Number of instructions compiled per second.

**Current**: 260K instructions/second  
**Target**: >1M instructions/second  
**Formula**: `instructions / (compilation_time_seconds)`

**Interpretation**:
- **>1M**: ✅ Excellent (mission target achieved)
- **500K-1M**: ✅ Good (near target)
- **100K-500K**: ⚠️ Fair (needs optimization)
- **<100K**: ❌ Poor (significant optimization needed)

#### 3. Gate Generation Rate
**Definition**: Total gates generated per second.

**Current**: ~262M gates/second  
**Formula**: `total_gates / compilation_time_seconds`

**Interpretation**: Higher is better, but depends on gate complexity.

### Secondary Metrics

#### 4. Test Pass Rate
**Definition**: Percentage of benchmark tests that pass.

**Current**: 68.8% (comprehensive), 88.2% (edge cases)  
**Target**: >95%

#### 5. Instruction Set Coverage
**Definition**: Percentage of RV32I instructions implemented.

**Current**: 95.9% (47/49 instructions)  
**Target**: 100%

#### 6. Memory Efficiency
**Definition**: Peak memory usage during compilation.

**Monitoring**: Track `max_wire_id`, circuit capacity usage

## 📈 Benchmarking Methodology

### Benchmark Categories

#### 1. Single Instruction Benchmarks
Test individual instructions in isolation.

```c
// Example: Benchmark ADD instruction
riscv_compiler_t* compiler = riscv_compiler_create();
size_t gates_before = compiler->circuit->num_gates;
clock_t start = clock();

riscv_compile_instruction(compiler, 0x002081B3);  // ADD x3, x1, x2

clock_t end = clock();
size_t gates_used = compiler->circuit->num_gates - gates_before;
double time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
```

**Metrics Collected**:
- Gate count per instruction
- Compilation time per instruction  
- Wire allocation efficiency

#### 2. Program-Level Benchmarks
Test complete RISC-V programs.

```c
// Example: Fibonacci program benchmark
uint32_t fibonacci[] = {
    0x00100093,  // ADDI x1, x0, 1
    0x00100113,  // ADDI x2, x0, 1  
    0x002081B3,  // ADD x3, x1, x2
    // ... more instructions
};

riscv_circuit_t* circuit = riscv_compile_program(fibonacci, program_size);
```

**Metrics Collected**:
- Total gate count
- Average gates per instruction
- Compilation throughput
- Memory usage patterns

#### 3. Stress Testing
Test compiler limits and stability.

```c
// Example: Compile 10,000 instructions
for (int i = 0; i < 10000; i++) {
    riscv_compile_instruction(compiler, 0x002081B3);
}
```

**Metrics Collected**:
- Maximum sustainable throughput
- Memory growth patterns
- Stability under load

### Statistical Analysis

#### 1. Central Tendency
- **Mean**: Average performance across all tests
- **Median**: Middle value (less sensitive to outliers)
- **Mode**: Most common performance level

#### 2. Variability
- **Standard Deviation**: Performance consistency
- **Min/Max**: Performance range
- **Percentiles**: Distribution analysis

#### 3. Trend Analysis
- **Regression Analysis**: Performance trends over time
- **Before/After Comparisons**: Optimization impact
- **Correlation Analysis**: Relationship between metrics

## 🔧 Running Benchmarks

### Standard Benchmark Suite

```bash
# Run all benchmarks
cd riscv_compiler
./run_all_tests.sh

# Individual benchmark components
cd build
./test_comprehensive      # Instruction coverage
./test_edge_cases         # Robustness testing
./test_sha3              # Security validation
./benchmark_comprehensive # Performance analysis
```

### Custom Benchmark Scripts

#### Performance Tracking Script
```bash
#!/bin/bash
# benchmark_tracking.sh - Track performance over time

echo "RISC-V Compiler Performance Tracking"
echo "Date: $(date)"
echo "Commit: $(git rev-parse --short HEAD)"
echo "======================================"

cd build

# Run comprehensive benchmark and extract key metrics
./benchmark_comprehensive | tee benchmark_results.txt

# Extract metrics
GATES_PER_INST=$(grep "Average gates/instr" benchmark_results.txt | awk '{print $3}')
INST_PER_SEC=$(grep "Compilation speed" benchmark_results.txt | awk '{print $3}')
PASS_RATE=$(grep "Benchmark pass rate" benchmark_results.txt | awk '{print $4}')

# Log to performance history
echo "$(date),$(git rev-parse --short HEAD),$GATES_PER_INST,$INST_PER_SEC,$PASS_RATE" >> ../performance_history.csv

echo ""
echo "Performance Summary:"
echo "  Gates/instruction: $GATES_PER_INST"
echo "  Instructions/second: $INST_PER_SEC"
echo "  Test pass rate: $PASS_RATE"
```

#### Optimization Impact Analysis
```bash
#!/bin/bash
# optimization_analysis.sh - Compare before/after optimization

echo "Optimization Impact Analysis"
echo "============================"

# Baseline measurement
echo "Measuring baseline performance..."
cd build
./benchmark_comprehensive > baseline_results.txt

# Extract baseline metrics
BASELINE_GATES=$(grep "Average gates/instr" baseline_results.txt | awk '{print $3}')
BASELINE_SPEED=$(grep "Compilation speed" baseline_results.txt | awk '{print $3}')

echo "Baseline: $BASELINE_GATES gates/instr, $BASELINE_SPEED instr/sec"

# Apply optimization (example: enable Booth multiplier)
echo "Applying optimization..."
# (Code changes would go here)

# Measure optimized performance
echo "Measuring optimized performance..."
make -j4  # Rebuild with optimizations
./benchmark_comprehensive > optimized_results.txt

# Extract optimized metrics
OPTIMIZED_GATES=$(grep "Average gates/instr" optimized_results.txt | awk '{print $3}')
OPTIMIZED_SPEED=$(grep "Compilation speed" optimized_results.txt | awk '{print $3}')

echo "Optimized: $OPTIMIZED_GATES gates/instr, $OPTIMIZED_SPEED instr/sec"

# Calculate improvements
GATE_IMPROVEMENT=$(echo "scale=1; 100 * (1 - $OPTIMIZED_GATES / $BASELINE_GATES)" | bc)
SPEED_IMPROVEMENT=$(echo "scale=1; 100 * ($OPTIMIZED_SPEED / $BASELINE_SPEED - 1)" | bc)

echo ""
echo "Optimization Results:"
echo "  Gate reduction: $GATE_IMPROVEMENT%"
echo "  Speed improvement: $SPEED_IMPROVEMENT%"
```

### Continuous Integration Benchmarking

#### GitHub Actions Workflow Example
```yaml
# .github/workflows/performance.yml
name: Performance Benchmarks

on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Build compiler
      run: |
        mkdir build && cd build
        cmake ..
        make -j4
    
    - name: Run performance benchmarks
      run: |
        cd build
        ./benchmark_comprehensive > benchmark_results.txt
        
    - name: Extract metrics
      run: |
        GATES=$(grep "Average gates/instr" build/benchmark_results.txt | awk '{print $3}')
        SPEED=$(grep "Compilation speed" build/benchmark_results.txt | awk '{print $3}')
        PASS_RATE=$(grep "pass rate" build/benchmark_results.txt | awk '{print $4}')
        
        echo "GATES_PER_INSTRUCTION=$GATES" >> $GITHUB_ENV
        echo "INSTRUCTIONS_PER_SECOND=$SPEED" >> $GITHUB_ENV
        echo "PASS_RATE=$PASS_RATE" >> $GITHUB_ENV
    
    - name: Performance regression check
      run: |
        if (( $(echo "$GATES_PER_INSTRUCTION > 1200" | bc -l) )); then
          echo "❌ Performance regression: $GATES_PER_INSTRUCTION gates/instr (limit: 1200)"
          exit 1
        fi
        
        if (( $(echo "$INSTRUCTIONS_PER_SECOND < 200000" | bc -l) )); then
          echo "❌ Performance regression: $INSTRUCTIONS_PER_SECOND instr/sec (minimum: 200K)"
          exit 1
        fi
        
        echo "✅ Performance within acceptable bounds"
```

## 📊 Interpreting Results

### Performance Analysis Framework

#### 1. Gate Count Analysis

**Categorization by Instruction Type**:
```
Optimal (≤50 gates):     XOR, AND, LUI
Good (51-300 gates):     ADD, SUB, OR, ADDI, branches  
Fair (301-1000 gates):   Shifts, jumps
Poor (>1000 gates):     MUL, complex memory operations
```

**Root Cause Analysis**:
- **High arithmetic gates**: Need Kogge-Stone adder
- **High multiplication gates**: Need Booth's algorithm  
- **High shift gates**: Need logarithmic shifter
- **High memory gates**: Expected (cryptographic security)

#### 2. Speed Analysis

**Bottleneck Identification**:
```c
// Profiling different instruction types
struct instruction_profile {
    const char* category;
    double avg_time_ms;
    size_t avg_gates;
    double gates_per_ms;
} profiles[] = {
    {"Logic", 0.001, 32, 32000},      // Very fast
    {"Arithmetic", 0.004, 224, 56000}, // Fast  
    {"Shifts", 0.005, 960, 192000},    // Moderate
    {"Multiply", 0.008, 11632, 1454000} // Slow
};
```

**Optimization Priorities**:
1. **High Impact**: Optimize instructions with highest (gates × frequency)
2. **Low Hanging Fruit**: Enable existing optimizations (Booth multiplier)
3. **Systematic**: Improve fundamental operations (adders, shifters)

#### 3. Trend Analysis

**Performance Tracking Over Time**:
```python
# Example Python analysis script
import pandas as pd
import matplotlib.pyplot as plt

# Load performance history
df = pd.read_csv('performance_history.csv', 
                 names=['date', 'commit', 'gates_per_inst', 'inst_per_sec', 'pass_rate'])

# Plot trends
plt.figure(figsize=(12, 8))

plt.subplot(2, 2, 1)
plt.plot(df['date'], df['gates_per_inst'])
plt.title('Gate Efficiency Over Time')
plt.ylabel('Gates per Instruction')

plt.subplot(2, 2, 2)
plt.plot(df['date'], df['inst_per_sec'])
plt.title('Compilation Speed Over Time')
plt.ylabel('Instructions per Second')

plt.subplot(2, 2, 3)
plt.plot(df['date'], df['pass_rate'])
plt.title('Test Pass Rate Over Time')
plt.ylabel('Pass Rate (%)')

plt.tight_layout()
plt.savefig('performance_trends.png')
```

### Red Flags and Alerts

#### Critical Performance Regressions
- **Gate count increase >20%**: Investigate immediately
- **Speed decrease >50%**: Check for algorithmic changes
- **Pass rate drop >10%**: Review recent changes
- **Memory usage spike**: Check for resource leaks

#### Warning Signs
- **Inconsistent performance**: May indicate race conditions
- **Gradual degradation**: Technical debt accumulation
- **Platform-specific issues**: Environment dependencies

## 🎯 Optimization Workflow

### 1. Baseline Measurement
```bash
# Establish current performance baseline
./benchmark_comprehensive > baseline.txt
git add baseline.txt
git commit -m "Performance baseline before optimization"
```

### 2. Identify Bottlenecks
```bash
# Analyze results to find optimization targets
grep "FAIL\|gates:" baseline.txt
# Focus on highest gate count instructions
```

### 3. Apply Optimizations
```c
// Example: Enable Booth multiplier
#define USE_BOOTH_MULTIPLIER 1

// Implement optimization
void optimize_multiplication() {
    // Enable optimized Booth's algorithm
    build_booth_multiplier(circuit, a_bits, b_bits, product_bits, 32);
}
```

### 4. Measure Impact
```bash
# Rebuild and re-benchmark
make clean && make -j4
./benchmark_comprehensive > optimized.txt

# Compare results
diff baseline.txt optimized.txt
```

### 5. Validate and Document
```bash
# Ensure all tests still pass
../run_all_tests.sh

# Document optimization
git add -A
git commit -m "Optimize multiplication: Enable Booth's algorithm

- Reduces MUL gate count from 11,632 to ~3,000 gates (74% reduction)
- Improves overall efficiency from 1,009 to ~400 gates/instruction
- All tests continue to pass"
```

## 📋 Best Practices

### 1. Consistent Environment
- Use same hardware/OS for comparative benchmarks
- Control for system load and background processes
- Use multiple runs and statistical averaging

### 2. Meaningful Baselines
- Establish baseline before any optimization work
- Tag baseline commits for easy reference
- Document baseline conditions and environment

### 3. Incremental Testing
- Test each optimization individually
- Measure cumulative impact of multiple optimizations
- Be able to revert specific changes

### 4. Automated Monitoring
- Set up CI/CD performance checks
- Alert on significant regressions
- Track trends over time

### 5. Comprehensive Coverage
- Test all instruction categories
- Include edge cases and stress tests
- Validate both performance and correctness

## 🔍 Advanced Analysis

### Statistical Significance Testing
```python
# Example: Compare two performance measurements
from scipy import stats

baseline_times = [0.004, 0.0041, 0.0039, ...]  # Multiple measurements
optimized_times = [0.002, 0.0021, 0.0019, ...]

# Perform t-test
t_stat, p_value = stats.ttest_ind(baseline_times, optimized_times)

if p_value < 0.05:
    print("Performance improvement is statistically significant")
else:
    print("Performance difference may be due to measurement noise")
```

### Performance Modeling
```python
# Model performance scaling
import numpy as np
from sklearn.linear_model import LinearRegression

# Program size vs compilation time
program_sizes = np.array([10, 50, 100, 500, 1000])  # Instructions
compile_times = np.array([0.1, 0.5, 1.0, 5.2, 10.8])  # Seconds

# Fit linear model
model = LinearRegression()
model.fit(program_sizes.reshape(-1, 1), compile_times)

# Predict scaling
predicted_time_10k = model.predict([[10000]])[0]
print(f"Predicted time for 10K instructions: {predicted_time_10k:.1f} seconds")
```

### Memory Profiling
```c
// Track memory usage during compilation
void profile_memory_usage() {
    riscv_compiler_t* compiler = riscv_compiler_create();
    
    for (int i = 0; i < 1000; i++) {
        size_t memory_before = get_memory_usage();  // System-specific
        
        riscv_compile_instruction(compiler, 0x002081B3);
        
        size_t memory_after = get_memory_usage();
        printf("Instruction %d: Memory delta = %zu bytes\n", 
               i, memory_after - memory_before);
    }
    
    riscv_compiler_destroy(compiler);
}
```

For more advanced analysis techniques and custom benchmarking scenarios, see the `tests/` directory and performance analysis scripts in the repository.