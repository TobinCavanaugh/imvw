//
// Created by tobin on 12/15/24.
// Modified for MSVC compatibility
//

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <tr_raylib.h>

#ifndef TOBIN_DIALECT
#define TOBIN_DIALECT

#define u0 void

// Use standard C types (works on MSVC and GCC)
#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define f32 float
#define f64 double
#define f128 long double

// If using C11
#if !defined(_MSC_VER) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
_Static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");
#endif

// Max values
#define u8_MAX  UINT8_MAX
#define u16_MAX UINT16_MAX
#define u32_MAX UINT32_MAX
#define u64_MAX UINT64_MAX

#define i8_MAX  INT8_MAX
#define i8_MIN  INT8_MIN
#define i16_MAX INT16_MAX
#define i16_MIN INT16_MIN
#define i32_MAX INT32_MAX
#define i32_MIN INT32_MIN
#define i64_MAX INT64_MAX
#define i64_MIN INT64_MIN

// Casting macros
#define U8(__a)  ((u8)(__a))
#define U16(__a) ((u16)(__a))
#define U32(__a) ((u32)(__a))
#define U64(__a) ((u64)(__a))
#define I8(__a)  ((i8)(__a))
#define I16(__a) ((i16)(__a))
#define I32(__a) ((i32)(__a))
#define I64(__a) ((i64)(__a))
#define F32(__a) ((f32)(__a))
#define F128(__a) ((f128)(__a))

// Vector helpers (tr_raylib provides Vector2, Vector3)
#define V2f(a, b) (Vector2) {a, b}
#define v2f Vector2
#define V3f(a, b, c) (Vector3) {a, b, c}
#define v3f Vector3

#define NO_OP ((void)0)

static u0 stolow(char *str) {
    for (i32 i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

static u0 stoup(char *str) {
    for (i32 i = 0; str[i]; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define true 1
#define false 0

#endif //TOBIN_DIALECT
