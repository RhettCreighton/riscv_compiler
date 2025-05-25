#!/bin/bash

# Run all RISC-V compiler tests
echo "Running RISC-V Compiler Test Suite"
echo "=================================="
echo

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Track test results
total_passed=0
total_failed=0
failed_tests=""

# Function to run a test and collect results
run_test() {
    local test_name=$1
    local test_binary=$2
    
    echo -n "Running $test_name... "
    
    if ./$test_binary > /tmp/test_output_$$.txt 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        ((total_passed++))
    else
        echo -e "${RED}FAILED${NC}"
        ((total_failed++))
        failed_tests="$failed_tests\n  - $test_name"
        echo "  Output saved to: /tmp/test_output_$$.txt"
    fi
}

# Build tests first
echo "Building tests..."
cd build
make -j$(nproc) test_arithmetic test_differential test_jumps test_multiply test_upper_immediate test_system test_divide > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build complete."
echo

# Run individual instruction tests
echo "=== Individual Instruction Tests ==="
run_test "Jump Instructions" "test_jumps"
run_test "Multiplication Instructions" "test_multiply"
run_test "Division Instructions" "test_divide"
run_test "Upper Immediate Instructions" "test_upper_immediate"
run_test "System Instructions" "test_system"

echo
echo "=== Comprehensive Test Suites ==="
run_test "Arithmetic Unit Tests" "test_arithmetic"
run_test "Differential Tests" "test_differential"

# Summary
echo
echo "==================================="
echo "Test Summary:"
echo "  Total Passed: $total_passed"
echo "  Total Failed: $total_failed"

if [ $total_failed -eq 0 ]; then
    echo -e "  Result: ${GREEN}ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "  Result: ${RED}SOME TESTS FAILED${NC}"
    echo -e "\nFailed tests:$failed_tests"
    exit 1
fi