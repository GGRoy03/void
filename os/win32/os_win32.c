os_win32_state OSWin32State;

// [Internal Implementation]

internal HANDLE
OSWin32GetNativeHandle(os_handle Handle)
{
    HANDLE Result = (HANDLE)Handle.u64[0];
    return Result;
}

internal LRESULT CALLBACK
OSWin32WindowProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    os_inputs *Inputs = &OSWin32State.Inputs;

    switch(Message)
    {

    case WM_MOUSEMOVE:
    {
        i32 MouseX = GET_X_LPARAM(LParam);
        i32 MouseY = GET_Y_LPARAM(LParam);

        Inputs->MouseDelta.X += (f32)(MouseX - Inputs->MousePosition.X);
        Inputs->MouseDelta.Y += (f32)(MouseY - Inputs->MousePosition.Y);

        Inputs->MousePosition.X = (f32)MouseX;
        Inputs->MousePosition.Y = (f32)MouseY;
    } break;

    case WM_MOUSEWHEEL:
    {
        i32 Delta = GET_WHEEL_DELTA_WPARAM(WParam) / 10;

        f32 Notches = Delta / (f32)WHEEL_DELTA;
        f32 Lines   = Notches * Inputs->WheelScrollLine;

        Inputs->ScrollDeltaInLines += Lines;
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        u32 VKCode   = (u32)WParam;
        u32 KeyIndex = Win32InputTable[VKCode];

        b32 WasDown = ((LParam & ((size_t)1 << 30)) != 0);
        b32 IsDown  = ((LParam & ((size_t)1 << 31)) == 0);

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
        wide_string String   = WideString(&WideChar, 1);

        memory_region Local = EnterMemoryRegion(OSWin32State.Arena);

        byte_string New = WideStringToByteString(String, Local.Arena);

        u8 *Start = Inputs->UTF8Buffer.UTF8 + Inputs->UTF8Buffer.Count;
        u8 *End   = Inputs->UTF8Buffer.UTF8 + Inputs->UTF8Buffer.Size;

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

internal HWND
OSWin32InitializeWindow(vec2_i32 Size, i32 ShowCommand)
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

internal os_system_info
OSWin32QuerySystemInfo(void)
{
    SYSTEM_INFO SystemInfo = {0};
    GetSystemInfo(&SystemInfo);

    os_system_info Result = {0};
    Result.PageSize       = SystemInfo.dwPageSize;
    Result.ProcessorCount = SystemInfo.dwNumberOfProcessors;

    return Result;
}

internal vec2_i32
OSWin32GetClientSize(HWND HWindow)
{
    vec2_i32 Result = Vec2I32(0, 0);

    if(HWindow != INVALID_HANDLE_VALUE)
    {
        RECT ClientRect;
        GetClientRect(HWindow, &ClientRect);

        Result.X = ClientRect.right  - ClientRect.left;
        Result.Y = ClientRect.bottom - ClientRect.top;
    }

    return Result;
}

i32 WINAPI
wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR CmdLine, i32 ShowCmd)
{
    Useless(PrevInstance);
    Useless(CmdLine);
    Useless(Instance);

    // OS State (Always query system info first since arena depends on it)
    {
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &OSWin32State.Inputs.WheelScrollLine, 0);

        memory_arena_params ArenaParams = {0};
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(64);
        }

        OSWin32State.HWindow    = OSWin32InitializeWindow(Vec2I32(1920,1080), ShowCmd);
        OSWin32State.SystemInfo = OSWin32QuerySystemInfo();
        OSWin32State.Arena      = AllocateArena(ArenaParams);

        OSWin32State.Inputs.ButtonBuffer.Size = 128;
        OSWin32State.Inputs.UTF8Buffer.Size   = Kilobyte(1);

        OSWin32InitializeDWriteFactory(&OSWin32State.DWriteFactory);
    }

    // UI State
    {
        // Static Data
        {
            memory_arena_params Params = { 0 };
            {
                Params.AllocatedFromFile = __FILE__;
                Params.AllocatedFromLine = __LINE__;
                Params.ReserveSize       = Megabyte(1);
                Params.CommitSize        = Kilobyte(64);
            }

            UIState.StaticData       = AllocateArena(Params);
            UIState.Pipelines.Values = PushArray(UIState.StaticData, ui_pipeline, UIPipeline_Count);
            UIState.Pipelines.Size   = UIPipeline_Count;
        }

        // Resource Table
        {
            ui_resource_table_params  Params = { 0 };
            void                     *Memory = 0;
            {
                Params.HashSlotCount = 512;
                Params.EntryCount    = 2048;

                u64 Footprint = GetResourceTableFootprint(Params);
                Memory = PushArray(UIState.StaticData, u8, Footprint);
            }
            UIState.ResourceTable = PlaceResourceTableInMemory(Params, Memory);
        }

        // NOTE: Should this be here?
        InitializeConsoleMessageQueue(&UIState.Console);
    }

    // Render State
    {
        HWND     HWindow    = OSWin32State.HWindow;
        vec2_i32 ClientSize = OSWin32GetClientSize(HWindow);

        RenderState.Renderer = InitializeRenderer(HWindow, ClientSize, OSWin32State.Arena);
    }

    b32 IsRunning = 1;
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

        vec2_i32 ClientSize = OSWin32GetClientSize(OSWin32State.HWindow);

        UIBeginFrame(ClientSize);
        ShowConsoleUI();
        UIEndFrame();

        SubmitRenderCommands(RenderState.Renderer, ClientSize, &RenderState.PassList);

        Sleep(5);
    }

    // NOTE:
    // This shouldn't be needed but the debug layer for D3D is triggering
    // an error and preventing the window from closing if the resources
    // related to fonts aren't released. So this is for convenience :P

    ui_font_list *FontList = &UIState.Fonts;
    IterateLinkedList(FontList, ui_font *, Font)
    {
        OSReleaseFontContext(&Font->OSContext);
    }

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
            u32   ReadSize     = Kilobyte(16);
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
{
    Assert(Type >= OSCursor_Default && Type <= OSCursor_GrabHand);

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

internal os_system_info *
OSGetSystemInfo(void)
{
    os_system_info *Result = &OSWin32State.SystemInfo;
    return Result;
}

internal os_inputs *
OSGetInputs(void)
{
    os_inputs *Result = &OSWin32State.Inputs;
    return Result;
}

internal os_button_playback *
OSGetButtonPlayback(void)
{
    os_inputs          *Inputs = &OSWin32State.Inputs;
    os_button_playback *Result = &Inputs->ButtonBuffer;
    return Result;
}

internal os_utf8_playback *
OSGetTextPlayback(void)
{
    os_inputs        *Inputs = &OSWin32State.Inputs;
    os_utf8_playback *Result = &Inputs->UTF8Buffer;
    return Result;
}
