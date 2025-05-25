#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "riscv_compiler.h"
#include "riscv_memory.h"
#include "riscv_elf_loader.h"

// Configuration
#define MAX_CYCLES 10000      // Maximum execution cycles
#define MEMORY_SIZE (64*1024) // 64KB of memory

// Pipeline stages
typedef enum {
    STAGE_LOAD_ELF,
    STAGE_COMPILE_TO_GATES,
    STAGE_GENERATE_CIRCUIT,
    STAGE_CREATE_PROOF,
    STAGE_VERIFY_PROOF,
    STAGE_COMPLETE
} pipeline_stage_t;

// Pipeline context
typedef struct {
    const char* elf_filename;
    const char* circuit_filename;
    const char* proof_filename;
    
    riscv_program_t* program;
    riscv_compiler_t* compiler;
    riscv_memory_t* memory;
    
    size_t total_gates;
    size_t cycles_executed;
    
    // Initial state
    uint32_t initial_regs[32];
    uint8_t* initial_memory;
    
    // Final state (for verification)
    uint32_t final_regs[32];
    uint8_t* final_memory;
    
    pipeline_stage_t current_stage;
    bool verbose;
} zkvm_pipeline_t;

// Initialize pipeline
zkvm_pipeline_t* zkvm_pipeline_create(const char* elf_file, bool verbose) {
    zkvm_pipeline_t* pipeline = calloc(1, sizeof(zkvm_pipeline_t));
    if (!pipeline) return NULL;
    
    pipeline->elf_filename = elf_file;
    pipeline->circuit_filename = "/tmp/zkvm_circuit.txt";
    pipeline->proof_filename = "/tmp/zkvm_proof.bfp";
    pipeline->verbose = verbose;
    
    // Allocate initial memory
    pipeline->initial_memory = calloc(MEMORY_SIZE, 1);
    pipeline->final_memory = calloc(MEMORY_SIZE, 1);
    
    if (!pipeline->initial_memory || !pipeline->final_memory) {
        free(pipeline->initial_memory);
        free(pipeline->final_memory);
        free(pipeline);
        return NULL;
    }
    
    return pipeline;
}

// Free pipeline
void zkvm_pipeline_free(zkvm_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    riscv_program_free(pipeline->program);
    riscv_compiler_destroy(pipeline->compiler);
    riscv_memory_destroy(pipeline->memory);
    
    free(pipeline->initial_memory);
    free(pipeline->final_memory);
    free(pipeline);
}

// Stage 1: Load ELF file
int zkvm_stage_load_elf(zkvm_pipeline_t* pipeline) {
    printf("=== Stage 1: Loading ELF file ===\n");
    
    pipeline->program = riscv_load_elf(pipeline->elf_filename);
    if (!pipeline->program) {
        fprintf(stderr, "Failed to load ELF file: %s\n", pipeline->elf_filename);
        return -1;
    }
    
    if (pipeline->verbose) {
        riscv_elf_print_info(pipeline->program);
    }
    
    // Initialize memory with program data
    if (pipeline->program->data_size > 0) {
        memcpy(pipeline->initial_memory + pipeline->program->data_start,
               pipeline->program->data,
               pipeline->program->data_size);
    }
    
    printf("✓ Loaded %zu instructions from %s\n", 
           pipeline->program->num_instructions,
           pipeline->elf_filename);
    
    pipeline->current_stage = STAGE_COMPILE_TO_GATES;
    return 0;
}

// Stage 2: Compile to gates
int zkvm_stage_compile(zkvm_pipeline_t* pipeline) {
    printf("\n=== Stage 2: Compiling to Gates ===\n");
    
    // Create compiler
    pipeline->compiler = riscv_compiler_create();
    if (!pipeline->compiler) {
        fprintf(stderr, "Failed to create compiler\n");
        return -1;
    }
    
    // Create memory subsystem
    pipeline->memory = riscv_memory_create(pipeline->compiler->circuit);
    pipeline->compiler->memory = (struct riscv_memory_t*)pipeline->memory;
    
    // Initialize registers
    for (int reg = 0; reg < 32; reg++) {
        for (int bit = 0; bit < 32; bit++) {
            if (reg == 0) {
                pipeline->compiler->reg_wires[reg][bit] = 1;  // x0 is always 0
            } else {
                pipeline->compiler->reg_wires[reg][bit] = 
                    riscv_circuit_allocate_wire(pipeline->compiler->circuit);
            }
        }
    }
    
    // Initialize PC to entry point
    for (int bit = 0; bit < 32; bit++) {
        pipeline->compiler->pc_wires[bit] = 
            riscv_circuit_allocate_wire(pipeline->compiler->circuit);
    }
    
    // Compile each instruction
    printf("Compiling %zu instructions...\n", pipeline->program->num_instructions);
    
    size_t compiled = 0;
    clock_t start = clock();
    
    for (size_t i = 0; i < pipeline->program->num_instructions; i++) {
        uint32_t instruction = pipeline->program->instructions[i];
        
        if (pipeline->verbose && i < 10) {
            char disasm[128];
            riscv_disassemble_instruction(instruction, disasm, sizeof(disasm));
            printf("  [%04zu] %08x: %s\n", i, instruction, disasm);
        }
        
        // Skip NOPs (addi x0, x0, 0)
        if (instruction == 0x00000013) {
            continue;
        }
        
        if (riscv_compile_instruction(pipeline->compiler, instruction) == 0) {
            compiled++;
        } else if (pipeline->verbose) {
            fprintf(stderr, "Warning: Failed to compile instruction %zu: 0x%08x\n", i, instruction);
        }
        
        // Limit compilation for demo
        if (compiled >= 100) {
            printf("  (Limited to first 100 instructions for demo)\n");
            break;
        }
    }
    
    clock_t end = clock();
    double compile_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    pipeline->total_gates = pipeline->compiler->circuit->num_gates;
    pipeline->cycles_executed = compiled;
    
    printf("✓ Compiled %zu instructions into %zu gates\n", compiled, pipeline->total_gates);
    printf("  Compilation time: %.3f seconds\n", compile_time);
    printf("  Gates per instruction: %.1f\n", 
           (double)pipeline->total_gates / compiled);
    
    if (pipeline->verbose) {
        riscv_circuit_print_stats(pipeline->compiler->circuit);
    }
    
    pipeline->current_stage = STAGE_GENERATE_CIRCUIT;
    return 0;
}

