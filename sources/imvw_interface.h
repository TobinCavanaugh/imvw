//
// Created by tobin on 2025-03-15.
//

#ifndef IMVW_INTERFACE_H
#define IMVW_INTERFACE_H

#define Rectangle   WinRECTANGLE
#define CloseWindow WinCloseWindow

#include <windows.h>
#include <shlwapi.h>

#undef Rectangle
#undef CloseWindow

#define ShowCursor  WinShowCursor
#define LoadImage   WinLoadImage
#define PlaySound   WinPlaySound
#define DrawText    WinDrawText
#define DrawTextEx  WinDrawTextEx

#include <context_t.h>
#include <settings.h>
#include <dialect.h>
#include <string.h>
#include <Python.h>
#include <fileapi.h>
#include <stdio.h>
#include <tinyfiledialogs.h>

extern context_t ctx;
extern settings_t settings;

v2f CalculateWindowSize();

u8 IsDockedToMonitor(HWND hWnd);

u0 Camera_FitWindow();

u0 Load(char *path);

u0 Reload();

u0 Rotate(f32 amount);

u0 Rotate_By_Scroll();

u0 Rotate_By_Mouse();

u0 Camera_Home_Internal(u8 reset_zoom);

u0 Camera_Home_NoResetZoom();

u0 Camera_Home_ResetZoom();

u0 Toggle_Trilinear_Filtering();

u0 Camera_Pan();

u0 Open_File_Dialog();

u0 Copy_To_Clipboard();

LRESULT CALLBACK NewWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

u0 Edit_Settings_Json();

u0 Enable_Python();

u0 Disable_Python();

u0 Window_On_Top(void *state_ptr);

const char *pixel_format_to_str_s(PixelFormat format, char *buffer, i32 n);

u0 Open_Sibling(f32 *direction);

#endif //IMVW_INTERFACE_H
