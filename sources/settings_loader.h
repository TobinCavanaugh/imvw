//
// Created by tobin on 2025-02-22.
//

#ifndef SETTINGS_LOADER_H
#define SETTINGS_LOADER_H
#include "context_t.h"

extern context_t ctx;

u0 load_settings(cJSON *json, settings_t *out_settings) {
    // Parse the settings
    cJSON *actions = cJSON_GetObjectItem(json, "actions");
    if (actions == NULL) {
        fprintf(stderr, "Failed to load `settings` from your `imvw.json` settings file.\n");
    }

    cJSON *settings = cJSON_GetObjectItem(json, "settings");

    //TODO these need error handling

    out_settings->padding = cJSON_GetNumberValue(cJSON_GetObjectItem(settings, "padding"));
    if (isnan(out_settings->padding)) {
        //error
    }

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

        if (strstr(filter_name, "P") != NULL || strstr(filter_name, "NONE") != NULL) {
            out_settings->texture_filter = TEXTURE_FILTER_POINT;
        }
        if (strstr(filter_name, "BI") != NULL) {
            out_settings->texture_filter = TEXTURE_FILTER_BILINEAR;
        }
        if (strstr(filter_name, "TRI") != NULL) {
            out_settings->texture_filter = TEXTURE_FILTER_TRILINEAR;
        }
    } else {
        out_settings->texture_filter = TEXTURE_FILTER_BILINEAR;
    }

    ctx.tex_need_filter = 1;


    out_settings->bg_color = ( {
        Color color = BLACK;

        cJSON *color_obj = cJSON_GetObjectItem(settings, "bg_color");
        color.r = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "r"));
        color.g = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "g"));
        color.b = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "b"));
        color.a = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "a"));

        /*return*/
        color;
    });


    out_settings->bg_color_alt = ( {
        Color color = BLACK;

        cJSON *color_obj = cJSON_GetObjectItem(settings, "bg_color_alt");
        color.r = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "r"));
        color.g = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "g"));
        color.b = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "b"));
        color.a = cJSON_GetNumberValue(cJSON_GetObjectItem(color_obj, "a"));

        /*return*/
        color;
    });
}
#endif //SETTINGS_LOADER_H
