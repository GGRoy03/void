#include <array>

constexpr auto MakeWin32InputTable()
{
    std::array<uint32_t, 256> Table = {};
    Table[0x41] = OSInputKey_A;
    Table[0x42] = OSInputKey_B;
    Table[0x43] = OSInputKey_C;
    Table[0x44] = OSInputKey_D;
    Table[0x45] = OSInputKey_E;
    Table[0x46] = OSInputKey_F;
    Table[0x47] = OSInputKey_G;
    Table[0x48] = OSInputKey_H;
    Table[0x49] = OSInputKey_I;
    Table[0x4A] = OSInputKey_J;
    Table[0x4B] = OSInputKey_K;
    Table[0x4C] = OSInputKey_L;
    Table[0x4D] = OSInputKey_M;
    Table[0x4E] = OSInputKey_N;
    Table[0x4F] = OSInputKey_O;
    Table[0x50] = OSInputKey_P;
    Table[0x51] = OSInputKey_Q;
    Table[0x52] = OSInputKey_R;
    Table[0x53] = OSInputKey_S;
    Table[0x54] = OSInputKey_T;
    Table[0x55] = OSInputKey_U;
    Table[0x56] = OSInputKey_V;
    Table[0x57] = OSInputKey_W;
    Table[0x58] = OSInputKey_X;
    Table[0x59] = OSInputKey_Y;
    Table[0x5A] = OSInputKey_Z;

    Table[0x30] = OSInputKey_0;
    Table[0x31] = OSInputKey_1;
    Table[0x32] = OSInputKey_2;
    Table[0x33] = OSInputKey_3;
    Table[0x34] = OSInputKey_4;
    Table[0x35] = OSInputKey_5;
    Table[0x36] = OSInputKey_6;
    Table[0x37] = OSInputKey_7;
    Table[0x38] = OSInputKey_8;
    Table[0x39] = OSInputKey_9;

    Table[VK_RETURN]     = OSInputKey_Enter;
    Table[VK_SPACE]      = OSInputKey_Space;
    Table[VK_OEM_MINUS]  = OSInputKey_Minus;
    Table[VK_OEM_PLUS]   = OSInputKey_Equals;
    Table[VK_OEM_4]      = OSInputKey_LeftBracket;
    Table[VK_OEM_6]      = OSInputKey_RightBracket;
    Table[VK_OEM_5]      = OSInputKey_Backslash;
    Table[VK_OEM_1]      = OSInputKey_Semicolon;
    Table[VK_OEM_7]      = OSInputKey_Apostrophe;
    Table[VK_OEM_COMMA]  = OSInputKey_Comma;
    Table[VK_OEM_PERIOD] = OSInputKey_Period;
    Table[VK_OEM_2]      = OSInputKey_Slash;
    Table[VK_OEM_3]      = OSInputKey_Grave;
    return Table;
}

constexpr auto Win32InputTable = MakeWin32InputTable();

static os_win32_state OSWin32State;

// [static Implementation]

static HANDLE
OSWin32GetNativeHandle(os_handle Handle)
{
    HANDLE Result = (HANDLE)Handle.uint64_t[0];
    return Result;
}

