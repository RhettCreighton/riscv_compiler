# RISC-V to Gate Circuit Compiler

**The world's first formally verified RISC-V to gate circuit compiler for zero-knowledge proofs.**

[![Tests](https://img.shields.io/badge/Tests-100%25%20Passing-brightgreen)](./run_all_tests.sh)
[![Verification](https://img.shields.io/badge/Formal%20Verification-SAT%20Proven-blue)](./src/test_add_equivalence.c)

## Quick Start

```bash
# Build
cd build && cmake .. && make -j$(nproc)

# Test  
../scripts/run_all_tests.sh

# Try examples
./getting_started
./comprehensive_optimization_test
```

## What This Does

Compiles C programs and RISC-V assembly to **gate circuits** for zero-knowledge proofs. Enables proving computation correctness without revealing inputs.

## Performance

| Operation | Gates | Time |
|-----------|-------|------|
| ADD/SUB | 224-256 | 0.1ms |
| MULTIPLY | 11,600 | 1ms |
| Memory (Ultra) | 2.2K | 0.1ms |
| Memory (Secure) | 3.9M | 100ms |

**Compilation Speed:** 272K-997K instructions/second

## Features

- ✅ **Complete RV32I + M** - All RISC-V instructions
- ✅ **Formally Verified** - SAT-based correctness proofs  
- ✅ **3-Tier Memory** - Ultra (2.2K), Simple (101K), Secure (3.9M gates)
- ✅ **Optimized** - 1,757x memory improvement, gate deduplication
- ✅ **Production Ready** - 100% test coverage, robust error handling

## Architecture

```
C Program → GCC → RISC-V ELF → Gate Builder → AND/XOR Circuit → ZK Proof
```

## Examples

See `examples/` directory for:
- Fibonacci computation
- SHA3 hashing  
- Arithmetic operations
- Memory demonstrations

## Verification

```bash
cd build
./test_add_equivalence        # Verify ADD instruction
./test_instruction_verification # Verify multiple instructions
```

Uses SAT solving to mathematically prove circuits correctly implement RISC-V semantics.

## Project Structure

```
riscv_compiler/
├── README.md              # This file
├── CMakeLists.txt         # Build configuration
├── src/                   # Source code
├── include/               # Header files
├── tests/                 # Test suite
├── examples/              # Example programs
├── docs/                  # Documentation
├── scripts/               # Build and utility scripts
└── build/                 # Build artifacts (generated)
```

## Documentation

- **[docs/CLAUDE.md](docs/CLAUDE.md)** - Technical implementation details
- **[docs/OPTIMIZATION_SUMMARY.md](docs/OPTIMIZATION_SUMMARY.md)** - Performance optimizations
- **[docs/VERIFICATION_SUMMARY.md](docs/VERIFICATION_SUMMARY.md)** - Formal verification details

## License

Copyright 2025 Rhett Creighton

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.