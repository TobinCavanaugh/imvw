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
        if (rlfont.texture.width) {
            printf(LOG_FONT "Loaded font from pre-loaded memory (32px)\n");
        }
    }

    if (!rlfont.texture.width) {
        rlfont = GetFontDefault();
        printf(LOG_FONT"Loaded Raylib default font\n");
    }

    ctx.current_font = rlfont;
    GenTextureMipmaps(&ctx.current_font.texture);
    SetTextureFilter(ctx.current_font.texture, TEXTURE_FILTER_TRILINEAR);
}

#endif //FONT_LOADER_H
