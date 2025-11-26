// -----------------------------------------------------------------------------------
// Inputs Private Implementation

static bool
IsButtonStateClicked(os_button_state *State)
{
    bool Result = (State->EndedDown && State->HalfTransitionCount > 0);
    return Result;
}

static bool
IsKeyClicked(OSInputKey_Type Key)
{
    VOID_ASSERT(Key > OSInputKey_None && Key < OSInputKey_Count);

    os_inputs *Inputs = OSGetInputs();
    VOID_ASSERT(Inputs);

    bool Result = IsButtonStateClicked(&Inputs->KeyboardButtons[Key]);
    return Result;
}

// [Inputs]

static void 
ProcessInputMessage(os_button_state *NewState, bool IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown            = IsDown;
        NewState->HalfTransitionCount += 1;
    }
}

static vec2_float
OSGetMousePosition(void)
{
    os_inputs *Inputs = OSGetInputs();
    vec2_float   Result = Inputs->MousePosition;

    return Result;
}

static vec2_float
OSGetMouseDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    vec2_float   Result = Inputs->MouseDelta;

    return Result;
}

static float
OSGetScrollDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    float        Result = Inputs->ScrollDeltaInLines;

    return Result;
}

static bool
OSIsMouseClicked(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    bool              Result = (State->EndedDown && State->HalfTransitionCount > 0);

    return Result;
}

static bool
OSIsMouseHeld(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    bool              Result = (State->EndedDown && State->HalfTransitionCount == 0);

    return Result;
}

static bool 
OSIsMouseReleased(OSMouseButton_Type Button)
{
    os_inputs       *Inputs = OSGetInputs();
    os_button_state *State  = &Inputs->MouseButtons[Button];
    bool              Result = (!State->EndedDown && State->HalfTransitionCount > 0);

    return Result;
}

static void
OSClearInputs(os_inputs *Inputs)
{
    Inputs->ScrollDeltaInLines = 0.f;
    Inputs->MouseDelta         = vec2_float(0.f, 0.f);

    for (uint32_t Idx = 0; Idx < OS_KeyboardButtonCount; Idx++)
    {
        Inputs->KeyboardButtons[Idx].HalfTransitionCount = 0;
    }

    for (uint32_t Idx = 0; Idx < OS_MouseButtonCount; Idx++)
    {
        Inputs->MouseButtons[Idx].HalfTransitionCount = 0;
    }

    Inputs->ButtonBuffer.Count = 0;
    Inputs->UTF8Buffer.Count   = 0;
}

// [Agnostic File API]

static bool 
IsValidFile(os_read_file *File)
{
    bool Result = File->At < File->Content.Size;
    return Result;
}

static void 
SkipWhiteSpaces(os_read_file *File)
{
    while(IsValidFile(File) && IsWhiteSpace(PeekFile(File, 0)))
    {
        AdvanceFile(File, 1);
    }
}

static uint8_t *
PeekFilePointer(os_read_file *File)
{
    uint8_t *Result = &File->Content.String[File->At];
    return Result;
}

static uint8_t
PeekFile(os_read_file *File, int Offset)
{
    uint8_t Result = 0;

    if (File->At + Offset < File->Content.Size)
    {
        Result = File->Content.String[File->At + Offset];
    }

    return Result;
}

static void
AdvanceFile(os_read_file *File, uint32_t Count)
{
    File->At += Count;
}

// [Misc]

static bool
OSIsValidHandle(os_handle Handle)
{
    bool Result = (Handle.uint64_t[0] != 0);
    return Result;
}
