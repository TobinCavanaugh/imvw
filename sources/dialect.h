//
// Created by tobin on 12/15/24.
//

#include <ctype.h>
#include <stdint.h>
#include "raylib.h"

#ifndef CAA_EPIDEMIC_DIALECT_H
#define CAA_EPIDEMIC_DIALECT_H

#define u0 void

/* Use GCC's built-in types for fixed width integers */
#define u8 __UINT8_TYPE__
#define u16 __UINT16_TYPE__
#define u32 __UINT32_TYPE__
#define u64 __UINT64_TYPE__
#define i8 __INT8_TYPE__
#define i16 __INT16_TYPE__
#define i32 __INT32_TYPE__
#define i64 __INT64_TYPE__

#define f32 float
#define f64 double
#define f128 long double

// If using C11
#if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(f32) == 4);
_Static_assert(sizeof(f64) == 8);
_Static_assert(sizeof(f128) == 16);
#endif

/* Epsilon macros */

/* Maximum values for unsigned types */
#define u8_MAX __UINT8_MAX__
#define u16_MAX __UINT16_MAX__
#define u32_MAX __UINT32_MAX__
#define u64_MAX __UINT64_MAX__

/* Maximum and minimum values for signed types */
#define i8_MAX 127
#define i8_MIN (-128)
#define i16_MAX 32767
#define i16_MIN (-32768)
#define i32_MAX 2147483647
#define i32_MIN (-2147483648)
#define i64_MAX 9223372036854775807LL
#define i64_MIN (-9223372036854775807LL - 1LL)

/* Casting macros */
#define U8(__a) ((u8)(__a))
#define U16(__a) ((u16)(__a))
#define U32(__a) ((u32)(__a))
#define U64(__a) ((u64)(__a))
#define I8(__a) ((i8)(__a))
#define I16(__a) ((i16)(__a))
#define I32(__a) ((i32)(__a))
#define I64(__a) ((i64)(__a))
#define F32(__a) ((f32)(__a))
#define F128(__a) ((f128)(__a))

#define V2f(a, b) (Vector2) {a, b}
#define v2f Vector2
#define V3f(a, b, c) (Vector3) {a, b, c}
#define v3f Vector3

#define NO_OP ({ 0; })

static u0 stolow(char *str) {
    for (i32 i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

static u0 stoup(char *str) {
    for (i32 i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

#endif //CAA_EPIDEMIC_DIALECT_H
