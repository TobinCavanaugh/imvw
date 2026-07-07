#include "text.h"
#include "core/context_t.h"
#include "core/win_include.h"
#include "ui/font.h"
#include <stdarg.h>
#include <string.h>

extern context_t ctx;

f32 get_system_font_size() {
    NONCLIENTMETRICSA metrics = {0};
    metrics.cbSize = sizeof(NONCLIENTMETRICSA);
    f32 fontsize = 6;
    if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        fontsize += -F32(metrics.lfMessageFont.lfHeight);
    }
    return fontsize;
}

f32 draw_properties(f32 properties_line, char *format, ...) {
    va_list args;
    va_start(args, format);
    static char properties_working[PATH_MAX];
    vsnprintf(properties_working, PATH_MAX, format, args);
    va_end(args);

    memmove(properties_working + 1, properties_working, strlen(properties_working) + 1);
    properties_working[0] = ' ';
    strcat(properties_working, " ");

    f32 font_size = get_system_font_size();
    Color text_col = { 230, 230, 230, 255 };
    Color bg = { 0, 0, 0, 180 };

    // If we have a loaded font (handle != NULL — texture is a compat stub
    // in tr_raylib), use proper text rendering.
    // NOTE: tr_raylib's DrawTextPro draws at the font's baseSize regardless
    // of the fontSize parameter, so we must use that for layout. SSAA in
    // render/ssaa.c provides the frame-level anti-aliasing (1.1x downscale).
    if (ctx.current_font.handle != NULL) {
        i32 fs = ctx.current_font.baseSize ? ctx.current_font.baseSize : (i32)font_size;
        i32 tw = (i32) MeasureTextEx(ctx.current_font, properties_working, (f32)fs, 0).x;
        // Draw background rect via the batched pipeline, then flush so it
        // renders immediately — DrawTextPro renders through its own pipeline
        // which ignores the batch, so flushing here ensures the correct order.
        DrawRectangle(0, (i32) properties_line, tw, fs, bg);
        draw_flush();
        DrawTextPro(ctx.current_font, properties_working, V2f(0, properties_line),
                    V2f(0, 0), 0, (f32)fs, 0, text_col);
        return (f32)fs;
    }

    // Fallback: pixel-by-pixel from GDI-generated atlas data.
    if (ctx.fallback_atlas_pixels != NULL) {
        // Rough text-width estimate for the background rect.
        i32 len = (i32) strlen(properties_working);
        i32 tw = (i32)((f32)len * font_size * 0.65f);
        DrawRectangle(0, (i32) properties_line, tw, (i32) font_size, bg);
        draw_text_fallback(properties_working, 2.0f, properties_line + 2.0f,
                           font_size, text_col);
    }

    return font_size;
}
