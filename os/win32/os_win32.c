// [GLOBALS]

os_win32_state OSWin32State;

// [Internal Implementation]

internal HANDLE
OSWin32GetNativeHandle(os_handle Handle)
{
    HANDLE Result = (HANDLE)Handle.u64[0];
    return Result;
}

internal void
OSWin32SetConsoleColor(byte_string ANSISequence, WORD WinAttributes)
{
    b32  SupportsVT    = OSWin32State.ConsoleSupportsVT;
    HWND ConsoleHandle = OSWin32State.ConsoleHandle;

    if (SupportsVT)
    {
        OSWin32WriteToConsole(ANSISequence);
    }
    else if(ConsoleHandle != INVALID_HANDLE_VALUE)
    {
        SetConsoleTextAttribute(ConsoleHandle, WinAttributes);
    }
}

internal void
OSWin32WriteToConsole(byte_string ANSISequence)
{
    HWND          ConsoleHandle = OSWin32State.ConsoleHandle;
    memory_arena *Arena         = OSWin32State.Arena;

    if (ConsoleHandle != INVALID_HANDLE_VALUE)
    {
        wide_string WideString = ByteStringToWideString(Arena, ANSISequence);

        DWORD Written;
        WriteConsoleW(ConsoleHandle, WideString.String, (DWORD)WideString.Size, &Written, 0);

        PopArena(Arena, WideString.Size * sizeof(u16));
    }
}

internal LRESULT CALLBACK
OSWin32WindowProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    os_inputs *Inputs = &OSWin32State.Inputs;

    switch(Message)
    {

    case WM_SETCURSOR:
    {
        return TRUE;
    } break;

    case WM_MOUSEMOVE:
    {
        i32 MouseX = GET_X_LPARAM(LParam);
        i32 MouseY = GET_Y_LPARAM(LParam);

        Inputs->MouseDelta.X += (f32)(MouseX - Inputs->MousePosition.X);
        Inputs->MouseDelta.Y += (f32)(MouseY - Inputs->MousePosition.Y);

        Inputs->MousePosition.X = (f32)MouseX;
        Inputs->MousePosition.Y = (f32)MouseY;
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        u32 VKCode  = (u32)WParam;
        b32 WasDown = ((LParam & ((size_t)1 << 30)) != 0);
        b32 IsDown  = ((LParam & ((size_t)1 << 31)) == 0);

        if (WasDown != IsDown && VKCode < OS_KeyboardButtonCount)
        {
            ProcessInputMessage(&Inputs->KeyboardButtons[VKCode], IsDown);
        }
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

i32 WINAPI
wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR CmdLine, i32 ShowCmd)
{
    UNUSED(PrevInstance);
    UNUSED(CmdLine);
    UNUSED(Instance);

    // System Info
    {
        SYSTEM_INFO SystemInfo = {0};
        GetSystemInfo(&SystemInfo);

        OSWin32State.SystemInfo.PageSize = (u64)SystemInfo.dwPageSize;
    }

        // Memory
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = OSWin32State.SystemInfo.PageSize;
        Params.ReserveSize       = Kilobyte(10);

        OSWin32State.Arena = AllocateArena(Params);
    }

    // Window
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

        OSWin32State.WindowHandle = CreateWindowEx(0, WindowClass.lpszClassName,
                                                   "Engine", WS_OVERLAPPEDWINDOW,
                                                   0, 0, 1920, 1080, // WARN: Lazy
                                                   0, 0, WindowClass.hInstance, 0);
        ShowWindow(OSWin32State.WindowHandle, ShowCmd);
    }

    // Text
    {
        OSWin32State.TextBackend = PushArena(OSWin32State.Arena, sizeof(os_text_backend), AlignOf(os_text_backend));
        OSWin32AcquireTextBackend();
    }

    GameEntryPoint();

    return 0;
}

// [Per-OS API Memory Implementation]

internal void * 
OSReserveMemory(u64 Size)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
    return Result;
}

internal b32 
OSCommitMemory(void *Pointer, u64 Size)
{
    b32 Result = 0;
    if(Pointer)
    {
        Result = (VirtualAlloc(Pointer, Size, MEM_COMMIT, PAGE_READWRITE) != 0);
    }
    return Result;
}

