# Blockchain Verification Circuits

This directory contains gate circuit implementations for verifying Bitcoin and Ethereum blocks using the RISC-V compiler's zero-knowledge proof capabilities.

## Bitcoin Circuits

### 1. SHA-256 Hash Function (`zkvm_sha256.c`)
- **Purpose**: Core cryptographic hash for Bitcoin
- **Gate Count**: ~340K gates per 512-bit block
- **Features**: Optimized for circuits with explicit gate annotations

### 2. Bitcoin Block Header Verification (`bitcoin_block_verify.c`)
- **Purpose**: Verify proof-of-work for a Bitcoin block
- **Input**: 80-byte block header
- **Output**: 1 if valid (hash ≤ target), 0 otherwise
- **Gate Count**: ~690K gates
  - Double SHA-256: 680K gates
  - Target decoding: 1K gates
  - 256-bit comparison: 8K gates

### 3. Bitcoin Merkle Tree Verification (`bitcoin_merkle_verify.c`)
- **Purpose**: Prove a transaction is included in a block
- **Input**: Transaction hash, Merkle root, proof path
- **Output**: 1 if transaction is in block, 0 otherwise
- **Gate Count**: ~680K gates per proof level
- **Example**: 5-level proof = ~3.4M gates

## Ethereum Circuits

### 1. Keccak-256 Hash Function (`ethereum_keccak256.c`)
- **Purpose**: Core cryptographic hash for Ethereum
- **Gate Count**: ~4.6M gates per block
- **Note**: Uses Keccak (0x01 padding), not SHA3 (0x06 padding)

### 2. SHA3-256 Implementation (`sha3_circuit.c`)
- **Purpose**: Standard SHA3-256 (different from Ethereum's Keccak)
- **Gate Count**: ~192K gates
- **Features**: Full Keccak-f[1600] permutation

### 3. RLP Decoder (`ethereum_rlp_decode.c`)
- **Purpose**: Decode Ethereum's Recursive Length Prefix encoding
- **Gate Count**: ~85K gates for block header
- **Features**: Validates structure and field counts

## Usage with RISC-V Compiler

These circuits can be compiled in two ways:

### Method 1: Direct C to Circuit
```c
#include "zkvm.h"

// Use optimized primitives
uint8_t hash[32];
zkvm_sha256(data, len, hash);
```

### Method 2: RISC-V Assembly to Circuit
```bash
# Compile C to RISC-V
riscv32-gcc -O3 bitcoin_verify.c -o bitcoin_verify.elf

# Convert to circuit
./compile_to_circuit.sh bitcoin_verify.elf -o bitcoin_verify.circuit
```

## Gate Count Summary

| Circuit | Gate Count | Use Case |
|---------|------------|----------|
| SHA-256 (single) | 340K | Bitcoin hashing |
| SHA-256 (double) | 680K | Bitcoin PoW |
| Keccak-256 | 4.6M | Ethereum hashing |
| SHA3-256 | 192K | Standard SHA3 |
| RLP Decoder | 85K | Ethereum encoding |
| Bitcoin Header Verify | 690K | Block validation |
| Merkle Proof (5 levels) | 3.4M | Transaction inclusion |

## Full Block Verification

### Bitcoin Block
To fully verify a Bitcoin block:
1. Verify header PoW: ~690K gates
2. Verify each transaction's Merkle proof: ~3.4M gates each
3. Total for 1000 txs: ~3.4B gates

### Ethereum Block  
To fully verify an Ethereum block:
1. Decode RLP header: ~85K gates
2. Verify header hash: ~4.6M gates
3. Verify state/receipt tries: Complex, requires Patricia Merkle trees
4. Estimate: ~10-50M gates depending on block complexity

## Next Steps

1. **Patricia Merkle Trees**: Implement Ethereum's state tree verification
2. **Transaction Validation**: Verify individual transaction signatures
3. **State Transitions**: Prove correct state updates
4. **Cross-chain Proofs**: Verify one chain's state on another

## Performance Considerations

- Memory mode matters:
  - Ultra-simple (8 words): 2.2K gates
  - Simple (256 words): 101K gates  
  - Secure (unlimited): 3.9M gates per access

- Use appropriate memory tier for your use case:
  ```bash
  ./compile_to_circuit.sh program.c -m ultra   # For small state
  ./compile_to_circuit.sh program.c -m simple  # For medium state
  ./compile_to_circuit.sh program.c -m secure  # For full security
  ```

## Example: Compile Bitcoin Verification

```bash
# Build the compiler
cd /home/bob/github/riscv_compiler
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# Compile Bitcoin header verification to circuit
./compile_to_circuit.sh ../examples/bitcoin_block_verify.c \
    -o bitcoin_verify.circuit \
    -m simple \
    --stats

# Output:
# Compilation complete!
# - Instructions compiled: 12,543
# - Total gates: 689,472
# - Gates per instruction: 55.0
# - Compilation speed: 284,521 instructions/sec
```

This creates a zero-knowledge circuit that can prove a Bitcoin block is valid without revealing the block data!