static LRESULT CALLBACK
OSWin32WindowProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    os_inputs *Inputs = &OSWin32State.Inputs;

    switch(Message)
    {

    case WM_MOUSEMOVE:
    {
        int MouseX = GET_X_LPARAM(LParam);
        int MouseY = GET_Y_LPARAM(LParam);

        Inputs->MouseDelta.X += (float)(MouseX - Inputs->MousePosition.X);
        Inputs->MouseDelta.Y += (float)(MouseY - Inputs->MousePosition.Y);

        Inputs->MousePosition.X = (float)MouseX;
        Inputs->MousePosition.Y = (float)MouseY;
    } break;

    case WM_MOUSEWHEEL:
    {
        int Delta = GET_WHEEL_DELTA_WPARAM(WParam) / 10;

        float Notches = Delta / (float)WHEEL_DELTA;
        float Lines   = Notches * Inputs->WheelScrollLine;

        Inputs->ScrollDeltaInLines += Lines;
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        uint32_t VKCode   = (uint32_t)WParam;
        uint32_t KeyIndex = Win32InputTable[VKCode];

        bool WasDown = ((LParam & ((size_t)1 << 30)) != 0);
        bool IsDown  = ((LParam & ((size_t)1 << 31)) == 0);

        if (WasDown != IsDown && VKCode < OS_KeyboardButtonCount)
        {
            ProcessInputMessage(&Inputs->KeyboardButtons[KeyIndex], IsDown);
        }

        os_button_action Action =
        {
            .IsPressed = ((!WasDown) && IsDown),
            .Keycode   = KeyIndex,
        };

        if(Inputs->ButtonBuffer.Count < Inputs->ButtonBuffer.Size)
        {
            Inputs->ButtonBuffer.Actions[Inputs->ButtonBuffer.Count++] = Action;
        }
    } break;

    case WM_CHAR:
    {
        WCHAR       WideChar = (WCHAR)WParam;
        wide_string String   = WideString((uint16_t *)&WideChar, 1);

        memory_region Local = EnterMemoryRegion(OSWin32State.Arena);

        byte_string New = WideStringToByteString(String, Local.Arena);

        uint8_t *Start = Inputs->UTF8Buffer.UTF8 + Inputs->UTF8Buffer.Count;
        uint8_t *End   = Inputs->UTF8Buffer.UTF8 + Inputs->UTF8Buffer.Size;

        if(IsValidByteString(New) && (Start + New.Size < End))
        {
            MemoryCopy(Start, New.String, New.Size);
            Inputs->UTF8Buffer.Count += New.Size;
        }

        LeaveMemoryRegion(Local);
    } break;

    case WM_LBUTTONDOWN:
    {
        ProcessInputMessage(&Inputs->MouseButtons[OSMouseButton_Left], 1);
    } break;

    case WM_LBUTTONUP:
    {
        ProcessInputMessage(&Inputs->MouseButtons[OSMouseButton_Left], 0);
    } break;

    case WM_RBUTTONDOWN:
    {
        ProcessInputMessage(&Inputs->MouseButtons[OSMouseButton_Right], 1);
    } break;

    case WM_RBUTTONUP:
    {
        ProcessInputMessage(&Inputs->MouseButtons[OSMouseButton_Right], 0);
    } break;

    case WM_CLOSE:
    {
        DestroyWindow(Handle);
        return 0;
    } break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    } break;

    }

    return DefWindowProc(Handle, Message, WParam, LParam);
}

