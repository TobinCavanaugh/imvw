// pthread_win32.h — Minimal Win32 pthread shim.
// Only used on MSVC where real pthread.h doesn't exist.
// MinGW-w64 (w64devkit) bundles winpthreads — real pthread.h is available.
#ifndef PTHREAD_WIN32_H
#define PTHREAD_WIN32_H

// MinGW-w64 has a real pthread.h — use that instead of the shim.
#ifdef __MINGW32__
#include <pthread.h>
#else

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

typedef HANDLE pthread_t;

typedef void* (*pthread_routine_t)(void*);

static DWORD WINAPI pthread_win32_wrapper(LPVOID lpParam) {
    void** params = (void**)lpParam;
    pthread_routine_t routine = (pthread_routine_t)params[0];
    void* arg = params[1];
    void* result = routine(arg);
    free(params);
    return (DWORD)(uintptr_t)result;
}

static inline int pthread_create(pthread_t* thread, const void* attr,
                                  void* (*start_routine)(void*), void* arg) {
    (void)attr;
    void** params = (void**)malloc(2 * sizeof(void*));
    if (!params) return -1;
    params[0] = (void*)start_routine;
    params[1] = arg;
    HANDLE h = CreateThread(NULL, 0, pthread_win32_wrapper, params, 0, NULL);
    if (!h) { free(params); return -1; }
    *thread = h;
    return 0;
}

static inline int pthread_detach(pthread_t thread) {
    if (thread) CloseHandle(thread);
    return 0;
}

static inline int pthread_join(pthread_t thread, void** retval) {
    if (!thread) return -1;
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (retval) *retval = NULL;
    CloseHandle(thread);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
}

#endif // !__MINGW32__
#endif // PTHREAD_WIN32_H
