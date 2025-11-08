#pragma once

// [INCLUDES & LINKING]

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <imm.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Gdi32.lib")

typedef struct IDWriteFactory IDWriteFactory;
typedef struct os_text_input os_text_input;

typedef struct os_win32_state
{
    // Memory
    memory_arena *Arena;

    // External (Queried by agnostic code)
    os_system_info SystemInfo;
    os_inputs      Inputs;

    // Internal (Queried by win32 specific code)
    IDWriteFactory *DWriteFactory;
    HWND            HWindow;
} os_win32_state;

extern os_win32_state OSWin32State;

read_only global u32 Win32InputTable[256] =
{
    [0x41] = OSInputKey_A,
    [0x42] = OSInputKey_B,
    [0x43] = OSInputKey_C,
    [0x44] = OSInputKey_D,
    [0x45] = OSInputKey_E,
    [0x46] = OSInputKey_F,
    [0x47] = OSInputKey_G,
    [0x48] = OSInputKey_H,
    [0x49] = OSInputKey_I,
    [0x4A] = OSInputKey_J,
    [0x4B] = OSInputKey_K,
    [0x4C] = OSInputKey_L,
    [0x4D] = OSInputKey_M,
    [0x4E] = OSInputKey_N,
    [0x4F] = OSInputKey_O,
    [0x50] = OSInputKey_P,
    [0x51] = OSInputKey_Q,
    [0x52] = OSInputKey_R,
    [0x53] = OSInputKey_S,
    [0x54] = OSInputKey_T,
    [0x55] = OSInputKey_U,
    [0x56] = OSInputKey_V,
    [0x57] = OSInputKey_W,
    [0x58] = OSInputKey_X,
    [0x59] = OSInputKey_Y,
    [0x5A] = OSInputKey_Z,

    [0x30] = OSInputKey_0,
    [0x31] = OSInputKey_1,
    [0x32] = OSInputKey_2,
    [0x33] = OSInputKey_3,
    [0x34] = OSInputKey_4,
    [0x35] = OSInputKey_5,
    [0x36] = OSInputKey_6,
    [0x37] = OSInputKey_7,
    [0x38] = OSInputKey_8,
    [0x39] = OSInputKey_9,

    [VK_RETURN]          = OSInputKey_Enter,
    [VK_SPACE]           = OSInputKey_Space,
    [VK_OEM_MINUS]       = OSInputKey_Minus,
    [VK_OEM_PLUS]        = OSInputKey_Equals,
    [VK_OEM_4]           = OSInputKey_LeftBracket,
    [VK_OEM_6]           = OSInputKey_RightBracket,
    [VK_OEM_5]           = OSInputKey_Backslash,
    [VK_OEM_1]           = OSInputKey_Semicolon,
    [VK_OEM_7]           = OSInputKey_Apostrophe,
    [VK_OEM_COMMA]       = OSInputKey_Comma,
    [VK_OEM_PERIOD]      = OSInputKey_Period,
    [VK_OEM_2]           = OSInputKey_Slash,
    [VK_OEM_3]           = OSInputKey_Grave,
};
