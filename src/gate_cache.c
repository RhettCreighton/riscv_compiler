#include "riscv_compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Gate pattern caching and deduplication system
// This significantly reduces gate count by reusing common subcircuits

#define CACHE_SIZE 65536  // Must be power of 2
#define MAX_PATTERN_SIZE 32

// Hash function for gate patterns
typedef struct {
    uint32_t inputs[MAX_PATTERN_SIZE];
    gate_type_t types[MAX_PATTERN_SIZE];
    size_t size;
    uint64_t hash;
} gate_pattern_t;

typedef struct cache_entry {
    gate_pattern_t pattern;
    uint32_t* output_wires;
    size_t num_outputs;
    struct cache_entry* next;  // For collision handling
} cache_entry_t;

typedef struct {
    cache_entry_t** buckets;
    size_t hits;
    size_t misses;
    size_t total_gates_saved;
} gate_cache_t;

// Global cache instance
static gate_cache_t* g_cache = NULL;

// FNV-1a hash function
static uint64_t hash_pattern(const gate_pattern_t* pattern) {
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
    
    // Hash the pattern size
    hash ^= pattern->size;
    hash *= 0x100000001b3ULL;  // FNV prime
    
    // Hash inputs and types
    for (size_t i = 0; i < pattern->size; i++) {
        hash ^= pattern->inputs[i];
        hash *= 0x100000001b3ULL;
        hash ^= pattern->types[i];
        hash *= 0x100000001b3ULL;
    }
    
    return hash;
}

// Initialize the gate cache
gate_cache_t* gate_cache_create(void) {
    gate_cache_t* cache = calloc(1, sizeof(gate_cache_t));
    if (!cache) return NULL;
    
    cache->buckets = calloc(CACHE_SIZE, sizeof(cache_entry_t*));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }
    
    return cache;
}

// Destroy the gate cache
void gate_cache_destroy(gate_cache_t* cache) {
    if (!cache) return;
    
    // Free all entries
    for (size_t i = 0; i < CACHE_SIZE; i++) {
        cache_entry_t* entry = cache->buckets[i];
        while (entry) {
            cache_entry_t* next = entry->next;
            free(entry->output_wires);
            free(entry);
            entry = next;
        }
    }
    
    free(cache->buckets);
    free(cache);
}

// Look up a pattern in the cache
cache_entry_t* gate_cache_lookup(gate_cache_t* cache, const gate_pattern_t* pattern) {
    uint64_t hash = hash_pattern(pattern);
    size_t bucket = hash & (CACHE_SIZE - 1);
    
    cache_entry_t* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->pattern.hash == hash &&
            entry->pattern.size == pattern->size &&
            memcmp(entry->pattern.inputs, pattern->inputs, 
                   pattern->size * sizeof(uint32_t)) == 0 &&
            memcmp(entry->pattern.types, pattern->types,
                   pattern->size * sizeof(gate_type_t)) == 0) {
            cache->hits++;
            return entry;
        }
        entry = entry->next;
    }
    
    cache->misses++;
    return NULL;
}

// Insert a pattern into the cache
void gate_cache_insert(gate_cache_t* cache, const gate_pattern_t* pattern,
                      uint32_t* output_wires, size_t num_outputs) {
    cache_entry_t* entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) return;
    
    // Copy pattern
    entry->pattern = *pattern;
    entry->pattern.hash = hash_pattern(pattern);
    
    // Copy output wires
    entry->output_wires = malloc(num_outputs * sizeof(uint32_t));
    if (!entry->output_wires) {
        free(entry);
        return;
    }
    memcpy(entry->output_wires, output_wires, num_outputs * sizeof(uint32_t));
    entry->num_outputs = num_outputs;
    
    // Insert into hash table
    size_t bucket = entry->pattern.hash & (CACHE_SIZE - 1);
    entry->next = cache->buckets[bucket];
    cache->buckets[bucket] = entry;
}

// Common patterns for caching

// Cache a 32-bit adder pattern
uint32_t* build_cached_adder_32(riscv_circuit_t* circuit, uint32_t* a, uint32_t* b) {
    if (!g_cache) {
        g_cache = gate_cache_create();
    }
    
    // Create pattern for 32-bit adder
    gate_pattern_t pattern = {0};
    pattern.size = 64;  // 32 bits of a + 32 bits of b
    
    // Add input wires to pattern
    for (int i = 0; i < 32; i++) {
        pattern.inputs[i] = a[i];
        pattern.inputs[32 + i] = b[i];
    }
    
    // Check cache
    cache_entry_t* cached = gate_cache_lookup(g_cache, &pattern);
    if (cached) {
        g_cache->total_gates_saved += 200;  // Approximate gates in 32-bit adder
        return cached->output_wires;
    }
    
    // Build the adder
    uint32_t* sum = malloc(33 * sizeof(uint32_t));  // 32 bits + carry
    sum[32] = build_sparse_kogge_stone_adder(circuit, a, b, sum, 32);
    
    // Cache the result
    gate_cache_insert(g_cache, &pattern, sum, 33);
    
    return sum;
}

