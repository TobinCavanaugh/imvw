//
// Created by tobin on 12/15/24.
//

#include <stdint.h>
#include "raylib.h"

#ifndef CAA_EPIDEMIC_DIALECT_H
#define CAA_EPIDEMIC_DIALECT_H

#define u0 void

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define f32 float
#define f128 long double

#define V2f(a, b) (Vector2) {a, b}
#define v2f Vector2
#define V3f(a, b, c) (Vector3) {a, b, c}
#define v3f Vector3

#endif //CAA_EPIDEMIC_DIALECT_H
