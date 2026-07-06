// dirent_win32.h — Minimal Win32 shim for POSIX dirent.h (MSVC compatibility)
// Provides struct dirent, opendir, readdir, closedir using FindFirstFile/FindNextFile
#ifndef DIRENT_WIN32_H
#define DIRENT_WIN32_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define NAME_MAX 255

typedef struct dirent {
    char  d_name[NAME_MAX + 1];
} dirent;

typedef struct {
    HANDLE          find_handle;
    WIN32_FIND_DATAA find_data;
    int             first;
    char            path[512];
} DIR;

static inline DIR* opendir(const char* dirname) {
    if (!dirname) return NULL;
    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) return NULL;
    memset(dir, 0, sizeof(DIR));

    // Append \* to the path for FindFirstFile
    snprintf(dir->path, sizeof(dir->path), "%s\*", dirname);
    dir->find_handle = FindFirstFileA(dir->path, &dir->find_data);
    if (dir->find_handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;
    static struct dirent entry;

    if (dir->first) {
        dir->first = 0;
    } else {
        if (!FindNextFileA(dir->find_handle, &dir->find_data))
            return NULL;
    }

    strncpy(entry.d_name, dir->find_data.cFileName, NAME_MAX);
    entry.d_name[NAME_MAX] = '\0';
    return &entry;
}

static inline int closedir(DIR* dir) {
    if (!dir) return -1;
    FindClose(dir->find_handle);
    free(dir);
    return 0;
}

#endif // DIRENT_WIN32_H
