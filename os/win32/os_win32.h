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

// [FORWARD DECLARATIONS]

typedef struct os_text_backend os_text_backend;
typedef struct IDXGISurface    IDXGISurface;

// [CORE TYPES]

typedef struct os_win32_state
{
    // Misc
    memory_arena *Arena;

    // PER_OS
    os_text_backend *TextBackend;
    os_system_info   SystemInfo;
    os_inputs        Inputs;

    // WIN32
    HWND WindowHandle;
} os_win32_state;

// [Globals]

extern os_win32_state OSWin32State;

// [Win32 Specific API]

internal HANDLE OSWin32GetNativeHandle  (os_handle Handle);

// [Windowing]

internal HWND OSWin32GetWindowHandle(void);

// [Console]

internal void OSWin32SetConsoleColor  (byte_string ANSISequence, WORD WinAttributes);
internal void OSWin32WriteToConsole   (byte_string ANSISequence);

// [Text]

void OSWin32AcquireTextBackend  (void);
void OSWin32ReleaseTextBackend  (os_text_backend *TextBackend);
