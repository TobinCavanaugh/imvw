// Lightweight frame profiler — replaces manual tN timestamp arithmetic.
//
// Usage in the frame loop:
//   profiler_begin_frame(loop_entry_ms);
//   // ... phase 1 ...
//   profiler_mark("phase 1 name");
//   // ... phase 2 ...
//   profiler_mark("phase 2 name");
//   // ...
//   if (frame_count % 120 == 0) profiler_report(120);
//
// Each mark() measures the elapsed time since the previous call and adds
// it to that phase's accumulator.  No numbered variables required.

#ifndef PROFILER_H
#define PROFILER_H

#include "core/dialect.h"

#ifdef IMVW_PROFILE

// Start profiling a new frame.  Resets all phase accumulators and records
// the frame start timestamp.  Must be called once per frame before any mark().
void profiler_begin_frame(f128 frame_start_ms);

// Mark the end of the current phase and the start of the next.
// Adds the elapsed time since the previous mark (or begin_frame) to the
// named phase's accumulator.  String literals are safe (internally copied).
void profiler_mark(const char *name);

// Print accumulated timing data for all phases, averaged over
// frame_count frames, to stderr in a formatted table.
void profiler_report(int frame_count);

#else

#define profiler_begin_frame(x) ((void)0)
#define profiler_mark(x)        ((void)0)
#define profiler_report(x)      ((void)0)

#endif // IMVW_PROFILE

#endif // PROFILER_H
