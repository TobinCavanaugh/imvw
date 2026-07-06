//
// Created by tobin on 2025-05-20.
// Modified for MSVC: QueryPerformanceCounter instead of clock_gettime
//

#include "imvw_time.h"
#include "win_include.h"
#include "dialect.h"
#include "imvw_interface.h"

static LARGE_INTEGER g_qpc_freq = {0};
static int g_qpc_init = 0;

f128 getTimeHD_ms() {
    if (!g_qpc_init) {
        QueryPerformanceFrequency(&g_qpc_freq);
        g_qpc_init = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (f128)(counter.QuadPart * 1000LL) / (f128)g_qpc_freq.QuadPart;
}

// NOTE: sleep_ms is provided by pthread_time.h (included via win_include.h -> pthread_time.h)
// Do NOT redefine it here.
