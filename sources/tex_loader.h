#ifndef TEX_LOADER_H
#define TEX_LOADER_H

#include "dialect.h"
#include "image_decoder.h"

#define ERR_TEX(errname) fprintf(stderr, "ERR|TEX: \n\tat: %s:%d \n\t`%s`\n", __FILE__, __LINE__, errname);

extern context_t ctx;

typedef struct tex_load_ctx_t {
    char *path;
    Image *out_img;
    _Atomic u8 *out_loading;
    _Atomic u8 *out_ready;
} tex_load_ctx_t;

void *internal_tex_load(void *raw_ptr) {
    tex_load_ctx_t *load_ctx = (tex_load_ctx_t *) raw_ptr;

    // Load into CPU RAM (100% Thread Safe!)
    *load_ctx->out_img = imvw_load_image_extended(load_ctx->path);

    if(!load_ctx->out_img->width || !load_ctx->out_img->data) {
        ERR_TEX("Failed to decode image data into RAM");
        // Create a dummy magenta texture so the program doesn't crash
        *load_ctx->out_img = GenImageColor(2, 2, MAGENTA);
    }

    // Signal main thread to upload to GPU
    *load_ctx->out_ready = 1;
    *load_ctx->out_loading = 0;

    free(load_ctx);
    return NULL;
}

u0 imvw_tex_load() {
    if (!ctx.tex_need_load || ctx.tex_loading) return;

    tex_load_ctx_t *load_ctx = (tex_load_ctx_t*) malloc(sizeof(tex_load_ctx_t));
    load_ctx->path = ctx.current_path;
    load_ctx->out_img = &ctx.loading_img;
    load_ctx->out_loading = &ctx.tex_loading;
    load_ctx->out_ready = &ctx.img_ready_to_upload;

    ctx.tex_loading = 1;

    pthread_t thr;
    pthread_create(&thr, NULL, internal_tex_load, (void *) load_ctx);
    pthread_detach(thr);
}

#endif //TEX_LOADER_H
