// #define PLATFORM_DESKTOP_SDL 1

// #include "raylib.h"
// #define Rectangle RECTANGLE
// #define CloseWindow RLCloseWindow
// #define CloseWindow() ({ rlglClose(); SendMessage((HWND) GetWindowHandle(), WM_CLOSE, 0, 0 ); })
// #define ShowCursor  RLShowCursor
// #define LoadImage   RLLoadImage
// #define PlaySound   RLPlaySound
// #define DrawText    RLDrawText
// #define DrawTextEx  RLDrawTextEx

//// --- Windows / Raylib compatibility block ---
//#define CloseWindow Win32_CloseWindow
//#define Rectangle Win32_Rectangle
//#define ShowCursor Win32_ShowCursor
//
//#include <windows.h>
//#include <shellapi.h> // For ShellExecute
//#include <shlwapi.h>  // For StrFormatByteSize64
//#include <shlobj.h>   // For SHGetFolderPathA
//
//// Undefine Windows macros that conflict with Raylib
//#undef CloseWindow
//#undef Rectangle
//#undef ShowCursor
//#undef LoadImage
//#undef DrawText
//#undef DrawTextEx
//#undef PlaySound
//// --------------------------------------------

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
#include "GLFW/glfw3.h"

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

#define WINDOW_TITLE "Window title"

#define CTRL_DOWN ( IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) )
#define ALT_DOWN ( IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) )
#define SHIFT_DOWN ( (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) )
#define SHIFT_FINE ( SHIFT_DOWN ? 0.5f : 1.0f )

//namespace IMVW {
/// Key actions
key_action_t *actions_array = NULL;
i32 actions_count = 0;

/// Python Scripts
char **python_scripts_array = NULL;
i32 python_scripts_count;

/// Settings TODO implement defaults. Easiest to just json string line load
/// TODO: Implement arguments for opening with values, i.e. --on_top 1
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
        .current_window_title = ""
};


context_t prev_ctx;


