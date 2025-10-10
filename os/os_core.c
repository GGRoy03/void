internal void 
ProcessInputMessage(os_button_state *NewState, b32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown            = IsDown;
        NewState->HalfTransitionCount += 1;
    }
}

// [Agnostic File API]

internal b32 
IsValidFile(os_read_file *File)
{
    b32 Result = File->At < File->Content.Size;
    return Result;
}

internal void 
SkipWhiteSpaces(os_read_file *File)
{
    while(IsValidFile(File) && IsWhiteSpace(PeekFile(File, 0)))
    {
        AdvanceFile(File, 1);
    }
}

internal u8 *
PeekFilePointer(os_read_file *File)
{
    u8 *Result = &File->Content.String[File->At];
    return Result;
}

internal u8
PeekFile(os_read_file *File, i32 Offset)
{
    u8 Result = 0;

    if (File->At + Offset < File->Content.Size)
    {
        Result = File->Content.String[File->At + Offset];
    }

    return Result;
}

internal void
AdvanceFile(os_read_file *File, u32 Count)
{
    File->At += Count;
}

// [Misc]

internal b32
OSIsValidHandle(os_handle Handle)
{
    b32 Result = (Handle.u64[0] != 0);
    return Result;
}
