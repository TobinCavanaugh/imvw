//
// Created by tobin on 2025-03-15.
//

#include "imvw_interface.h"

#include <dirent.h>
#include <external/stb_image.h>
#include "slfile.h"


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
    // Set title path
    strcpy(ctx.current_path, path);
    strcpy(ctx.current_window_title, "imvw | ");
    strcat(ctx.current_window_title, ctx.current_path);

    if (ctx.current_tex.width != 0) {
        UnloadTexture(ctx.current_tex);
    }

    // Load the info of the image, assign it to our current texture then fit
    // the window correctly. This means our window fits the size of our tex
    // before it's fully loaded :)
    i32 wid, hei, channels;
    stbi_info(path, &wid, &hei, &channels);

    ctx.current_tex.width = wid;
    ctx.current_tex.height = hei;
    ctx.tex_channels = channels;

    struct _stat info;
    if (_stat(ctx.current_path, &info) == 0) {
        ctx.tex_fsize = info.st_size;
    } else {
        ctx.tex_fsize = 0;
    }

    Camera_FitWindow();

    ctx.tex_need_load = 1;

    SetWindowTitle(ctx.current_window_title);
}

u0 Reload() {
    Load(ctx.current_path);
}

u0 Rotate(f32 amount) {
    Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), ctx.real_camera);
    ctx.target_camera.offset = GetMousePosition();
    ctx.target_camera.target = mwp;

    //TODO SHIFT_FINE should be a setting
    ctx.target_camera.rotation += amount * ctx.frame_time * 125.0f * settings.rotation_speed * SHIFT_FINE;
}

u0 Rotate_By_Scroll() {
    Rotate(GetMouseWheelMove());
}

u0 Rotate_By_Mouse() {
    // Vector2 mwp = GetScreenToWorld2D(GetMousePosition(), real_camera);
    // target_camera.offset = GetMousePosition();
    // target_camera.target = mwp;
    ctx.target_camera.rotation += ctx.mouse_delta.x / (f32) GetScreenWidth() * 360.0f * SHIFT_FINE * (
        ctx.frame_time * 100.0f * settings.rotation_speed);
}

u0 Camera_Home_Internal(u8 reset_zoom) {
    ctx.target_camera.offset = V2f(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f);
    ctx.target_camera.target = V2f(0, 0);

    // Round the camera rotation to nearest 360
    f32 rot = ctx.target_camera.rotation;
    ctx.target_camera.rotation = roundf(rot / 360.f) * 360.f;

    if (!reset_zoom) {
        return;
    }

    // Perfectly zoom in
    f32 screen_width = GetScreenWidth();
    f32 screen_height = GetScreenHeight();

    f32 texture_width = (ctx.current_tex.width + settings.padding * 2.0f);
    f32 texture_height = (ctx.current_tex.height + settings.padding * 2.0f);

    f32 horizontal_zoom = screen_width / texture_width;
    f32 vertical_zoom = screen_height / texture_height;
    ctx.target_camera.zoom = min(horizontal_zoom, vertical_zoom);
}

u0 Camera_Home_NoResetZoom() {
    Camera_Home_Internal(false);
}

u0 Camera_Home_ResetZoom() {
    Camera_Home_Internal(true);
}