void* thread_playerinput(void*arg) {
    prev_ctx = ctx;
    ctx.one = 1;
    while (1) {
        f128 frame_start = getTimeHD_ms();
        /*Run through all of the key actions*/
        KEY_ACTIONS:
        {
            i32 i = 0;
            for (; i < actions_count; i++) {
                key_action_t a = actions_array[i];
                u8 happened = 0;

                // Press key
                happened += (a.press != KEY_NULL && IsKeyPressed(a.press) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                // Hold key
                happened += ((a.hold != KEY_NULL) && (IsKeyDown(a.hold)) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                // Mouse buttons
                happened += ((a.priv_use_mouse && IsMouseButtonDown(a.button)) &&
                             (a.modifier == KEY_NULL || IsKeyDown(a.modifier)));

                if (happened) {
                    flut_func_t fft;
                    if (flut_get(a.func, &fft)) {
                        // Default case
                        if (a.arg_type == ARG_TYPE_NONE) {
                            fft.func(NULL);
                        } else {
                            a.arg_type == ARG_TYPE_NUM ? fft.func(&a.arg_num) : NO_OP;
                            a.arg_type == ARG_TYPE_BOOL ? fft.func(&a.arg_bool) : NO_OP;
                            a.arg_type == ARG_TYPE_STR ? fft.func(a.arg_str) : NO_OP;
                            a.arg_type == ARG_TYPE_OBJECT ? fft.func(a.arg_obj) : NO_OP;
                        }
                    } else {
                        fprintf(stderr, "Could not find function of name `%s`\n", a.func);
                    }
                }
            }
        }

        // Sleep for time to ensure input framerate doesn't exceed 60fps
        f128 frame_end = getTimeHD_ms();
        f128 elapsed = frame_end - frame_start;

        //TODO add config for target framerate
        //TODO fix framerate independence issues in imvw_interface.h

        f32 target_frametime_ms = 16.67f; // More precise for 60fps
        f32 sleeptime = target_frametime_ms - elapsed;

        if (sleeptime > 0) {
            sleep_ms(I32(sleeptime));
        }

        f128 total_elapsed = getTimeHD_ms() - frame_start;
        ctx.frame_time = F128(total_elapsed) / 1000.;
    }

    return NULL;
}

f32 get_system_font_size() {
    NONCLIENTMETRICSA metrics = {0};
    metrics.cbSize = sizeof(NONCLIENTMETRICSA);

    // This is guesstimation, but decent
    f32 fontsize = 6;
    if (SystemParametersInfoA(0x0029/*SPI_GETNONCLIENTMETRICS*/, metrics.cbSize, &metrics, 0)) {
        fontsize += -F32(metrics.lfMessageFont.lfHeight);
    }

    return fontsize;
}


f32 draw_properties(f32 properties_line, char *format, ...) {
    if (ctx.current_font.texture.height == 0) {
        return 0;
    }

    // Get our varargs
    va_list args;
    va_start(args, format);

    // PATH_MAX is a reasonable max length
    static char properties_working[PATH_MAX];
    vsnprintf(properties_working, PATH_MAX, format, args);

    // Add a space before the string to improve left padding
    memmove(properties_working + 1, properties_working, strlen(properties_working) + 1);
    properties_working[0] = ' ';

    // Add space after
    strcat(properties_working, " ");

    f32 font_size = get_system_font_size();

    // Draw boxes behind the text
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawRectangle(0, properties_line, (i32) MeasureTextEx(ctx.current_font, properties_working, font_size, 0).x,
                  font_size, settings.bg_color);
    EndBlendMode();

    // Draw the actual text
    DrawTextPro(ctx.current_font, properties_working, V2f(0, properties_line), V2f(0, 0), 0, font_size, 0, WHITE);

    return font_size;
}

u0 load_all() {
    //INFO: Setting this up to load asynchronously will not help our boot times.
    // the core issue with slow startups (~300ms) is due to some Shintel iris
    // drivers issue. Particularly the ChoosePixelFormat function, which takes
    // 1.2s (SECONDS!!) when profiled with VTune.
    char *settings_path = ASSETS_PATH"imvw.json";
    if (!FileExists(settings_path)) {
        fprintf(stderr, "Settings file `imvw.json` not found... Generating a default one.\n");
        // TODO need a default settings file. Easiest just to string literal json file.
    }
    FILE *file = fopen(settings_path, "rb");

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Alloc all of this on the heap. Potential for issues here
    char *json_text = (char *) malloc(size + 1);
    if (json_text == NULL) {
        fprintf(
                stderr,
                "Failed to allocate memory buffer of size `%llu` bytes. Your `imvw.json` file is too large!\n",
                size + 1);
        exit(1);
    }
    // Load all the text in
    fread(json_text, 1, size, file);

    // Parse the json
    cJSON *json_data = cJSON_Parse(json_text);
    if (json_data == NULL) {
        fprintf(stderr, "Failed to parse `imvw.json`. cJSON error here: \n`\n%s\n`\n", cJSON_GetErrorPtr());
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
}

// TODO: Add toggle help screen function
// TODO: add support for reloading from json in program runtime
// TODO: Pixel grid support
// TODO: Construction lines background or something
// TODO: Better zooming
int main(char argc, char **argv) {
    // Have no console window
    HWND v = GetConsoleWindow();
    ShowWindow(v, 0 /*SW_HIDE*/);

    i64 start_time = timeGetTime();

    // Create our new window
    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    // SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);

    load_all();

    ctx.main_window = glfwGetCurrentContext();

    SetTargetFPS(60);

    flut_add(exit), flut_add(puts);
    flut_add(Camera_Pan);
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
    flut_add(printf);
    flut_add(Toggle_Properties);

    ctx.target_camera = ctx.real_camera;

    // Add our new window proc
    //TODO pot err
    ctx.default_wind_proc = GetWindowLongPtr((HWND) (HWND) GetWindowHandle(), -4/*GWLP_WNDPROC*/);
    SetWindowLongPtrA((HWND) (HWND) GetWindowHandle(), -4/*GWLP_WNDPROC*/, (i64) NewWindowProc);

    Texture2D bg = LoadTexture(ASSETS_PATH "construction.png");

    // Load default file
    if (argc > 1) {
        char argv_path[MAX_PATH] = {'\0'};

        i32 i = 1;
        while (i < argc) {
            strcat(argv_path, argv[i]);
            ++i;
        }

        Load(argv_path);
    } else {
        Load(ASSETS_PATH
             "test2.png");
    }

    Camera_Home_ResetZoom();

    if (settings.python_scripting) {
        python_run_script_func(python_scripts_array, python_scripts_count, "start");
    }

    pthread_t pt;
    pthread_create(&pt, NULL, (thread_playerinput), NULL);

    printf("IMVW|LOG: startup_time_ms:%lld\n", timeGetTime() - start_time);

    HRESULT result;
    f32 nearest;
    f32 dt;

    // We do a pre-render step so the window never flashes white
    goto RENDER;

    // Program loop
    while (!WindowShouldClose()) {
        if (settings.python_scripting) {
            python_run_script_func(python_scripts_array, python_scripts_count, "update");
        }

        ctx.mouse_pos = GetMousePosition();
        ctx.mouse_delta = GetMouseDelta(); //TODO this doesnt update out of window

        // TODO Place in thread to auto reload settings etc.
        // DWORD result;
        // result = WaitForSingleObject(file_watch, INFINITE);
        // if (WAIT_OBJECT_0 != result) {
        //     printf("XXX");
        // } else {
        //     printf("yYyy");
        // }

        // Reset the zoom if the window is changing maximized state
        if (settings.maximized != IsWindowMaximized()) {
            Camera_Home_NoResetZoom();
            settings.maximized = IsWindowMaximized();
        }

        // Maximize / hide border
        if (IsKeyPressed(KEY_F11) || (ALT_DOWN && IsKeyPressed(KEY_ENTER))) {
            if (IsWindowMaximized()) {
                ToggleFullscreen();
            } else {
                settings.undecorated = !settings.undecorated;
                settings.undecorated
                ? SetWindowState(FLAG_WINDOW_UNDECORATED)
                : ClearWindowState(FLAG_WINDOW_UNDECORATED);
            }
        }

        // Update window states
        if (IsWindowState(FLAG_WINDOW_TOPMOST) != settings.on_top) {
            settings.on_top ? SetWindowState(FLAG_WINDOW_TOPMOST) : ClearWindowState(FLAG_WINDOW_TOPMOST);
        }
        if (IsWindowState(FLAG_WINDOW_UNDECORATED) != settings.undecorated) {
            settings.undecorated
            ? SetWindowState(FLAG_WINDOW_UNDECORATED)
            : ClearWindowState(FLAG_WINDOW_UNDECORATED);
        }

        // Right click thing
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            SendMessage((HWND) (HWND) GetWindowHandle(), 0x007B /*WM_CONTEXTMENU*/,
                        (WPARAM) (HWND) GetWindowHandle(), 0);
            goto RENDER;
        }

        char appDataPath[MAX_PATH];
        result = SHGetFolderPathA(NULL, 0x001a/*Appdata*/ , NULL, 0, appDataPath);
        if (SUCCEEDED(result)) {
            strcat(appDataPath, "\\imvw\\");
            CreateDirectoryA(appDataPath, NULL);
            strcat(appDataPath, "cache.png");

            if (!FileExists(appDataPath) && ctx.current_tex.id) {
                Image img = LoadImageFromTexture(ctx.current_tex);
                ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
                ImageResize(&img, max(img.width / 2, 512), max(img.width / 2, 512));
                ExportImage(img, appDataPath);
            }
        }

        // Pan window TODO FIX
        if ((IsMouseButtonDown(0) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) && ALT_DOWN && !CTRL_DOWN) {
            v2f pos = GetWindowPosition();

            v2f d = GetMouseDelta();
            pos.x += d.x * SHIFT_FINE * 0.75f;
            pos.y += d.y * SHIFT_FINE * 0.75f;
            if (!IsWindowMaximized()) {
                SetWindowPosition((i32) round(pos.x), (i32) round(pos.y));
            }
        }

        // Scroll to Zoom in
        if (GetMouseWheelMove() && !ALT_DOWN) {
            Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), ctx.real_camera);
            ctx.target_camera.offset = GetMousePosition();
            ctx.target_camera.target = mwp;

            // Zooming feels slow at small values
            // TODO this sucks
            ctx.target_camera.zoom += GetMouseWheelMove() * GetFrameTime() * SHIFT_FINE *
                                      settings.zoom_speed *
                                      (ctx.target_camera.zoom > 1 ? 2.0f : 1.0f) *
                                      (ctx.target_camera.zoom > 2 ? 2.0f : 1.0f) *
                                      (ctx.target_camera.zoom > 3 ? 2.0f : 1.0f) *
                                      (ctx.target_camera.zoom > 4 ? 1.5f : 1.0f) *
                                      (ctx.target_camera.zoom > 9 ? 1.5f : 1.0f) *
                                      (ctx.target_camera.zoom > 50 ? 1.5f : 1.0f) *
                                      (ctx.target_camera.zoom > 80 ? 2.0f : 1.0f) * 2.0f;

            f32 max_zoom_in =
                    100.0f / (max(ctx.current_tex.height, ctx.current_tex.width) + settings.padding * 2.0f);
            // f32 min_zoom_out = max(GetScreenHeight(), GetScreenWidth()) ;
            // f32 min_zoom_out = max(ctx.current_texture.width, ctx.current_texture.height);
            f32 min_zoom_out = 128;
            ctx.target_camera.zoom = max(min(ctx.target_camera.zoom, min_zoom_out), max_zoom_in);
            // printf("[[%.5f]]", ctx.target_camera.zoom);
        }


        // If we are very close to a 360 degree interval, set it to 0
        nearest = roundf(ctx.real_camera.rotation / 360.f) * 360.f;
        if (fabs(ctx.real_camera.rotation - nearest) < EPSILON) {
            ctx.real_camera.rotation = 0;
        }

        /// Lerp camera fields TODO: Move this to other thread maybe
        dt = GetFrameTime();
        ctx.real_camera.offset = Vector2Lerp(ctx.real_camera.offset, ctx.target_camera.offset,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.target = Vector2Lerp(ctx.real_camera.target, ctx.target_camera.target,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.rotation = Lerp(ctx.real_camera.rotation, ctx.target_camera.rotation,
                                        dt * settings.lerpSpeed_rotate);
        ctx.real_camera.zoom = max(Lerp(ctx.real_camera.zoom, ctx.target_camera.zoom, dt * settings.lerpSpeed_zoom),
                                   0);

        roundCamera2DValues(&ctx.real_camera, 0.001f);
        roundCamera2DValues(&ctx.target_camera, 0.001f);

        /// Rendering
        RENDER:
        BeginDrawing();
        BeginMode2D(ctx.real_camera);
        {
            // Load image if needed
            if (ctx.tex_need_load && !ctx.tex_loading) {
                // Unload texture time is near 0ms
                if (ctx.current_tex.width != 0) {
                    UnloadTexture(ctx.current_tex);
                }

                // Clear the background, and display it
                ClearBackground(settings.bg_color);
                EndDrawing();
                BeginDrawing();

                // Load the image here
                imvw_tex_load();

                Camera_FitWindow();
                Camera_Home_Internal(true);
            }

            // Set the filter
            if (ctx.tex_need_filter && !ctx.tex_loading && !ctx.tex_need_load) {
                SetTextureFilter(ctx.current_tex, settings.texture_filter);
                ctx.tex_need_filter = 0;
            }

            ClearBackground(settings.bg_color);


            // // TODO construction lines
            // // Get the bounds of the visible area in world coordinates
            // v2f ul = GetScreenToWorld2D(V2f(0, 0), ctx.real_camera);
            // v2f br = GetScreenToWorld2D(V2f(GetScreenWidth(), GetScreenHeight()), ctx.real_camera);
            //
            // i32 startX = (i32) (floor(ul.x / bg.width) * bg.width);
            // i32 startY = (i32) (floor(ul.y / bg.height) * bg.height);
            //
            // // Calculate the ending grid cell
            // i32 endX = (i32) (ceil(br.x / bg.width) * bg.width);
            // i32 endY = (i32) (ceil(br.y / bg.height) * bg.height);
            //
            // // Draw only the visible grid cells
            // for (i32 x = startX; x < endX; x += bg.width) {
            //     for (i32 y = startY; y < endY; y += bg.height) {
            //         DrawTexture(bg, x, y, WHITE);
            //     }
            // }

            // Draw the main texture
            if (ctx.current_tex.height != 0 && !ctx.tex_need_load && !ctx.tex_loading) {
                if (!settings.infinite_tile) {
                    // Draw the texture regularly
                    DrawTexture(ctx.current_tex,
                                -I32(round(ctx.current_tex.width / 2.0f)),
                                -I32(round(ctx.current_tex.height / 2.0f)), WHITE);
                } else {
                    // Get the visible area in world coordinates
                    v2f ul = GetScreenToWorld2D(V2f(0, 0), ctx.real_camera);
                    v2f br = GetScreenToWorld2D(V2f((f32) GetScreenWidth(), (f32) GetScreenHeight()), ctx.real_camera);

                    // Calculate the starting position (floor to get complete tiles)
                    i32 start_x = (i32) floor(ul.x / ctx.current_tex.width) * ctx.current_tex.width
                                  - ctx.current_tex.width;
                    i32 start_y = (i32) floor(ul.y / ctx.current_tex.height) * ctx.current_tex.height
                                  - ctx.current_tex.height;

                    // Calculate the ending position (ceil to include partial tiles)
                    i32 end_x = (i32) ceil(br.x / ctx.current_tex.width) * ctx.current_tex.width
                                + ctx.current_tex.width;
                    i32 end_y = (i32) ceil(br.y / ctx.current_tex.height) * ctx.current_tex.height
                                + ctx.current_tex.height;

                    // Draw only the tiles visible on screen
                    for (i32 xx = start_x; xx < end_x; xx += ctx.current_tex.width) {
                        for (i32 yy = start_y; yy < end_y; yy += ctx.current_tex.height) {
                            DrawTexture(ctx.current_tex,
                                        xx + ctx.current_tex.width - ctx.current_tex.width / 2.f,
                                        yy + ctx.current_tex.height - ctx.current_tex.height / 2.f,
                                        WHITE);
                        }
                    }
                }
            }
        }
        EndMode2D();

        // Draw image properties
        if (settings.properties_show && !ctx.tex_loading && ctx.current_tex.width > 0) {
            //TODO make this configurable or implemented in python mayhaps

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

        EndDrawing();

        END_OF_FRAME:


    }

    CloseWindow();

    return 0;
}
//}
