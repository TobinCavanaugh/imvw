#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include "external/stb_image.h"

#include "win_include.h"

#include <GLFW/glfw3.h>
#include <Python.h>

#include "external/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <external/miniaudio.h>

#include "raymath.h"
#include "dialect.h"
#include "flut.h"
#include "external/tinyfiledialogs.h"
#include "settings.h"
#include "actions_loader.h"
#include "settings_loader.h"
#include "python_loader.h"
#include "context_t.h"
#include "imvw_interface.h"
#include "font_loader.h"
#include "tex_loader.h"
#include "imvw_time.h"

#define nameof(a) #a

#define SCREEN_WIDTH (800)
#define SCREEN_HEIGHT (450)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define WINDOW_TITLE "IMVW"

#define CTRL_DOWN ( IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) )
#define ALT_DOWN ( IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) )
#define SHIFT_DOWN ( (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) )
#define SHIFT_FINE ( SHIFT_DOWN ? 0.5f : 1.0f )

key_action_t *actions_array = NULL;
i32 actions_count = 0;

char **python_scripts_array = NULL;
i32 python_scripts_count;

settings_t settings = {0};

context_t ctx = {
        .current_tex = {0},
        .tex_loading = 0,
        .tex_need_load = 0,
        .real_camera = (Camera2D) {
                .offset = (v2f) {0, 0},
                .target = (v2f) {0, 0},
                .rotation = 0.0f,
                .zoom = 1.0f
        },
        .current_path = "",
        .current_window_title = "",
        .shaders_loaded_arr = NULL,
        .shaders_count = 0,
        .shaders_custom_arr = NULL
};

context_t prev_ctx;

