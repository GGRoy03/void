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

static float
OSGetScrollDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    float      Result = Inputs->ScrollDeltaInLines;

    return Result;
}

static void
OSClearInputs(os_inputs *Inputs)
{
    Inputs->ScrollDeltaInLines = 0.f;

    for (uint32_t Idx = 0; Idx < OS_KeyboardButtonCount; Idx++)
    {
        Inputs->KeyboardButtons[Idx].HalfTransitionCount = 0;
    }

    // TODO: Pointer clear?
}

// [Agnostic File API]

static bool 
IsValidFile(os_read_file *File)
{
    bool Result = File->At < File->Content.Size;
    return Result;
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
