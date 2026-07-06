//
// Created by tobin on 2025-03-19.
//

#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "dialect.h"

#define LOG_FONT "LOG|FONT: "

extern context_t ctx;
extern settings_t settings;

static u0 imvw_font_load() {
    Font rlfont = {0};

    if (ctx.font_data) {
        // Reduced from 256 to 32 for much faster rasterization
        rlfont = LoadFontFromMemory(".ttf", ctx.font_data, ctx.font_data_size, 32, NULL, 0);
        if (rlfont.handle) {
            printf(LOG_FONT "Loaded font from pre-loaded memory (32px)\n");
        }
    }

    if (!rlfont.handle) {
        rlfont = GetFontDefault();
        printf(LOG_FONT"Loaded tr_raylib default font\n");
    }

    ctx.current_font = rlfont;
    // GenTextureMipmaps + SetTextureFilter are texture-only — not applicable to atlas-based fonts
}

#endif //FONT_LOADER_H
