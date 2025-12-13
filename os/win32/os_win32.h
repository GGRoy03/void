#pragma once

// [INCLUDES & LINKING]

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <imm.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "GDI32.lib")

typedef struct os_win32_state
{
    // Memory
    memory_arena *Arena;

    // External (Queried by agnostic code)
    os_system_info SystemInfo;
    os_inputs      Inputs;

    // internal (Queried by win32 specific code)
    HWND            HWindow;
} os_win32_state;

extern os_win32_state OSWin32State;
