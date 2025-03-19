//
// Created by tobin on 2025-03-19.
//

#ifndef FONT_LOADER_H
#define FONT_LOADER_H

#include "dialect.h"

extern context_t ctx;
extern settings_t settings;

GLFWwindow *font_load_wind = NULL;

u0 _internal_font_load(void *) {
    // font_load_wind = glfwGetCurrentContext();
    glfwMakeContextCurrent(font_load_wind);

    char *win_font_name = "C:\\Windows\\Fonts\\segoeui.ttf";
    Font rlfont = LoadFontEx(win_font_name, 256, NULL, 0);

    // If we have a custom font to load
    if (settings.program_font_path != NULL && strlen(settings.program_font_path) > 0) {
        Font custom_font = LoadFontEx(settings.program_font_path, 256, NULL, 0);
        UnloadFont(rlfont);
        rlfont = custom_font;
    }

    GenTextureMipmaps(&rlfont.texture);
    SetTextureFilter(rlfont.texture, TEXTURE_FILTER_TRILINEAR);

    ctx.current_font = rlfont;

    glfwDestroyWindow(font_load_wind);
}

static u0 imvw_font_load() {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    font_load_wind = glfwCreateWindow(1, 1, "Font Loader", NULL, ctx.main_window);

    pthread_t thr;
    pthread_create(&thr, NULL, _internal_font_load, NULL);
    pthread_detach(thr);
}

#endif //FONT_LOADER_H
