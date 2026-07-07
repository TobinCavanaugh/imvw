#ifndef FONT_H
#define FONT_H

#include "core/dialect.h"

#define LOG_FONT "LOG|FONT: "

// Atlas cell dimensions — large enough for any GDI-rendered glyph.
// Sampled at 3x for better quality when downscaled to display size.
#define FALLBACK_CELL_W  48
#define FALLBACK_CELL_H  64
#define FALLBACK_COLS    16
#define FALLBACK_ROWS    6

// Draw a single line of text by stamping glyph pixels from the in-memory
// fallback atlas as individual DrawRectangle calls.  Only used when
// no usable raylib Font is available.
void draw_text_fallback(const char *text, float x, float y,
                         float font_size, Color color);

// Load the application font.  Tries, in order:
//   1. Pre-loaded font data (from async_loader)
//   2. Well-known Windows font paths + assets fallback
//   3. raylib GetFontDefault()
//   4. GDI-generated fallback atlas (pixel-by-pixel drawing)
void imvw_font_load(void);

#endif //FONT_H
