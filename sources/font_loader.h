//
// Created by tobin on 2025-03-19.
//

#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "dialect.h"

#define LOG_FONT "LOG|FONT: "

extern context_t ctx;
extern settings_t settings;

// Atlas cell dimensions — large enough for any GDI-rendered glyph.
#define FALLBACK_CELL_W  16
#define FALLBACK_CELL_H  20
#define FALLBACK_COLS    16
#define FALLBACK_ROWS    6

// Draw a single line of text by stamping glyph pixels from the in-memory
// fallback atlas as individual DrawRectangle calls.  Only used when
// ctx.current_font.texture.height == 0 (no usable raylib Font).
static void draw_text_fallback(const char *text, float x, float y,
                                float font_size, Color color) {
    u8 *pix = ctx.fallback_atlas_pixels;
    int a_w = ctx.fallback_atlas_w;
    int a_h = ctx.fallback_atlas_h;
    if (!pix || a_w <= 0 || a_h <= 0) return;

    float scale = font_size / (float)FALLBACK_CELL_H;
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 3.0f) scale = 3.0f;   // sanity cap
    int iscale = (int)(scale + 0.5f);
    if (iscale < 1) iscale = 1;

    float cur_x = x;
    for (const char *p = text; *p; p++) {
        int c = (unsigned char)*p;
        if (c < 32 || c > 127) continue;

        int idx  = c - 32;
        int col  = idx % FALLBACK_COLS;
        int row  = idx / FALLBACK_COLS;
        int cx   = col * FALLBACK_CELL_W;
        int cy   = row * FALLBACK_CELL_H;

        // Determine actual glyph width (rightmost non-empty column).
        int glyph_w = ctx.fallback_atlas_widths[idx];
        if (glyph_w < 1) glyph_w = FALLBACK_CELL_W;

        // Stamp glyph — merge adjacent horizontal on-pixels into one wider rect
        // to massively cut draw-call count (~320 per char → ~8 per char).
        for (int py = 0; py < FALLBACK_CELL_H; py++) {
            int run_start = -1;
            for (int px = 0; px < glyph_w; px++) {
                int si = ((cy + py) * a_w + (cx + px)) * 4;
                int on = (pix[si + 3] > 0);
                if (on && run_start < 0) run_start = px;
                if (!on && run_start >= 0) {
                    DrawRectangle((int)(cur_x + run_start * scale),
                                  (int)(y     + py        * scale),
                                  (int)((px - run_start) * scale), iscale, color);
                    run_start = -1;
                }
            }
            if (run_start >= 0)
                DrawRectangle((int)(cur_x + run_start * scale),
                              (int)(y     + py          * scale),
                              (int)((glyph_w - run_start) * scale), iscale, color);
        }
        cur_x += (float)glyph_w * scale;
    }
}