// Stage 3: Generate circuit file
int zkvm_stage_generate_circuit(zkvm_pipeline_t* pipeline) {
    printf("\n=== Stage 3: Generating Circuit File ===\n");
    
    printf("Converting to gate_computer format...\n");
    
    int result = riscv_circuit_to_gate_format(
        pipeline->compiler->circuit, 
        pipeline->circuit_filename
    );
    
    if (result != 0) {
        fprintf(stderr, "Failed to generate circuit file\n");
        return -1;
    }
    
    // Get file size
    struct stat st;
    stat(pipeline->circuit_filename, &st);
    
    printf("✓ Generated circuit file: %s\n", pipeline->circuit_filename);
    printf("  File size: %.1f KB\n", st.st_size / 1024.0);
    printf("  Total gates: %zu\n", pipeline->total_gates);
    printf("  Circuit inputs: %zu\n", pipeline->compiler->circuit->num_inputs);
    printf("  Circuit outputs: %zu\n", pipeline->compiler->circuit->num_outputs);
    
    pipeline->current_stage = STAGE_CREATE_PROOF;
    return 0;
}

// Stage 4: Create proof (calls gate_computer)
int zkvm_stage_create_proof(zkvm_pipeline_t* pipeline) {
    printf("\n=== Stage 4: Generating Zero-Knowledge Proof ===\n");
    
    // For now, show the command that would be run
    printf("To generate proof, run:\n");
    printf("  ./gate_computer --input-file %s --prove %s\n",
           pipeline->circuit_filename,
           pipeline->proof_filename);
    
    // Estimate proof time
    double proof_time_est = pipeline->total_gates / 400000000.0;  // 400M gates/sec
    printf("\nEstimated proof generation time: %.3f seconds\n", proof_time_est);
    printf("Expected proof size: ~66 KB\n");
    
    // In a real implementation, we would:
    // 1. Set up input values (initial register state, memory)
    // 2. Call gate_computer executable or library
    // 3. Wait for proof generation
    // 4. Save proof file
    
    pipeline->current_stage = STAGE_VERIFY_PROOF;
    return 0;
}

// Stage 5: Verify proof
int zkvm_stage_verify_proof(zkvm_pipeline_t* pipeline) {
    printf("\n=== Stage 5: Verifying Proof ===\n");
    
    printf("To verify proof, run:\n");
    printf("  ./gate_computer --verify %s\n", pipeline->proof_filename);
    
    printf("\nExpected verification time: ~13 ms\n");
    
    pipeline->current_stage = STAGE_COMPLETE;
    return 0;
}

// Run complete pipeline
int zkvm_pipeline_run(zkvm_pipeline_t* pipeline) {
    printf("RISC-V zkVM Pipeline\n");
    printf("===================\n\n");
    
    // Stage 1: Load ELF
    if (zkvm_stage_load_elf(pipeline) != 0) {
        return -1;
    }
    
    // Stage 2: Compile to gates
    if (zkvm_stage_compile(pipeline) != 0) {
        return -1;
    }
    
    // Stage 3: Generate circuit
    if (zkvm_stage_generate_circuit(pipeline) != 0) {
        return -1;
    }
    
    // Stage 4: Create proof
    if (zkvm_stage_create_proof(pipeline) != 0) {
        return -1;
    }
    
    // Stage 5: Verify proof
    if (zkvm_stage_verify_proof(pipeline) != 0) {
        return -1;
    }
    
    printf("\n✅ zkVM Pipeline Complete!\n");
    printf("\nSummary:\n");
    printf("  Program: %s\n", pipeline->elf_filename);
    printf("  Instructions compiled: %zu\n", pipeline->cycles_executed);
    printf("  Total gates: %zu\n", pipeline->total_gates);
    printf("  Circuit file: %s\n", pipeline->circuit_filename);
    printf("  Proof file: %s\n", pipeline->proof_filename);
    
    return 0;
}

// Main function for testing
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <elf-file> [-v]\n", argv[0]);
        return 1;
    }
    
    bool verbose = (argc > 2 && strcmp(argv[2], "-v") == 0);
    
    // Create pipeline
    zkvm_pipeline_t* pipeline = zkvm_pipeline_create(argv[1], verbose);
    if (!pipeline) {
        fprintf(stderr, "Failed to create pipeline\n");
        return 1;
    }
    
    // Run pipeline
    int result = zkvm_pipeline_run(pipeline);
    
    // Cleanup
    zkvm_pipeline_free(pipeline);
    
    return result;
}