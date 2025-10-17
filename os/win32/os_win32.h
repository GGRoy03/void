#pragma once

// [INCLUDES & LINKING]

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "shlwapi.lib")

typedef struct IDWriteFactory IDWriteFactory;

// [CORE TYPES]

typedef struct os_win32_state
{
    // memory
    memory_arena *Arena;

    // External (Queried by agnostic code)
    os_system_info SystemInfo;
    os_inputs      Inputs;

    // Internal (Queried by win32 specific code)
    IDWriteFactory *DWriteFactory;
} os_win32_state;

extern os_win32_state OSWin32State;
