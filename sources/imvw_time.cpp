//
// Created by tobin on 2025-05-20.
//

// #include "imvw_time.h"

#include "imvw_time.h"
#include <pthread_time.h>
// #include <windows.h>
// #include <shlobj.h>
//#include "raylib_win_compat.h"
#include "dialect.h"
#include "imvw_interface.h"

f128 getTimeHD_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000);
}

u0 sleep_ms(i32 milliseconds) {
    // Busy waiting for longer than 100ms doesn't make much sense
    if (milliseconds < 1) {
    SPIN:
        f128 start = getTimeHD_ms();
        while (1) {
            if (getTimeHD_ms() - start >= milliseconds) {
                return;
            }
        }
    } else {
        // Create a waitable timer
        HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);

        // If creating the timer fails, just default to spinning
        if (timer == NULL) {
            goto SPIN;
            return;
        }

        // Set timer to negative value for relative time
        LARGE_INTEGER li;
        li.QuadPart = -1 * (10000LL * milliseconds); // Convert to 100-nanosecond intervals

        // Set the timer
        if (SetWaitableTimer(timer, &li, 0, NULL, NULL, false)) {
            // Wait for the timer to expire
            WaitForSingleObject(timer, INFINITE);
        }

        // Close the timer handle
        CloseHandle(timer);
    }
}
