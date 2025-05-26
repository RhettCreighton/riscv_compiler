#!/bin/bash
# compile_to_circuit.sh - Compile C or RISC-V to gate circuits

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Usage
usage() {
    echo "Usage: $0 [options] <input_file>"
    echo ""
    echo "Compile C or RISC-V assembly to gate circuits for zero-knowledge proofs."
    echo ""
    echo "Options:"
    echo "  -o <file>      Output circuit file (default: <input>.circuit)"
    echo "  -O <level>     Optimization level (0-3, default: 2)"
    echo "  -m <mode>      Memory mode: ultra|simple|secure (default: simple)"
    echo "  --stats        Show circuit statistics"
    echo "  --profile      Generate gate count profile"
    echo "  --verify       Run formal verification on output"
    echo "  --visualize    Generate circuit visualization (requires graphviz)"
    echo "  -h, --help     Show this help message"
    echo ""
    echo "Examples:"
    echo "  # Compile C program to circuit"
    echo "  $0 program.c -o program.circuit --stats"
    echo ""
    echo "  # Compile RISC-V assembly"
    echo "  $0 program.s -o program.circuit"
    echo ""
    echo "  # Compile with ultra-fast memory and verify"
    echo "  $0 -m ultra --verify program.c"
    exit 1
}

# Parse arguments
INPUT_FILE=""
OUTPUT_FILE=""
OPT_LEVEL="2"
MEMORY_MODE="simple"
SHOW_STATS=false
PROFILE=false
VERIFY=false
VISUALIZE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -o)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -O)
            OPT_LEVEL="$2"
            shift 2
            ;;
        -m)
            MEMORY_MODE="$2"
            shift 2
            ;;
        --stats)
            SHOW_STATS=true
            shift
            ;;
        --profile)
            PROFILE=true
            shift
            ;;
        --verify)
            VERIFY=true
            shift
            ;;
        --visualize)
            VISUALIZE=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            ;;
        *)
            INPUT_FILE="$1"
            shift
            ;;
    esac
done

# Check input file
if [ -z "$INPUT_FILE" ]; then
    echo -e "${RED}Error: No input file specified${NC}"
    usage
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${RED}Error: Input file '$INPUT_FILE' not found${NC}"
    exit 1
fi

# Determine file type and output name
FILE_EXT="${INPUT_FILE##*.}"
BASE_NAME="${INPUT_FILE%.*}"

if [ -z "$OUTPUT_FILE" ]; then
    OUTPUT_FILE="${BASE_NAME}.circuit"
fi

# Build the compiler if needed
if [ ! -f "${BUILD_DIR}/riscv_zkvm_pipeline" ]; then
    echo -e "${YELLOW}Building RISC-V compiler...${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. > /dev/null 2>&1
    make riscv_zkvm_pipeline -j$(nproc) > /dev/null 2>&1
    cd - > /dev/null
fi

