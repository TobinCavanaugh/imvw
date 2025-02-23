#include <windows.h>

#define Rectangle   RECTANGLE
#define CloseWindow WinCloseWindow
#define ShowCursor  WinShowCursor
#define LoadImage   WinLoadImage
#define PlaySound   WinPlaySound
#define DrawText    WinDrawText
#define DrawTextEx  WinDrawTextEx

#include "raylib.h"

// #define Rectangle RECTANGLE
#define CloseWindow RLCloseWindow
// #define CloseWindow() ({ rlglClose(); SendMessage(GetWindowHandle(), WM_CLOSE, 0, 0 ); })
#define ShowCursor  RLShowCursor
#define LoadImage   RLLoadImage
#define PlaySound   RLPlaySound
#define DrawText    RLDrawText
#define DrawTextEx  RLDrawTextEx
#include <commdlg.h>

// TODO make this generic or something

#include <Python.h>

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <external/stb_image_write.h>
#include <external/stb_image_write.h>

#include "raymath.h"
#include "GLFW/glfw3.h"

#include "dialect.h"
#include "flut.h"
#include "keyboard_key.h"
#include "tinyfiledialogs.h"
#include "settings.h"
#include "actions_loader.h"
#include "rlgl.h"
#include "settings_loader.h"
#include "GLFW/glfw3native.h"

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


/// Current fields TODO: Put into context struct
Texture2D current_texture = {0};
TextureFilter current_filter = TEXTURE_FILTER_TRILINEAR;
char current_path[PATH_MAX] = {0};
char current_window_title[PATH_MAX] = {0};

/// Cameras
Camera2D real_camera = (Camera2D){.target = V2f(0, 0), .offset = V2f(0, 0), .zoom = 1.0f, .rotation = 0.0f};
Camera2D target_camera;

/// Key actions
key_action_t *actions_array = NULL;
i32 actions_count = 0;

/// Settings
settings_t settings = {0};

// TODO: Add toggle help screen function


v2f CalculateWindowSize() {
    f32 aspect_ratio = (f32) current_texture.width / (f32) current_texture.height;

    f32 max_w = current_texture.width;
    f32 max_h = current_texture.height;

    // Maximum size
    f32 sw = (f32) (settings.max_window_w);
    f32 sh = (f32) (settings.max_window_h);
    // If its too large for the monitor
    if (current_texture.width >= sw || current_texture.height >= sh) {
        if (current_texture.width >= current_texture.height) {
            max_w = sw;
            max_h = sw * 1.0f / aspect_ratio;
        } else {
            max_h = sh;
            max_w = sh * aspect_ratio;
        }
    }

    // Minimum size
    f32 mw = (f32) (settings.min_window_w);
    f32 mh = (f32) (settings.min_window_h);
    // Handle minimum size case
    if (current_texture.width <= mw || current_texture.height <= mh) {
        if (current_texture.width >= current_texture.height) {
            max_w = mw;
            max_h = mw * 1.0f / aspect_ratio;
        } else {
            max_h = mh;
            max_w = mh * aspect_ratio;
        }
    }

    return V2f(max_w, max_h);
}

u8 IsDockedToMonitor(HWND hWnd) {
    WINDOWPLACEMENT placement = {sizeof(WINDOWPLACEMENT)};
    GetWindowPlacement(hWnd, &placement);
    RECT rc;
    GetWindowRect(hWnd, &rc);

    return placement.showCmd == SW_SHOWNORMAL
           && (rc.left != placement.rcNormalPosition.left ||
               rc.top != placement.rcNormalPosition.top ||
               rc.right != placement.rcNormalPosition.right ||
               rc.bottom != placement.rcNormalPosition.bottom);
}

u0 Camera_FitWindow() {
    if (!IsWindowMaximized() && !IsDockedToMonitor(GetWindowHandle())) {
        v2f size = CalculateWindowSize();
        SetWindowSize((i32) size.x, (i32) size.y);
    }
}

u0 Load(char *path) {
    strcpy(current_path, path);
    strcpy(current_window_title, "imgvw | ");
    strcat(current_window_title, current_path);

    if (current_texture.width != 0) {
        UnloadTexture(current_texture);
    }

    current_texture = LoadTexture(path);
    SetWindowTitle(current_window_title);
    GenTextureMipmaps(&current_texture);
    SetTextureFilter(current_texture, current_filter);

    //TODO MOVE LOGIC TO SECONDARY THREAD, PERFORM LOADING ON MAIN THREAD
    Camera_FitWindow();
}

u0 Reload() {
    Load(current_path);
}

