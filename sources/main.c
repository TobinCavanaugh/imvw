#include "raylib.h"

// #define Rectangle RECTANGLE
// #define CloseWindow RLCloseWindow
// #define CloseWindow() ({ rlglClose(); SendMessage(GetWindowHandle(), WM_CLOSE, 0, 0 ); })
#define ShowCursor  RLShowCursor
#define LoadImage   RLLoadImage
#define PlaySound   RLPlaySound
#define DrawText    RLDrawText
#define DrawTextEx  RLDrawTextEx

// TODO make this generic or something

#include <Python.h>

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "raymath.h"
#include "GLFW/glfw3.h"

#include "dialect.h"
#include "flut.h"
#include "keyboard_key.h"
#include "tinyfiledialogs.h"
#include "settings.h"
#include "actions_loader.h"
#include "settings_loader.h"
#include "python_loader.h"
#include "context_t.h"
#include "imvw_interface.h"

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

/// Key actions
key_action_t *actions_array = NULL;
i32 actions_count = 0;

/// Python Scripts
char **python_scripts_array = NULL;
i32 python_scripts_count;

/// Settings TODO implement defaults. Easiest to just json string line load
settings_t settings = {0};

context_t ctx = {
    .current_tex = {0},
    .tex_loading = 0,
    .tex_need_load = 0,
    .real_camera = (Camera2D){.target = (v2f){0, 0}, .offset = (v2f){0, 0}, .zoom = 1.0f, .rotation = 0.0f},
    .current_path = "",
    .current_window_title = ""
};

u0 thread_playerinput() {
    while (1) {
        /*Run through all of the key actions*/
    KEY_ACTIONS: {
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

        Sleep(16);
    }
}

// TODO: Add toggle help screen function

//TODO add support for reloading from json in program runtime
int main(char argc, char **argv) {
    SetExitKey(KEY_NULL);

    // Have no console window
    HWND v = GetConsoleWindow();
    ShowWindow(v, SW_HIDE);

    // Create our new window
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
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
    flut_add(printf);

    ctx.target_camera = ctx.real_camera;

    // Add our new window proc
    ctx.default_wind_proc = GetWindowLongPtr(GetWindowHandle(), GWLP_WNDPROC);
    SetWindowLongPtr(GetWindowHandle(), GWLP_WNDPROC, (LONG_PTR) NewWindowProc);

    // TODO relocate to other file. And other thread? {
    char *settings_path = ASSETS_PATH"imvw.json";
    if (!FileExists(settings_path)) {
        fprintf(stderr, "Settings file `imvw.json` not found... Generating a default one.\n");
        // TODO need a default settings file. Easiest just to string literal json file.
    }
    HANDLE file_watch = FindFirstChangeNotificationA(ASSETS_PATH"imvw.json", FALSE,
                                                     FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
    FILE *file = fopen(settings_path, "rb");

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Alloc all of this on the heap. Potential for issues here
    char *json_text = malloc(size + 1);
    if (json_text == NULL) {
        fprintf(stderr, "Failed to allocate memory buffer of size `%d` bytes. Your `imvw.json` file is too large!\n");
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

    free(json_text);
    cJSON_Delete(json_data);

    //TODO }

    if (argc > 1) {
        char argv_path[MAX_PATH] = {'\0'};

        i32 i = 1;
        while (i < argc) {
            strcat(argv_path, argv[i]);
            ++i;
        }

        Load(argv_path);
    } else {
        Load(ASSETS_PATH"test2.png");
    }

    Camera_Home_ResetZoom();

    i32 index = 0;

    if (settings.python_scripting) {
        python_run_script_func(python_scripts_array, python_scripts_count, "start");
    }

    pthread_t pt;
    pthread_create(&pt, NULL, thread_playerinput, NULL);

    // Program loop
    while (!WindowShouldClose()) {
        if (settings.python_scripting) {
            python_run_script_func(python_scripts_array, python_scripts_count, "update");
        }

        ctx.mouse_pos = GetMousePosition();
        ctx.mouse_delta = GetMouseDelta();

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
            settings.undecorated ? SetWindowState(FLAG_WINDOW_UNDECORATED) : ClearWindowState(FLAG_WINDOW_UNDECORATED);
        }

        // Have this navigate images
        if (IsKeyPressed(KEY_RIGHT)) {
            ++index;
            Load(TextFormat("%stest%d.png", ASSETS_PATH, index));
            Camera_Home_ResetZoom();
        }
        if (IsKeyPressed(KEY_LEFT)) {
            --index;
            Load(TextFormat("%stest%d.png", ASSETS_PATH, index));
            Camera_Home_ResetZoom();
        }

        // Right click thing
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            SendMessage(GetWindowHandle(), WM_CONTEXTMENU, GetWindowHandle(), 0);
            goto RENDER;
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

            f32 max_zoom_in = 100.0f / (max(ctx.current_tex.height, ctx.current_tex.width) + settings.padding * 2.0f);
            // f32 min_zoom_out = max(GetScreenHeight(), GetScreenWidth()) ;
            // f32 min_zoom_out = max(ctx.current_texture.width, ctx.current_texture.height);
            f32 min_zoom_out = 128;
            ctx.target_camera.zoom = max(min(ctx.target_camera.zoom, min_zoom_out), max_zoom_in);
            // printf("[[%.5f]]", ctx.target_camera.zoom);
        }

        /// Lerp camera fields
        f32 dt = GetFrameTime();
        ctx.real_camera.offset = Vector2Lerp(ctx.real_camera.offset, ctx.target_camera.offset,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.target = Vector2Lerp(ctx.real_camera.target, ctx.target_camera.target,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.rotation = Lerp(ctx.real_camera.rotation, ctx.target_camera.rotation,
                                        dt * settings.lerpSpeed_rotate);
        ctx.real_camera.zoom = max(Lerp(ctx.real_camera.zoom, ctx.target_camera.zoom, dt * settings.lerpSpeed_zoom), 0);


        /// Rendering
    RENDER:
        BeginDrawing();
        BeginMode2D(ctx.real_camera); {
            // Load image if needed
            if (ctx.tex_need_load) {
                ctx.tex_loading = 1;

                // Clear the background, and display it
                ClearBackground(settings.bg_color);
                EndDrawing();
                BeginDrawing();

                ctx.current_tex = LoadTexture(ctx.current_path);
                GenTextureMipmaps(&ctx.current_tex);
                ctx.tex_loading = 0, ctx.tex_need_load = 0;
            }

            // Set the filter
            if (ctx.tex_need_filter) {
                SetTextureFilter(ctx.current_tex, settings.texture_filter);
                ctx.tex_need_filter = 0;
            }

            // ClearBackground((Color){0, 0, 0, 128});
            ClearBackground(settings.bg_color);
            if (ctx.current_tex.height != 0 && !ctx.tex_need_load && !ctx.tex_loading) {
                DrawTexture(ctx.current_tex, -ctx.current_tex.width / 2.0f, -ctx.current_tex.height / 2.0f, WHITE);
            }
        }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
