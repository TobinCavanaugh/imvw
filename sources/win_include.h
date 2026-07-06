//
// Created by tobin on 2026-02-23.
//

#ifndef IMVW_WIN_INCLUDE_H
#define IMVW_WIN_INCLUDE_H

#define CloseWindow Win32_CloseWindow
#define Rectangle Win32_Rectangle
#define ShowCursor Win32_ShowCursor

#include <windows.h>
#define PATH_MAX MAX_PATH
#include <shellapi.h> // For ShellExecute
#include <shlwapi.h>  // For StrFormatByteSize64
#include <shlobj.h>   // For SHGetFolderPathA
#include <pthread_time.h>

// Undefine Windows macros that conflict with Raylib
#undef CloseWindow
#undef Rectangle
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound
// --------------------------------------------

#undef near
#undef far


#include "tr_raylib.h"

#endif //IMVW_WIN_INCLUDE_H