// Cache an 8-bit XOR pattern (common in SHA3)
void build_cached_xor_8(riscv_circuit_t* circuit, uint32_t* a, uint32_t* b, uint32_t* result) {
    if (!g_cache) {
        g_cache = gate_cache_create();
    }
    
    // Create pattern
    gate_pattern_t pattern = {0};
    pattern.size = 16;
    for (int i = 0; i < 8; i++) {
        pattern.inputs[i] = a[i];
        pattern.inputs[8 + i] = b[i];
        pattern.types[i] = GATE_XOR;
    }
    
    // Check cache
    cache_entry_t* cached = gate_cache_lookup(g_cache, &pattern);
    if (cached) {
        memcpy(result, cached->output_wires, 8 * sizeof(uint32_t));
        g_cache->total_gates_saved += 8;
        return;
    }
    
    // Build XOR gates
    for (int i = 0; i < 8; i++) {
        result[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a[i], b[i], result[i], GATE_XOR);
    }
    
    // Cache the result
    gate_cache_insert(g_cache, &pattern, result, 8);
}

// Deduplicate identical gates in the circuit
void deduplicate_gates(riscv_circuit_t* circuit) {
    // Hash table for gate deduplication
    typedef struct {
        uint32_t left;
        uint32_t right;
        gate_type_t type;
        uint32_t output;
    } gate_key_t;
    
    #define DEDUP_SIZE 65536
    gate_key_t* dedup_table = calloc(DEDUP_SIZE, sizeof(gate_key_t));
    
    // Wire remapping table
    uint32_t* wire_remap = calloc(circuit->next_wire_id, sizeof(uint32_t));
    for (uint32_t i = 0; i < circuit->next_wire_id; i++) {
        wire_remap[i] = i;  // Identity mapping initially
    }
    
    // First pass: identify duplicate gates
    size_t new_gate_count = 0;
    gate_t* new_gates = malloc(circuit->num_gates * sizeof(gate_t));
    
    for (size_t i = 0; i < circuit->num_gates; i++) {
        gate_t* gate = &circuit->gates[i];
        
        // Apply remapping to inputs
        uint32_t left = wire_remap[gate->left_input];
        uint32_t right = wire_remap[gate->right_input];
        
        // Normalize gate (ensure left <= right for commutative gates)
        if (gate->type == GATE_AND || gate->type == GATE_XOR) {
            if (left > right) {
                uint32_t temp = left;
                left = right;
                right = temp;
            }
        }
        
        // Hash the gate
        uint64_t hash = ((uint64_t)left << 33) | ((uint64_t)right << 1) | gate->type;
        size_t bucket = hash & (DEDUP_SIZE - 1);
        
        // Check for duplicate
        bool found = false;
        for (size_t j = 0; j < 10; j++) {  // Linear probe
            size_t idx = (bucket + j) & (DEDUP_SIZE - 1);
            gate_key_t* entry = &dedup_table[idx];
            
            if (entry->output == 0) {
                // Empty slot, insert new gate
                entry->left = left;
                entry->right = right;
                entry->type = gate->type;
                entry->output = gate->output;
                break;
            } else if (entry->left == left && entry->right == right && 
                      entry->type == gate->type) {
                // Found duplicate!
                wire_remap[gate->output] = entry->output;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Keep this gate
            new_gates[new_gate_count] = *gate;
            new_gates[new_gate_count].left_input = left;
            new_gates[new_gate_count].right_input = right;
            new_gate_count++;
        }
    }
    
    // Replace gates array
    free(circuit->gates);
    circuit->gates = new_gates;
    circuit->num_gates = new_gate_count;
    
    // Update capacity
    circuit->capacity = circuit->num_gates * 1.5;  // Add some headroom
    circuit->gates = realloc(circuit->gates, circuit->capacity * sizeof(gate_t));
    
    free(dedup_table);
    free(wire_remap);
}

// Print cache statistics
void gate_cache_print_stats(void) {
    if (!g_cache) return;
    
    printf("Gate Cache Statistics:\n");
    printf("  Cache hits: %zu\n", g_cache->hits);
    printf("  Cache misses: %zu\n", g_cache->misses);
    printf("  Hit rate: %.1f%%\n", 
           100.0 * g_cache->hits / (g_cache->hits + g_cache->misses + 1));
    printf("  Total gates saved: %zu\n", g_cache->total_gates_saved);
}

// Template-based gate generation for common patterns

// Generate a bit-parallel operation (e.g., 32 parallel XORs)
void build_parallel_op(riscv_circuit_t* circuit, 
                      uint32_t* a, uint32_t* b, uint32_t* result,
                      size_t bits, gate_type_t type) {
    // Check if this exact pattern was already built
    static uint32_t last_a[32] = {0};
    static uint32_t last_b[32] = {0};
    static uint32_t last_result[32] = {0};
    static gate_type_t last_type = -1;
    static size_t last_bits = 0;
    
    bool can_reuse = (bits == last_bits && type == last_type);
    if (can_reuse) {
        for (size_t i = 0; i < bits; i++) {
            if (a[i] != last_a[i] || b[i] != last_b[i]) {
                can_reuse = false;
                break;
            }
        }
    }
    
    if (can_reuse) {
        // Reuse previous result
        memcpy(result, last_result, bits * sizeof(uint32_t));
        g_cache->total_gates_saved += bits;
        return;
    }
    
    // Build new gates
    for (size_t i = 0; i < bits; i++) {
        result[i] = riscv_circuit_allocate_wire(circuit);
        riscv_circuit_add_gate(circuit, a[i], b[i], result[i], type);
    }
    
    // Cache for next time
    if (bits <= 32) {
        memcpy(last_a, a, bits * sizeof(uint32_t));
        memcpy(last_b, b, bits * sizeof(uint32_t));
        memcpy(last_result, result, bits * sizeof(uint32_t));
        last_type = type;
        last_bits = bits;
    }
}