internal void
OSRelease(void *Memory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

// [File Implementation - OS Specific]

internal byte_string
OSAppendToLaunchDirectory(byte_string Input, memory_arena *Arena)
{
    byte_string Result = ByteString(0, 0);

    u8    Directory[OS_MAX_PATH];
    DWORD Size = GetModuleFileNameA(0, (LPSTR)Directory, OS_MAX_PATH);
    if (Size)
    { 
        PathRemoveFileSpecA((LPSTR)Directory);

        u64 FinalSize = Size + Input.Size + 1;
        if (FinalSize <= OS_MAX_PATH)
        {
            Result.String = PushArray(Arena, u8, FinalSize);
            Result.Size   = FinalSize;

            snprintf((char *)Result.String, FinalSize, "%s/%s", Directory, Input.String);
        }
        else
        {
        }
    }
    else
    {
    }

    return Result;
}

internal os_handle
OSFindFile(byte_string Path)
{
    os_handle Handle = {0};

    if (Path.String)
    {
        Handle.u64[0] = (u64) CreateFileA((LPCSTR)Path.String, GENERIC_READ, FILE_SHARE_READ, NULL,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    return Handle;
}

internal u64
OSFileSize(os_handle Handle)
{
    u64 Result = 0;

    if (OSIsValidHandle(Handle))
    {
        HANDLE FileHandle = OSWin32GetNativeHandle(Handle);
        if (FileHandle)
        {
            LARGE_INTEGER NativeResult = { 0 };
            b32 Success = GetFileSizeEx(FileHandle, &NativeResult);
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

internal os_read_file
OSReadFile(os_handle Handle, memory_arena *Arena)
{
    os_read_file Result     = {0};
    HANDLE       FileHandle = (HANDLE)Handle.u64[0];

    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSizeWin32;
        if (GetFileSizeEx(FileHandle, &FileSizeWin32))
        {
            DWORD Unused       = 0;
            BOOL  Success      = 1;
            u64   FileSize     = FileSizeWin32.QuadPart;
            u64   ToRead       = FileSize;
            u32   ReadSize     = Kilobyte(8);
            u8   *WritePointer = 0;

            Result.Content.Size   = FileSize;
            Result.Content.String = PushArena(Arena, FileSize, AlignOf(u8));

            while (Success && ToRead > ReadSize)
            {
                WritePointer = Result.Content.String + (FileSize - ToRead);

                Success = ReadFile(FileHandle, WritePointer, ReadSize, &Unused, NULL);
                ToRead -= ReadSize;
            }

            if (Success)
            {
                WritePointer     = Result.Content.String + (FileSize - ToRead);
                Result.FullyRead = (b32)ReadFile(FileHandle, WritePointer, (u32)ToRead, &Unused, NULL);
            }
        }

    }

    return Result;
}

internal void
OSReleaseFile(os_handle Handle)
{
    HANDLE FileHandle = OSWin32GetNativeHandle(Handle);
    if (FileHandle)
    {
        CloseHandle(FileHandle);
    }
}

// [Windowing]

internal void
OSSetCursor(OSCursor_Type Type)
{   Assert(Type >= OSCursor_Default && Type <= OSCursor_GrabHand);

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

internal b32
OSUpdateWindow()
{
    b32 Result = 1;

    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        if(Message.message == WM_QUIT)
        {
            Result = 0;
            return Result;
        }

        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return Result;
}

internal void
OSSleep(u32 Milliseconds)
{
    Sleep(Milliseconds);
}

// [OS State | PER_OS]

internal os_system_info *
OSGetSystemInfo(void)
{
    os_system_info *Result = &OSWin32State.SystemInfo;
    return Result;
}

internal vec2_i32
OSGetClientSize(void)
{
    vec2_i32 Result = { 0 };
    HWND     Handle = OSWin32State.WindowHandle;

    if (Handle)
    {
        RECT ClientRect;
        GetClientRect(Handle, &ClientRect);

        Result.X = (ClientRect.right  - ClientRect.left);
        Result.Y = (ClientRect.bottom - ClientRect.top);
    }

    return Result;
}

internal vec2_f32
OSGetMousePosition(void)
{
    vec2_f32 Result = OSWin32State.Inputs.MousePosition;
    return Result;
}

internal vec2_f32
OSGetMouseDelta(void)
{
    vec2_f32 Result = OSWin32State.Inputs.MouseDelta;
    return Result;
}

internal b32
OSIsMouseClicked(OSMouseButton_Type Button)
{
    os_button_state *State = &OSWin32State.Inputs.MouseButtons[Button];

    b32 Result = (State->EndedDown && State->HalfTransitionCount > 0);
    return Result;
}

internal b32
OSIsMouseHeld(OSMouseButton_Type Button)
{
    os_button_state *State = &OSWin32State.Inputs.MouseButtons[Button];

    b32 Result = (State->EndedDown && State->HalfTransitionCount == 0);
    return Result;
}

internal b32 
OSIsMouseReleased(OSMouseButton_Type Button)
{
    os_button_state *State = &OSWin32State.Inputs.MouseButtons[Button];

    b32 Result = (!State->EndedDown && State->HalfTransitionCount > 0);
    return Result;
}

internal void
OSClearInputs(void)
{
    os_inputs *Inputs = &OSWin32State.Inputs;

    Inputs->ScrollDelta = 0.f;
    Inputs->MouseDelta  = Vec2F32(0.f, 0.f);

    for (u32 Idx = 0; Idx < OS_KeyboardButtonCount; Idx++)
    {
        Inputs->KeyboardButtons[Idx].HalfTransitionCount = 0;
    }

    for (u32 Idx = 0; Idx < OS_MouseButtonCount; Idx++)
    {
        Inputs->MouseButtons[Idx].HalfTransitionCount = 0;
    }
}

// [Per-OS API Crash/Debug Implementation]

internal void
OSAbort(i32 ExitCode)
{
    ExitProcess(ExitCode);
}

// [WIN32 SPECIFIC API]

internal HWND 
OSWin32GetWindowHandle(void)
{
    HWND Result = OSWin32State.WindowHandle;
    return Result;
}
