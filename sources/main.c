
#include "external/stb_image.h"

#include "win_include.h"

// GLFW removed (tr_raylib uses Win32)
#include <Python.h>

#include "external/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <trlib.h>
#include "pthread_win32.h"
// #include <external/miniaudio.h> // not needed

// raymath.h replaced by tr_raylib.h
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
#include "ssaa.h"

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
        .real_camera = {
                .offset = {0, 0},
                .target = {0, 0},
                .rotation = 0.0f,
                .zoom = 1.0f
        },
        .current_path = "",
        .current_window_title = "",
        .shaders_loaded_arr = NULL,
        .shaders_count = 0,
        .shaders_custom_arr = NULL
};
// Pre-fetch modifier key states once per frame so per-action checks use the
// cached values instead of calling IsKeyDown (GetAsyncKeyState) repeatedly.
#define MOD_CTRL  (IsKeyDown(KEY_LEFT_CONTROL)  || IsKeyDown(KEY_RIGHT_CONTROL))
#define MOD_SHIFT (IsKeyDown(KEY_LEFT_SHIFT)    || IsKeyDown(KEY_RIGHT_SHIFT))
#define MOD_ALT   (IsKeyDown(KEY_LEFT_ALT)      || IsKeyDown(KEY_RIGHT_ALT))
#define MOD_SUPER (IsKeyDown(KEY_LEFT_SUPER)    || IsKeyDown(KEY_RIGHT_SUPER))
#define MOD_ANY   (MOD_CTRL || MOD_SHIFT || MOD_ALT || MOD_SUPER)

