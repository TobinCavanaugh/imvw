//
// Created by tobin on 2025-03-15.
//

#ifndef CONTEXT_T_H
#define CONTEXT_T_H

 #include <raylib.h>
#include "dialect.h"
#include <pthread.h>
#include "GLFW/glfw3.h"
#include "settings.h"

#define _Atomic
#include <stdatomic.h>

#include <math.h>
#include <string.h>

// #ifdef _WIN64
// typedef __int64 LONG_PTR;
// #else
//     typedef long LONG_PTR;
// #endif

typedef struct {
    i32 one;

    Texture2D current_tex;
    Image loading_img;
    _Atomic u8 img_ready_to_upload;

    _Atomic u8 tex_loading;
    _Atomic u8 tex_need_load;
    _Atomic u8 tex_need_filter;

    u8 tex_channels;
    i64 tex_fsize;

    v2f mouse_pos;
    v2f mouse_delta;

    f32 frame_time;

    Camera2D real_camera;
    Camera2D target_camera;

    i32 window_width, window_height;

    Shader *shaders_loaded_arr;
    custom_shader_t *shaders_custom_arr;
    i32 shaders_count;

    Font current_font;

    char **shader_fs_sources;
    char **shader_vs_sources;
    u8 *font_data;
    i32 font_data_size;

    i64 default_wind_proc;
    GLFWwindow *main_window;

    bool use_alt_bg;

    char current_path[PATH_MAX];
    char current_window_title[PATH_MAX];
} context_t;

static u0 roundCamera2DValues(Camera2D *camera, float epsilon) {
    if (fabs(camera->offset.x) <= epsilon) {
        camera->offset.x = 0.0f;
    }
    if (fabs(camera->offset.y) <= epsilon) {
        camera->offset.y = 0.0f;
    }
    if (fabs(camera->target.x) <= epsilon) {
        camera->target.x = 0.0f;
    }
    if (fabs(camera->target.y) <= epsilon) {
        camera->target.y = 0.0f;
    }
    if (fabs(camera->rotation) <= epsilon) {
        camera->rotation = 0.0f;
    }
    if (fabs(camera->zoom - 1.0f) <= epsilon) {
        // Assuming zoom should be around 1.0f
        camera->zoom = 1.0f;
    }
}

static u8 context_equals(const context_t *a, const context_t *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    if (!a->one || !b->one) {
        return 0;
    }

    // Compare Texture2D structures
    if (memcmp(&a->current_tex, &b->current_tex, sizeof(Texture2D)) != 0) return 0;

    // // Compare u8 fields
    if (a->tex_loading != b->tex_loading) return 0;
    if (a->tex_need_load != b->tex_need_load) return 0;
    if (a->tex_need_filter != b->tex_need_filter) return 0;
    if (a->tex_channels != b->tex_channels) return 0;
    if (a->tex_fsize != b->tex_fsize) return 0;

    // Compare v2f structures
    if (memcmp(&a->mouse_pos, &b->mouse_pos, sizeof(v2f)) != 0) return 0;
    if (memcmp(&a->mouse_delta, &b->mouse_delta, sizeof(v2f)) != 0) return 0;

    // Compare Camera2D structures
    if (memcmp(&a->real_camera, &b->real_camera, sizeof(Camera2D)) != 0) return 0;
    if (memcmp(&a->target_camera, &b->target_camera, sizeof(Camera2D)) != 0) return 0;
    //
    // // Compare Font structures
    // if (memcmp(&a->current_font, &b->current_font, sizeof(Font)) != 0) return 0;
    //
    // // Compare LONG_PTR fields
    // if (a->default_wind_proc != b->default_wind_proc) return 0;
    //
    // Compare string fields
    if (strcmp(a->current_path, b->current_path) != 0) return 0;
    if (strcmp(a->current_window_title, b->current_window_title) != 0) return 0;

    // If all checks passed, the contexts are equal
    return 1;
}


#endif //CONTEXT_T_H
