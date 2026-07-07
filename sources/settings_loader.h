#ifndef SETTINGS_LOADER_H
#define SETTINGS_LOADER_H

#include "context_t.h"
#include <stdlib.h>

extern context_t ctx;

u0 load_settings(cJSON *json, settings_t *out_settings) {
    cJSON *settings = cJSON_GetObjectItem(json, "settings");
    if (settings) {
        out_settings->padding = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "padding"));
        out_settings->on_top = cJSON_IsTrue(cJSON_GetObjectItem(settings, "on_top"));
        out_settings->undecorated = cJSON_IsTrue(cJSON_GetObjectItem(settings, "undecorated"));
        out_settings->maximized = cJSON_IsTrue(cJSON_GetObjectItem(settings, "maximized"));
        out_settings->python_scripting = cJSON_IsTrue(cJSON_GetObjectItem(settings, "python_scripting"));
        out_settings->infinite_tile = cJSON_IsTrue(cJSON_GetObjectItem(settings, "infinite_tile"));
        out_settings->rotation_speed = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "rotation_speed"));
        out_settings->zoom_speed = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "zoom_speed"));
        out_settings->lerpSpeed_pan = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "lerpSpeed_pan"));
        out_settings->lerpSpeed_zoom = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "lerpSpeed_zoom"));
        out_settings->lerpSpeed_rotate = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "lerpSpeed_rotate"));
        out_settings->max_window_w = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "max_window_w"));
        out_settings->max_window_h = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "max_window_h"));
        out_settings->min_window_w = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "min_window_w"));
        out_settings->min_window_h = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "min_window_h"));
        out_settings->target_fps = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "target_fps"));

        char *filter_name = cJSON_GetStringValue(cJSON_GetObjectItem(settings, "texture_filter"));
        if (filter_name) {
            stoup(filter_name);
            if (strstr(filter_name, "P") || strstr(filter_name, "NONE"))
                out_settings->texture_filter = TEXTURE_FILTER_POINT;
            else if (strstr(filter_name, "TRI")) out_settings->texture_filter = TEXTURE_FILTER_TRILINEAR;
            else out_settings->texture_filter = TEXTURE_FILTER_BILINEAR;
        }

        ctx.tex_need_filter = 1;

        // Color Parsing
        cJSON *bg = cJSON_GetObjectItem(settings, "bg_color");
        if (bg) {
            out_settings->bg_color = (Color) {
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg, "r")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg, "g")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg, "b")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg, "a"))
            };
        }

        cJSON *bg_alt = cJSON_GetObjectItem(settings, "bg_color_alt");
        if (bg_alt) {
            out_settings->bg_color_alt = (Color) {
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg_alt, "r")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg_alt, "g")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg_alt, "b")),
                    (u8) cJSON_GetNumberValue(cJSON_GetObjectItem(bg_alt, "a"))
            };
        }

        cJSON *fp = cJSON_GetObjectItem(settings, "program_font_path");
        if (cJSON_IsString(fp)) {
            out_settings->program_font_path = strdup(cJSON_GetStringValue(fp));
        }
    }

    // --- NEW: Shader Loading Logic ---
    cJSON *shaders_array = cJSON_GetObjectItem(json, "shaders");
    if (cJSON_IsArray(shaders_array)) {
        out_settings->shader_count = cJSON_GetArraySize(shaders_array);

        // Allocate array of pointers to custom_shader_t
        out_settings->shaders = (custom_shader_t **) malloc(sizeof(custom_shader_t *) * out_settings->shader_count);

        for (int i = 0; i < out_settings->shader_count; i++) {
            cJSON *item = cJSON_GetArrayItem(shaders_array, i);
            custom_shader_t *s = (custom_shader_t *) malloc(sizeof(custom_shader_t));

            // TODO rewrite this
            char *path = cJSON_GetStringValue(cJSON_GetObjectItem(item, "path"));
            s->fs_path = path ? strdup(path) : NULL;
            s->vs_path = NULL; // Default to NULL if not provided in JSON

            s->enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"));

            s->name = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")));

            out_settings->shaders[i] = s;
        }
    } else {
        out_settings->shader_count = 0;
        out_settings->shaders = NULL;
    }

    // Existing actions check
    cJSON *actions = cJSON_GetObjectItem(json, "actions");
    if (actions == NULL) {
        fprintf(stderr, "Failed to load `actions` from your `imvw.json` settings file.\n");
    }
}

#endif