// Process all registered actions once. Must be called from the main thread
// (or any thread that also calls BeginDrawing/EndDrawing) because tr_raylib
// input arrays (g_curr_keys, g_curr_mouse) are updated inside BeginDrawing().
static void process_actions(void) {
    // Skip all input processing when the window doesn't have focus —
    // tr_raylib polls keys via GetAsyncKeyState which ignores focus,
    // so physically held keys would otherwise register as held even
    // when our window is in the background.
    if (!ctx.focused) return;

    // We need a way to track which keys we've already polled this iteration.
    // Since key constants go up to ~348, a fixed array is fine.
    static u8 pressed_cache[512];
    static u8 down_cache[512];

    // Zero out the cache for this frame
    memset(pressed_cache, 0, sizeof(pressed_cache));
    memset(down_cache, 0, sizeof(down_cache));

    // Batch all modifier key states once — 8 syscalls total, regardless of
    // how many actions reference them.
    static u8 cached_mod_ctrl  = 0;
    static u8 cached_mod_shift = 0;
    static u8 cached_mod_alt   = 0;
    static u8 cached_mod_super = 0;
    static u8 cached_mod_any   = 0;
    cached_mod_ctrl  = MOD_CTRL;
    cached_mod_shift = MOD_SHIFT;
    cached_mod_alt   = MOD_ALT;
    cached_mod_super = MOD_SUPER;
    cached_mod_any   = MOD_ANY;

    for (i32 i = 0; i < actions_count; i++) {
        key_action_t a = actions_array[i];
        u8 happened = 0;

        u8 modifier_match = false;
        if (a.modifier == KEY_ANY) {
            modifier_match = true;
        } else if (a.modifier != KEY_NULL) {
            // Check specific modifier from cache if it's one of the four
            // pairs, otherwise fall through to IsKeyDown.
            switch (a.modifier) {
                case KEY_LEFT_CONTROL:  case KEY_RIGHT_CONTROL:
                    modifier_match = cached_mod_ctrl;  break;
                case KEY_LEFT_SHIFT:    case KEY_RIGHT_SHIFT:
                    modifier_match = cached_mod_shift; break;
                case KEY_LEFT_ALT:      case KEY_RIGHT_ALT:
                    modifier_match = cached_mod_alt;   break;
                case KEY_LEFT_SUPER:    case KEY_RIGHT_SUPER:
                    modifier_match = cached_mod_super; break;
                default:
                    modifier_match = IsKeyDown(a.modifier);
                    break;
            }
        } else {
            modifier_match = !cached_mod_any;
        }

        if (modifier_match) {
            // Cache the KeyPress and KeyDown state so subsequent actions
            // on the same key don't miss the event.
            if (a.press != KEY_NULL) {
                if (pressed_cache[a.press] == 0) {
                    pressed_cache[a.press] = IsKeyPressed(a.press) ? 2 : 1;
                }
                if (pressed_cache[a.press] == 2) happened = 1;
            }

            if (a.hold != KEY_NULL) {
                if (down_cache[a.hold] == 0) {
                    down_cache[a.hold] = IsKeyDown(a.hold) ? 2 : 1;
                }
                if (down_cache[a.hold] == 2) happened = 1;
            }

            if (a.priv_use_mouse && IsMouseButtonDown(a.button)) {
                happened = 1;
            }
        }

        if (!happened) continue;

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

f32 get_system_font_size() {
    NONCLIENTMETRICSA metrics = {0};
    metrics.cbSize = sizeof(NONCLIENTMETRICSA);
    f32 fontsize = 6;
    if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        fontsize += -F32(metrics.lfMessageFont.lfHeight);
    }
    return fontsize;
}

// draw_flush is defined in tr_raylib_draw.c (not declared in headers)
// but has external linkage — force-flushes the batched draw queue.
// Use extern "C" in case trlib was compiled as C (static lib).
#ifdef __cplusplus
extern "C" void draw_flush(void);
#else
extern void draw_flush(void);
#endif

f32 draw_properties(f32 properties_line, char *format, ...) {
    va_list args;
    va_start(args, format);
    static char properties_working[PATH_MAX];
    vsnprintf(properties_working, PATH_MAX, format, args);
    va_end(args);

    memmove(properties_working + 1, properties_working, strlen(properties_working) + 1);
    properties_working[0] = ' ';
    strcat(properties_working, " ");

    f32 font_size = get_system_font_size();
    Color text_col = { 230, 230, 230, 255 };
    Color bg = { 0, 0, 0, 180 };

    // If we have a loaded font (handle != NULL — texture is a compat stub
    // in tr_raylib), use proper text rendering.
    // NOTE: tr_raylib's DrawTextPro ignores the fontSize parameter and
    // always draws at the font's baseSize, so we must use that for the
    // background rect height and line spacing.
    if (ctx.current_font.handle != NULL) {
        i32 fs = ctx.current_font.baseSize ? ctx.current_font.baseSize : (i32)font_size;
        i32 tw = (i32) MeasureTextEx(ctx.current_font, properties_working, (f32)fs, 0).x;
        // Draw background rect via the batched pipeline, then flush so it
        // renders immediately — DrawTextPro renders through its own pipeline
        // which ignores the batch, so flushing here ensures the correct order.
        DrawRectangle(0, (i32) properties_line, tw, fs, bg);
        draw_flush();
        DrawTextPro(ctx.current_font, properties_working, V2f(0, properties_line),
                    V2f(0, 0), 0, (f32)fs, 0, text_col);
        return (f32)fs;
    }

    // Fallback: pixel-by-pixel from GDI-generated atlas data.
    if (ctx.fallback_atlas_pixels != NULL) {
        // Rough text-width estimate for the background rect.
        i32 len = (i32) strlen(properties_working);
        i32 tw = (i32)((f32)len * font_size * 0.65f);
        DrawRectangle(0, (i32) properties_line, tw, (i32) font_size, bg);
        draw_text_fallback(properties_working, 2.0f, properties_line + 2.0f,
                           font_size, text_col);
    }

    return font_size;
}

// TODO defer this
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
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            custom_shader_t t = ctx.shaders_custom_arr[i];
            free(t.name);
            if (t.vs_path) free(t.vs_path);
            if (t.fs_path) free(t.fs_path);
        }
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

    // 4. Load shaders from memory buffers if available
    for (i32 i = 0; i < settings.shader_count; i++) {
        custom_shader_t *s = settings.shaders[i];
        ctx.shaders_custom_arr[i] = *s;

        if (ctx.shader_fs_sources && ctx.shader_fs_sources[i]) {
            ctx.shaders_loaded_arr[ctx.shaders_count] = LoadShaderFromMemory(
                    (ctx.shader_vs_sources ? ctx.shader_vs_sources[i] : NULL),
                    ctx.shader_fs_sources[i]
            );

            if (ctx.shaders_loaded_arr[ctx.shaders_count].id != 0) {
                printf("IMVW|LOG: Loaded custom shader [%d]: %s (from memory)\n", ctx.shaders_count, s->name);
                ctx.shaders_count++;
            } else {
                fprintf(stderr, "IMVW|ERR: Shader compilation failed from memory: %s\n", s->name);
            }
        }
    }
}

