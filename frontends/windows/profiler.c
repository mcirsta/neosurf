#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static FILE *f;
static LARGE_INTEGER freq;
static CRITICAL_SECTION cs;
static int inited;
static __thread int in_handler;
static HMODULE mod;

__attribute__((no_instrument_function)) static uint64_t now_qpc(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}

__attribute__((no_instrument_function)) static void close_prof(void)
{
    if (f)
        fclose(f);
    DeleteCriticalSection(&cs);
}

__attribute__((no_instrument_function)) static void init_prof(void)
{
    if (inited)
        return;
    inited = 1;
    QueryPerformanceFrequency(&freq);
    InitializeCriticalSection(&cs);
    f = fopen("instrument.log", "wb");
    mod = GetModuleHandleW(NULL);
    if (f)
        fprintf(f, "B %p\n", mod);
    atexit(close_prof);
}

__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *func, void *caller)
{
    if (in_handler)
        return;
    in_handler = 1;
    init_prof();
    EnterCriticalSection(&cs);
    fprintf(f, "E %p %p %llu %lu\n", func, caller, (unsigned long long)now_qpc(), GetCurrentThreadId());
    LeaveCriticalSection(&cs);
    in_handler = 0;
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *func, void *caller)
{
    if (in_handler)
        return;
    in_handler = 1;
    EnterCriticalSection(&cs);
    fprintf(f, "X %p %p %llu %lu\n", func, caller, (unsigned long long)now_qpc(), GetCurrentThreadId());
    LeaveCriticalSection(&cs);
    in_handler = 0;
}
