//
// Created by tobin on 2025-02-21.
//

#ifndef FLUT_H
#define FLUT_H

#define FLUT_FUNC_NAME_MAX 64

#include "../core/dialect.h"

typedef struct {
    char key[FLUT_FUNC_NAME_MAX];
    u0 (*func)(void *);
} flut_func_t;

extern flut_func_t *flut_array;
extern i32 flut_count;

void _flut_add(flut_func_t func);

#define flut_add(function) _flut_add((flut_func_t) { .key = #function, .func = (void(*)(void*))(function) })

u8 flut_get(char *key, flut_func_t *out);


#endif //FLUT_H