static f128 last_timestamp = 0;

void log_step(const char *name) {
    f128 current = getTimeHD_ms();
    printf("IMVW|PROF: %-30s | %8.2Lf ms\n", name, current - last_timestamp);
    last_timestamp = current;
}

typedef struct {
    int argc;
    char **argv;
    int target_w;
    int target_h;
    volatile int config_ready;
    volatile int size_ready;
} loader_data_t;

static char *load_file_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = (char *) malloc(size + 1);
    fread(text, 1, size, file);
    text[size] = '\0';
    fclose(file);
    return text;
}

void *async_loader(void *arg) {
    loader_data_t *ld = (loader_data_t *) arg;

    imvw_init_loaders();

    char *json_text = load_file_text(ASSETS_PATH"imvw.json");
    cJSON *json_data = json_text ? cJSON_Parse(json_text) : NULL;
    if (json_text) free(json_text);

    if (json_data) {
        load_actions(json_data, &actions_array, &actions_count);
        load_settings(json_data, &settings);
        if (settings.python_scripting) {
            Enable_Python();
            load_python(json_data, &python_scripts_array, &python_scripts_count);
        }

        // Pre-load shaders into memory
        if (settings.shader_count > 0) {
            ctx.shader_fs_sources = (char **) calloc(settings.shader_count, sizeof(char *));
            ctx.shader_vs_sources = (char **) calloc(settings.shader_count, sizeof(char *));
            for (int i = 0; i < settings.shader_count; i++) {
                char full_path[PATH_MAX];
                if (settings.shaders[i]->fs_path) {
                    snprintf(full_path, sizeof(full_path), "%s%s", ASSETS_PATH, settings.shaders[i]->fs_path);
                    ctx.shader_fs_sources[i] = load_file_text(full_path);
                }
                if (settings.shaders[i]->vs_path) {
                    snprintf(full_path, sizeof(full_path), "%s%s", ASSETS_PATH, settings.shaders[i]->vs_path);
                    ctx.shader_vs_sources[i] = load_file_text(full_path);
                }
            }
        }

        // Pre-load font into memory
        char *font_path = settings.program_font_path;
        if (!font_path || !strlen(font_path)) font_path = "assets\\segoeui.ttf";
        FILE *f = fopen(font_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            ctx.font_data_size = (int) ftell(f);
            fseek(f, 0, SEEK_SET);
            ctx.font_data = (u8 *) malloc(ctx.font_data_size);
            fread(ctx.font_data, 1, ctx.font_data_size, f);
            fclose(f);
        }

        cJSON_Delete(json_data);
    }

    ld->config_ready = 1;

    // Determine initial image path to get dimensions
    char initial_path[MAX_PATH] = {0};
    if (ld->argc > 1) {
        for (int i = 1; i < ld->argc; i++) strcat(initial_path, ld->argv[i]);
    } else {
        strcpy(initial_path, ASSETS_PATH "test2.png");
    }

    int img_w, img_h, img_c;
    if (stbi_info(initial_path, &img_w, &img_h, &img_c)) {
        // Calculate aspect ratio and window size
        f32 aspect = (f32) img_w / (f32) img_h;
        f32 sw = settings.max_window_w;
        f32 sh = settings.max_window_h;
        f32 mw = settings.min_window_w;
        f32 mh = settings.min_window_h;

        f32 target_w = (f32) img_w;
        f32 target_h = (f32) img_h;

        if (target_w >= sw || target_h >= sh) {
            if (target_w >= target_h) {
                target_w = sw;
                target_h = sw / aspect;
            } else {
                target_h = sh;
                target_w = sh * aspect;
            }
        }
        if (target_w <= mw || target_h <= mh) {
            if (target_w >= target_h) {
                target_w = mw;
                target_h = mw / aspect;
            } else {
                target_h = mh;
                target_w = mh * aspect;
            }
        }
        ld->target_w = (int) target_w;
        ld->target_h = (int) target_h;
    } else {
        ld->target_w = SCREEN_WIDTH;
        ld->target_h = SCREEN_HEIGHT;
    }
    ld->size_ready = 1;

    return NULL;
}

