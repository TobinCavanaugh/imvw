// pthread_time.h — stub for MSVC (provides sleep_ms used by imvw)
#ifndef PTHREAD_TIME_H
#define PTHREAD_TIME_H

#include <windows.h>

static inline void sleep_ms(int ms) {
    if (ms > 0) Sleep((DWORD)ms);
}

#endif
