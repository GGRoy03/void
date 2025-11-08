// -----------------------------------------------------------------------------------
// Inputs Private Implementation

internal b32
IsButtonStateClicked(os_button_state *State)
{
    b32 Result = (State->EndedDown && State->HalfTransitionCount > 0);
    return Result;
}

internal b32
IsKeyClicked(OSInputKey_Type Key)
{
    Assert(Key > OSInputKey_None && Key < OSInputKey_Count);

    os_inputs *Inputs = OSGetInputs();
    Assert(Inputs);

    b32 Result = IsButtonStateClicked(&Inputs->KeyboardButtons[Key]);
    return Result;
}

// [Inputs]

internal void 
ProcessInputMessage(os_button_state *NewState, b32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown            = IsDown;
        NewState->HalfTransitionCount += 1;
    }
}

internal vec2_f32
OSGetMousePosition(void)
{
    os_inputs *Inputs = OSGetInputs();
    vec2_f32   Result = Inputs->MousePosition;

    return Result;
}

internal vec2_f32
OSGetMouseDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    vec2_f32   Result = Inputs->MouseDelta;

    return Result;
}

internal f32
OSGetScrollDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    f32        Result = Inputs->ScrollDeltaInLines;

    return Result;
}

internal b32
OSIsMouseClicked(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    b32              Result = (State->EndedDown && State->HalfTransitionCount > 0);

    return Result;
}

internal b32
OSIsMouseHeld(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    b32              Result = (State->EndedDown && State->HalfTransitionCount == 0);

    return Result;
}

internal b32 
OSIsMouseReleased(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    b32              Result = (!State->EndedDown && State->HalfTransitionCount > 0);

    return Result;
}

internal void
OSClearInputs(os_inputs *Inputs)
{
    Inputs->ScrollDeltaInLines = 0.f;
    Inputs->MouseDelta         = Vec2F32(0.f, 0.f);

    for (u32 Idx = 0; Idx < OS_KeyboardButtonCount; Idx++)
    {
        Inputs->KeyboardButtons[Idx].HalfTransitionCount = 0;
    }

    for (u32 Idx = 0; Idx < OS_MouseButtonCount; Idx++)
    {
        Inputs->MouseButtons[Idx].HalfTransitionCount = 0;
    }

    Inputs->ButtonBuffer.Count = 0;
    Inputs->TextBuffer.Count   = 0;
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
