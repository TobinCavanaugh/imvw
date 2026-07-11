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
#include "image/decoders.h"
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

// draw_reinit is defined in tr_raylib_draw.c — re-creates all raylib pipeline
// resources (shaders, buffers, samplers, blend/rasterizer states) on a new device.
extern void draw_reinit(ID3D11Device* device, ID3D11DeviceContext* ctx);

static f128 last_timestamp = 0;


static void log_step(const char *name) {
    f128 current = getTimeHD_ms();
    printf("IMVW|PROF: %-30s | %8.2Lf ms\n", name, current - last_timestamp);
    last_timestamp = current;
}

// Called after a successful WARP→HW driver upgrade.
// Every D3D11 resource created on the WARP device must be released and
// re-created on the new hardware device.
static void on_driver_upgrade(TRState *s) {
    fprintf(stderr, "IMVW|LOG: Recreating D3D11 resources on HW device...\n");

    // 1. SSAA — release old WARP offscreen target, create new one
    ssaa_cleanup();
    ssaa_init(tr_get_device(s), settings.ssaa_scale);
    ssaa_resize(ctx.window_width, ctx.window_height, tr_get_device(s));

    // 2. Raylib draw pipeline — clean up old WARP resources and re-create on HW device
    draw_reinit(tr_get_device(s), tr_get_context(s));

    // 3. Font — release old WARP font texture, flag for lazy reload from main loop
    if (ctx.current_font.handle != NULL) {
        UnloadFont(ctx.current_font);
        ctx.current_font = (Font){0};
    }
    ctx.font_need_reload = 1;

    // 4. Custom shaders — release old WARP shader COM objects before device is destroyed
    if (ctx.shaders_loaded_arr != NULL) {
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            if (ctx.shaders_loaded_arr[i].id != 0)
                UnloadShader(ctx.shaders_loaded_arr[i]);
        }
        free(ctx.shaders_loaded_arr);
        ctx.shaders_loaded_arr = NULL;
    }
    if (ctx.shaders_custom_arr != NULL) {
        free(ctx.shaders_custom_arr);
        ctx.shaders_custom_arr = NULL;
    }
    ctx.shaders_count = 0;
    ctx.shaders_need_reload = 1;  // re-create on next frame

    // 5. Current image texture — unload old WARP texture, async decode will re-upload
    if (ctx.current_tex.id != 0) {
        UnloadTexture(ctx.current_tex);
        ctx.current_tex = (Texture2D){0};
    }
    ctx.tex_channels = 0;
    ctx.tex_fsize = 0;
    ctx.tex_need_load = 1;  // async decode will re-upload

    fprintf(stderr, "IMVW|LOG: D3D11 resource upgrade complete\n");
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

    loader_data_t ld = {argc, argv, 800, 450, 0, 0, 0};
    pthread_t loader_thr;
    pthread_create(&loader_thr, NULL, async_loader, &ld);

    // Wait briefly for size metadata, but don't hang if it's slow
    int timeout = 100; // ms
    while (!ld.size_ready && timeout-- > 0) Sleep(1);

    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    // ── Request WARP-first boot for faster startup ──
    // tr_get_state() returns NULL before InitWindow, so we pass NULL and
    // the mode is stored in a pending global that InitWindow will pick up.
    {
        const char *mode_str = "HARDWARE";
        TRDriverMode mode = TR_DRIVER_WARP_FIRST;
        switch (mode) {
            case TR_DRIVER_WARP:       mode_str = "WARP";       break;
            case TR_DRIVER_WARP_FIRST: mode_str = "WARP_FIRST"; break;
            default:                   mode_str = "HARDWARE";   break;
        }
        fprintf(stderr, "IMVW|LOG: Requesting driver mode: %s\n", mode_str);
        tr_set_driver_mode(NULL, mode);
    }

    InitWindow(ld.target_w, ld.target_h, "IMVW");
    log_step("InitWindow");

    // ── Log which D3D11 adapter we actually got ──
    {
        ID3D11Device *dev = tr_get_device(tr_get_state());
        if (dev) {
            IDXGIDevice *dxgiDev = NULL;
            HRESULT hr = dev->lpVtbl->QueryInterface(dev, &IID_IDXGIDevice, (void**)&dxgiDev);
            if (SUCCEEDED(hr) && dxgiDev) {
                IDXGIAdapter *adapter = NULL;
                hr = dxgiDev->lpVtbl->GetAdapter(dxgiDev, &adapter);
                if (SUCCEEDED(hr) && adapter) {
                    DXGI_ADAPTER_DESC desc;
                    adapter->lpVtbl->GetDesc(adapter, &desc);
                    fprintf(stderr, "IMVW|LOG: D3D11 Adapter: %ls (Vendor=0x%04X)\n",
                            desc.Description, desc.VendorId);
                    adapter->lpVtbl->Release(adapter);
                }
                dxgiDev->lpVtbl->Release(dxgiDev);
            }
        }
    }

    // Register callbacks for WARP→HW driver upgrade
    tr_set_upgrade_callback(on_driver_upgrade);


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

    // If the async_loader already decoded the image (into ctx.loading_img),
    // suppress the separate decode thread that Load() would trigger.
    if (ld.img_decoded) {
        ctx.tex_need_load = 0;
    }

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