void *thread_playerinput(void *arg) {
    prev_ctx = ctx;
    ctx.one = 1;
    while (1) {
        f128 frame_start = getTimeHD_ms();
        {
            for (i32 i = 0; i < actions_count; i++) {
                key_action_t a = actions_array[i];
                u8 happened = 0;

                happened += (a.press != KEY_NULL && IsKeyPressed(a.press) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                happened += ((a.hold != KEY_NULL) && (IsKeyDown(a.hold)) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                happened += ((a.priv_use_mouse && IsMouseButtonDown(a.button)) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                if (happened) {
                    flut_func_t fft;
                    if (flut_get(a.func, &fft)) {
                        if (a.arg_type == ARG_TYPE_NONE) {
                            fft.func(NULL);
                        } else {
                            if (a.arg_type == ARG_TYPE_NUM) fft.func(&a.arg_num);
                            else if (a.arg_type == ARG_TYPE_BOOL) fft.func(&a.arg_bool);
                            else if (a.arg_type == ARG_TYPE_STR) fft.func(a.arg_str);
                            else if (a.arg_type == ARG_TYPE_OBJECT) fft.func(a.arg_obj);
                        }
                    } else {
                        fprintf(stderr, "Could not find function of name `%s`\n", a.func);
                    }
                }
            }
        }

        f128 frame_end = getTimeHD_ms();
        f128 elapsed = frame_end - frame_start;
        f32 target_frametime_ms = 16.67f;
        f32 sleeptime = target_frametime_ms - (f32) elapsed;

        if (sleeptime > 0) {
            sleep_ms(I32(sleeptime));
        }

        f128 total_elapsed = getTimeHD_ms() - frame_start;
        ctx.frame_time = (f32) (total_elapsed / 1000.0);
    }
    return NULL;
}

f32 get_system_font_size() {
    NONCLIENTMETRICSA metrics = {0};
    metrics.cbSize = sizeof(NONCLIENTMETRICSA);
    f32 fontsize = 6;
    if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        fontsize += -F32(metrics.lfMessageFont.lfHeight);
    }
    return fontsize;
}

f32 draw_properties(f32 properties_line, char *format, ...) {
    if (ctx.current_font.texture.height == 0) return 0;

    va_list args;
    va_start(args, format);
    static char properties_working[PATH_MAX];
    vsnprintf(properties_working, PATH_MAX, format, args);
    va_end(args);

    memmove(properties_working + 1, properties_working, strlen(properties_working) + 1);
    properties_working[0] = ' ';
    strcat(properties_working, " ");

    f32 font_size = get_system_font_size();

    BeginBlendMode(BLEND_MULTIPLIED);
    DrawRectangle(0, (i32) properties_line, (i32) MeasureTextEx(ctx.current_font, properties_working, font_size, 0).x,
                  (i32) font_size, settings.bg_color);
    EndBlendMode();

    DrawTextPro(ctx.current_font, properties_working, V2f(0, properties_line), V2f(0, 0), 0, font_size, 0, WHITE);
    return font_size;
}

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

    // 4. Load shaders from paths
    for (i32 i = 0; i < settings.shader_count; i++) {
        custom_shader_t *s = settings.shaders[i];

        // Copy metadata to context for UI/Toggle access
        ctx.shaders_custom_arr[i] = *s;

        if (s->fs_path) {
            char full_vs_path[PATH_MAX];
            char full_fs_path[PATH_MAX];

            // Construct paths; allow for NULL vertex shader (Raylib default)
            const char *vs = NULL;
            if (s->vs_path && strlen(s->vs_path) > 0) {
                snprintf(full_vs_path, sizeof(full_vs_path), "%s%s", ASSETS_PATH, s->vs_path);
                vs = full_vs_path;
            }
            snprintf(full_fs_path, sizeof(full_fs_path), "%s%s", ASSETS_PATH, s->fs_path);

            if (FileExists(full_fs_path)) {
                ctx.shaders_loaded_arr[ctx.shaders_count] = LoadShader(vs, full_fs_path);

                // Verify the shader compiled correctly
                if (ctx.shaders_loaded_arr[ctx.shaders_count].id != 0) {
                    printf("IMVW|LOG: Loaded custom shader [%d]: %s\n", ctx.shaders_count, s->name);
                    ctx.shaders_count++;
                } else {
                    fprintf(stderr, "IMVW|ERR: Shader compilation failed: %s\n", full_fs_path);
                }
            } else {
                fprintf(stderr, "IMVW|ERR: Shader file missing: %s\n", full_fs_path);
            }
        }
    }
}

u0 load_all() {
    char *settings_path = ASSETS_PATH"imvw.json";
    FILE *file = fopen(settings_path, "rb");
    if (!file) {
        fprintf(stderr, "Settings file `imvw.json` not found.\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_text = (char *) malloc(size + 1);
    fread(json_text, 1, size, file);
    json_text[size] = '\0';
    fclose(file);

    cJSON *json_data = cJSON_Parse(json_text);
    if (json_data == NULL) {
        fprintf(stderr, "Failed to parse `imvw.json`.\n");
        free(json_text);
        return;
    }

    load_actions(json_data, &actions_array, &actions_count);
    load_settings(json_data, &settings);

    if (settings.python_scripting) {
        Enable_Python();
        load_python(json_data, &python_scripts_array, &python_scripts_count);
    }

    cJSON_Delete(json_data);
    free(json_text);

    imvw_font_load();
    load_custom_shaders();
}

i32 main(i32 argc, char **argv) {
    HWND v = GetConsoleWindow();
    ShowWindow(v, SW_HIDE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    i64 start_time = timeGetTime();

    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);

    load_all();

    ctx.main_window = glfwGetCurrentContext();
    SetTargetFPS(settings.target_fps);

    // Registering functions
    flut_add(exit), flut_add(puts);
    flut_add(Camera_PanMouse);
    flut_add(Camera_Home_ResetZoom);
    flut_add(Camera_Home_NoResetZoom);
    flut_add(Reload);
    flut_add(Toggle_Trilinear_Filtering);
    flut_add(Rotate_By_Mouse);
    flut_add(Rotate_By_Scroll);
    flut_add(Copy_To_Clipboard);
    flut_add(Edit_Settings_Json);
    flut_add(Enable_Python);
    flut_add(Disable_Python);
    flut_add(Open_File_Dialog);
    flut_add(Open_Sibling);
    flut_add(Toggle_Properties);
    flut_add(Toggle_BG_Color);
    flut_add(Camera_ZoomHold);
    flut_add(Camera_ZoomAmt);
    flut_add(Camera_PanX);
    flut_add(Camera_PanY);
    flut_add(Shader_Toggle);

    ctx.target_camera = ctx.real_camera;

    ctx.default_wind_proc = GetWindowLongPtr((HWND) GetWindowHandle(), GWLP_WNDPROC);
    SetWindowLongPtrA((HWND) GetWindowHandle(), GWLP_WNDPROC, (i64) NewWindowProc);

    {
        HWND hwnd = (HWND) GetWindowHandle();
        HINSTANCE hInst = GetModuleHandle(NULL);
        HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
    }

    if (argc > 1) {
        char argv_path[MAX_PATH] = {0};
        for (int i = 1; i < argc; i++) strcat(argv_path, argv[i]);
        Load(argv_path);
    } else {
        Load(ASSETS_PATH "test2.png");
    }

    Camera_Home_ResetZoom();

    if (settings.python_scripting) {
        python_run_script_func(python_scripts_array, python_scripts_count, "start");
    }

    pthread_t pt;
    pthread_create(&pt, NULL, (thread_playerinput), NULL);

    printf("IMVW|LOG: startup_time_ms:%lld\n", timeGetTime() - start_time);

    while (!WindowShouldClose()) {
        if (settings.python_scripting) {
            python_run_script_func(python_scripts_array, python_scripts_count, "update");
        }

        ctx.mouse_pos = GetMousePosition();
        ctx.mouse_delta = GetMouseDelta();

        if (settings.maximized != IsWindowMaximized()) {
            Camera_Home_NoResetZoom();
            settings.maximized = IsWindowMaximized();
        }

        ctx.window_width = GetRenderWidth();
        ctx.window_height = GetRenderHeight();

        if (IsKeyPressed(KEY_F11) || (ALT_DOWN && IsKeyPressed(KEY_ENTER))) {
            if (IsWindowMaximized()) ToggleFullscreen();
            else {
                settings.undecorated = !settings.undecorated;
                settings.undecorated ? SetWindowState(FLAG_WINDOW_UNDECORATED) : ClearWindowState(
                        FLAG_WINDOW_UNDECORATED);
            }
        }

        if (IsWindowState(FLAG_WINDOW_TOPMOST) != settings.on_top) {
            settings.on_top ? SetWindowState(FLAG_WINDOW_TOPMOST) : ClearWindowState(FLAG_WINDOW_TOPMOST);
        }

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            SendMessage((HWND) GetWindowHandle(), 0x007B /*WM_CONTEXTMENU*/, (WPARAM) GetWindowHandle(), 0);
        }

        if (GetMouseWheelMove() && !ALT_DOWN) {
            Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), ctx.real_camera);
            ctx.target_camera.offset = GetMousePosition();
            ctx.target_camera.target = mwp;
            Camera_ZoomHold(GetMouseWheelMove() * GetFrameTime());
        }

        f32 dt = GetFrameTime();
        ctx.real_camera.offset = Vector2Lerp(ctx.real_camera.offset, ctx.target_camera.offset,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.target = Vector2Lerp(ctx.real_camera.target, ctx.target_camera.target,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.rotation = Lerp(ctx.real_camera.rotation, ctx.target_camera.rotation,
                                        dt * settings.lerpSpeed_rotate);
        ctx.real_camera.zoom = max(Lerp(ctx.real_camera.zoom, ctx.target_camera.zoom, dt * settings.lerpSpeed_zoom),
                                   0.001f);

        roundCamera2DValues(&ctx.real_camera, 0.001f);
        roundCamera2DValues(&ctx.target_camera, 0.001f);

        BeginDrawing();
        ClearBackground(ctx.use_alt_bg ? settings.bg_color_alt : settings.bg_color);

        BeginMode2D(ctx.real_camera);
        {
            if (ctx.tex_need_load && !ctx.tex_loading && !ctx.img_ready_to_upload) {
                imvw_tex_load();
            }

            if (ctx.img_ready_to_upload) {
                if (ctx.current_tex.id != 0) UnloadTexture(ctx.current_tex);
                ctx.current_tex = LoadTextureFromImage(ctx.loading_img);
                GenTextureMipmaps(&ctx.current_tex);
                SetTextureFilter(ctx.current_tex, settings.texture_filter);
                UnloadImage(ctx.loading_img);
                ctx.img_ready_to_upload = 0;
                ctx.tex_need_load = 0;
                ctx.tex_need_filter = 0;
                Camera_FitWindow();
                Camera_Home_Internal(true);
            }

            if (ctx.tex_need_filter && !ctx.tex_loading && !ctx.tex_need_load) {
                SetTextureFilter(ctx.current_tex, settings.texture_filter);
                ctx.tex_need_filter = 0;
            }

            if (ctx.current_tex.id != 0 && !ctx.tex_need_load && !ctx.tex_loading) {
                if (!settings.infinite_tile) {
                    DrawTexture(ctx.current_tex, -I32(round(ctx.current_tex.width / 2.0f)),
                                -I32(round(ctx.current_tex.height / 2.0f)), WHITE);
                } else {
                    v2f ul = GetScreenToWorld2D(V2f(0, 0), ctx.real_camera);
                    v2f br = GetScreenToWorld2D(V2f((f32) GetScreenWidth(), (f32) GetScreenHeight()), ctx.real_camera);
                    i32 start_x =
                            (i32) floor(ul.x / ctx.current_tex.width) * ctx.current_tex.width - ctx.current_tex.width;
                    i32 start_y = (i32) floor(ul.y / ctx.current_tex.height) * ctx.current_tex.height -
                                  ctx.current_tex.height;
                    i32 end_x =
                            (i32) ceil(br.x / ctx.current_tex.width) * ctx.current_tex.width + ctx.current_tex.width;
                    i32 end_y =
                            (i32) ceil(br.y / ctx.current_tex.height) * ctx.current_tex.height + ctx.current_tex.height;
                    for (i32 xx = start_x; xx < end_x; xx += ctx.current_tex.width) {
                        for (i32 yy = start_y; yy < end_y; yy += ctx.current_tex.height) {
                            DrawTexture(ctx.current_tex, xx + ctx.current_tex.width / 2 - ctx.current_tex.width / 2,
                                        yy + ctx.current_tex.height / 2 - ctx.current_tex.height / 2, WHITE);
                        }
                    }
                }
            }
        }
        EndMode2D();

        if (settings.properties_show && !ctx.tex_loading && ctx.current_tex.width > 0) {
            // Property line height
            f32 pl = 0;

            // Draw the image dimensions
            pl += draw_properties(pl, "%dx%d", ctx.current_tex.width, ctx.current_tex.height);

            // Draw the file size
            char buf[64 + 32];
            StrFormatByteSize64(ctx.tex_fsize, buf, sizeof(buf));
            pl += draw_properties(pl, "%s", buf);

            // Draw how many channels the image has
            pl += draw_properties(pl, "Channels: %d", ctx.tex_channels);

            // Display pixelformat
            pixel_format_to_str_s((PixelFormat) ctx.current_tex.format, buf, sizeof(buf));
            pl += draw_properties(pl, "Format: %s", buf);

            // Uggo
            pl += draw_properties(pl, "Filter: %s",
                                  settings.texture_filter == TEXTURE_FILTER_BILINEAR
                                  ? "Bilinear"
                                  : (settings.texture_filter == TEXTURE_FILTER_TRILINEAR
                                     ? "Trilinear"
                                     : "Point"));

            pl += draw_properties(pl, "%.2fms", ctx.frame_time * 1000.);
            pl += draw_properties(pl, "%.2ffps ", 1. / ctx.frame_time);
        }

        // --- Custom Shader Pass ---
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            custom_shader_t info = ctx.shaders_custom_arr[i];
            if(!info.enabled) continue;

            Shader shader = ctx.shaders_loaded_arr[i];
            // Standard Uniforms
            int resLoc = GetShaderLocation(shader, "screenResolution");
            int targetLoc = GetShaderLocation(shader, "cameraTarget");
            int offsetLoc = GetShaderLocation(shader, "cameraOffset");
            int zoomLoc = GetShaderLocation(shader, "cameraZoom");

            Vector2 res = {(float) GetScreenWidth(), (float) GetScreenHeight()};
            SetShaderValue(shader, resLoc, &res, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, targetLoc, &ctx.real_camera.target, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, offsetLoc, &ctx.real_camera.offset, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, zoomLoc, &ctx.real_camera.zoom, SHADER_UNIFORM_FLOAT);

            BeginShaderMode(shader);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
            EndShaderMode();
        }
        EndBlendMode();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
