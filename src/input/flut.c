#include "flut.h"
#include <stdlib.h>
#include <string.h>

flut_func_t *flut_array = NULL;
i32 flut_count = 0;

void _flut_add(flut_func_t func) {
    flut_array = (flut_func_t*) realloc(flut_array, sizeof(flut_func_t) * (flut_count + 1));
    flut_array[flut_count] = func;
    ++flut_count;
}

static flut_func_t *flut_find(char *key) {
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
