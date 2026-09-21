#include "main_loop.h"
#include "core/context_t.h"
#include "core/settings.h"
#include "core/win_include.h"
#include "input/actions.h"
#include "input/flut.h"
#include "render/ssaa.h"
#include "render/shaders.h"
#include "ui/text.h"
#include "ui/font.h"
#include "image/tex_loader.h"
#include "utils/imvw_time.h"
#include "utils/profiler.h"
#include "imvw_interface.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <trlib.h>

extern context_t ctx;
extern settings_t settings;
extern char **python_scripts_array;
extern i32 python_scripts_count;

extern void python_run_script_func(char **scripts, i32 count, char *func_name);

#define ALT_DOWN ( IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) )

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

void imvw_main_loop(void) {
    i32 frame_count = 0;
    f128 loop_entry_ms;
    static u8 bg_idle = 0;
    static u8 skip_actions = 0;

    while (!WindowShouldClose()) {
        // Keep focus state synchronized with OS foreground window
        HWND hwnd = (HWND) GetWindowHandle();
        if (hwnd && (GetForegroundWindow() == hwnd || GetActiveWindow() == hwnd)) {
            ctx.focused = 1;
        }

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
            ctx.mouse_delta = V2f(0, 0);
        } else {
            if (bg_idle) {
                GetMouseDelta();
                ctx.mouse_delta = V2f(0, 0);
                skip_actions = 2;
            }
            bg_idle = 0;
        }

        loop_entry_ms = getTimeHD_ms();
        frame_count++;
        profiler_begin_frame(loop_entry_ms);
        if (settings.python_scripting) {
            python_run_script_func(python_scripts_array, python_scripts_count, "update");
        }

        // Track window dimensions for SSAA
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

        profiler_mark("pre-frame (python+input+state)");

        f32 dt = GetFrameTime();
        ctx.real_camera.offset = Vector2Lerp(ctx.real_camera.offset, ctx.target_camera.offset,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.target = Vector2Lerp(ctx.real_camera.target, ctx.target_camera.target,
                                             dt * settings.lerpSpeed_pan);
        ctx.real_camera.rotation = Lerp(ctx.real_camera.rotation, ctx.target_camera.rotation,
                                        dt * settings.lerpSpeed_rotate);
        ctx.real_camera.zoom = max(Lerp(ctx.real_camera.zoom, ctx.target_camera.zoom,
                                        dt * settings.lerpSpeed_zoom), 0.001f);

        roundCamera2DValues(&ctx.real_camera, 0.001f);
        roundCamera2DValues(&ctx.target_camera, 0.001f);

        profiler_mark("camera lerp");

        // If unfocused but not yet asleep, check whether the camera has converged.
        // Don't enter bg_idle during the initial startup frames (< 30 frames / ~0.5s)
        if (!ctx.focused && !bg_idle && frame_count > 30) {
            f32 drift =
                fabs(ctx.real_camera.offset.x  - ctx.target_camera.offset.x) +
                fabs(ctx.real_camera.offset.y  - ctx.target_camera.offset.y) +
                fabs(ctx.real_camera.target.x  - ctx.target_camera.target.x) +
                fabs(ctx.real_camera.target.y  - ctx.target_camera.target.y) +
                fabs(ctx.real_camera.rotation  - ctx.target_camera.rotation) +
                fabs(ctx.real_camera.zoom      - ctx.target_camera.zoom) * 100.0f;
            if (drift <= 0.05f) {
                bg_idle = 1;
            }
        }

        BeginDrawing();
        profiler_mark("BeginDrawing (poll_input)");

        if (skip_actions) {
            skip_actions--;
        } else {
            process_actions();
        }
        profiler_mark("process_actions");

        ClearBackground(BLANK);

        ID3D11DeviceContext    *d3d_ctx = tr_get_context(tr_get_state());
        ID3D11RenderTargetView *d3d_rtv = tr_get_rtv(tr_get_state());
        Color bg_active = ctx.active_bg_color_index >= 0
            ? settings.bg_colors[ctx.active_bg_color_index].color
            : settings.bg_color;
        if (d3d_ctx && d3d_rtv) ssaa_begin_frame(d3d_ctx, d3d_rtv, bg_active);
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        {
            Color cb = ctx.active_bg_color_index >= 0
                ? settings.bg_colors[ctx.active_bg_color_index].color
                : settings.bg_color;
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), cb);
        }
        EndBlendMode();

        profiler_mark("clear + SSAA + bg rect");

        BeginMode2D(ctx.real_camera);
        {
            if (ctx.tex_need_load && !ctx.tex_loading && !ctx.img_ready_to_upload) {
                imvw_tex_load();
            }

            // Lazy reload font after driver upgrade (load fails inside the callback)
            if (ctx.font_need_reload && ctx.current_font.handle == NULL) {
                imvw_font_load();
                if (ctx.current_font.handle != NULL) {
                    ctx.font_need_reload = 0;
                    fprintf(stderr, "IMVW|LOG: Font re-loaded after driver upgrade\n");
                }
            }

            // Lazy reload custom shaders after driver upgrade
            if (ctx.shaders_need_reload) {
                load_custom_shaders();
                ctx.shaders_need_reload = 0;
                fprintf(stderr, "IMVW|LOG: Shaders re-loaded after driver upgrade\n");
            }

            // 1. Process async thumbnail upload if ready and full image hasn't been uploaded yet
            if (ctx.thumb_ready_to_upload) {
                uint64_t req = ctx.thumb_req_id;
                Image thumb = ctx.thumb_img;
                ctx.thumb_img = (Image){0};
                ctx.thumb_ready_to_upload = 0;

                if (req == ctx.load_request_id && !ctx.img_uploaded && thumb.data != NULL) {
                    if (ctx.current_tex.id != 0) UnloadTexture(ctx.current_tex);
                    if (ctx.active_image.data != NULL) UnloadImage(ctx.active_image);

                    int orig_w = ctx.current_tex.width;
                    int orig_h = ctx.current_tex.height;

                    ctx.current_tex = LoadTextureFromImage(thumb);
                    SetTextureFilter(ctx.current_tex, settings.texture_filter);

                    if (orig_w > 0 && orig_h > 0) {
                        ctx.current_tex.width = orig_w;
                        ctx.current_tex.height = orig_h;
                    }
                    ctx.active_image = thumb;
                } else {
                    if (thumb.data) UnloadImage(thumb);
                }
            }

            // 2. Process async full-resolution image upload
            if (ctx.img_ready_to_upload) {
                uint64_t req = ctx.img_req_id;
                Image decoded = ctx.loading_img;
                ctx.loading_img = (Image){0};
                ctx.img_ready_to_upload = 0;

                if (req == ctx.load_request_id && decoded.data != NULL) {
                    if (ctx.current_tex.id != 0) UnloadTexture(ctx.current_tex);
                    if (ctx.active_image.data != NULL) UnloadImage(ctx.active_image);

                    ctx.current_tex = LoadTextureFromImage(decoded);
                    GenTextureMipmaps(&ctx.current_tex);
                    SetTextureFilter(ctx.current_tex, settings.texture_filter);
                    ctx.active_image = decoded;
                    ctx.img_uploaded = 1;
                    ctx.tex_need_load = 0;
                    ctx.tex_need_filter = 0;
                    Camera_FitWindow();

                    u8 t = true;
                    Camera_Home_Internal(&t);
                    if (frame_count <= 2) {
                        ctx.real_camera = ctx.target_camera;
                    }
                } else {
                    if (decoded.data) UnloadImage(decoded);
                }
            }

            if (ctx.tex_need_filter && ctx.current_tex.id != 0) {
                SetTextureFilter(ctx.current_tex, settings.texture_filter);
                ctx.tex_need_filter = 0;
            }

            if (ctx.current_tex.id != 0) {
                if (!settings.infinite_tile) {
                    DrawTexture(ctx.current_tex, -I32(round(ctx.current_tex.width / 2.0f)),
                                -I32(round(ctx.current_tex.height / 2.0f)), WHITE);
                } else {
                    v2f ul = GetScreenToWorld2D(V2f(0, 0), ctx.real_camera);
                    v2f br = GetScreenToWorld2D(V2f((f32) GetScreenWidth(), (f32) GetScreenHeight()), ctx.real_camera);
                    i32 start_x = (i32) floor(ul.x / ctx.current_tex.width) * ctx.current_tex.width - ctx.current_tex.width;
                    i32 start_y = (i32) floor(ul.y / ctx.current_tex.height) * ctx.current_tex.height - ctx.current_tex.height;
                    i32 end_x   = (i32) ceil(br.x / ctx.current_tex.width) * ctx.current_tex.width + ctx.current_tex.width;
                    i32 end_y   = (i32) ceil(br.y / ctx.current_tex.height) * ctx.current_tex.height + ctx.current_tex.height;
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

        profiler_mark("image drawing");

        // --- Custom Shader Pass ---
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        for (i32 i = 0; i < ctx.shaders_count; i++) {
            custom_shader_t info = ctx.shaders_custom_arr[i];
            Shader shader = ctx.shaders_loaded_arr[i];
            if (!info.enabled) continue;

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

        profiler_mark("custom shader pass");

        // Flush + SSAA resolve to backbuffer
        draw_flush();
        if (d3d_ctx && d3d_rtv)
            ssaa_end_frame(d3d_ctx, d3d_rtv, ctx.window_width, ctx.window_height);

        profiler_mark("draw_flush + ssaa_end_frame");

        // ── Properties text (after SSAA resolve, at 1:1 on the backbuffer) ──
        if (settings.properties_show && !ctx.tex_loading && ctx.current_tex.width > 0) {
            f32 pl = 0;
            char buf[64 + 32];

            pl += draw_properties(pl, "%dx%d", ctx.current_tex.width, ctx.current_tex.height);

            StrFormatByteSize64(ctx.tex_fsize, buf, sizeof(buf));
            pl += draw_properties(pl, "%s", buf);

            pl += draw_properties(pl, "Channels: %d", ctx.tex_channels);

            pixel_format_to_str_s((PixelFormat) ctx.current_tex.format, buf, sizeof(buf));
            pl += draw_properties(pl, "Format: %s", buf);

            pl += draw_properties(pl, "Filter: %s",
                                  settings.texture_filter == TEXTURE_FILTER_BILINEAR
                                  ? "Bilinear"
                                  : (settings.texture_filter == TEXTURE_FILTER_TRILINEAR
                                     ? "Trilinear"
                                     : "Point"));

            pl += draw_properties(pl, "%.2fms", ctx.frame_time * 1000.);
            pl += draw_properties(pl, "%.2ffps ", 1. / ctx.frame_time);

            {
                TRDriverMode dm = tr_get_driver_mode(tr_get_state());
                const char *driver_str = (dm == TR_DRIVER_HARDWARE) ? "GPU" : "WARP";
                pl += draw_properties(pl, "Renderer: %s", driver_str);
            }

            {
                char settings_path[MAX_PATH] = {0};
                GetFullPathNameA(ASSETS_PATH "imvw.json", MAX_PATH, settings_path, NULL);
                pl += draw_properties(pl, "Settings: %s", settings_path);
            }

            draw_flush();
        }

        profiler_mark("properties text");

        EndDrawing();

        ctx.frame_time = GetFrameTime();

        profiler_mark("EndDrawing (vsync idle)");

        tr_pump_messages(tr_get_state());

        profiler_mark("tr_pump_messages");

        if (frame_count % 120 == 0) {
            profiler_report(120);
        }
    }
}
