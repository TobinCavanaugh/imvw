#ifndef SETTINGS_LOADER_H
#define SETTINGS_LOADER_H

#include "core/context_t.h"
#include "external/yyjson.h"
#include <stdlib.h>

extern context_t ctx;

u0 load_settings(yyjson_val *json, settings_t *out_settings) {
    yyjson_val *settings = yyjson_obj_get(json, "settings");
    if (settings) {
        yyjson_val *v;
        v = yyjson_obj_get(settings, "padding");
        if (v) out_settings->padding = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "on_top");
        if (v) out_settings->on_top = (u8) yyjson_get_bool(v);
        v = yyjson_obj_get(settings, "undecorated");
        if (v) out_settings->undecorated = (u8) yyjson_get_bool(v);
        v = yyjson_obj_get(settings, "maximized");
        if (v) out_settings->maximized = (u8) yyjson_get_bool(v);
        v = yyjson_obj_get(settings, "python_scripting");
        if (v) out_settings->python_scripting = (u8) yyjson_get_bool(v);
        v = yyjson_obj_get(settings, "infinite_tile");
        if (v) out_settings->infinite_tile = (u8) yyjson_get_bool(v);
        v = yyjson_obj_get(settings, "rotation_speed");
        if (v) out_settings->rotation_speed = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "zoom_speed");
        if (v) out_settings->zoom_speed = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "lerpSpeed_pan");
        if (v) out_settings->lerpSpeed_pan = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "lerpSpeed_zoom");
        if (v) out_settings->lerpSpeed_zoom = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "lerpSpeed_rotate");
        if (v) out_settings->lerpSpeed_rotate = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "max_window_w");
        if (v) out_settings->max_window_w = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "max_window_h");
        if (v) out_settings->max_window_h = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "min_window_w");
        if (v) out_settings->min_window_w = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "min_window_h");
        if (v) out_settings->min_window_h = (f32) yyjson_get_num(v);
        v = yyjson_obj_get(settings, "target_fps");
        if (v) out_settings->target_fps = (i32) yyjson_get_num(v);

        yyjson_val *filter_name = yyjson_obj_get(settings, "texture_filter");
        if (yyjson_is_str(filter_name)) {
            char fn[64];
            strcpy(fn, yyjson_get_str(filter_name));
            stoup(fn);
            if (strstr(fn, "P") || strstr(fn, "NONE"))
                out_settings->texture_filter = TEXTURE_FILTER_POINT;
            else if (strstr(fn, "TRI")) out_settings->texture_filter = TEXTURE_FILTER_TRILINEAR;
            else out_settings->texture_filter = TEXTURE_FILTER_BILINEAR;
        }

        ctx.tex_need_filter = 1;

        // Color Parsing
        yyjson_val *bg = yyjson_obj_get(settings, "bg_color");
        if (bg) {
            out_settings->bg_color = (Color) {
                    (u8) yyjson_get_num(yyjson_obj_get(bg, "r")),
                    (u8) yyjson_get_num(yyjson_obj_get(bg, "g")),
                    (u8) yyjson_get_num(yyjson_obj_get(bg, "b")),
                    (u8) yyjson_get_num(yyjson_obj_get(bg, "a"))
            };
        }

        // Parse named background colors from the "bg_colors" object.
        // Each key is a color name (e.g. "black", "white") and its value
        // is an { r, g, b, a } object.  Actions can reference these names
        // by passing the color name as a string argument.
        yyjson_val *bg_colors = yyjson_obj_get(settings, "bg_colors");
        if (yyjson_is_obj(bg_colors)) {
            out_settings->bg_color_count = 0;
            size_t idx, max;
            yyjson_val *key, *val;
            yyjson_obj_foreach(bg_colors, idx, max, key, val) {
                if (out_settings->bg_color_count >= MAX_BG_COLORS) break;
                strncpy(out_settings->bg_colors[out_settings->bg_color_count].name,
                        yyjson_get_str(key), BG_COLOR_NAME_MAX - 1);
                out_settings->bg_colors[out_settings->bg_color_count].color = (Color) {
                    (u8) yyjson_get_num(yyjson_obj_get(val, "r")),
                    (u8) yyjson_get_num(yyjson_obj_get(val, "g")),
                    (u8) yyjson_get_num(yyjson_obj_get(val, "b")),
                    (u8) yyjson_get_num(yyjson_obj_get(val, "a"))
                };
                out_settings->bg_color_count++;
            }
        }

        yyjson_val *fp = yyjson_obj_get(settings, "program_font_path");
        if (yyjson_is_str(fp)) {
            out_settings->program_font_path = strdup(yyjson_get_str(fp));
        }

        // SSAA scale factor (default 1.1 if unset or <= 0)
        yyjson_val *ss = yyjson_obj_get(settings, "ssaa_scale");
        f32 ss_val = ss ? (f32) yyjson_get_num(ss) : 0.0f;
        out_settings->ssaa_scale = (ss_val > 0.0f) ? ss_val : 1.1f;
    }

    // --- NEW: Shader Loading Logic ---
    yyjson_val *shaders_array = yyjson_obj_get(json, "shaders");
    if (yyjson_is_arr(shaders_array)) {
        out_settings->shader_count = (i32) yyjson_arr_size(shaders_array);

        // Allocate array of pointers to custom_shader_t
        out_settings->shaders = (custom_shader_t **) malloc(sizeof(custom_shader_t *) * out_settings->shader_count);

        for (int i = 0; i < out_settings->shader_count; i++) {
            yyjson_val *item = yyjson_arr_get(shaders_array, i);
            custom_shader_t *s = (custom_shader_t *) malloc(sizeof(custom_shader_t));

            // TODO rewrite this
            yyjson_val *path_val = yyjson_obj_get(item, "path");
            char *path = yyjson_is_str(path_val) ? (char*) yyjson_get_str(path_val) : NULL;
            s->fs_path = path ? strdup(path) : NULL;
            s->vs_path = NULL; // Default to NULL if not provided in JSON

            yyjson_val *enabled_val = yyjson_obj_get(item, "enabled");
            s->enabled = enabled_val ? (u8) yyjson_get_bool(enabled_val) : 0;

            s->name = strdup(yyjson_get_str(yyjson_obj_get(item, "name")));

            out_settings->shaders[i] = s;
        }
    } else {
        out_settings->shader_count = 0;
        out_settings->shaders = NULL;
    }

    // Existing actions check
    yyjson_val *actions = yyjson_obj_get(json, "actions");
    if (actions == NULL) {
        fprintf(stderr, "Failed to load `actions` from your `imvw.json` settings file.\n");
    }
}

#endif