u0 Toggle_Trilinear_Filtering() {
    // if (settings.texture_filter == TEXTURE_FILTER_TRILINEAR) {
    //     settings.texture_filter = TEXTURE_FILTER_POINT;
    // } else {
    //     settings.texture_filter = TEXTURE_FILTER_TRILINEAR;
    // }
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

u0 Camera_Pan() {
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

u0 Copy_To_Clipboard() {
    Image img = LoadImageFromTexture(ctx.current_tex);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    // Ungodly
    u8 *bgraData = malloc(img.width * img.height * 4);
    if (bgraData == NULL) {
        fprintf(stderr, "Failed to allocate memory for BGRA data...\n");
        UnloadImage(img);
        return;
    }

    // TODO What the fuck. Without this the image is BGRA
    // This is also sick because we get to load the texture from GPU to memory
    // then to heap then to copy. So if its a large image we're loading the
    // image like 3 times. Fucked.
    // Convert RGBA to BGRA
    for (int i = 0; i < img.width * img.height; i++) {
        bgraData[i * 4 + 0] = ((u8 *) img.data)[i * 4 + 2]; // B
        bgraData[i * 4 + 1] = ((u8 *) img.data)[i * 4 + 1]; // G
        bgraData[i * 4 + 2] = ((u8 *) img.data)[i * 4 + 0]; // R
        bgraData[i * 4 + 3] = ((u8 *) img.data)[i * 4 + 3]; // A
    }

    HBITMAP hbm = CreateBitmap(img.width, img.height, 1, 32, bgraData);
    // HBITMAP hbm = CreateBitmap(img.width, img.height, 1, 32, img.data);

    if (OpenClipboard(GetWindowHandle())) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hbm);
        CloseClipboard();
    } else {
        fprintf(stderr, "Failure to open clipboard...\n");
    }

    DeleteObject(hbm);
    UnloadImage(img);
    free(bgraData);
}

LRESULT CALLBACK NewWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CONTEXTMENU: {
            // Check if the right-click is on the title bar
            if ((HWND) wParam == hwnd) {
                // Create a context menu
                HMENU hMenu = CreatePopupMenu();

                //TODO make this extensible via configuration | script
                // { "text" : "Keep on top", "type" : "checkbox", setting:"on_top" }

                AppendMenu(hMenu, MF_STRING, 3, "Focus");

                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, settings.on_top ? MF_CHECKED : MF_UNCHECKED, 1, "Keep on top");
                AppendMenu(hMenu, settings.undecorated ? MF_CHECKED : MF_UNCHECKED, 2, "Undecorated");
                AppendMenu(hMenu, settings.properties_show ? MF_CHECKED : MF_UNCHECKED, 4, "Show Properties");

                // { "text" : "Keep on top", "type" : "checkbox", setting:"on_top" }


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
                case 4:
                    // Show image properties
                    settings.properties_show = !settings.properties_show;
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
    return CallWindowProc(ctx.default_wind_proc, hwnd, uMsg, wParam, lParam);
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
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: formatStr = "UNCOMPRESSED_GRAYSCALE";
            break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: formatStr = "UNCOMPRESSED_GRAY_ALPHA";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5: formatStr = "UNCOMPRESSED_R5G6B5";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: formatStr = "UNCOMPRESSED_R8G8B8";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1: formatStr = "UNCOMPRESSED_R5G5B5A1";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: formatStr = "UNCOMPRESSED_R4G4B4A4";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: formatStr = "UNCOMPRESSED_R8G8B8A8";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32: formatStr = "UNCOMPRESSED_R32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32: formatStr = "UNCOMPRESSED_R32G32B32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: formatStr = "UNCOMPRESSED_R32G32B32A32";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16: formatStr = "UNCOMPRESSED_R16";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16: formatStr = "UNCOMPRESSED_R16G16B16";
            break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: formatStr = "UNCOMPRESSED_R16G16B16A16";
            break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGB: formatStr = "COMPRESSED_DXT1_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA: formatStr = "COMPRESSED_DXT1_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA: formatStr = "COMPRESSED_DXT3_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA: formatStr = "COMPRESSED_DXT5_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ETC1_RGB: formatStr = "COMPRESSED_ETC1_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_ETC2_RGB: formatStr = "COMPRESSED_ETC2_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA: formatStr = "COMPRESSED_ETC2_EAC_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_PVRT_RGB: formatStr = "COMPRESSED_PVRT_RGB";
            break;
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA: formatStr = "COMPRESSED_PVRT_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA: formatStr = "COMPRESSED_ASTC_4x4_RGBA";
            break;
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA: formatStr = "COMPRESSED_ASTC_8x8_RGBA";
            break;
        default: formatStr = "UNKNOWN_FORMAT";
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

    dir_iterate(dir,
                open_sib_iterate,
                (file_skip_flags){
                    .keep_nav = 0, .skip_hidden = 1, .recurse = 0, .skip_files = 0, .skip_directories = 1
                });

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