# Temporary files
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Step 1: Compile to RISC-V if needed
if [ "$FILE_EXT" = "c" ]; then
    echo -e "${GREEN}Step 1: Compiling C to RISC-V...${NC}"
    
    ELF_FILE="${TEMP_DIR}/program.elf"
    
    # Check for RISC-V toolchain
    if command -v riscv32-unknown-elf-gcc &> /dev/null; then
        RISCV_GCC="riscv32-unknown-elf-gcc"
    elif command -v riscv64-unknown-linux-gnu-gcc &> /dev/null; then
        RISCV_GCC="riscv64-unknown-linux-gnu-gcc"
        echo -e "${YELLOW}Warning: Using 64-bit toolchain, consider installing riscv32-unknown-elf-gcc${NC}"
    else
        echo -e "${RED}Error: RISC-V toolchain not found${NC}"
        echo "Install with: sudo apt install gcc-riscv64-linux-gnu"
        exit 1
    fi
    
    # Compile C to RISC-V
    CFLAGS="-O${OPT_LEVEL} -march=rv32im -mabi=ilp32"
    CFLAGS="$CFLAGS -nostdlib -nostartfiles -ffreestanding"
    CFLAGS="$CFLAGS -I${SCRIPT_DIR}/include"
    
    # Add memory mode define
    case $MEMORY_MODE in
        ultra)
            CFLAGS="$CFLAGS -DULTRA_FAST_MEMORY"
            ;;
        simple)
            CFLAGS="$CFLAGS -DSIMPLE_MEMORY"
            ;;
        secure)
            CFLAGS="$CFLAGS -DSECURE_MEMORY"
            ;;
    esac
    
    # Add default linker script if exists
    if [ -f "${SCRIPT_DIR}/examples/link.ld" ]; then
        LDFLAGS="-T ${SCRIPT_DIR}/examples/link.ld"
    else
        LDFLAGS=""
    fi
    
    $RISCV_GCC $CFLAGS $LDFLAGS "$INPUT_FILE" -o "$ELF_FILE" 2>"${TEMP_DIR}/gcc.log"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Compilation failed:${NC}"
        cat "${TEMP_DIR}/gcc.log"
        exit 1
    fi
    
    # Show code size
    SIZE_OUTPUT=$($RISCV_GCC-size "$ELF_FILE" 2>/dev/null || echo "Size info not available")
    echo "  Code size: $(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $1}') bytes"
    
elif [ "$FILE_EXT" = "s" ] || [ "$FILE_EXT" = "S" ]; then
    echo -e "${GREEN}Step 1: Assembling RISC-V...${NC}"
    
    ELF_FILE="${TEMP_DIR}/program.elf"
    
    # Assemble
    if command -v riscv32-unknown-elf-as &> /dev/null; then
        RISCV_AS="riscv32-unknown-elf-as"
        RISCV_LD="riscv32-unknown-elf-ld"
    else
        RISCV_AS="riscv64-unknown-linux-gnu-as"
        RISCV_LD="riscv64-unknown-linux-gnu-ld"
    fi
    
    $RISCV_AS -march=rv32im "$INPUT_FILE" -o "${TEMP_DIR}/program.o"
    $RISCV_LD "${TEMP_DIR}/program.o" -o "$ELF_FILE"
    
else
    # Assume it's already an ELF file
    ELF_FILE="$INPUT_FILE"
fi

# Step 2: Compile RISC-V to Circuit
echo -e "${GREEN}Step 2: Compiling RISC-V to circuit...${NC}"

PIPELINE_ARGS=""

# Add memory mode
case $MEMORY_MODE in
    ultra)
        PIPELINE_ARGS="$PIPELINE_ARGS --memory-mode ultra"
        echo "  Memory: Ultra-fast mode (8 words, 2.2K gates)"
        ;;
    simple)
        PIPELINE_ARGS="$PIPELINE_ARGS --memory-mode simple"  
        echo "  Memory: Simple mode (256 words, 101K gates)"
        ;;
    secure)
        PIPELINE_ARGS="$PIPELINE_ARGS --memory-mode secure"
        echo "  Memory: Secure mode (unlimited, 3.9M gates/access)"
        ;;
esac

# Run the pipeline
"${BUILD_DIR}/riscv_zkvm_pipeline" "$ELF_FILE" "$OUTPUT_FILE" $PIPELINE_ARGS 2>"${TEMP_DIR}/pipeline.log"

if [ $? -ne 0 ]; then
    echo -e "${RED}Circuit compilation failed:${NC}"
    cat "${TEMP_DIR}/pipeline.log"
    exit 1
fi

# Extract stats from log
TOTAL_GATES=$(grep "Total gates:" "${TEMP_DIR}/pipeline.log" | awk '{print $3}')
TOTAL_INSTR=$(grep "instructions compiled" "${TEMP_DIR}/pipeline.log" | awk '{print $1}')

if [ -n "$TOTAL_GATES" ] && [ -n "$TOTAL_INSTR" ]; then
    echo -e "${GREEN}Success!${NC} Compiled $TOTAL_INSTR instructions to $TOTAL_GATES gates"
    GATES_PER_INSTR=$((TOTAL_GATES / TOTAL_INSTR))
    echo "  Average: $GATES_PER_INSTR gates/instruction"
