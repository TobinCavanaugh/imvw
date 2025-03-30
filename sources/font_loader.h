//
// Created by tobin on 2025-03-19.
//

#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "dialect.h"

#define LOG_FONT "FONT: "

extern context_t ctx;
extern settings_t settings;

static u0 imvw_font_load() {
    Font rlfont = {0};

    // If we have a custom font to load
    if (settings.program_font_path != NULL && strlen(settings.program_font_path) > 0) {
        Font custom_font = LoadFontEx(settings.program_font_path, 256, NULL, 0);
        UnloadFont(rlfont);
        rlfont = custom_font;

        if (!rlfont.texture.width) {
            printf(LOG_FONT "Failed to load custom font from `%s`. Falling back to system font.\n",
                   settings.program_font_path);
        } else {
            printf(LOG_FONT "Loaded custom font from `%s`\n", settings.program_font_path);
        }
    }

    if (!rlfont.texture.width) {
        char *win_font_name = "C:\\Windows\\Fonts\\segoeui.ttf";
        rlfont = LoadFont(win_font_name);

        if (!rlfont.texture.width) {
            printf(LOG_FONT"Failed to load system font from `%s`. Falling back to default Raylib font.\n",
                   win_font_name);
        } else {
            printf(LOG_FONT"Loaded system font from `%s`\n", win_font_name);
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
