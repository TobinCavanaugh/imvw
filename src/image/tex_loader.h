#ifndef TEX_LOADER_H
#define TEX_LOADER_H

#include "core/dialect.h"
#include "image/decoders.h"
#include <windows.h>
#include <shlobj.h>

#define ERR_TEX(errname) fprintf(stderr, "ERR|TEX: \n\tat: %s:%d \n\t`%s`\n", __FILE__, __LINE__, errname);

extern context_t ctx;

typedef struct tex_load_ctx_t {
    char path[PATH_MAX];
    uint64_t request_id;
} tex_load_ctx_t;

static inline void *internal_thumb_load(void *raw_ptr) {
    tex_load_ctx_t *load_ctx = (tex_load_ctx_t *) raw_ptr;
    uint64_t req_id = load_ctx->request_id;

    // Initialize COM on dedicated worker thread for Windows Shell thumbnail extraction
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (req_id == ctx.load_request_id && !ctx.img_uploaded) {
        Image thumb = imvw_get_thumbnail(load_ctx->path, 1024, 1024);
        if (thumb.data != NULL) {
            // Only hand over thumbnail if full image has not yet been uploaded and request is current
            if (req_id == ctx.load_request_id && !ctx.img_uploaded) {
                if (ctx.thumb_img.data && ctx.thumb_img.data != ctx.active_image.data) {
                    UnloadImage(ctx.thumb_img);
                }
                ctx.thumb_img = thumb;
                ctx.thumb_req_id = req_id;
                ctx.thumb_ready_to_upload = 1;
            } else {
                UnloadImage(thumb);
            }
        }
    }

    CoUninitialize();
    free(load_ctx);
    return NULL;
}

static inline void *internal_tex_load(void *raw_ptr) {
    tex_load_ctx_t *load_ctx = (tex_load_ctx_t *) raw_ptr;
    uint64_t req_id = load_ctx->request_id;

    // Decode full-resolution image immediately with zero thumbnail delay
    Image decoded = imvw_load_image_extended(load_ctx->path);

    // If a newer image was requested while decoding, discard result immediately
    if (req_id != ctx.load_request_id) {
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
    ctx.img_req_id = req_id;
    ctx.img_ready_to_upload = 1;
    ctx.tex_loading = 0;
    ctx.tex_need_load = 0;

    free(load_ctx);
    return NULL;
}

static inline u0 imvw_tex_load() {
    uint64_t req_id = ++ctx.load_request_id;
    ctx.tex_loading = 1;
    ctx.img_uploaded = 0;

    // 1. Launch full image decoder immediately
    tex_load_ctx_t *load_ctx = (tex_load_ctx_t*) malloc(sizeof(tex_load_ctx_t));
    if (!load_ctx) return;
    strncpy(load_ctx->path, ctx.current_path, PATH_MAX - 1);
    load_ctx->path[PATH_MAX - 1] = '\0';
    load_ctx->request_id = req_id;

    pthread_t thr_load;
    pthread_create(&thr_load, NULL, internal_tex_load, (void *) load_ctx);
    pthread_detach(thr_load);

    // 2. Concurrently fetch thumbnail in background (fire-and-forget, non-blocking)
    tex_load_ctx_t *thumb_ctx = (tex_load_ctx_t*) malloc(sizeof(tex_load_ctx_t));
    if (thumb_ctx) {
        strncpy(thumb_ctx->path, ctx.current_path, PATH_MAX - 1);
        thumb_ctx->path[PATH_MAX - 1] = '\0';
        thumb_ctx->request_id = req_id;

        pthread_t thr_thumb;
        pthread_create(&thr_thumb, NULL, internal_thumb_load, (void *) thumb_ctx);
        pthread_detach(thr_thumb);
    }
}

#endif //TEX_LOADER_H
