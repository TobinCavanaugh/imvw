//
// Created by tobin on 2025-03-19.
//

#ifndef TEX_LOADER_H
#define TEX_LOADER_H

#include "dialect.h"

#define LOG_TEX "TEX: "
#define ERR_TEX(errname) fprintf(stderr, "TEX|ERR: \n\tat: %s:%d \n\t`%s`\n", __FILE__, __LINE__, errname);

extern context_t ctx;
extern settings_t settings;

typedef struct tex_load_ctx_t {
    char *path;
    Texture2D *out_tex;
    _Atomic
    u8 *out_loading;
    _Atomic
    u8 *out_need_load;
    GLFWwindow *glfw_window;
} tex_load_ctx_t;

// Internal texture loading function for thread
u0 *internal_tex_load(void *raw_ptr) {
    // Cast the raw pointer to our context struct
    struct tex_load_ctx_t *load_ctx = (struct tex_load_ctx_t *) raw_ptr;

    // Set our current context to our loading context
    glfwMakeContextCurrent(load_ctx->glfw_window);

    // Load the texture
    *load_ctx->out_tex = LoadTexture(load_ctx->path);

    if(!load_ctx->out_tex->width) {
        ERR_TEX("Failed to load texture");
    }

    // Generate mipmaps
    GenTextureMipmaps(load_ctx->out_tex);

    // Destroy the loading window
    glfwDestroyWindow(load_ctx->glfw_window);

    // Set the loaded flag
    *load_ctx->out_loading = 0;
    *load_ctx->out_need_load = 0;

    // Free the context
    free(load_ctx);

    return NULL;
}

// Async texture loading function
u0 imvw_tex_load() {
    // Check if a texture is already loading
    if (!ctx.tex_need_load || ctx.tex_loading) {
        printf("Texture already loading or loaded >:(\n");
        return;
    }

    // Set window hint for invisible window
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Allocate context for the loading thread
    struct tex_load_ctx_t *load_ctx = malloc(sizeof(struct tex_load_ctx_t));

    // Populate the context
    load_ctx->path = ctx.current_path;
    load_ctx->out_tex = &ctx.current_tex;
    load_ctx->out_loading = &ctx.tex_loading;
    load_ctx->out_need_load = &ctx.tex_need_load;

    // Create the loading window based on main context
    load_ctx->glfw_window = glfwCreateWindow(1, 1, "Tex Loader", NULL, ctx.main_window);
    if(!load_ctx->glfw_window) {
        ERR_TEX("Failed to create glfw window");
    }

    // Set the loading flag
    ctx.tex_loading = 1;

    // Create and detach the thread
    pthread_t thr;
    pthread_create(&thr, NULL, *internal_tex_load, (void *) load_ctx);
    pthread_detach(thr);
}

#endif //TEX_LOADER_H
