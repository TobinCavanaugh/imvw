#ifndef TEX_LOADER_H
#define TEX_LOADER_H

#include "core/dialect.h"
#include "image/decoders.h"

#define ERR_TEX(errname) fprintf(stderr, "ERR|TEX: \n\tat: %s:%d \n\t`%s`\n", __FILE__, __LINE__, errname);

extern context_t ctx;

typedef struct tex_load_ctx_t {
    char path[PATH_MAX];
    uint64_t request_id;
} tex_load_ctx_t;

static inline void *internal_tex_load(void *raw_ptr) {
    tex_load_ctx_t *load_ctx = (tex_load_ctx_t *) raw_ptr;

    // Decode full-resolution image in background thread
    Image decoded = imvw_load_image_extended(load_ctx->path);

    // If a newer image was requested while decoding, discard this result immediately
    if (load_ctx->request_id != ctx.load_request_id) {
        if (decoded.data) UnloadImage(decoded);
        free(load_ctx);
        return NULL;
    }

    if (!decoded.width || !decoded.data) {
        ERR_TEX("Failed to decode image data into RAM");
        decoded = GenImageColor(2, 2, MAGENTA);
    }

    if (ctx.loading_img.data && ctx.loading_img.data != ctx.active_image.data) {
        UnloadImage(ctx.loading_img);
    }
    ctx.loading_img = decoded;
    ctx.img_ready_to_upload = 1;
    ctx.tex_loading = 0;
    ctx.tex_need_load = 0;

    free(load_ctx);
    return NULL;
}

static inline u0 imvw_tex_load() {
    uint64_t req_id = ++ctx.load_request_id;
    ctx.tex_loading = 1;

    tex_load_ctx_t *load_ctx = (tex_load_ctx_t*) malloc(sizeof(tex_load_ctx_t));
    if (!load_ctx) return;
    strncpy(load_ctx->path, ctx.current_path, PATH_MAX - 1);
    load_ctx->path[PATH_MAX - 1] = '\0';
    load_ctx->request_id = req_id;

    pthread_t thr;
    pthread_create(&thr, NULL, internal_tex_load, (void *) load_ctx);
    pthread_detach(thr);
}

#endif //TEX_LOADER_H
