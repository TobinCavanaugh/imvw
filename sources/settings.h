//
// Created by tobin on 2025-02-22.
//

#ifndef SETTINGS_H
#define SETTINGS_H

#include "dialect.h"

// TODO Allow custom shader args
typedef struct {
    char *name;

    char *fs_path;
    char *vs_path;
    u8 enabled;
} custom_shader_t;

typedef struct {
    f32 padding;
    u8 on_top;
    u8 undecorated;
    u8 maximized;

    f32 rotation_speed;
    f32 zoom_speed;

    u8 python_scripting;

    u8 infinite_tile;

    u8 properties_show; // TODO put in JSON
    char *program_font_path; // TODO put in JSON

    custom_shader_t **shaders;
    u32 shader_count;

    i32 target_fps;

    f32 lerpSpeed_pan;
    f32 lerpSpeed_zoom;
    f32 lerpSpeed_rotate;

    // TODO treat as %
    f32 max_window_w;
    f32 max_window_h;

    f32 min_window_w;
    f32 min_window_h;

    int texture_filter;

    Color bg_color;
    Color bg_color_alt;
} settings_t;

#endif //SETTINGS_H
