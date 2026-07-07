#include "init.h"
#include "core/win_include.h"   /* must be early: sets up CloseWindow→Win32_CloseWindow
                                   before tr_raylib.h/any header pulls in windows.h */
#include "async_loader.h"
#include "core/context_t.h"
#include "core/settings.h"
#include "input/flut.h"
#include "input/actions.h"
#include "render/ssaa.h"
#include "render/shaders.h"
#include "ui/font.h"
#include "ui/text.h"
#include "utils/imvw_time.h"
#include "imvw_interface.h"
#include <trlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern context_t ctx;
extern settings_t settings;
extern char **python_scripts_array;
extern i32 python_scripts_count;

// python_run_script_func is defined in scripting/python_loader.h (TU-as-header)
extern void python_run_script_func(char **scripts, i32 count, char *func_name);

static f128 last_timestamp = 0;

static void log_step(const char *name) {
    f128 current = getTimeHD_ms();
    printf("IMVW|PROF: %-30s | %8.2Lf ms\n", name, current - last_timestamp);
    last_timestamp = current;
}

void imvw_init(int argc, char **argv) {
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

    loader_data_t ld = {argc, argv, 800, 450, 0, 0};
    pthread_t loader_thr;
    pthread_create(&loader_thr, NULL, async_loader, &ld);

    // Wait briefly for size metadata, but don't hang if it's slow
    int timeout = 100; // ms
    while (!ld.size_ready && timeout-- > 0) Sleep(1);

    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(ld.target_w, ld.target_h, "IMVW");
    log_step("InitWindow");

    // Initialise SSAA offscreen render target.
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

    // Mark the context as initialized
    ctx.one = 1;

    printf("IMVW|LOG: total_startup_time_ms: %.2Lf\n", getTimeHD_ms() - overall_start);
}

void imvw_cleanup(void) {
    ssaa_cleanup();
    CloseWindow();
}
