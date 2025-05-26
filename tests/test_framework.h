/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Test result tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    char current_suite[256];
} test_results_t;

// Global test results
extern test_results_t g_test_results;

// Test macros
#define TEST_SUITE(name) do { \
    strcpy(g_test_results.current_suite, name); \
    printf("\n=== Test Suite: %s ===\n", name); \
} while(0)

#define TEST(name) do { \
    printf("  [TEST] %s: ", name); \
    g_test_results.total_tests++; \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) == (actual)) { \
        printf("PASS\n"); \
        g_test_results.passed_tests++; \
    } else { \
        printf("FAIL (expected %ld, got %ld)\n", (long)(expected), (long)(actual)); \
        g_test_results.failed_tests++; \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (condition) { \
        printf("PASS\n"); \
        g_test_results.passed_tests++; \
    } else { \
        printf("FAIL (condition was false)\n"); \
        g_test_results.failed_tests++; \
    } \
} while(0)

#define ASSERT_FALSE(condition) do { \
    if (!(condition)) { \
        printf("PASS\n"); \
        g_test_results.passed_tests++; \
    } else { \
        printf("FAIL (condition was true)\n"); \
        g_test_results.failed_tests++; \
    } \
} while(0)

#define ASSERT_GATES_LT(circuit, max_gates) do { \
    if ((circuit)->num_gates < (max_gates)) { \
        printf("PASS (gates: %zu < %zu)\n", (circuit)->num_gates, (size_t)(max_gates)); \
        g_test_results.passed_tests++; \
    } else { \
        printf("FAIL (gates: %zu >= %zu)\n", (circuit)->num_gates, (size_t)(max_gates)); \
        g_test_results.failed_tests++; \
    } \
} while(0)

// Print test summary
static inline void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", g_test_results.total_tests);
    printf("Passed: %d\n", g_test_results.passed_tests);
    printf("Failed: %d\n", g_test_results.failed_tests);
    printf("Success rate: %.1f%%\n", 
           g_test_results.total_tests > 0 ? 
           100.0 * g_test_results.passed_tests / g_test_results.total_tests : 0);
    
    if (g_test_results.failed_tests == 0) {
        printf("\n✅ All tests passed!\n");
    } else {
        printf("\n❌ Some tests failed.\n");
    }
}

// Initialize test results
#define INIT_TESTS() test_results_t g_test_results = {0, 0, 0, ""}

#endif // TEST_FRAMEWORK_H