#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

#include <pthread.h>

typedef struct {
    int argc;
    char **argv;
    int target_w;
    int target_h;
    volatile int config_ready;
    volatile int size_ready;
} loader_data_t;

// Background thread that loads JSON config, shader sources, fonts, and
// determines initial image dimensions for window sizing.
void *async_loader(void *arg);

#endif //ASYNC_LOADER_H
