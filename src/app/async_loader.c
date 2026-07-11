#include "async_loader.h"
#include <Python.h>
#include "core/context_t.h"
#include "core/settings.h"
#include "config/actions_loader.h"
#include "config/settings_loader.h"
#include "scripting/python_loader.h"
#include "image/decoders.h"
#include "ui/font.h"
#include "input/actions.h"
#include "external/stb_image.h"
#include "external/yyjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern context_t ctx;
extern settings_t settings;
extern char **python_scripts_array;
extern i32 python_scripts_count;

static char *load_file_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = (char *) malloc(size + 1);
    fread(text, 1, size, file);
    text[size] = '\0';
    fclose(file);
    return text;
}

void *async_loader(void *arg) {
    loader_data_t *ld = (loader_data_t *) arg;

    imvw_init_loaders();

    char *json_text = load_file_text(ASSETS_PATH"imvw.json");
    yyjson_doc *doc = json_text ? yyjson_read(json_text, strlen(json_text),
        YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS) : NULL;
    if (json_text) free(json_text);

    if (doc) {
        yyjson_val *root = yyjson_doc_get_root(doc);
        load_actions(root, &actions_array, &actions_count);
        load_settings(root, &settings);
        if (settings.python_scripting) {
            Enable_Python();
            load_python(root, &python_scripts_array, &python_scripts_count);
        }

        // Pre-load shaders into memory
        if (settings.shader_count > 0) {
            ctx.shader_fs_sources = (char **) calloc(settings.shader_count, sizeof(char *));
            ctx.shader_vs_sources = (char **) calloc(settings.shader_count, sizeof(char *));
            for (int i = 0; i < settings.shader_count; i++) {
                char full_path[PATH_MAX];
                if (settings.shaders[i]->fs_path) {
                    snprintf(full_path, sizeof(full_path), "%s%s", ASSETS_PATH, settings.shaders[i]->fs_path);
                    ctx.shader_fs_sources[i] = load_file_text(full_path);
                }
                if (settings.shaders[i]->vs_path) {
                    snprintf(full_path, sizeof(full_path), "%s%s", ASSETS_PATH, settings.shaders[i]->vs_path);
                    ctx.shader_vs_sources[i] = load_file_text(full_path);
                }
            }
        }

        // Pre-load font into memory
        char *font_path = settings.program_font_path;
        if (!font_path || !strlen(font_path)) {
            fprintf(stderr, "IMVW|FONT: no program_font_path in config \x97 falling back to assets\\segoeui.ttf\n");
            font_path = "assets\\segoeui.ttf";
        }
        FILE *f = fopen(font_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            ctx.font_data_size = (int) ftell(f);
            fseek(f, 0, SEEK_SET);
            ctx.font_data = (u8 *) malloc(ctx.font_data_size);
            fread(ctx.font_data, 1, ctx.font_data_size, f);
            fclose(f);
        } else {
            fprintf(stderr, "IMVW|FONT: ERROR \x97 could not open `%s` for font pre-load\n", font_path);
        }

        yyjson_doc_free(doc);
    }

    ld->config_ready = 1;

    // Determine initial image path to get dimensions
    char initial_path[MAX_PATH] = {0};
    if (ld->argc > 1) {
        for (int i = 1; i < ld->argc; i++) strcat(initial_path, ld->argv[i]);
    } else {
        strcpy(initial_path, ASSETS_PATH "test2.png");
    }

    int img_w, img_h, img_c;
    if (stbi_info(initial_path, &img_w, &img_h, &img_c)) {
        // Calculate aspect ratio and window size
        f32 aspect = (f32) img_w / (f32) img_h;
        f32 sw = settings.max_window_w;
        f32 sh = settings.max_window_h;
        f32 mw = settings.min_window_w;
        f32 mh = settings.min_window_h;

        f32 target_w = (f32) img_w;
        f32 target_h = (f32) img_h;

        if (target_w >= sw || target_h >= sh) {
            if (target_w >= target_h) {
                target_w = sw;
                target_h = sw / aspect;
            } else {
                target_h = sh;
                target_w = sh * aspect;
            }
        }
        if (target_w <= mw || target_h <= mh) {
            if (target_w >= target_h) {
                target_w = mw;
                target_h = mw / aspect;
            } else {
                target_h = mh;
                target_w = mh * aspect;
            }
        }
        ld->target_w = (int) target_w;
        ld->target_h = (int) target_h;
    } else {
        ld->target_w = 800;
        ld->target_h = 450;
    }
    ld->size_ready = 1;

    // Decode the full image while the main thread creates the window.
    // Write directly into ctx so the result survives even if the main thread
    // has already moved past the img_decoded check by the time we finish.
    // Guard: if the main thread already spawned its own decode (tex_loading),
    // skip to avoid overwriting its result mid-flight.
    if (!ctx.tex_loading && !ctx.img_ready_to_upload) {
        ctx.loading_img = imvw_load_image_extended(initial_path);
        if (ctx.loading_img.width == 0 || ctx.loading_img.data == NULL) {
            fprintf(stderr, "IMVW|LOADER: failed to decode `%s` \x97 using magenta dummy\n", initial_path);
            ctx.loading_img = GenImageColor(2, 2, MAGENTA);
        }
        ctx.img_ready_to_upload = 1;
    }
    ld->img_decoded = 1;

    return NULL;
}