u0 Rotate(f32 amount) {
    Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), real_camera);
    target_camera.offset = GetMousePosition();
    target_camera.target = mwp;
    target_camera.rotation += amount * GetFrameTime() * 125.0f * settings.rotation_speed * SHIFT_FINE;
}

u0 Rotate_By_Scroll() {
    Rotate(GetMouseWheelMove());
}

u0 Rotate_By_Mouse() {
    // Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), real_camera);
    // target_camera.offset = GetMousePosition();
    // target_camera.target = mwp;
    target_camera.rotation += GetMouseDelta().x / (f32) GetScreenWidth() * 360.0f * SHIFT_FINE * (
        GetFrameTime() * 100.0f * settings.rotation_speed);
}

u0 Camera_Home_Internal(u8 reset_zoom) {
    target_camera.offset = V2f(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f);
    target_camera.target = V2f(0, 0);
    target_camera.rotation = 0;

    if (!reset_zoom) {
        return;
    }

    // Perfectly zoom in
    f32 screen_width = GetScreenWidth();
    f32 screen_height = GetScreenHeight();

    f32 texture_width = (current_texture.width + settings.padding * 2.0f);
    f32 texture_height = (current_texture.height + settings.padding * 2.0f);

    f32 horizontal_zoom = screen_width / texture_width;
    f32 vertical_zoom = screen_height / texture_height;
    target_camera.zoom = min(horizontal_zoom, vertical_zoom);
}

u0 Camera_Home_NoResetZoom() {
    Camera_Home_Internal(false);
}

u0 Camera_Home_ResetZoom() {
    Camera_Home_Internal(true);
}


u0 Toggle_Trilinear_Filtering() {
    if (current_filter == TEXTURE_FILTER_TRILINEAR) {
        current_filter = TEXTURE_FILTER_POINT;
    } else {
        current_filter = TEXTURE_FILTER_TRILINEAR;
    }

    SetTextureFilter(current_texture, current_filter);
}

u0 Camera_Pan() {
    v2f d = GetMouseDelta();
    target_camera.offset.x += d.x * SHIFT_FINE;
    target_camera.offset.y += d.y * SHIFT_FINE;

    Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), target_camera);
    target_camera.offset = GetMousePosition();
    target_camera.target = mwp;
}

u0 Copy_To_Clipboard() {
    //TODO BROKEN
    fprintf(stderr, "Copy to clipboard not properly implemented.");
    return;
    OpenClipboard(GetWindowHandle());

    Image img = LoadImageFromTexture(current_texture);

    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }

    HBITMAP hbp = LoadBitmap(GetWindowHandle(), current_path);
    SetClipboardData(CF_BITMAP, &hbp);

    CloseClipboard();

    UnloadImage(img);
}

LONG_PTR default_wind_proc;

LRESULT CALLBACK NewWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CONTEXTMENU: {
            // Check if the right-click is on the title bar
            if ((HWND) wParam == hwnd) {
                // Create a context menu
                HMENU hMenu = CreatePopupMenu();

                AppendMenu(hMenu, settings.on_top ? MF_CHECKED : MF_UNCHECKED, 1, "Keep on top");
                AppendMenu(hMenu, settings.undecorated ? MF_CHECKED : MF_UNCHECKED, 2, "Undecorated");
                AppendMenu(hMenu, MF_STRING, 3, "Focus");
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

                // AppendMenu(hMenu, MF_STRING, SC_MOVE, "Move");

                AppendMenu(hMenu, MF_STRING, SC_MINIMIZE, "Minimize");

                if (IsWindowMaximized()) {
                    AppendMenu(hMenu, MF_STRING, SC_RESTORE, "Restore");
                } else {
                    AppendMenu(hMenu, MF_STRING, SC_MAXIMIZE, "Maximize");
                }
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, SC_CLOSE, "Close");

                // Show the context menu
                POINT pt;
                GetCursorPos(&pt);
                i32 result = TrackPopupMenu(hMenu, TPM_CENTERALIGN | TPM_HORPOSANIMATION | TPM_RETURNCMD, pt.x, pt.y, 0,
                                            hwnd, NULL);


                // If we dont select an option, fix the mouse delta still being
                // applied to the camera move. Doesn't work quite right if you
                // scroll, then right click, then click away. This could be
                // fixed by making a custom input struct that we can control
                // with more precision.
                if (result == 0) {
                    POINT p;
                    GetCursorPos(&p);

                    ScreenToClient(hwnd, &p);
                    SetMousePosition(p.x, p.y);
                    SetMousePosition(p.x, p.y);
                } else {
                    // TPM_RETURNCMD doesnt send the message, so we have to
                    SendMessage(GetWindowHandle(), WM_COMMAND, result, 0);
                }
                // Nothing selected
                DestroyMenu(hMenu);
            }
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1: // Keep on Top
                    settings.on_top = !settings.on_top;
                // settings.on_top ? SetWindowState(FLAG_WINDOW_TOPMOST) : ClearWindowState(FLAG_WINDOW_TOPMOST);
                    break;
                case 2: // Undecorate
                    settings.undecorated = !settings.undecorated;
                    break;
                case 3:
                    Camera_Home_ResetZoom();
                    break;
                case SC_MOVE:
                    SendMessage(GetWindowHandle(), WM_SYSCOMMAND, SC_MOVE, 0);
                    break;
                case SC_MINIMIZE:
                    SendMessage(GetWindowHandle(), WM_SYSCOMMAND, SC_MINIMIZE, 0);
                    break;
                case SC_RESTORE:
                    SendMessage(GetWindowHandle(), WM_SYSCOMMAND, SC_RESTORE, 0);
                    break;
                case SC_MAXIMIZE:
                    SendMessage(GetWindowHandle(), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                    break;
                case SC_CLOSE:
                    SendMessage(GetWindowHandle(), WM_SYSCOMMAND, SC_CLOSE, 0);
                    break;
                default:
                    printf("/////////");
                    break;
            }
            return 0;
        }
    }
    // Call the original window procedure for default processing
    // return DefWindowProc(hwnd, uMsg, wParam, lParam);
    return CallWindowProc(default_wind_proc, hwnd, uMsg, wParam, lParam);
}