i32 main(i32 argc, char **argv) {
    // ── DPI awareness (must be set before any window creation) ──
    {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) {
            typedef BOOL (WINAPI *SetProcessDpiAwarenessContext_t)(HANDLE);
            SetProcessDpiAwarenessContext_t pFn =
                (SetProcessDpiAwarenessContext_t)GetProcAddress(hUser32,
                    "SetProcessDpiAwarenessContext");
            if (pFn) {
                // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (HANDLE)-4
                pFn((HANDLE)(LONG_PTR)-4);
            } else {
                // Fallback: SetProcessDpiAwareness (shcore.dll, Win 8.1+)
                typedef HRESULT (WINAPI *SetProcessDpiAwareness_t)(int);
                HMODULE hShcore = LoadLibraryA("shcore.dll");
                if (hShcore) {
                    SetProcessDpiAwareness_t pAware =
                        (SetProcessDpiAwareness_t)GetProcAddress(hShcore,
                            "SetProcessDpiAwareness");
                    // PROCESS_PER_MONITOR_DPI_AWARE = 2
                    if (pAware) pAware(2);
                    FreeLibrary(hShcore);
                } else {
                    // Last resort: SetProcessDPIAware (Vista+)
                    SetProcessDPIAware();
                }
            }
        }
    }

    // Enable precision timers on Windows to make Sleep(1) actually 1ms
    timeBeginPeriod(1);

    // Disable OS window ghosting to prevent false-positive "Not Responding" states
    {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) {
            typedef void (WINAPI *DisableGhosting_t)(void);
            DisableGhosting_t pDisableGhosting = (DisableGhosting_t)GetProcAddress(hUser32, "DisableProcessWindowsGhosting");
            if (pDisableGhosting) {
                pDisableGhosting();
            }
        }
    }

    f128 overall_start = getTimeHD_ms();
    last_timestamp = overall_start;

    ShowWindow(GetConsoleWindow(), SW_HIDE);

    loader_data_t ld = {argc, argv, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0};
    pthread_t loader_thr;
    pthread_create(&loader_thr, NULL, async_loader, &ld);

    // Wait for async_loader to determine image dimensions

    // Wait briefly for size metadata, but don't hang if it's slow
    int timeout = 100; // ms
    while (!ld.size_ready && timeout-- > 0) Sleep(1);

    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(ld.target_w, ld.target_h, WINDOW_TITLE);
    log_step("InitWindow");

    // Initialise SSAA offscreen render target.
    // NOTE: must come AFTER pthread_join so settings (including ssaa_scale)
    // have been loaded from the JSON config.
    {
        ID3D11Device *dev = tr_get_device(tr_get_state());
        if (dev) {
            ssaa_init(dev, settings.ssaa_scale);
            ssaa_resize(ld.target_w, ld.target_h, dev);
        }
    }

    log_step("loader_thread + ssaa_init join");

    imvw_font_load();
    log_step("  imvw_font_load");
    load_custom_shaders();
    log_step("  load_custom_shaders");

    ctx.main_window = GetWindowHandle();
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
    flut_add(printf);
    flut_add(Window_Toggle_Maximized);
    log_step("flut_add registrations");

    ctx.target_camera = ctx.real_camera;

    ctx.default_wind_proc = (WNDPROC) SetWindowLongPtrA((HWND) GetWindowHandle(), GWLP_WNDPROC, (LONG_PTR) NewWindowProc);

    // Disable IME for this window to prevent focus-loss deadlocks with the OS IME/DWM subsystem
    {
        HMODULE hImm = LoadLibraryA("imm32.dll");
        if (hImm) {
            typedef BOOL (WINAPI *ImmAssociateContext_t)(HWND, void*);
            ImmAssociateContext_t pImmAssociateContext = 
                (ImmAssociateContext_t)GetProcAddress(hImm, "ImmAssociateContext");
            if (pImmAssociateContext) {
                pImmAssociateContext((HWND)GetWindowHandle(), NULL);
            }
            FreeLibrary(hImm);
        }
    }

    // tr_create (inside InitWindow) pumps messages before our subclass is
    // installed, so WM_SETFOCUS was already consumed by the original proc.
    // Explicitly query focus state now that the subclass is in place.
    ctx.focused = (GetForegroundWindow() == (HWND)GetWindowHandle()) ? 1 : 0;

    {
        HWND hwnd = (HWND) GetWindowHandle();
        HINSTANCE hInst = GetModuleHandle(NULL);
        HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
    }
    log_step("Icon Load");

    if (argc > 1) {
        char argv_path[MAX_PATH] = {0};
        for (int i = 1; i < argc; i++) strcat(argv_path, argv[i]);
        Load(argv_path);
    } else {
        Load(ASSETS_PATH "test2.png");
    }
    log_step("Initial Load (Load call)");

    Camera_Home_ResetZoom();

    if (settings.python_scripting) {
        python_run_script_func(python_scripts_array, python_scripts_count, "start");
        log_step("Python Startup Script");
    }

    // Mark the context as initialized (was previously done in the input thread)
    ctx.one = 1;

    printf("IMVW|LOG: total_startup_time_ms: %.2Lf\n", getTimeHD_ms() - overall_start);

