//
// Created by tobin on 2025-03-15.
//

#ifndef CONTEXT_T_H
#define CONTEXT_T_H

#include <raylib.h>
#include <dialect.h>
#include <pthread.h>

#ifdef _WIN64
typedef __int64 LONG_PTR;
#else
    typedef long LONG_PTR;
#endif

typedef struct {
    Texture2D current_tex;

    u8 tex_loading;
    u8 tex_need_load;
    u8 tex_need_filter;
    u8 tex_channels;
    i64 tex_fsize;

    v2f mouse_pos;
    v2f mouse_delta;

    f32 frame_time;

    Camera2D real_camera;
    Camera2D target_camera;

    Font current_font;

    LONG_PTR default_wind_proc;

    char current_path[PATH_MAX];
    char current_window_title[PATH_MAX];
} context_t;


#endif //CONTEXT_T_H
