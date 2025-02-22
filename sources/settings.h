//
// Created by tobin on 2025-02-22.
//

#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct {
    f32 padding;
    u8 on_top;
    u8 undecorated;

    f32 rotation_speed;
    f32 zoom_speed;

    f32 lerpSpeed_pan;
    f32 lerpSpeed_zoom;
    f32 lerpSpeed_rotate;

    // TODO treat as %
    f32 max_window_w;
    f32 max_window_h;

    f32 min_window_w;
    f32 min_window_h;

    Color bg_color;
} settings_t;

#endif //SETTINGS_H