//TODO add a shit load of logging features

u0 Edit_Settings_Json() {
    //TODO not perfect...
    ShellExecute(0, "open", ASSETS_PATH"imvw.json", 0, 0, SW_SHOWNORMAL);
}

//TODO add support for reloading from json in program runtime

int main(char argc, char **argv) {
    // Have no console window
    HWND v = GetConsoleWindow();
    ShowWindow(v, SW_HIDE);

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

    target_camera = real_camera;

    SetExitKey(KEY_NULL);

    // Create our new window
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(60);

    // Add our new window proc
    default_wind_proc = GetWindowLongPtr(GetWindowHandle(), GWLP_WNDPROC);
    SetWindowLongPtr(GetWindowHandle(),GWLP_WNDPROC, (LONG_PTR) NewWindowProc);

    // TODO relocate to other file {
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

    free(json_text);
    cJSON_free(json_data);

    //TODO }

    if (settings.python_scripting) {
        Py_Initialize();
        // PyRun_SimpleString("print('Hello from Python!')");
    }


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

    // Program loop
    while (!WindowShouldClose()) {
        /*Run through all of the key actions*/ {
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
                        fft.func(a.args);
                    } else {
                        fprintf(stderr, "Could not find function of name `%s`\n", a.func);
                    }
                }
            }
        }

        // Reset the zoom if the window is changing maximized state
        if (settings.maximized != IsWindowMaximized()) {
            Camera_Home_NoResetZoom();
            settings.maximized = IsWindowMaximized();
        }

        if (IsKeyPressed(KEY_O)) {
            char const *lFilterPatterns[2] = {"*.png", "*.jpg"};
            char *out = tinyfd_openFileDialog(
                "Select a PNG file",
                NULL,
                2,
                lFilterPatterns,
                "*.png|*.jpg",
                0);

            printf("[[%s]]", out);
            if (out != NULL && strlen(out) > 0) {
                Load(out);
                Camera_Home_ResetZoom();
            }
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

        if (IsKeyPressed(KEY_K)) {
            settings.on_top = !settings.on_top;
        }

        if (IsWindowState(FLAG_WINDOW_TOPMOST) != settings.on_top) {
            settings.on_top ? SetWindowState(FLAG_WINDOW_TOPMOST) : ClearWindowState(FLAG_WINDOW_TOPMOST);
        }
        if (IsWindowState(FLAG_WINDOW_UNDECORATED) != settings.undecorated) {
            settings.undecorated ? SetWindowState(FLAG_WINDOW_UNDECORATED) : ClearWindowState(FLAG_WINDOW_UNDECORATED);
        }

        if (IsKeyPressed(KEY_RIGHT)) {
            ++index;
            Load(TextFormat("%stest%d.png", ASSETS_PATH, index));
            Camera_Home_ResetZoom();
        }

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            SendMessage(GetWindowHandle(), WM_CONTEXTMENU, GetWindowHandle(), 0);

            goto RENDER;
        }

        if (IsKeyPressed(KEY_LEFT)) {
            --index;
            Load(TextFormat("%stest%d.png", ASSETS_PATH, index));
            Camera_Home_ResetZoom();
        }

        BeginDrawing();

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

        // if (IsWindowState(FLAG_WINDOW_UNDECORATED)) {
        //     POINT p;
        //     GetCursorPos(&p);
        //
        //     //~10 pixels epsilon
        //     RECT rec;
        //     GetWindowRect(GetWindowHandle(), &rec);
        //
        //     SetMouseCursor(MOUSE_CURSOR_ARROW);
        //
        //     //(1) bottom side
        //     if (abs(p.y - rec.top) < 10 && (p.x > rec.left && p.x < rec.right)) {
        //         SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
        //         //Resize horizontal
        //         is_resizing = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? (is_resizing | (1 << 1)) : 0;
        //     }
        //     //(2) right side
        //     if (abs(p.x - rec.right) < 10 && (p.y > rec.top && p.y < rec.bottom)) {
        //         SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        //         //Resize horizontal
        //         is_resizing = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? (is_resizing | (1 << 2)) : 0;
        //     }
        //     //(3) bottom side
        //     if (abs(p.y - rec.bottom) < 10 && (p.x > rec.left && p.x < rec.right)) {
        //         SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
        //         //Resize horizontal
        //         is_resizing = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? (is_resizing | (1 << 3)) : 0;
        //     }
        //     //(4) left side
        //     if (abs(p.x - rec.left) < 10 && (p.y > rec.top && p.y < rec.bottom)) {
        //         SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        //         //Resize horizontal
        //         is_resizing = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? (is_resizing | (1 << 4)) : 0;
        //     }
        // }


        // if (is_resizing) {
        //     if (is_resizing & (1 << 2)) {
        //         SetWindowSize(max(GetScreenWidth() + GetMouseDelta().x, settings.min_window_w), GetScreenHeight());
        //     }
        //     if (is_resizing & (1 << 3)) {
        //         SetWindowSize(GetScreenWidth(), max(GetScreenHeight() + GetMouseDelta().y, settings.min_window_h));
        //     }
        // }

        // Scroll to Zoom in
        if (GetMouseWheelMove() && !ALT_DOWN) {
            Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), real_camera);
            target_camera.offset = GetMousePosition();
            target_camera.target = mwp;

            // Zooming feels slow at small values
            // TODO this sucks
            target_camera.zoom += GetMouseWheelMove() * GetFrameTime() * SHIFT_FINE *
                    settings.zoom_speed *
                    (target_camera.zoom > 1 ? 2.0f : 1.0f) *
                    (target_camera.zoom > 2 ? 2.0f : 1.0f) *
                    (target_camera.zoom > 3 ? 2.0f : 1.0f) *
                    (target_camera.zoom > 4 ? 1.5f : 1.0f) *
                    (target_camera.zoom > 9 ? 1.5f : 1.0f) *
                    (target_camera.zoom > 50 ? 1.5f : 1.0f) *
                    (target_camera.zoom > 80 ? 2.0f : 1.0f) * 2.0f;

            f32 max_zoom_in = 100.0f / (max(current_texture.height, current_texture.width) + settings.padding * 2.0f);
            // f32 min_zoom_out = max(GetScreenHeight(), GetScreenWidth()) ;
            // f32 min_zoom_out = max(current_texture.width, current_texture.height);
            f32 min_zoom_out = 128;
            target_camera.zoom = max(min(target_camera.zoom, min_zoom_out), max_zoom_in);
            // printf("[[%.5f]]", target_camera.zoom);
        }

        /// Lerp camera fields
        f32 dt = GetFrameTime();
        real_camera.offset = Vector2Lerp(real_camera.offset, target_camera.offset, dt * settings.lerpSpeed_pan);
        real_camera.target = Vector2Lerp(real_camera.target, target_camera.target, dt * settings.lerpSpeed_pan);
        real_camera.rotation = Lerp(real_camera.rotation, target_camera.rotation, dt * settings.lerpSpeed_rotate);
        real_camera.zoom = max(Lerp(real_camera.zoom, target_camera.zoom, dt * settings.lerpSpeed_zoom), 0);

        /// Rendering
    RENDER:
        BeginMode2D(real_camera); {
            // ClearBackground((Color){0, 0, 0, 128});
            ClearBackground(settings.bg_color);
            DrawTexture(current_texture, -current_texture.width / 2.0f, -current_texture.height / 2.0f, WHITE);
        }
        EndDrawing();
    }

    return
            0;
}
