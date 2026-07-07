// pthread_win32.h — Minimal Win32 shim providing pthread_t, pthread_create, pthread_detach
// for MSVC builds. Use CreateThread + CloseHandle under the hood.
#ifndef PTHREAD_WIN32_H
#define PTHREAD_WIN32_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>  // malloc, free — MUST be before usage in this header

// pthread_t is a HANDLE (CreateThread returns HANDLE)
typedef HANDLE pthread_t;

// Thread function signature: void* (*)(void*)
typedef void* (*pthread_routine_t)(void*);

// Wrapper struct to pass routine + arg to CreateThread
// (CreateThread is fine for our use since we don't use CRT per-thread data)
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
    if (retval) *retval = NULL;  // we can't easily get the return value
    CloseHandle(thread);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
}

#endif // PTHREAD_WIN32_H