#ifdef IMVW_PROFILE
    // Frame timing accumulators
    f128 t_pre     = 0, t_lerp = 0, t_begin = 0, t_actions = 0,
         t_setup = 0, t_img  = 0, t_props   = 0, t_shaders= 0,
         t_flush  = 0, t_present = 0, t_pump    = 0, t_total  = 0;
#endif

    i32 frame_count = 0;
    f128 loop_entry_ms;
    static u8 bg_idle = 0;
    static u8 skip_actions = 0;
    while (!WindowShouldClose()) {
        // When the window loses focus, render one final frame then enter a
        // low-power idle loop that only pumps messages at 20 Hz.  This drops
        // CPU/GPU usage to ~0% for background windows (useful with many
        // instances open).
        if (!ctx.focused) {
            if (bg_idle) {
                Sleep(50);
                tr_pump_messages(tr_get_state());
                continue;
            }
            // Don't set bg_idle here — we wait for camera lerps to converge
            // before entering sleep (checked after the lerp section below),
            // so background animations like Camera_Home_NoResetZoom finish
            // smoothly before the window goes idle.
            ctx.mouse_delta = V2f(0, 0);
        } else {
            if (bg_idle) {
                // Waking from sleep: flush stale mouse state so the click
                // that activated the window doesn't trigger pan/mouse actions
                // (IsMouseButtonDown would return true for that click).
                GetMouseDelta();
                ctx.mouse_delta = V2f(0, 0);
                skip_actions = 2;  // skip actions for 2 frames on wake
            }
            bg_idle = 0;
        }

        loop_entry_ms = getTimeHD_ms();
        frame_count++;
#ifdef IMVW_PROFILE
        f128 t0 = loop_entry_ms;
#endif
        if (settings.python_scripting) {
            python_run_script_func(python_scripts_array, python_scripts_count, "update");
        }

        // Track window dimensions for SSAA — if the window was resized,
        // recreate the offscreen render target at the new size.
        static int prev_win_w = 0, prev_win_h = 0;
        ctx.window_width  = GetRenderWidth();
        ctx.window_height = GetRenderHeight();
        if (ctx.window_width != prev_win_w || ctx.window_height != prev_win_h) {
            prev_win_w = ctx.window_width;
            prev_win_h = ctx.window_height;
            ID3D11Device *dev = tr_get_device(tr_get_state());
            if (dev) ssaa_resize(ctx.window_width, ctx.window_height, dev);
        }

        ctx.mouse_pos = GetMousePosition();
        ctx.mouse_delta = GetMouseDelta();

        if (ctx.focused && (IsKeyPressed(KEY_F11) || (ALT_DOWN && IsKeyPressed(KEY_ENTER)))) {
            if (IsWindowMaximized()) {
                settings.maximized = 0;
                settings.undecorated = 0;
            } else {
                settings.maximized = 1;
                settings.undecorated = !settings.undecorated;
            }
        }

        static u8 last_undecorated = 0;
        if (settings.undecorated != last_undecorated) {
            settings.undecorated ? SetWindowState(FLAG_WINDOW_UNDECORATED)
                                 : ClearWindowState(FLAG_WINDOW_UNDECORATED);
            last_undecorated = settings.undecorated;
        }

        u8 last_maximized = IsWindowMaximized();
        if (settings.maximized != last_maximized) {
            settings.maximized ? MaximizeWindow() : RestoreWindow();
            Camera_Home_NoResetZoom();
        }

        if (IsWindowState(FLAG_WINDOW_TOPMOST) != settings.on_top) {
            settings.on_top ? SetWindowState(FLAG_WINDOW_TOPMOST) : ClearWindowState(FLAG_WINDOW_TOPMOST);
        }

        if (ctx.focused && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            SendMessage((HWND) GetWindowHandle(), 0x007B /*WM_CONTEXTMENU*/, (WPARAM) GetWindowHandle(), 0);
        }

        if (!ALT_DOWN) {
            f32 wheel = GetMouseWheelMove();
            if (wheel != 0) {
                Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), ctx.real_camera);
                ctx.target_camera.offset = GetMousePosition();
                ctx.target_camera.target = mwp;

                f32 z = wheel * 0.15f;
                Camera_ZoomHold(&z);
            }
        }

