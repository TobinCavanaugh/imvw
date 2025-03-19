//
// Created by tobin on 2025-03-19.
//

#ifndef TEX_LOADER_H
#define TEX_LOADER_H

#include "dialect.h"

extern context_t ctx;
extern settings_t settings;

GLFWwindow *tex_load_wind;

u0 _internal_tex_load(void *) {
    glfwMakeContextCurrent(tex_load_wind);

    ctx.current_tex = LoadTexture(ctx.current_path);
    GenTextureMipmaps(&ctx.current_tex);

    glfwDestroyWindow(tex_load_wind);

    ctx.tex_loading = 0;
    ctx.tex_need_load = 0;
}

u0 imvw_tex_load() {
    if (ctx.tex_loading) {
        printf("NOT YET >:(\n");
        return;
    }

    ctx.tex_loading = 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    tex_load_wind = glfwCreateWindow(1, 1, "Tex Loader", NULL, ctx.main_window);

    pthread_t thr;
    pthread_create(&thr, NULL, _internal_tex_load, NULL);
    pthread_detach(thr);
}

#endif //TEX_LOADER_H