static HWND
OSWin32InitializeWindow(vec2_int Size, int ShowCommand)
{
    WNDCLASSEX WindowClass = { 0 };
    WindowClass.cbSize        = sizeof(WNDCLASSEX);
    WindowClass.style         = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc   = OSWin32WindowProc;
    WindowClass.hInstance     = GetModuleHandle(0);
    WindowClass.hIcon         = LoadIcon(0, IDI_APPLICATION);
    WindowClass.hCursor       = LoadCursor(0, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    WindowClass.lpszMenuName  = 0;
    WindowClass.lpszClassName = "Game Window";
    WindowClass.hIconSm       = LoadIcon(0, IDI_APPLICATION);

    if(!RegisterClassEx(&WindowClass))
    {
        MessageBox(0, "Error registering class", "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND Handle = CreateWindowEx(0, WindowClass.lpszClassName,
                                 "Engine", WS_OVERLAPPEDWINDOW,
                                 0, 0, Size.X, Size.Y,
                                 0, 0, WindowClass.hInstance, 0);
    ShowWindow(Handle, ShowCommand);

    return Handle;
}

static os_system_info
OSWin32QuerySystemInfo(void)
{
    SYSTEM_INFO SystemInfo = {};
    GetSystemInfo(&SystemInfo);

    os_system_info Result = {};
    Result.PageSize       = SystemInfo.dwPageSize;
    Result.ProcessorCount = SystemInfo.dwNumberOfProcessors;

    return Result;
}

static vec2_int
OSWin32GetClientSize(HWND HWindow)
{
    vec2_int Result = vec2_int(0, 0);

    if(HWindow != INVALID_HANDLE_VALUE)
    {
        RECT ClientRect;
        GetClientRect(HWindow, &ClientRect);

        Result.X = ClientRect.right  - ClientRect.left;
        Result.Y = ClientRect.bottom - ClientRect.top;
    }

    return Result;
}

static int WINAPI
wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR CmdLine, int ShowCmd)
{
    VOID_UNUSED(PrevInstance);
    VOID_UNUSED(CmdLine);
    VOID_UNUSED(Instance);

    if(AllocConsole())
    {
        FILE *f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }

    // OS State (Always query system info first since arena depends on it)
    {
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &OSWin32State.Inputs.WheelScrollLine, 0);

        memory_arena_params ArenaParams = {0};
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = VOID_MEGABYTE(1);
            ArenaParams.CommitSize        = VOID_KILOBYTE(64);
        }

        OSWin32State.HWindow    = OSWin32InitializeWindow(vec2_int(1920,1080), ShowCmd);
        OSWin32State.SystemInfo = OSWin32QuerySystemInfo();
        OSWin32State.Arena      = AllocateArena(ArenaParams);

        OSWin32State.Inputs.ButtonBuffer.Size = 128;
        OSWin32State.Inputs.UTF8Buffer.Size   = VOID_KILOBYTE(1);

        OSWin32InitializeDWriteFactory(&OSWin32State.DWriteFactory);
    }

    CreateVoidContext(); // NOTE: Perhaps lazily intialized?

    // Render State
    {
        HWND     HWindow    = OSWin32State.HWindow;
        vec2_int ClientSize = OSWin32GetClientSize(HWindow);

        RenderState.Renderer = InitializeRenderer(HWindow, ClientSize, OSWin32State.Arena);
    }

    BeginProfile();
    uint32_t PrintProfilingFrame      = 0;
    uint32_t PrintProfilingFrameDelta = 1000;

    bool IsRunning = 1;
    while(IsRunning)
    {
        OSClearInputs(&OSWin32State.Inputs);

        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            if(Message.message == WM_QUIT)
            {
                IsRunning = 0;
                break;
            }

            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        vec2_int ClientSize = OSWin32GetClientSize(OSWin32State.HWindow);

        {
            TimeBlock("UI Logic");

            UIBeginFrame(ClientSize);
            UIEndFrame();
        }

        {
            TimeBlock("Render Commands");
            SubmitRenderCommands(RenderState.Renderer, ClientSize, &RenderState.PassList);
        }

        {
            TimeBlock("Sleeping");
            Sleep(5);
        }


        PrintProfilingFrame += 1;
        if(PrintProfilingFrame == PrintProfilingFrameDelta)
        {
            EndAndPrintProfile();
            PrintProfilingFrame = 0;
        }
    }

    // NOTE:
    // This shouldn't be needed but the debug layer for D3D is triggering
    // an error and preventing the window from closing if the resources
    // related to fonts aren't released. So this is for convenience :P

    ui_font_list *FontList = &(GetVoidContext().Fonts);
    IterateLinkedList(FontList, ui_font *, Font)
    {
        OSReleaseFontContext(&Font->OSContext);
    }

    return 0;
}

// [Per-OS API Memory Implementation]

static void * 
OSReserveMemory(uint64_t Size)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
    return Result;
}

static bool 
OSCommitMemory(void *Pointer, uint64_t Size)
{
    bool Result = 0;
    if(Pointer)
    {
        Result = (VirtualAlloc(Pointer, Size, MEM_COMMIT, PAGE_READWRITE) != 0);
    }
    return Result;
}