#ifdef IMVW_PROFILE
        f128 t1 = getTimeHD_ms();  t_pre += t1 - t0;  t0 = t1;
#endif

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

#ifdef IMVW_PROFILE
        f128 t2 = getTimeHD_ms();  t_lerp += t2 - t0;  t0 = t2;
#endif

        // If unfocused but not yet asleep, check whether the camera has
        // converged close enough to its target to safely enter the sleep
        // loop.  We sum absolute differences across all camera components
        // (the zoom term is scaled because it's typically ~1.0 while the
        // others are in pixels/degrees).  This prevents the window from
        // freezing mid-animation when the user tabs away mid-lerp.
        if (!ctx.focused && !bg_idle) {
            f32 drift =
                fabs(ctx.real_camera.offset.x  - ctx.target_camera.offset.x) +
                fabs(ctx.real_camera.offset.y  - ctx.target_camera.offset.y) +
                fabs(ctx.real_camera.target.x  - ctx.target_camera.target.x) +
                fabs(ctx.real_camera.target.y  - ctx.target_camera.target.y) +
                fabs(ctx.real_camera.rotation  - ctx.target_camera.rotation) +
                fabs(ctx.real_camera.zoom      - ctx.target_camera.zoom) * 100.0f;
            if (drift <= 0.05f) {
                bg_idle = 1;  // converged — sleep next iteration
            }
        }

        BeginDrawing();