// Generate a fallback font atlas using Windows GDI.  Stores raw RGBA pixel
// data in ctx.fallback_atlas_pixels (for pixel-by-pixel text drawing) and
// per-glyph widths in ctx.fallback_atlas_widths[].
static void generate_gdi_atlas() {
    int atlas_w = FALLBACK_COLS * FALLBACK_CELL_W;  // 256
    int atlas_h = FALLBACK_ROWS * FALLBACK_CELL_H;  // 120

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = atlas_w;
    bmi.bmiHeader.biHeight      = -atlas_h;          // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbm || !bits) {
        if (hbm) DeleteObject(hbm);
        DeleteDC(hdc);
        return;
    }
    SelectObject(hdc, hbm);

    // Zero the pixel buffer so un-painted areas stay transparent.
    memset(bits, 0, (size_t)atlas_w * atlas_h * 4);

    static const char *font_names[] = {
        "Segoe UI", "Tahoma", "Arial", "Microsoft Sans Serif",
        "MS Shell Dlg 2", "System"
    };
    HFONT hfont = NULL;
    for (int i = 0; i < 6 && !hfont; i++) {
        hfont = CreateFontA(-FALLBACK_CELL_H, 0, 0, 0, FW_NORMAL,
                            FALSE, FALSE, FALSE, ANSI_CHARSET,
                            OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, font_names[i]);
    }
    if (hfont) SelectObject(hdc, hfont);
    SetBkColor(hdc, RGB(0, 0, 0));
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    // Render each printable ASCII character into its cell.
    for (int c = 32; c < 128; c++) {
        int idx  = c - 32;
        int col  = idx % FALLBACK_COLS;
        int row  = idx / FALLBACK_COLS;
        int cx   = col * FALLBACK_CELL_W;
        int cy   = row * FALLBACK_CELL_H;
        char ch[2] = { (char)c, 0 };
        RECT r = { cx, cy, cx + FALLBACK_CELL_W, cy + FALLBACK_CELL_H };
        ExtTextOutA(hdc, cx + 2, cy + 2, ETO_OPAQUE, &r, ch, 1, NULL);
    }

    // Copy GDI BGRX pixels → RGBA byte array.
    ctx.fallback_atlas_w = atlas_w;
    ctx.fallback_atlas_h = atlas_h;
    ctx.fallback_atlas_pixels = (u8 *)malloc((size_t)atlas_w * atlas_h * 4);

    u32 *src = (u32 *)bits;
    u8  *dst = ctx.fallback_atlas_pixels;
    for (int i = 0; i < atlas_w * atlas_h; i++) {
        u32 p = src[i];
        dst[0] = (p >> 16) & 0xFF;  // R
        dst[1] = (p >>  8) & 0xFF;  // G
        dst[2] = (p >>  0) & 0xFF;  // B
        dst[3] = (p & 0x00FFFFFF) ? 255 : 0;  // A
        dst += 4;
    }

    // Measure per-glyph pixel width (rightmost non-empty column).
    dst = ctx.fallback_atlas_pixels;
    for (int i = 0; i < 96; i++) {
        int col  = i % FALLBACK_COLS;
        int row  = i / FALLBACK_COLS;
        int cx   = col * FALLBACK_CELL_W;
        int cy   = row * FALLBACK_CELL_H;
        int found = 0;
        for (int px = cx + FALLBACK_CELL_W - 1; px >= cx && !found; px--) {
            for (int py = cy; py < cy + FALLBACK_CELL_H && !found; py++) {
                int si = (py * atlas_w + px) * 4;
                if (dst[si + 3] > 0) {
                    ctx.fallback_atlas_widths[i] = px - cx + 1;
                    found = 1;
                }
            }
        }
        if (!found) ctx.fallback_atlas_widths[i] = 1;
    }

    printf(LOG_FONT "Generated GDI fallback atlas (%dx%d)\n", atlas_w, atlas_h);

    // Cleanup GDI.
    if (hfont) DeleteObject(hfont);
    DeleteObject(hbm);
    DeleteDC(hdc);
}

static void imvw_font_load() {
    Font rlfont = {0};

    // 1. Try pre-loaded font data (segoeui.ttf or program_font_path)
    // NOTE: In tr_raylib the Font.texture field is a compat stub that is
    // never populated — the real loaded state is in Font.handle (TRFont*).
    if (ctx.font_data) {
        rlfont = LoadFontFromMemory(".ttf", ctx.font_data, ctx.font_data_size, 16, NULL, 0);
        if (rlfont.handle != NULL) {
            printf(LOG_FONT "Loaded from pre-loaded memory (handle=%p)\n", rlfont.handle);
        } else {
            printf(LOG_FONT "LoadFontFromMemory failed (handle=NULL)\n");
            rlfont = (Font){0};
        }
    }

    // 2. LoadFont from well-known paths.
    if (rlfont.handle == NULL) {
        const char *paths[] = {
            settings.program_font_path,
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\tahoma.ttf",
            "C:\\Windows\\Fonts\\consola.ttf",
            ASSETS_PATH "imvw.ttf",
        };
        for (i32 i = 0; i < 6; i++) {
            if (!paths[i]) continue;
            rlfont = LoadFont(paths[i]);
            if (rlfont.handle != NULL) {
                printf(LOG_FONT "Loaded from `%s` (handle=%p)\n", paths[i], rlfont.handle);
                break;
            }
        }
    }

    // 3. Try raylib default.
    if (rlfont.handle == NULL) {
        rlfont = GetFontDefault();
        if (rlfont.handle != NULL) {
            printf(LOG_FONT "Used GetFontDefault() (handle=%p)\n", rlfont.handle);
        }
    }

    ctx.current_font = rlfont;
    printf(LOG_FONT "Font handle=%p  baseSize=%d\n", rlfont.handle, rlfont.baseSize);

    // 4. Generate GDI fallback if still no usable font.
    if (ctx.current_font.handle == NULL) {
        printf(LOG_FONT "No usable font — generating GDI fallback atlas\n");
        generate_gdi_atlas();
    }
}

#endif //FONT_LOADER_H
