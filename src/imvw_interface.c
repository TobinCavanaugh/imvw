//
// Created by tobin on 2025-03-15.
//

#include "imvw_interface.h"

#include "core/platform/dirent_win32.h"
#include <external/stb_image.h>
#include <commdlg.h>

#include "utils/slfile.h"


// TODO DUPLICATED SHIFT_FINE
#define SHIFT_DOWN ( (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) )
#define SHIFT_FINE ( SHIFT_DOWN ? 0.5f : 1.0f )

#define SPIN_IF_TEX_LOADING while(ctx.tex_loading || ctx.tex_need_load) { Sleep(16); }

v2f CalculateWindowSize() {
    f32 aspect_ratio = (f32) ctx.current_tex.width / (f32) ctx.current_tex.height;

    f32 max_w = ctx.current_tex.width;
    f32 max_h = ctx.current_tex.height;

    // Maximum size
    f32 sw = (f32) (settings.max_window_w);
    f32 sh = (f32) (settings.max_window_h);
    // If its too large for the monitor
    if (ctx.current_tex.width >= sw || ctx.current_tex.height >= sh) {
        if (ctx.current_tex.width >= ctx.current_tex.height) {
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
    if (ctx.current_tex.width <= mw || ctx.current_tex.height <= mh) {
        if (ctx.current_tex.width >= ctx.current_tex.height) {
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

    return placement.showCmd == 1 /*SW_SHOWNORMAL*/
           && (rc.left != placement.rcNormalPosition.left ||
               rc.top != placement.rcNormalPosition.top ||
               rc.right != placement.rcNormalPosition.right ||
               rc.bottom != placement.rcNormalPosition.bottom);
}

u0 Camera_FitWindow() {
    if (!IsWindowMaximized() && !IsDockedToMonitor((HWND) GetWindowHandle())) {
        v2f size = CalculateWindowSize();
        if (GetScreenWidth() != (i32)size.x || GetScreenHeight() != (i32)size.y) {
            SetWindowSize((i32) size.x, (i32) size.y);
        }
    }
}

u0 Load(char *path) {

    // TODO FIX THIS
    strcpy(ctx.current_path, path);
    strcpy(ctx.current_window_title, "imvw | ");
    strcat(ctx.current_window_title, ctx.current_path);

    ctx.tex_need_load = 1;

    // Use metadata for initial dimensions if possible to avoid window flickering
    int w, h, c;
    if (stbi_info(path, &w, &h, &c)) {
        ctx.current_tex.width = w;
        ctx.current_tex.height = h;
        ctx.tex_channels = c;
    } else {
        ctx.current_tex.width = 1;
        ctx.current_tex.height = 1;
        ctx.tex_channels = 1;
    }

    struct _stat info;
    ctx.tex_fsize = (_stat(ctx.current_path, &info) == 0) ? info.st_size : 0;

    // Fit window based on initial dimensions
    Camera_FitWindow();

    // Trigger the background thread in tex_loader.h
    ctx.tex_need_load = 1;

    SetWindowTitle(ctx.current_window_title);
}

// TODO Also make reload settings etc.
u0 Reload() {
    Load(ctx.current_path);
}

u0 Rotate(f32 *amount) {
    Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), ctx.real_camera);
    ctx.target_camera.offset = GetMousePosition();
    ctx.target_camera.target = mwp;

    //TODO SHIFT_FINE should be a setting
    ctx.target_camera.rotation += *amount * ctx.frame_time * 125.0f * settings.rotation_speed * SHIFT_FINE;
}

u0 Toggle_BG_Color(char *name) {
    if (!name) { ctx.active_bg_color_index = -1; return; }
    for (int i = 0; i < settings.bg_color_count; i++) {
        if (strcmp(settings.bg_colors[i].name, name) == 0) {
            ctx.active_bg_color_index = (ctx.active_bg_color_index == i) ? -1 : i;
            return;
        }
    }
}

u0 Rotate_By_Scroll() {
    f32 c = GetMouseWheelMove();
    Rotate(&c);
}

u0 Rotate_By_Mouse() {
    ctx.target_camera.rotation += ctx.mouse_delta.x / (f32) GetScreenWidth() * 360.0f * SHIFT_FINE *
                                  (ctx.frame_time * 100.0f * settings.rotation_speed);

    // ctx.target_camera.rotation += ctx.mouse_delta.y / (f32) GetScreenWidth() * 360.0f * SHIFT_FINE *
    //         (ctx.frame_time * 100.0f * settings.rotation_speed);
}

u0 Shader_Toggle(char *str) {
    for (i32 i = 0; i < ctx.shaders_count; i++) {
        custom_shader_t *t = &ctx.shaders_custom_arr[i];
        if (strcmp(str, t->name) == 0) t->enabled = !t->enabled;
//        printf("\t- %d %s\n", t->enabled, t->name);
    }
}

u0 Camera_ZoomAmt(f32 *amount) {
    // Calculate the exponential scale factor
    // You may need to tweak settings.zoom_speed slightly since the math changed
    f32 scale_factor = expf(*amount * settings.zoom_speed);

    // Apply multiplicative zoom
    ctx.target_camera.zoom *= scale_factor;

    // Calculate bounds
    // The smallest we allow the image to get (e.g., shrinking it to fit ~100px)
    f32 min_zoom_out = 100.0f / (fmaxf(ctx.current_tex.height, ctx.current_tex.width) + settings.padding * 2.0f);

    // The largest we allow the image to get (128x scale)
    f32 max_zoom_in = 128.0f;

    // Clamp the zoom
    ctx.target_camera.zoom = fmaxf(fminf(ctx.target_camera.zoom, max_zoom_in), min_zoom_out);
}

u0 Camera_ZoomHold(f32 *amount) {
    // Calculate the exponential scale factor
    // You may need to tweak settings.zoom_speed slightly since the math changed
    f32 scale_factor = expf(*amount * SHIFT_FINE * settings.zoom_speed);

    // Apply multiplicative zoom
    ctx.target_camera.zoom *= scale_factor;

    // Calculate bounds
    // The smallest we allow the image to get (e.g., shrinking it to fit ~100px)
    f32 min_zoom_out = 100.0f / (fmaxf(ctx.current_tex.height, ctx.current_tex.width) + settings.padding * 2.0f);

    // The largest we allow the image to get (128x scale)
    f32 max_zoom_in = 128.0f;

    // Clamp the zoom
    ctx.target_camera.zoom = fmaxf(fminf(ctx.target_camera.zoom, max_zoom_in), min_zoom_out);
}

u0 Camera_Home_Internal(u8 *reset_zoom) {
    ctx.target_camera.offset = V2f(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f);
    ctx.target_camera.target = V2f(0, 0);

    // Round the camera rotation to nearest 360
    f32 rot = ctx.target_camera.rotation;
    ctx.target_camera.rotation = roundf(rot / 360.f) * 360.f;

    if (!*reset_zoom) {
        return;
    }

    // Perfectly zoom in
    f32 screen_width = GetScreenWidth();
    f32 screen_height = GetScreenHeight();

    f32 texture_width = (ctx.current_tex.width + settings.padding * 2.0f);
    f32 texture_height = (ctx.current_tex.height + settings.padding * 2.0f);

    f32 horizontal_zoom = screen_width / texture_width;
    f32 vertical_zoom = screen_height / texture_height;
    ctx.target_camera.zoom = MIN(horizontal_zoom, vertical_zoom);
}

u0 Camera_Home_NoResetZoom() {
    u8 t = false;
    Camera_Home_Internal(&t);
}

u0 Camera_Home_ResetZoom() {
    u8 t = true;
    Camera_Home_Internal(&t);
}


u0 Toggle_Trilinear_Filtering() {
    ctx.tex_need_filter = 1;
    switch (settings.texture_filter) {
        case TEXTURE_FILTER_POINT:
            settings.texture_filter = TEXTURE_FILTER_TRILINEAR;
            break;
        case TEXTURE_FILTER_TRILINEAR:
            settings.texture_filter = TEXTURE_FILTER_POINT;
            break;
        default:
            settings.texture_filter = TEXTURE_FILTER_POINT;
            break;
    }
}

u0 Camera_PanX(float *x) {
    ctx.target_camera.target.x += *x / ctx.target_camera.zoom;
}

u0 Camera_PanY(float *y) {
    ctx.target_camera.target.y += *y / ctx.target_camera.zoom;
}

u0 Window_Toggle_Maximized() {
    settings.maximized = !settings.maximized;
}

u0 Camera_PanMouse() {
    //TODO Camera panning is a little shitty and laggy

    // v2f d = GetMouseDelta();
    v2f d = ctx.mouse_delta;
    ctx.target_camera.offset.x += d.x * SHIFT_FINE;
    ctx.target_camera.offset.y += d.y * SHIFT_FINE;

    Vector2 mwp = GetScreenToWorld2D(ctx.mouse_pos, ctx.target_camera);
    ctx.target_camera.offset = ctx.mouse_pos;
    ctx.target_camera.target = mwp;

    // ctx.target_camera.offset = ctx.mouse_pos;
    // ctx.target_camera.target = ctx.mwp;
}

u0 Open_File_Dialog() {
    // TODO: Putting this in the json might be reasonable...
    char const *lFilterPatterns[] = {
            "*.png", "*.jpg", "*.jpeg", "jfif", "*.tif", "*.tiff",
            "*.psd", "*.webp", "*.bmp", "*.pnm", "*.hdr", "*.gif",
            "*.tga", "*.ico"
    };
    int numFilters = sizeof(lFilterPatterns) / sizeof(lFilterPatterns[0]);

    char description[512] = "Image Files (";
    for (int i = 0; i < numFilters; i++) {
        strcat(description, lFilterPatterns[i]);
        if (i < numFilters - 1) {
            strcat(description, "; ");
        }
    }
    strcat(description, ")");

    // Build combined pattern string: "*.png;*.jpg;*.jpeg;..."
    char allPatterns[256] = {0};
    for (int i = 0; i < numFilters; i++) {
        if (i > 0) strcat(allPatterns, ";");
        strcat(allPatterns, lFilterPatterns[i]);
    }

    // Build GetOpenFileNameA filter string:
    // "Description\0Pattern\0All Files\0*.*\0\0"
    char filter[1024] = {0};
    char *fp = filter;
    size_t desclen = strlen(description);
    memcpy(fp, description, desclen); fp += desclen + 1;
    size_t patlen = strlen(allPatterns);
    memcpy(fp, allPatterns, patlen); fp += patlen + 1;
    memcpy(fp, "All Files", 9); fp += 10;
    memcpy(fp, "*.*", 3); fp += 4;

    char szFile[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND) GetWindowHandle();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select an image file";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        Load(szFile);
        Camera_Home_ResetZoom();
    }
}

u0 Open_File_Dialog_New() {
}

u0 Copy_To_Clipboard() {
    if (ctx.current_tex.width == 0) {
        fprintf(stderr, "IMVW : COPY : ERR : No currently existing texture\n");
        return;
    }
    printf("COPYING\n");
    Image img = LoadImageFromTexture(ctx.current_tex);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if (OpenClipboard((HWND) (HWND) GetWindowHandle())) {
        // EmptyClipboard();
        // SetClipboardData(CF_BITMAP, hbm);
        // CloseClipboard();
        fprintf(stderr, "Copying to clipboard was not implemented...\n");
    } else {
        fprintf(stderr, "Failure to open clipboard...\n");
    }

    // UnloadImage(img);
}

u0 Duplicate_Instance(u0) {
    // path for self exe, path for open program, + extra for args etc.
    char path[MAX_PATH * 3];
    GetModuleFileName(NULL, path, MAX_PATH);

    strcat(path, " ");
    strcat(path, "\"");
    strcat(path, ctx.current_path);
    strcat(path, "\"");

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    CreateProcessA(
            NULL, // path,
            path, // ctx.current_path,
            NULL,
            NULL,
            FALSE,
            0x8/*DETACHED_PROCESS*/,
            NULL,
            NULL,
            &si,
            &pi);
}


u0 Toggle_Properties() {
    settings.properties_show = !settings.properties_show;
}

// out_settings->infinite_tile = 1;
LRESULT CALLBACK NewWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    const i32 op_on_top = 1, op_undecorate = 2, op_focus = 3, op_properties = 4, op_filtering = 5, op_open = 6,
            op_duplicate = 7;

//    printf("------- IMVW | PROC | 0x%08x\n", uMsg);

    switch (uMsg) {
        case 0x7f : {
            return 0;
        }
        case WM_SIZE : {
            if (wParam == SIZE_MAXIMIZED) settings.maximized = 1;
            else if (wParam == SIZE_RESTORED) settings.maximized = 0;

            // Keep the camera centred when the window is resized, matching
            // Camera_Home_NoResetZoom behavior (which also resets target to origin)
            i32 new_w = LOWORD(lParam);
            i32 new_h = HIWORD(lParam);
            if (new_w > 0 && new_h > 0) {
                ctx.target_camera.offset = V2f((f32) new_w / 2.0f, (f32) new_h / 2.0f);
                ctx.target_camera.target = V2f(0, 0);
                // Snap real_camera immediately to avoid one-frame lerp lag
                // which causes ghosting, wrong zoom-to-mouse, and apparent squishing
                ctx.real_camera.offset = ctx.target_camera.offset;
                ctx.real_camera.target = ctx.target_camera.target;
                ctx.real_camera.rotation = ctx.target_camera.rotation;
                ctx.real_camera.zoom = ctx.target_camera.zoom;
                // Update ortho projection matrix immediately for this frame.
                // During live resize (drag), trlib's WM_SIZE handler resizes the
                // swapchain but does NOT set the resized flag, so tr_handle_resize
                // in BeginDrawing skips draw_update_size — causing the ortho matrix
                // to stay at the old dimensions and the image to stretch.
                draw_update_size(new_w, new_h);
            }
            break;
        }
        case WM_CONTEXTMENU: {
            // Check if the right-click is on the title bar
            if ((HWND) wParam == hwnd) {
                // Create a context menu
                HMENU hMenu = CreatePopupMenu();

                HMENU options_menu = CreatePopupMenu();
                HMENU window_menu = CreatePopupMenu();

                //TODO make this extensible via configuration | script
                // { "text" : "Keep on top", "type" : "checkbox", setting:"on_top" }

                AppendMenu(hMenu, MF_STRING, op_focus, "Focus");


                AppendMenu(hMenu, settings.properties_show ? MF_CHECKED : MF_UNCHECKED, op_properties,
                           "Show Properties");

                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

                // AppendMenu(hMenu, MF_STRING, SC_MOVE, "Move");

                /*Options menu popout*/
                {
                    AppendMenu(options_menu, MF_STRING, op_filtering,
                               settings.texture_filter == TEXTURE_FILTER_TRILINEAR
                               ? "Enable Point Filtering"
                               : "Enable Trilinear Filtering");
                    AppendMenu(hMenu, MF_STRING | MF_POPUP, (u64) options_menu, "Options");
                }

                /*Window menu popout*/ {
                    AppendMenu(window_menu, MF_STRING, op_open, "Open");
                    AppendMenu(window_menu, MF_STRING, op_duplicate, "Duplicate");

                    AppendMenu(window_menu, MF_SEPARATOR, 0, NULL);

                    AppendMenu(window_menu, settings.on_top ? MF_CHECKED : MF_UNCHECKED, op_on_top, "Keep on top");
                    AppendMenu(window_menu, settings.undecorated ? MF_CHECKED : MF_UNCHECKED, op_undecorate,
                               "Undecorated");


                    AppendMenu(window_menu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(window_menu, MF_STRING, SC_MINIMIZE, "Minimize");

                    if (IsWindowMaximized()) {
                        AppendMenu(window_menu, MF_STRING, SC_RESTORE, "Restore");
                    } else {
                        AppendMenu(window_menu, MF_STRING, SC_MAXIMIZE, "Maximize");
                    }
                    AppendMenu(window_menu, MF_STRING, SC_CLOSE, "Close");


                    // Add the popout menu
                    AppendMenu(hMenu, MF_STRING | MF_POPUP, (u64) window_menu, "Window");
                }

                // Show the context menu
                POINT pt;
                GetCursorPos(&pt);
                i32 result = TrackPopupMenu(hMenu, TPM_CENTERALIGN | TPM_HORPOSANIMATION | TPM_RETURNCMD, pt.x,
                                            pt.y, 0,
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
                    SendMessage((HWND) GetWindowHandle(), WM_COMMAND, result, 0);
                }
                // Nothing selected
                DestroyMenu(hMenu);
            }
            return 0;
        }
        case WM_COMMAND: {
            uint16_t word = (uint16_t) (wParam);
            // Switch statements require literals
            if (word == op_on_top) {
                // Keep on Top
                settings.on_top = !settings.on_top;
            } else if (word == op_undecorate) {
                // Undecorate
                settings.undecorated = !settings.undecorated;
                settings.undecorated ? SetWindowState(FLAG_WINDOW_UNDECORATED) : ClearWindowState(
                        FLAG_WINDOW_UNDECORATED);
            } else if (word == op_focus) {
                Camera_Home_ResetZoom();
            } else if (word == op_properties) {
                // Show image properties
                settings.properties_show = !settings.properties_show;
            } else if (word == op_filtering) {
                Toggle_Trilinear_Filtering();
            } else if (word == op_open) {
                Open_File_Dialog();
            } else if (word == op_duplicate) {
                Duplicate_Instance();
                // Open_File_Dialog();
            } else if (word == SC_MOVE) {
                SendMessage((HWND) GetWindowHandle(), WM_SYSCOMMAND, SC_MOVE, 0);
            } else if (word == SC_MINIMIZE) {
                SendMessage((HWND) GetWindowHandle(), WM_SYSCOMMAND, SC_MINIMIZE, 0);
            } else if (word == SC_RESTORE) {
                SendMessage((HWND) GetWindowHandle(), WM_SYSCOMMAND, SC_RESTORE, 0);
            } else if (word == SC_MAXIMIZE) {
                SendMessage((HWND) GetWindowHandle(), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
            } else if (word == SC_CLOSE) {
                SendMessage((HWND) GetWindowHandle(), WM_SYSCOMMAND, SC_CLOSE, 0);
            } else {
                printf("/////////");
            }
            return 0;
        }
        case WM_SETFOCUS:{
            ctx.focused = 1;
            break;
        }
        case WM_KILLFOCUS:{
            ctx.focused = 0;
            break;
        }

        case WM_KEYDOWN:
        case WM_KEYUP: {
            break;
        }
        case WM_PAINT: {
            ValidateRect(hwnd, NULL);
            break;
        }
        case WM_MENUCHAR: {
            // Prevent beep sound when Alt+Key is pressed
            return (LRESULT)(MNC_CLOSE << 16);
        }
    }
    return CallWindowProcA((WNDPROC) ctx.default_wind_proc, hwnd, uMsg, wParam, lParam);
}

//TODO add a shit load of logging features
//TODO LOD image loading

u0 Edit_Settings_Json() {
    //TODO not perfect...
    ShellExecute(0, "open", ASSETS_PATH"imvw.json", 0, 0, SW_SHOWNORMAL);
}

u0 Enable_Python() {
    settings.python_scripting = 1;
    Py_Initialize();
}

u0 Disable_Python() {
    Py_Finalize();
    settings.python_scripting = 0;
}

u0 Window_On_Top(void *state_ptr) {
    if (state_ptr == NULL) {
        settings.on_top = !settings.on_top;
        return;
    }

    u8 state = *(u8 *) state_ptr;
    settings.on_top = state;
}

const char *pixel_format_to_str_s(PixelFormat format, char *buffer, i32 n) {
    const char *formatStr = NULL;

    switch (format) {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
            formatStr = "UNCOMPRESSED_GRAYSCALE";
            break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            formatStr = "UNCOMPRESSED_GRAY_ALPHA";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
            formatStr = "UNCOMPRESSED_R5G6B5";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            formatStr = "UNCOMPRESSED_R8G8B8";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            formatStr = "UNCOMPRESSED_R5G5B5A1";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            formatStr = "UNCOMPRESSED_R4G4B4A4";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            formatStr = "UNCOMPRESSED_R8G8B8A8";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
            formatStr = "UNCOMPRESSED_R32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
            formatStr = "UNCOMPRESSED_R32G32B32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            formatStr = "UNCOMPRESSED_R32G32B32A32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16:
            formatStr = "UNCOMPRESSED_R16";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
            formatStr = "UNCOMPRESSED_R16G16B16";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
            formatStr = "UNCOMPRESSED_R16G16B16A16";
            break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGB:
            formatStr = "COMPRESSED_DXT1_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA:
            formatStr = "COMPRESSED_DXT1_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA:
            formatStr = "COMPRESSED_DXT3_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA:
            formatStr = "COMPRESSED_DXT5_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ETC1_RGB:
            formatStr = "COMPRESSED_ETC1_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_ETC2_RGB:
            formatStr = "COMPRESSED_ETC2_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA:
            formatStr = "COMPRESSED_ETC2_EAC_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_PVRT_RGB:
            formatStr = "COMPRESSED_PVRT_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA:
            formatStr = "COMPRESSED_PVRT_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA:
            formatStr = "COMPRESSED_ASTC_4x4_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA:
            formatStr = "COMPRESSED_ASTC_8x8_RGBA";
            break;
        default:
            formatStr = "UNKNOWN_FORMAT";
            break;
    }

    // Copy the string to the buffer, ensuring it doesn't exceed the buffer size
    if (buffer && n > 0) {
        strncpy(buffer, formatStr, n - 1);
        buffer[n - 1] = '\0'; // Ensure null-termination
    }

    return buffer;
}

int is_image(const char *filename) {
    int width, height, channels;

    // Attempt to read image info
    if (stbi_info(filename, &width, &height, &channels)) {
        return 1; // It's an image
    }
    return 0; // Not an image
}


u8 self_found = 0;
char prev_sib[PATH_MAX];
char next_sib[PATH_MAX];

u0 open_sib_iterate(slfile_t *file) {
    // If we just found our guy
    if (strcmp(ctx.current_path, file->full_path) == 0) {
        self_found = 1;
        goto END;
    }

    NEXT:
    // File after the one we found
    if (self_found == 1 && is_image(file->full_path)) {
        strcpy(next_sib, file->full_path);
        self_found = 2;
        goto END;
    }

    PREV:
    // File before the one we found
    if (!self_found && is_image(file->full_path)) {
        strcpy(prev_sib, file->full_path);
        goto END;
    }

    END:
    slfile_free(file);
}

u0 Open_Sibling(f32 *direction) {
    SPIN_IF_TEX_LOADING;

    char dir[MAX_PATH];
    GetFullPathName(ctx.current_path, MAX_PATH, dir, NULL);

    printf("fp: `%s`\n", dir);
    PathRemoveFileSpec(dir);
    strcat(dir, "\\");

    self_found = 0;
    prev_sib[0] = 0;
    next_sib[0] = 0;

    dir_iterate(dir, open_sib_iterate,
                (file_skip_flags) {
                        .keep_nav = 0, .skip_hidden = 1,
                        .recurse = 0, .skip_files = 0,
                        .skip_directories = 1
                });

    //TODO
    // Somehow untie this from the loading or allow for cancelling currently
    // loading file

    if (*direction > 0) {
        printf(">>>");
        if (next_sib[0]) {
            Load(next_sib);
        }
    } else if (*direction < 0) {
        printf("<<<");
        if (prev_sib[0]) {
            Load(prev_sib);
        }
    }
}