#ifdef IMVW_PROFILE
        f128 t3 = getTimeHD_ms();  t_begin += t3 - t0;  t0 = t3;
#endif

        // ── Action processing (runs after poll_input so key state is current) ──
        // Skip actions for 2 frames after waking from background sleep so the
        // activation click (WM_LBUTTONDOWN that regains focus) doesn't trigger
        // Camera_PanMouse via IsMouseButtonDown.
        if (skip_actions) {
            skip_actions--;
        } else {
            process_actions();
        }
#ifdef IMVW_PROFILE
        f128 t4 = getTimeHD_ms();  t_actions += t4 - t0;  t0 = t4;
#endif

        // HACK: D3D11 ClearRenderTargetView alpha doesn't propagate through DWM
        // correctly for transparent windows, so we clear to transparent then draw
        // a fullscreen rect through the shader pipeline with premultiplied blend.
        // NOTE: This must run BEFORE ssaa_begin_frame so it clears the BACKBUFFER
        // (set by BeginDrawing) — not the SSAA offscreen target.
        ClearBackground(BLANK);

        // ── Redirect rendering to SSAA offscreen target (1.1× supersampling) ──
        ID3D11DeviceContext    *d3d_ctx = tr_get_context(tr_get_state());
        ID3D11RenderTargetView *d3d_rtv = tr_get_rtv(tr_get_state());
        Color bg_clear = ctx.use_alt_bg ? settings.bg_color_alt : settings.bg_color;
        if (d3d_ctx && d3d_rtv) ssaa_begin_frame(d3d_ctx, d3d_rtv, bg_clear);
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        {
            Color cb = ctx.use_alt_bg ? settings.bg_color_alt : settings.bg_color;
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), cb);
        }
        EndBlendMode();

#ifdef IMVW_PROFILE
        f128 t5 = getTimeHD_ms();  t_setup += t5 - t4;  t0 = t5;
#endif

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

                u8 t = true;
                Camera_Home_Internal(&t);
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

#ifdef IMVW_PROFILE
        f128 t6 = getTimeHD_ms();  t_img += t6 - t5;  t0 = t6;
#endif

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

#ifdef IMVW_PROFILE
        f128 t7 = getTimeHD_ms();  t_props += t7 - t6;  t0 = t7;
#endif

        // --- Custom Shader Pass ---

        // TODO blend mode should be set by custom_shader_t
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            custom_shader_t info = ctx.shaders_custom_arr[i];
            Shader shader = ctx.shaders_loaded_arr[i];
            if (!info.enabled) continue;

            // Standard Uniforms
            // NOTE: SSAA renders at 1.1× viewport, so SV_POSITION in the pixel
            // shader is in offscreen coordinates.  We scale BOTH cameraOffset
            // AND cameraZoom by SSAA_SCALE so factors cancel out:
            //   (screenPos - camOff*1.1) / (zoom*1.1) + target
            //   = (screenPos/1.1 - camOff) / zoom + target
            // Panning also cancels correctly: Δ*1.1 / (zoom*1.1) = Δ/zoom.
            float ss = ssaa_get_scale();
            int resLoc    = GetShaderLocation(shader, "screenResolution");
            int targetLoc = GetShaderLocation(shader, "cameraTarget");
            int offsetLoc = GetShaderLocation(shader, "cameraOffset");
            int zoomLoc   = GetShaderLocation(shader, "cameraZoom");

            Vector2 res     = {(float) GetScreenWidth(), (float) GetScreenHeight()};
            Vector2 cam_off = {ctx.real_camera.offset.x * ss, ctx.real_camera.offset.y * ss};
            float   cam_zom = ctx.real_camera.zoom * ss;
            SetShaderValue(shader, resLoc,    &res,     SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, targetLoc, &ctx.real_camera.target, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, offsetLoc, &cam_off, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, zoomLoc,   &cam_zom, SHADER_UNIFORM_FLOAT);

            BeginShaderMode(shader);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
            EndShaderMode();
        }
        EndBlendMode();

