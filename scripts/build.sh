#!/bin/bash

# Build script for RISC-V zkVM Compiler
# This script sets up and builds the complete compiler with all optimizations

set -e  # Exit on any error

echo "🚀 Building RISC-V zkVM Compiler"
echo "================================"
echo

# Create build directory
if [ ! -d "build" ]; then
    echo "📁 Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DRISCV_COMPILER_BUILD_EXAMPLES=ON -DRISCV_COMPILER_BUILD_TESTS=ON

# Build everything in parallel
echo "🔨 Building all targets..."
make -j$(nproc)

echo
echo "✅ Build complete!"
echo
echo "📊 Build Summary:"
echo "=================="

# List all executables
echo "🧪 Test Programs:"
ls -la test_* 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'

echo
echo "🎯 Example Programs:"  
ls -la *demo* 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'
ls -la fibonacci_zkvm_demo 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'
ls -la optimized_arithmetic_demo 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'

echo
echo "🔧 Benchmark Programs:"
ls -la benchmark_* 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'

echo
echo "📚 Library:"
ls -la libriscv_compiler.* 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'

echo
echo "🎉 Ready to run tests!"
echo "   ./test_complete_rv32i     # Comprehensive instruction set test"
echo "   ./fibonacci_zkvm_demo     # Real-world benchmark" 
echo "   ./test_adder_improvements # Performance validation"
echo "   ../test.sh               # Run all tests"
echo

cd ..