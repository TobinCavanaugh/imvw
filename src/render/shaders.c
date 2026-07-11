#include "shaders.h"
#include "core/context_t.h"
#include "core/settings.h"
#include <stdlib.h>
#include <stdio.h>

extern context_t ctx;
extern settings_t settings;

void load_custom_shaders() {
    // 1. Cleanup existing shaders and memory
    if (ctx.shaders_loaded_arr != NULL) {
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            if (ctx.shaders_loaded_arr[i].id != 0) {
                UnloadShader(ctx.shaders_loaded_arr[i]);
            }
        }
        free(ctx.shaders_loaded_arr);
        ctx.shaders_loaded_arr = NULL;
    }

    if (ctx.shaders_custom_arr != NULL) {
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            custom_shader_t t = ctx.shaders_custom_arr[i];
            // NOTE: name/vs_path/fs_path are shallow-copied from
            // settings.shaders[i] during init, so we must NOT free them here
            // (they are owned by settings for the app's lifetime).
            (void)t;
        }
        free(ctx.shaders_custom_arr);
        ctx.shaders_custom_arr = NULL;
    }

    ctx.shaders_count = 0;

    // 2. Early exit if no shaders defined in settings
    if (settings.shader_count <= 0) return;

    // 3. Allocate arrays based on settings count
    ctx.shaders_loaded_arr = (Shader *) malloc(sizeof(Shader) * settings.shader_count);
    ctx.shaders_custom_arr = (custom_shader_t *) malloc(sizeof(custom_shader_t) * settings.shader_count);

    if (!ctx.shaders_loaded_arr || !ctx.shaders_custom_arr) {
        fprintf(stderr, "IMVW|ERR: Failed to allocate memory for shaders\n");
        return;
    }

    // 4. Load shaders from memory buffers if available
    for (i32 i = 0; i < settings.shader_count; i++) {
        custom_shader_t *s = settings.shaders[i];
        ctx.shaders_custom_arr[i] = *s;

        if (ctx.shader_fs_sources && ctx.shader_fs_sources[i]) {
            ctx.shaders_loaded_arr[ctx.shaders_count] = LoadShaderFromMemory(
                    (ctx.shader_vs_sources ? ctx.shader_vs_sources[i] : NULL),
                    ctx.shader_fs_sources[i]
            );

            if (ctx.shaders_loaded_arr[ctx.shaders_count].id != 0) {
                printf("IMVW|LOG: Loaded custom shader [%d]: %s (from memory)\n", ctx.shaders_count, s->name);
                ctx.shaders_count++;
            } else {
                fprintf(stderr, "IMVW|ERR: Shader compilation failed from memory: %s\n", s->name);
            }
        }
    }
}