fi

# Step 3: Show statistics if requested
if [ "$SHOW_STATS" = true ]; then
    echo -e "\n${GREEN}Circuit Statistics:${NC}"
    
    # Parse circuit file for stats
    if [ -f "$OUTPUT_FILE" ]; then
        AND_GATES=$(grep -c "AND" "$OUTPUT_FILE" 2>/dev/null || echo "0")
        XOR_GATES=$(grep -c "XOR" "$OUTPUT_FILE" 2>/dev/null || echo "0")
        
        echo "  AND gates: $AND_GATES"
        echo "  XOR gates: $XOR_GATES"
        echo "  Total gates: $((AND_GATES + XOR_GATES))"
        
        # Estimate proving time (rough)
        PROVING_TIME_MS=$((TOTAL_GATES / 1000))
        echo "  Estimated proving time: ${PROVING_TIME_MS}ms"
    fi
fi

# Step 4: Generate profile if requested
if [ "$PROFILE" = true ]; then
    echo -e "\n${GREEN}Generating gate count profile...${NC}"
    
    # This would run a profiling version of the compiler
    echo "  (Profile generation not yet implemented)"
    # TODO: Implement profiling
fi

# Step 5: Run verification if requested
if [ "$VERIFY" = true ]; then
    echo -e "\n${GREEN}Running formal verification...${NC}"
    
    if [ -f "${BUILD_DIR}/test_instruction_verification" ]; then
        # This would verify the circuit
        echo "  (Circuit verification not yet integrated)"
        # TODO: Integrate verification
    else
        echo -e "${YELLOW}  Verification tools not built${NC}"
    fi
fi

# Step 6: Visualize if requested
if [ "$VISUALIZE" = true ]; then
    echo -e "\n${GREEN}Generating circuit visualization...${NC}"
    
    if command -v dot &> /dev/null; then
        # Generate DOT file
        DOT_FILE="${BASE_NAME}.dot"
        SVG_FILE="${BASE_NAME}.svg"
        
        # Simple visualization (first 100 gates)
        echo "digraph Circuit {" > "$DOT_FILE"
        echo "  rankdir=LR;" >> "$DOT_FILE"
        echo "  node [shape=box];" >> "$DOT_FILE"
        
        # Add nodes for gates (limit to first 100)
        head -100 "$OUTPUT_FILE" | grep -E "AND|XOR" | while read line; do
            GATE_ID=$(echo "$line" | awk '{print $1}')
            GATE_TYPE=$(echo "$line" | awk '{print $5}')
            LEFT=$(echo "$line" | awk '{print $2}')
            RIGHT=$(echo "$line" | awk '{print $3}')
            OUTPUT=$(echo "$line" | awk '{print $4}')
            
            COLOR="lightblue"
            if [ "$GATE_TYPE" = "AND" ]; then
                COLOR="lightgreen"
            fi
            
            echo "  g${GATE_ID} [label=\"${GATE_TYPE}\\n${GATE_ID}\", fillcolor=$COLOR, style=filled];" >> "$DOT_FILE"
        done
        
        echo "}" >> "$DOT_FILE"
        
        dot -Tsvg "$DOT_FILE" -o "$SVG_FILE"
        echo "  Visualization saved to: $SVG_FILE"
        rm "$DOT_FILE"
    else
        echo -e "${YELLOW}  Graphviz not installed (apt install graphviz)${NC}"
    fi
fi

echo -e "\n${GREEN}Circuit saved to: $OUTPUT_FILE${NC}"

# Tips based on results
if [ -n "$TOTAL_GATES" ]; then
    if [ "$TOTAL_GATES" -gt 10000000 ]; then
        echo -e "${YELLOW}Tip: Circuit is large (>10M gates). Consider optimization.${NC}"
    elif [ "$TOTAL_GATES" -lt 100000 ]; then
        echo -e "${GREEN}Excellent! Circuit is very efficient (<100K gates).${NC}"
    fi
fi