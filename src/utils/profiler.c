#include "profiler.h"
#include "utils/imvw_time.h"
#include <stdio.h>
#include <string.h>

#ifdef IMVW_PROFILE

#define PROFILER_MAX_PHASES 32
#define PROFILER_NAME_MAX   48

static struct {
    char name[PROFILER_NAME_MAX];
    f128 total; // accumulated time across all frames (reset each frame)
} phases[PROFILER_MAX_PHASES];
static int phase_count = 0; // number of phases discovered; persists across frames

static f128 last_time;       // timestamp of the previous mark()/begin_frame()
static f128 frame_start;     // timestamp passed to begin_frame()

void profiler_begin_frame(f128 frame_start_ms) {
    frame_start = frame_start_ms;
    last_time   = frame_start_ms;
    // Reset accumulator values but keep the phase list so we don't
    // repeatedly strcmp the same names every frame.
    for (int i = 0; i < phase_count; i++) {
        phases[i].total = 0;
    }
    // phase_count intentionally NOT reset — entries persist across frames.
}

void profiler_mark(const char *name) {
    f128 now = getTimeHD_ms();
    f128 dt  = now - last_time;
    last_time = now;

    // Find existing phase or add a new one.
    int idx = -1;
    for (int i = 0; i < phase_count; i++) {
        if (strcmp(phases[i].name, name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && phase_count < PROFILER_MAX_PHASES) {
        idx = phase_count++;
        strncpy(phases[idx].name, name, PROFILER_NAME_MAX - 1);
        phases[idx].name[PROFILER_NAME_MAX - 1] = '\0';
        phases[idx].total = 0;
    }
    if (idx >= 0) {
        phases[idx].total += dt;
    }
}

void profiler_report(int frame_count) {
    f128 wall_total = last_time - frame_start;
    f128 n = (f128)frame_count;

    fprintf(stderr, "\n--- FRAME PROFILE (avg over %d frames) ---\n", frame_count);
    for (int i = 0; i < phase_count; i++) {
        fprintf(stderr, "  %-40s %5.2Lf us\n",
                phases[i].name, phases[i].total * 1000.0L / n);
    }
    fprintf(stderr, "  -----------------------------------------------------\n");

    // Compute active CPU work: wall total minus any phases that include
    // idle time (vsync present and message pumping).
    f128 idle = 0;
    for (int i = 0; i < phase_count; i++) {
        if (strstr(phases[i].name, "vsync") ||
            strstr(phases[i].name, "pump_messages")) {
            idle += phases[i].total;
        }
    }
    fprintf(stderr, "  %-40s %5.2Lf us\n",
            "Active CPU work", (wall_total - idle) * 1000.0L / n);
    fprintf(stderr, "  %-40s %5.2Lf us  (%.0Lf FPS)\n",
            "Total (wall clock)", wall_total * 1000.0L / n,
            1000.0L * n / wall_total);
}

#endif // IMVW_PROFILE
