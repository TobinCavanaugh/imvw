#ifndef TEXT_H
#define TEXT_H

#include "core/dialect.h"

// draw_flush is defined in tr_raylib_draw.c (not declared in headers)
// but has external linkage — force-flushes the batched draw queue.
#ifdef __cplusplus
extern "C" void draw_flush(void);
#else
extern void draw_flush(void);
#endif

// Query the system's message font size (used as the default text height).
f32 get_system_font_size(void);

// Draw a single line of properties overlay text at the given y-position.
// Returns the line height for stacking subsequent lines.
f32 draw_properties(f32 properties_line, char *format, ...);

#endif //TEXT_H
