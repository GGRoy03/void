#pragma once

// [Includes & Linking]

#include <windows.h>
#pragma comment(lib, "user32")

// [Core Types]

typedef struct os_win32_state
{
    // Misc
    memory_arena *Arena;

    // Info
    os_system_info SystemInfo;

    // Handles
    HWND WindowHandle;

    // Console
    HWND ConsoleHandle;
    b32  ConsoleSupportsVT;
} os_win32_state;

// [Globals]

global os_win32_state OSWin32State;

// [Win32 Specific API]

// [Windowing]
internal HWND OSWin32GetWindowHandle(void);

// [Console]

internal void OSWin32SetConsoleColor  (byte_string ANSISequence, WORD WinAttributes);
internal void OSWin32WriteToConsole   (byte_string ANSISequence);
