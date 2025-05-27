# SPDX-FileCopyrightText: 2025 Rhett Creighton
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.10)
project(riscv_compiler C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3")

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/riscv_compiler.c
    src/arithmetic_gates.c
    src/booth_radix4.c
    src/riscv_branches.c
    src/riscv_shifts.c
    src/riscv_jumps.c
    src/riscv_upper_immediate.c
    src/riscv_multiply.c
    src/riscv_divide.c
    src/riscv_system.c
    src/riscv_loadstore.c
    src/riscv_memory.c
    src/riscv_memory_simple.c
    src/riscv_memory_ultra_simple.c
    src/gate_deduplication.c
    src/optimized_shifts.c
    src/optimized_branches.c
    src/riscv_elf_loader.c
    src/circuit_format_converter.c
    src/riscv_zkvm_pipeline.c
    src/sha3_circuit.c
    src/kogge_stone_adder.c
    src/booth_multiplier_optimized.c
    src/gate_cache.c
    src/parallel_compiler.c
    src/instruction_fusion.c
    src/riscv_compiler_optimized.c
    src/memory_constraints.c
)

# Create library
add_library(riscv_compiler STATIC ${SOURCES})

# Option to build examples
option(RISCV_COMPILER_BUILD_EXAMPLES "Build example programs" ON)
option(RISCV_COMPILER_BUILD_TESTS "Build test programs" ON)

# Examples
if(RISCV_COMPILER_BUILD_EXAMPLES)
    add_executable(simple_riscv_demo examples/simple_riscv.c)
    target_link_libraries(simple_riscv_demo riscv_compiler)
    
    add_executable(fibonacci_riscv_demo examples/fibonacci_riscv.c)
    target_link_libraries(fibonacci_riscv_demo riscv_compiler)
    
    add_executable(full_riscv_demo examples/full_riscv_demo.c)
    target_link_libraries(full_riscv_demo riscv_compiler)
    
    add_executable(fibonacci_zkvm_demo examples/fibonacci_zkvm_demo.c)
    target_link_libraries(fibonacci_zkvm_demo riscv_compiler)
    
    add_executable(optimized_arithmetic_demo examples/optimized_arithmetic_demo.c)
    target_link_libraries(optimized_arithmetic_demo riscv_compiler)
    
    add_executable(memory_aware_example examples/memory_aware_example.c)
    target_link_libraries(memory_aware_example riscv_compiler)
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/memory_demo.c")
        add_executable(memory_demo examples/memory_demo.c)
        target_link_libraries(memory_demo riscv_compiler)
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/memory_comparison.c")
        add_executable(memory_comparison examples/memory_comparison.c)
        target_link_libraries(memory_comparison riscv_compiler)
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/memory_ultra_comparison.c")
        add_executable(memory_ultra_comparison examples/memory_ultra_comparison.c)
        target_link_libraries(memory_ultra_comparison riscv_compiler)
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/tutorial_complete.c")
        add_executable(tutorial_complete examples/tutorial_complete.c)
        target_link_libraries(tutorial_complete riscv_compiler)
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/getting_started.c")
        add_executable(getting_started examples/getting_started.c)
        target_link_libraries(getting_started riscv_compiler)
    endif()
endif()

