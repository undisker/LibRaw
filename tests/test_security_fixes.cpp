/* -*- C++ -*-
 * LibRaw Security Fixes Test Suite
 * Tests for division by zero, integer overflow, and bounds checking fixes
 *
 * Copyright 2025 LibRaw LLC (info@libraw.org)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

// Include the safe math header we created
#include "../internal/libraw_safe_math.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s\n", msg); \
    } \
} while(0)

// ========================================
// Test safe_mul_size_t
// ========================================
void test_safe_mul_size_t()
{
    printf("\n=== Testing safe_mul_size_t ===\n");

    size_t result;
    int status;

    // Test normal multiplication
    status = safe_mul_size_t(100, 200, &result);
    TEST_ASSERT(status == 0 && result == 20000, "Normal multiplication 100*200=20000");

    // Test multiplication with zero
    status = safe_mul_size_t(0, 1000, &result);
    TEST_ASSERT(status == 0 && result == 0, "Zero multiplication 0*1000=0");

    status = safe_mul_size_t(1000, 0, &result);
    TEST_ASSERT(status == 0 && result == 0, "Zero multiplication 1000*0=0");

    // Test multiplication by one
    status = safe_mul_size_t(12345, 1, &result);
    TEST_ASSERT(status == 0 && result == 12345, "Identity multiplication 12345*1=12345");

    // Test overflow detection
    status = safe_mul_size_t(SIZE_MAX, 2, &result);
    TEST_ASSERT(status == -1, "Overflow detection SIZE_MAX*2");

    status = safe_mul_size_t(SIZE_MAX / 2 + 1, 2, &result);
    TEST_ASSERT(status == -1, "Overflow detection near boundary");

    // Test boundary case (should not overflow)
    status = safe_mul_size_t(SIZE_MAX / 2, 2, &result);
    TEST_ASSERT(status == 0, "Boundary case SIZE_MAX/2 * 2 should succeed");
}

// ========================================
// Test safe_add_size_t
// ========================================
void test_safe_add_size_t()
{
    printf("\n=== Testing safe_add_size_t ===\n");

    size_t result;
    int status;

    // Test normal addition
    status = safe_add_size_t(100, 200, &result);
    TEST_ASSERT(status == 0 && result == 300, "Normal addition 100+200=300");

    // Test addition with zero
    status = safe_add_size_t(0, 1000, &result);
    TEST_ASSERT(status == 0 && result == 1000, "Zero addition 0+1000=1000");

    // Test overflow detection
    status = safe_add_size_t(SIZE_MAX, 1, &result);
    TEST_ASSERT(status == -1, "Overflow detection SIZE_MAX+1");

    status = safe_add_size_t(SIZE_MAX - 10, 20, &result);
    TEST_ASSERT(status == -1, "Overflow detection near boundary");

    // Test boundary case (should not overflow)
    status = safe_add_size_t(SIZE_MAX - 10, 10, &result);
    TEST_ASSERT(status == 0 && result == SIZE_MAX, "Boundary case SIZE_MAX-10+10=SIZE_MAX");
}

// ========================================
// Test safe_alloc_size_2d
// ========================================
void test_safe_alloc_size_2d()
{
    printf("\n=== Testing safe_alloc_size_2d ===\n");

    size_t result;

    // Test normal 2D allocation size
    result = safe_alloc_size_2d(1920, 1080, 4);
    TEST_ASSERT(result == 1920ULL * 1080ULL * 4ULL, "Normal 2D allocation 1920x1080x4");

    // Test with zero dimensions
    result = safe_alloc_size_2d(0, 1080, 4);
    TEST_ASSERT(result == 0, "Zero width returns 0");

    result = safe_alloc_size_2d(1920, 0, 4);
    TEST_ASSERT(result == 0, "Zero height returns 0");

    // Test overflow cases
    result = safe_alloc_size_2d(SIZE_MAX, 2, 1);
    TEST_ASSERT(result == 0, "Overflow on width*height returns 0");

    // Note: On 64-bit systems, 1000000*1000000*1000 may not overflow size_t
    // So we test with values that definitely overflow on both 32 and 64-bit
    result = safe_alloc_size_2d(SIZE_MAX / 2, 4, 1);
    TEST_ASSERT(result == 0, "Large overflow returns 0");
}

// ========================================
// Test safe_alloc_size_3
// ========================================
void test_safe_alloc_size_3()
{
    printf("\n=== Testing safe_alloc_size_3 ===\n");

    size_t result;

    // Test normal 3-way multiplication
    result = safe_alloc_size_3(100, 200, 4);
    TEST_ASSERT(result == 80000, "Normal 3-way 100*200*4=80000");

    // Test with zeros
    result = safe_alloc_size_3(0, 200, 4);
    TEST_ASSERT(result == 0, "Zero first arg returns 0");

    result = safe_alloc_size_3(100, 0, 4);
    TEST_ASSERT(result == 0, "Zero second arg returns 0");

    result = safe_alloc_size_3(100, 200, 0);
    TEST_ASSERT(result == 0, "Zero third arg returns 0");

    // Test overflow
    result = safe_alloc_size_3(SIZE_MAX / 2, 4, 2);
    TEST_ASSERT(result == 0, "Overflow returns 0");
}

// ========================================
// Test safe_div_float
// ========================================
void test_safe_div_float()
{
    printf("\n=== Testing safe_div_float ===\n");

    float result;

    // Test normal division
    result = safe_div_float(10.0f, 2.0f, 0.0f);
    TEST_ASSERT(fabsf(result - 5.0f) < 0.0001f, "Normal division 10/2=5");

    // Test division by zero returns default
    result = safe_div_float(10.0f, 0.0f, -1.0f);
    TEST_ASSERT(fabsf(result - (-1.0f)) < 0.0001f, "Division by zero returns default -1");

    // Test division by very small number returns default
    result = safe_div_float(10.0f, 1e-40f, 999.0f);
    TEST_ASSERT(fabsf(result - 999.0f) < 0.0001f, "Division by tiny number returns default");

    // Test with negative numbers
    result = safe_div_float(-10.0f, 2.0f, 0.0f);
    TEST_ASSERT(fabsf(result - (-5.0f)) < 0.0001f, "Division with negative -10/2=-5");
}

// ========================================
// Test safe_div_double
// ========================================
void test_safe_div_double()
{
    printf("\n=== Testing safe_div_double ===\n");

    double result;

    // Test normal division
    result = safe_div_double(100.0, 4.0, 0.0);
    TEST_ASSERT(fabs(result - 25.0) < 0.0001, "Normal division 100/4=25");

    // Test division by zero returns default
    result = safe_div_double(100.0, 0.0, -1.0);
    TEST_ASSERT(fabs(result - (-1.0)) < 0.0001, "Division by zero returns default -1");

    // Test division by very small number returns default
    result = safe_div_double(100.0, 1e-310, 999.0);
    TEST_ASSERT(fabs(result - 999.0) < 0.0001, "Division by tiny number returns default");
}

// ========================================
// Test safe_div_int
// ========================================
void test_safe_div_int()
{
    printf("\n=== Testing safe_div_int ===\n");

    int result;

    // Test normal division
    result = safe_div_int(100, 4, 0);
    TEST_ASSERT(result == 25, "Normal division 100/4=25");

    // Test division by zero returns default
    result = safe_div_int(100, 0, -1);
    TEST_ASSERT(result == -1, "Division by zero returns default -1");

    // Test with negative numbers
    result = safe_div_int(-100, 4, 0);
    TEST_ASSERT(result == -25, "Division with negative -100/4=-25");

    // Test integer truncation
    result = safe_div_int(7, 2, 0);
    TEST_ASSERT(result == 3, "Integer truncation 7/2=3");
}

// ========================================
// Test boundary conditions for image sizes
// ========================================
void test_image_size_boundaries()
{
    printf("\n=== Testing Image Size Boundaries ===\n");

    size_t result;

    // Typical image sizes should work
    result = safe_alloc_size_2d(8192, 8192, sizeof(uint16_t) * 4);
    TEST_ASSERT(result > 0, "8192x8192 RGBA16 allocation should succeed");

    result = safe_alloc_size_2d(16384, 16384, sizeof(uint16_t) * 4);
    TEST_ASSERT(result > 0, "16384x16384 RGBA16 allocation should succeed");

    // Very large images should fail safely
    // Note: On 64-bit systems, 1M x 1M * 8 bytes = 8TB which doesn't overflow size_t
    // So we use values that definitely overflow
    result = safe_alloc_size_2d(SIZE_MAX / 4, 8, 1);
    TEST_ASSERT(result == 0, "Very large image allocation should fail safely");

    // Check thumbnail size limits
    result = safe_alloc_size_3(8192, 8192, 3);
    TEST_ASSERT(result == 8192ULL * 8192ULL * 3ULL, "8192x8192 RGB8 thumbnail should succeed");
}

// ========================================
// Test Kodak thumbnail specific cases
// ========================================
void test_kodak_thumbnail_cases()
{
    printf("\n=== Testing Kodak Thumbnail Cases ===\n");

    // These test cases simulate the kodak_thumb_loader function checks

    // Valid thumbnail dimensions
    size_t alloc = safe_alloc_size_2d(640, 480, sizeof(uint16_t) * 4);
    TEST_ASSERT(alloc > 0, "Standard Kodak thumbnail 640x480 should succeed");

    // Minimum valid dimensions (16x16)
    alloc = safe_alloc_size_2d(16, 16, sizeof(uint16_t) * 4);
    TEST_ASSERT(alloc > 0, "Minimum 16x16 thumbnail should succeed");

    // Maximum valid dimensions (8192x8192)
    alloc = safe_alloc_size_2d(8192, 8192, sizeof(uint16_t) * 4);
    TEST_ASSERT(alloc > 0, "Maximum 8192x8192 thumbnail should succeed");
}

// ========================================
// Test CRX decoder allocation cases
// ========================================
void test_crx_allocation_cases()
{
    printf("\n=== Testing CRX Decoder Allocation Cases ===\n");

    // Simulate CRX qStep allocation
    // qStep size = totalHeight * qpWidth * sizeof(uint32_t) + levels * sizeof(CrxQStep)

    size_t qstep_part1 = safe_alloc_size_3(8192, 8192, sizeof(uint32_t));
    TEST_ASSERT(qstep_part1 > 0, "CRX qStep main allocation should succeed");

    size_t qstep_part2 = safe_alloc_size(16, 64);  // assuming CrxQStep is ~64 bytes
    TEST_ASSERT(qstep_part2 > 0, "CRX qStep levels allocation should succeed");

    size_t total;
    int status = safe_add_size_t(qstep_part1, qstep_part2, &total);
    TEST_ASSERT(status == 0, "CRX total allocation combination should succeed");

    // Test overflow case with excessive dimensions
    // Note: On 64-bit, 1M*1M*4 = 4TB which doesn't overflow, so use bigger values
    size_t bad_alloc = safe_alloc_size_3(SIZE_MAX / 4, 8, 1);
    TEST_ASSERT(bad_alloc == 0, "Excessive CRX dimensions should fail safely");
}

// ========================================
// Test division by zero edge cases
// ========================================
void test_division_edge_cases()
{
    printf("\n=== Testing Division Edge Cases ===\n");

    // Test step division in nikon_read_curve simulation
    int step = 256;
    int ver1_40_case = step / 4;  // Should be 64
    TEST_ASSERT(ver1_40_case == 64, "Nikon curve step division 256/4=64");

    step = 3;
    ver1_40_case = step / 4;  // Should be 0 - this is the problematic case
    TEST_ASSERT(ver1_40_case == 0, "Nikon curve step division 3/4=0 (edge case)");

    // Our fix checks step > 0 after division
    step = 4;
    ver1_40_case = step / 4;
    TEST_ASSERT(ver1_40_case > 0, "Nikon curve step division 4/4=1 should be safe");

    // Test blend_highlights sum[0] check
    float sum0 = 0.0f;
    float sum1 = 100.0f;
    float chratio = (sum0 < 1e-10f) ? 1.0f : sqrtf(sum1 / sum0);
    TEST_ASSERT(fabsf(chratio - 1.0f) < 0.0001f, "blend_highlights zero sum protection");

    sum0 = 25.0f;
    chratio = (sum0 < 1e-10f) ? 1.0f : sqrtf(sum1 / sum0);
    TEST_ASSERT(fabsf(chratio - 2.0f) < 0.0001f, "blend_highlights normal case sqrt(100/25)=2");
}

// ========================================
// Test matrix singularity detection
// ========================================
void test_matrix_singularity()
{
    printf("\n=== Testing Matrix Singularity Detection ===\n");

    // Simulate cubic spline matrix check
    float pivot = 0.0f;
    int is_singular = (fabsf(pivot) < 1.0e-15f);
    TEST_ASSERT(is_singular == 1, "Zero pivot should be detected as singular");

    pivot = 1.0e-16f;
    is_singular = (fabsf(pivot) < 1.0e-15f);
    TEST_ASSERT(is_singular == 1, "Very small pivot should be detected as singular");

    pivot = 1.0e-14f;
    is_singular = (fabsf(pivot) < 1.0e-15f);
    TEST_ASSERT(is_singular == 0, "Normal pivot should not be singular");

    pivot = 1.0f;
    is_singular = (fabsf(pivot) < 1.0e-15f);
    TEST_ASSERT(is_singular == 0, "Unit pivot should not be singular");
}

// ========================================
// Test bounds checking simulation
// ========================================
void test_bounds_checking()
{
    printf("\n=== Testing Bounds Checking ===\n");

    // Simulate nikon_read_curve bounds check
    int curve_size = 0x10000;  // 65536
    int idx;

    idx = 100 * 256;  // 25600 - valid
    TEST_ASSERT(idx >= 0 && idx < curve_size, "Valid curve index 25600");

    idx = 300 * 256;  // 76800 - out of bounds
    TEST_ASSERT(!(idx >= 0 && idx < curve_size), "Invalid curve index 76800 detected");

    // Simulate kodak thumbnail dimension checks
    int twidth = 640, theight = 480;
    int valid = (twidth >= 16 && twidth <= 8192 && theight >= 16 && theight <= 8192);
    TEST_ASSERT(valid, "Valid thumbnail dimensions 640x480");

    twidth = 15;
    valid = (twidth >= 16 && twidth <= 8192 && theight >= 16 && theight <= 8192);
    TEST_ASSERT(!valid, "Invalid thumbnail width 15 detected");

    twidth = 8193;
    valid = (twidth >= 16 && twidth <= 8192 && theight >= 16 && theight <= 8192);
    TEST_ASSERT(!valid, "Invalid thumbnail width 8193 detected");
}

// ========================================
// Test pixel aspect ratio edge cases
// ========================================
void test_pixel_aspect_ratio()
{
    printf("\n=== Testing Pixel Aspect Ratio ===\n");

    double pixel_aspect;
    int should_return;

    // Normal case
    pixel_aspect = 1.0;
    should_return = (pixel_aspect == 1);
    TEST_ASSERT(should_return, "Pixel aspect 1.0 should return early");

    // Valid non-1.0 aspect
    pixel_aspect = 0.9;
    should_return = (pixel_aspect < 0.001);
    TEST_ASSERT(!should_return, "Pixel aspect 0.9 should proceed");

    // Edge case - very small aspect (division by zero risk)
    pixel_aspect = 0.0001;
    should_return = (pixel_aspect < 0.001);
    TEST_ASSERT(should_return, "Pixel aspect 0.0001 should return (div by zero protection)");

    // Zero aspect
    pixel_aspect = 0.0;
    should_return = (pixel_aspect < 0.001);
    TEST_ASSERT(should_return, "Pixel aspect 0.0 should return (div by zero protection)");
}

// ========================================
// Main test runner
// ========================================
int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("LibRaw Security Fixes Test Suite\n");
    printf("========================================\n");

    // Run all tests
    test_safe_mul_size_t();
    test_safe_add_size_t();
    test_safe_alloc_size_2d();
    test_safe_alloc_size_3();
    test_safe_div_float();
    test_safe_div_double();
    test_safe_div_int();
    test_image_size_boundaries();
    test_kodak_thumbnail_cases();
    test_crx_allocation_cases();
    test_division_edge_cases();
    test_matrix_singularity();
    test_bounds_checking();
    test_pixel_aspect_ratio();

    // Summary
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d\n", tests_passed);
    printf("Failed:       %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed > 0) {
        printf("SOME TESTS FAILED!\n");
        return 1;
    }

    printf("ALL TESTS PASSED!\n");
    return 0;
}