static void
OSRelease(void *Memory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

// [File Implementation - OS Specific]

static os_handle
OSFindFile(byte_string Path)
{
    os_handle Handle = {0};

    if (Path.String)
    {
        Handle.uint64_t[0] = (uint64_t) CreateFileA((LPCSTR)Path.String, GENERIC_READ, FILE_SHARE_READ, NULL,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    return Handle;
}

static uint64_t
OSFileSize(os_handle Handle)
{
    uint64_t Result = 0;

    if (OSIsValidHandle(Handle))
    {
        HANDLE FileHandle = OSWin32GetNativeHandle(Handle);
        if (FileHandle)
        {
            LARGE_INTEGER NativeResult = {};
            bool Success = GetFileSizeEx(FileHandle, &NativeResult);
            if (Success)
            {
                Result = NativeResult.QuadPart;
            }
            else
            {
            }
        }
    }

    return Result;
}

static os_read_file
OSReadFile(os_handle Handle, memory_arena *Arena)
{
    os_read_file Result     = {};
    HANDLE       FileHandle = (HANDLE)Handle.uint64_t[0];

    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSizeWin32;
        if (GetFileSizeEx(FileHandle, &FileSizeWin32))
        {
            DWORD Unused       = 0;
            BOOL  Success      = 1;
            uint64_t   FileSize     = FileSizeWin32.QuadPart;
            uint64_t   ToRead       = FileSize;
            uint32_t   ReadSize     = VOID_KILOBYTE(16);
            uint8_t   *WritePointer = 0;

            Result.Content.Size   = FileSize;
            Result.Content.String = (uint8_t *)PushArena(Arena, FileSize, AlignOf(uint8_t));

            while (Success && ToRead > ReadSize)
            {
                WritePointer = Result.Content.String + (FileSize - ToRead);

                Success = ReadFile(FileHandle, WritePointer, ReadSize, &Unused, NULL);
                ToRead -= ReadSize;
            }

            if (Success)
            {
                WritePointer     = Result.Content.String + (FileSize - ToRead);
                Result.FullyRead = (bool)ReadFile(FileHandle, WritePointer, (uint32_t)ToRead, &Unused, NULL);
            }
        }

    }

    return Result;
}

static void
OSReleaseFile(os_handle Handle)
{
    HANDLE FileHandle = OSWin32GetNativeHandle(Handle);
    if (FileHandle)
    {
        CloseHandle(FileHandle);
    }
}

// [Windowing]

static void
OSSetCursor(OSCursor_Type Type)
{
    VOID_ASSERT(Type >= OSCursor_Default && Type <= OSCursor_GrabHand);

    HCURSOR Cursor = 0;

    switch (Type)
    {
    case OSCursor_None:                      Cursor = LoadCursor(0, IDC_ARROW);    break;
    case OSCursor_Default:                   Cursor = LoadCursor(0, IDC_ARROW);    break;
    case OSCursor_EditText:                  Cursor = LoadCursor(0, IDC_IBEAM);    break;
    case OSCursor_Waiting:                   Cursor = LoadCursor(0, IDC_WAIT);     break;
    case OSCursor_Loading:                   Cursor = LoadCursor(0, IDC_WAIT);     break;
    case OSCursor_ResizeVertical:            Cursor = LoadCursor(0, IDC_SIZENS);   break;
    case OSCursor_ResizeHorizontal:          Cursor = LoadCursor(0, IDC_SIZEWE);   break;
    case OSCursor_ResizeDiagonalLeftToRight: Cursor = LoadCursor(0, IDC_SIZENWSE); break;
    case OSCursor_ResizeDiagonalRightToLeft: Cursor = LoadCursor(0, IDC_SIZENESW); break;
    case OSCursor_GrabHand:                  Cursor = LoadCursor(0, IDC_HAND);     break;
    }

    if (Cursor)
    {
        SetCursor(Cursor);
    }
}

// [Agnostic Queries]

static os_system_info *
OSGetSystemInfo(void)
{
    os_system_info *Result = &OSWin32State.SystemInfo;
    return Result;
}

static os_inputs *
OSGetInputs(void)
{
    os_inputs *Result = &OSWin32State.Inputs;
    return Result;
}

static os_button_playback *
OSGetButtonPlayback(void)
{
    os_inputs          *Inputs = &OSWin32State.Inputs;
    os_button_playback *Result = &Inputs->ButtonBuffer;
    return Result;
}

static os_utf8_playback *
OSGetTextPlayback(void)
{
    os_inputs        *Inputs = &OSWin32State.Inputs;
    os_utf8_playback *Result = &Inputs->UTF8Buffer;
    return Result;
}

static uint64_t
OSReadTimer(void)
{
    LARGE_INTEGER Value;
    QueryPerformanceCounter(&Value);
    return Value.QuadPart;
}

static uint64_t
OSGetTimerFrequency(void)
{
    LARGE_INTEGER Freq;
    QueryPerformanceFrequency(&Freq);
    return Freq.QuadPart;
}
