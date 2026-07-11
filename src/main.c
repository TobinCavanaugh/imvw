// imvw — Image Viewer
// Entry point.  All heavy lifting is delegated to domain modules.

#include "core/dialect.h"
#include "core/settings.h"
#include "core/context_t.h"
#include "input/actions.h"
#include "app/init.h"
#include "app/main_loop.h"

// ── Global state definitions ──
settings_t settings = {0};

context_t ctx = {
    .current_tex = {0},
    .tex_loading = 0,
    .tex_need_load = 0,
    .real_camera = {
        .offset = {0, 0},
        .target = {0, 0},
        .rotation = 0.0f,
        .zoom = 1.0f
    },
    .current_path = "",
    .current_window_title = "",
    .font_need_reload = 0,
    .shaders_need_reload = 0,
    .shaders_loaded_arr = NULL,
    .shaders_count = 0,
    .shaders_custom_arr = NULL,
    .active_bg_color_index = -1
};

char **python_scripts_array = NULL;
i32 python_scripts_count;

i32 main(i32 argc, char **argv) {
    imvw_init(argc, argv);
    imvw_main_loop();
    imvw_cleanup();
    return 0;
}