# Tests
if(RISCV_COMPILER_BUILD_TESTS)
    # Include test directory
    include_directories(tests)
    
    # Create test executables for individual instruction tests
    add_executable(test_jumps src/riscv_jumps.c)
    target_link_libraries(test_jumps riscv_compiler)
    target_compile_definitions(test_jumps PRIVATE TEST_MAIN)
    
    add_executable(test_multiply src/riscv_multiply.c)
    target_link_libraries(test_multiply riscv_compiler)
    target_compile_definitions(test_multiply PRIVATE TEST_MAIN)
    
    add_executable(test_upper_immediate src/riscv_upper_immediate.c)
    target_link_libraries(test_upper_immediate riscv_compiler)
    target_compile_definitions(test_upper_immediate PRIVATE TEST_MAIN)
    
    add_executable(test_system src/riscv_system.c)
    target_link_libraries(test_system riscv_compiler)
    target_compile_definitions(test_system PRIVATE TEST_MAIN)
    
    add_executable(test_divide src/riscv_divide.c)
    target_link_libraries(test_divide riscv_compiler)
    target_compile_definitions(test_divide PRIVATE TEST_MAIN)
    
    # Comprehensive test suites
    add_executable(test_arithmetic tests/test_arithmetic.c)
    target_link_libraries(test_arithmetic riscv_compiler)
    
    add_executable(test_differential tests/test_differential.c tests/riscv_emulator.c)
    target_link_libraries(test_differential riscv_compiler)
    
    add_executable(test_differential_programs tests/test_differential_programs.c tests/riscv_emulator.c tests/test_programs.c)
    target_link_libraries(test_differential_programs riscv_compiler)
    
    # SHA3 security tests
    add_executable(test_sha3 tests/test_sha3.c)
    target_link_libraries(test_sha3 riscv_compiler)
    
    # Comprehensive instruction coverage tests
    add_executable(test_comprehensive tests/test_comprehensive.c)
    target_link_libraries(test_comprehensive riscv_compiler)
    
    # Edge case and robustness tests
    add_executable(test_edge_cases tests/test_edge_cases.c)
    target_link_libraries(test_edge_cases riscv_compiler)
    
    # Benchmarks
    add_executable(benchmark_simple tests/benchmark_simple.c)
    target_link_libraries(benchmark_simple riscv_compiler)
    
    add_executable(benchmark_instructions tests/benchmark_instructions.c)
    target_link_libraries(benchmark_instructions riscv_compiler)
    
    add_executable(benchmark_comprehensive tests/benchmark_comprehensive.c)
    target_link_libraries(benchmark_comprehensive riscv_compiler)
    
    add_executable(benchmark_optimizations tests/benchmark_optimizations.c)
    target_link_libraries(benchmark_optimizations riscv_compiler)
    
    # Booth multiplier test
    add_executable(test_booth src/booth_multiplier.c)
    target_link_libraries(test_booth riscv_compiler)
    target_compile_definitions(test_booth PRIVATE TEST_BOOTH)
    
    # Formal verification tests
    add_executable(test_reference_impl 
        tests/test_reference_impl.c
        src/reference_implementations.c
        src/reference_branches.c
        src/reference_memory.c
    )
    target_link_libraries(test_reference_impl riscv_compiler)
    
    # MiniSAT integration
    add_library(minisat STATIC
        src/minisat/solver.c
    )
    target_include_directories(minisat PRIVATE src/minisat)
    
    
    
    # MiniSAT basic integration test
    add_executable(test_minisat_integration
        src/minisat_integration_test.c
    )
    target_link_libraries(test_minisat_integration minisat m)
    
    
    # Simple verification API test
    add_executable(test_verification_api
        src/test_verification_api.c
    )
    target_link_libraries(test_verification_api riscv_compiler)
    
    # SAT-based ADD equivalence test
    add_executable(test_add_equivalence
        src/test_add_equivalence.c
    )
    target_link_libraries(test_add_equivalence riscv_compiler minisat m)
    
    
    # Systematic instruction verification
    add_executable(test_instruction_verification
        src/test_instruction_verification.c
    )
    target_link_libraries(test_instruction_verification riscv_compiler minisat m)
    
    # zkVM examples (C to Circuit)
    add_executable(zkvm_sha256_example
        examples/zkvm_sha256.c
    )
    target_link_libraries(zkvm_sha256_example riscv_compiler)
    
    add_executable(efficient_sum_example
        examples/efficient_sum.c
    )
    target_link_libraries(efficient_sum_example riscv_compiler)
    
    # SHA3 reference implementation test
    add_executable(test_sha3_reference
        src/sha3_reference.c
    )
    target_compile_definitions(test_sha3_reference PRIVATE TEST_SHA3_REFERENCE)
    
    # SHA3 RISC-V implementation test
    add_executable(test_sha3_riscv
        examples/sha3_riscv.c
    )
    target_compile_definitions(test_sha3_riscv PRIVATE TEST_SHA3_RISCV)
    
    
    # Simple SHA3 test
    add_executable(test_sha3_simple
        src/sha3_simple_test.c
        src/sha3_reference.c
        tests/riscv_emulator.c
    )
    target_link_libraries(test_sha3_simple riscv_compiler)
    
    # Bitcoin block verification test
    add_executable(test_bitcoin_verify
        examples/test_bitcoin_verify.c
    )
    target_link_libraries(test_bitcoin_verify riscv_compiler)
    
    # Compilation paths comparison
    add_executable(compare_compilation_paths
        examples/compare_compilation_paths.c
    )
    target_link_libraries(compare_compilation_paths riscv_compiler)
    
    # Dual path demonstration with measurements
    add_executable(dual_path_demonstration
        examples/dual_path_demonstration.c
    )
    target_link_libraries(dual_path_demonstration riscv_compiler)
    
    # Dual path equivalence proof
    add_executable(dual_path_equivalence_proof
        examples/dual_path_equivalence_proof.c
    )
    target_link_libraries(dual_path_equivalence_proof riscv_compiler minisat m)
    
    # Complete equivalence prover
    add_executable(complete_equivalence_prover
        examples/complete_equivalence_prover.c
    )
    target_link_libraries(complete_equivalence_prover riscv_compiler minisat m)
    
    # Proof of code binding
    add_executable(proof_of_code_binding
        examples/proof_of_code_binding.c
    )
    target_link_libraries(proof_of_code_binding riscv_compiler)
endif()

# Install rules
install(TARGETS riscv_compiler DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)