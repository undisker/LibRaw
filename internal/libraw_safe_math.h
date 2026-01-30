/* -*- C++ -*-
 * File: internal/libraw_safe_math.h
 * Safe math operations for LibRaw - overflow protection and division safety
 *
 * LibRaw is free software; you can redistribute it and/or modify
 * it under the terms of the one of two licenses as you choose:
 *
 * 1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
 *    (See file LICENSE.LGPL provided in LibRaw distribution archive for details).
 *
 * 2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
 *    (See file LICENSE.CDDL provided in LibRaw distribution archive for details).
 */

#ifndef LIBRAW_SAFE_MATH_H
#define LIBRAW_SAFE_MATH_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Safe multiplication with overflow detection
 * Returns 0 on success, -1 on overflow
 */
static inline int safe_mul_size_t(size_t a, size_t b, size_t *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }
    if (a > SIZE_MAX / b) {
        *result = 0;
        return -1;  /* overflow */
    }
    *result = a * b;
    return 0;
}

static inline int safe_mul_int(int a, int b, int *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }
    /* Check for overflow in both directions */
    if (a > 0) {
        if (b > 0) {
            if (a > INT_MAX / b) { *result = 0; return -1; }
        } else {
            if (b < INT_MIN / a) { *result = 0; return -1; }
        }
    } else {
        if (b > 0) {
            if (a < INT_MIN / b) { *result = 0; return -1; }
        } else {
            if (a != 0 && b < INT_MAX / a) { *result = 0; return -1; }
        }
    }
    *result = a * b;
    return 0;
}

static inline int safe_mul_uint(unsigned int a, unsigned int b, unsigned int *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }
    if (a > UINT_MAX / b) {
        *result = 0;
        return -1;
    }
    *result = a * b;
    return 0;
}

static inline int safe_mul_int64(int64_t a, int64_t b, int64_t *result)
{
    if (a == 0 || b == 0) {
        *result = 0;
        return 0;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > INT64_MAX / b) { *result = 0; return -1; }
        } else {
            if (b < INT64_MIN / a) { *result = 0; return -1; }
        }
    } else {
        if (b > 0) {
            if (a < INT64_MIN / b) { *result = 0; return -1; }
        } else {
            if (a != 0 && b < INT64_MAX / a) { *result = 0; return -1; }
        }
    }
    *result = a * b;
    return 0;
}

/*
 * Safe addition with overflow detection
 */
static inline int safe_add_size_t(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) {
        *result = 0;
        return -1;
    }
    *result = a + b;
    return 0;
}

static inline int safe_add_int(int a, int b, int *result)
{
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *result = 0;
        return -1;
    }
    *result = a + b;
    return 0;
}

/*
 * Safe division - returns default value if divisor is zero or too small
 */
static inline int safe_div_int(int numerator, int divisor, int default_val)
{
    if (divisor == 0)
        return default_val;
    return numerator / divisor;
}

static inline unsigned int safe_div_uint(unsigned int numerator, unsigned int divisor, unsigned int default_val)
{
    if (divisor == 0)
        return default_val;
    return numerator / divisor;
}

static inline float safe_div_float(float numerator, float divisor, float default_val)
{
    if (fabsf(divisor) < FLT_MIN)
        return default_val;
    return numerator / divisor;
}

static inline double safe_div_double(double numerator, double divisor, double default_val)
{
    if (fabs(divisor) < DBL_MIN)
        return default_val;
    return numerator / divisor;
}

/*
 * Safe modulo - returns 0 if divisor is zero
 */
static inline int safe_mod_int(int numerator, int divisor)
{
    if (divisor == 0)
        return 0;
    return numerator % divisor;
}

static inline unsigned int safe_mod_uint(unsigned int numerator, unsigned int divisor)
{
    if (divisor == 0)
        return 0;
    return numerator % divisor;
}

/*
 * Safe array index validation
 * Returns clamped index within [0, max_index]
 */
static inline size_t safe_array_index(size_t index, size_t array_size)
{
    if (array_size == 0)
        return 0;
    if (index >= array_size)
        return array_size - 1;
    return index;
}

static inline int safe_array_index_int(int index, int array_size)
{
    if (array_size <= 0 || index < 0)
        return 0;
    if (index >= array_size)
        return array_size - 1;
    return index;
}

/*
 * Safe allocation size calculation
 * Returns 0 if overflow would occur, otherwise returns the calculated size
 */
static inline size_t safe_alloc_size(size_t count, size_t element_size)
{
    size_t result;
    if (safe_mul_size_t(count, element_size, &result) != 0)
        return 0;
    return result;
}

static inline size_t safe_alloc_size_2d(size_t dim_w, size_t dim_h, size_t element_size)
{
    size_t temp, result;
    if (safe_mul_size_t(dim_w, dim_h, &temp) != 0)
        return 0;
    if (safe_mul_size_t(temp, element_size, &result) != 0)
        return 0;
    return result;
}

static inline size_t safe_alloc_size_3(size_t a, size_t b, size_t c)
{
    size_t temp, result;
    if (safe_mul_size_t(a, b, &temp) != 0)
        return 0;
    if (safe_mul_size_t(temp, c, &result) != 0)
        return 0;
    return result;
}

/*
 * Check if allocation size is reasonable (within memory limits)
 */
static inline int is_allocation_reasonable(size_t size, size_t max_mb)
{
    size_t max_bytes;
    if (safe_mul_size_t(max_mb, 1024ULL * 1024ULL, &max_bytes) != 0)
        return 0;  /* overflow in limit calculation, be conservative */
    return size <= max_bytes;
}

/*
 * Safe left shift - prevents undefined behavior
 */
static inline unsigned int safe_lshift_uint(unsigned int value, int shift)
{
    if (shift < 0 || shift >= (int)(sizeof(unsigned int) * 8))
        return 0;
    return value << shift;
}

static inline uint64_t safe_lshift_uint64(uint64_t value, int shift)
{
    if (shift < 0 || shift >= 64)
        return 0;
    return value << shift;
}

/*
 * Safe file offset calculation
 */
static inline int64_t safe_file_offset(int64_t base, int64_t offset)
{
    /* Check for overflow */
    if (offset > 0 && base > INT64_MAX - offset)
        return -1;
    if (offset < 0 && base < INT64_MIN - offset)
        return -1;
    int64_t result = base + offset;
    /* File offsets should be non-negative */
    if (result < 0)
        return -1;
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBRAW_SAFE_MATH_H */
