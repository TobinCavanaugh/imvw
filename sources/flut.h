//
// Created by tobin on 2025-02-21.
//

#ifndef FLUT_H
#define FLUT_H

#define FLUT_FUNC_NAME_MAX 64
#include <flut.h>


typedef struct {
    char key[FLUT_FUNC_NAME_MAX];
    u0 (*func)(void *);
} flut_func_t;

//TODO possibly improve with a hashtable
flut_func_t *flut_array = NULL;
i32 flut_count = 0;

u0 *_flut_add(flut_func_t func) {
    flut_array = realloc(flut_array, sizeof(flut_func_t) * (flut_count + 1));
    flut_array[flut_count] = func;
    ++flut_count;
}

#define flut_add(function) _flut_add((flut_func_t) { .key = #function, .func = function })

flut_func_t *flut_find(char *key) {
    i32 i = 0;
    for (; i < flut_count; i++) {
        if (strcmp(key, flut_array[i].key) == 0) {
            return &flut_array[i];
        }
    }

    return NULL;
}

u8 flut_get(char *key, flut_func_t *out) {
    flut_func_t *fft = flut_find(key);
    if (fft != NULL) {
        *out = *fft;
        return 1;
    }

    return 0;
}


#endif //FLUT_H