#ifdef IMVW_PROFILE
        f128 t8 = getTimeHD_ms();  t_shaders += t8 - t7;  t0 = t8;
#endif

        // Flush any remaining batched draws to the offscreen target, then
        // restore the backbuffer and blit the offscreen → backbuffer with
        // bilinear filtering (downscales from 1.1× → 1.0× for SSAA).
        draw_flush();
        if (d3d_ctx && d3d_rtv)
            ssaa_end_frame(d3d_ctx, d3d_rtv, ctx.window_width, ctx.window_height);

#ifdef IMVW_PROFILE
        f128 t9 = getTimeHD_ms();  t_flush += t9 - t8;  t0 = t9;
#endif

        EndDrawing();

        // Update frame_time on the main thread (was previously set by the input thread)
        ctx.frame_time = GetFrameTime();

#ifdef IMVW_PROFILE
        f128 t10 = getTimeHD_ms();  t_present += t10 - t9;  t0 = t10;
#endif

        tr_pump_messages(tr_get_state());

#ifdef IMVW_PROFILE
        f128 t11 = getTimeHD_ms();  t_pump += t11 - t10;
        t_total += t11 - loop_entry_ms;

        // Print profile summary every 120 frames (~2s at 60fps)
        if (frame_count % 120 == 0) {
            f128 n = 120.0L;
            fprintf(stderr, "\n--- FRAME PROFILE (avg over %d frames) ---\n", 120);
            fprintf(stderr, "  Pre-frame (python+input+state checks): %5.2Lf us\n", t_pre     * 1000.0L / n);
            fprintf(stderr, "  Camera lerp:                           %5.2Lf us\n", t_lerp    * 1000.0L / n);
            fprintf(stderr, "  BeginDrawing (poll_input):             %5.2Lf us\n", t_begin   * 1000.0L / n);
            fprintf(stderr, "  process_actions:                       %5.2Lf us\n", t_actions * 1000.0L / n);
            fprintf(stderr, "  Clear+SSAA+bg rect:                   %5.2Lf us\n", t_setup   * 1000.0L / n);
            fprintf(stderr, "  Image drawing:                        %5.2Lf us\n", t_img     * 1000.0L / n);
            fprintf(stderr, "  Properties text:                      %5.2Lf us\n", t_props   * 1000.0L / n);
            fprintf(stderr, "  Custom shader pass:                   %5.2Lf us\n", t_shaders * 1000.0L / n);
            fprintf(stderr, "  draw_flush + ssaa_end_frame:          %5.2Lf us\n", t_flush   * 1000.0L / n);
            fprintf(stderr, "  EndDrawing (vsync idle):              %5.2Lf us\n", t_present * 1000.0L / n);
            fprintf(stderr, "  tr_pump_messages:                     %5.2Lf us\n", t_pump    * 1000.0L / n);
            fprintf(stderr, "  -----------------------------------------------------\n");
            fprintf(stderr, "  Active CPU work:                       %5.2Lf us\n",
                    (t_total - t_present - t_pump) * 1000.0L / n);
            fprintf(stderr, "  Wall time (TOTAL):                     %5.2Lf us  (%.0Lf FPS)\n",
                    t_total * 1000.0L / n, 1000.0L * n / t_total);
            t_pre = t_lerp = t_begin = t_actions = t_setup = t_img = t_props = t_shaders = t_flush = t_present = t_pump = t_total = 0;
        }
#endif
    }

    ssaa_cleanup();
    CloseWindow();
    return 0;
}
