typedef enum ConsoleStyle_Type
{
    ConsoleStyle_Window          = 1,
    ConsoleStyle_Header          = 2,
    ConsoleStyle_HeaderStatus    = 3,
    ConsoleStyle_WindowContainer = 4,
    ConsoleStyle_ScrollWindow    = 5,
    ConsoleStyle_InspectWindow   = 6,
    ConsoleStyle_MessageInfo     = 7,
    ConsoleStyle_MessageWarning  = 8,
    ConsoleStyle_MessageError    = 9,
    ConsoleStyle_Prompt          = 10,
} ConsoleStyle_Type;

typedef enum ConsoleMode_Type
{
    ConsoleMode_Default = 0, // Console
    ConsoleMode_Inspect = 1, // Console + Inspect (Dumps Info On Hover)
} ConsoleMode_Type;

typedef enum ConsoleConstant_Type
{
    ConsoleConstant_MessageCountLimit = 60,
    ConsoleConstant_MessageSizeLimit  = Kilobyte(2),
    ConsoleConstant_PromptBufferSize  = 128,
    ConsoleConstant_FlagSizeLimit     = 16,
} ConsoleConstant_Type;

typedef struct console_output
{
    byte_string       Message;
    ConsoleStyle_Type Style;
} console_output;

typedef struct console_ui
{
    ui_pipeline  *Pipeline;
    b32           IsInitialized;

    // State
    ConsoleMode_Type Mode;
    byte_string      StatusText;
    u8               PromptBuffer[ConsoleConstant_PromptBufferSize];
    u64              PromptBufferSize;

    // Transient
    memory_arena *FrameArena;

    // Scroll Buffer State
    u32 MessageLimit;
    u32 MessageHead;
    u32 MessageTail;
} console_ui;

internal b32
IsValidConsoleOutput(console_output Output)
{
    b32 Result = IsValidByteString(Output.Message) && (Output.Style >= ConsoleStyle_MessageInfo && Output.Style <= ConsoleStyle_MessageError);
    return Result;
}

internal console_output
FormatConsoleOutput(byte_string Message, ConsoleMessage_Severity Severity, memory_arena *Arena)
{
    console_output Result = {0};

    if(IsValidByteString(Message))
    {
        ConsoleStyle_Type Style  = ConsoleStyle_MessageInfo;
        byte_string       Prefix = ByteString(0, 0);

        switch(Severity)
        {

        case ConsoleMessage_Info:
        {
            Style  = ConsoleStyle_MessageInfo;
            Prefix = byte_string_literal("[Info] => ");
        } break;

        case ConsoleMessage_Warn:
        {
            Style  = ConsoleStyle_MessageWarning;
            Prefix = byte_string_literal("[Warn] => ");
        } break;

        case ConsoleMessage_Error:
        {
            Style  = ConsoleStyle_MessageError;
            Prefix = byte_string_literal("[Error] => ");
        } break;

        case ConsoleMessage_Fatal:
        {
            Style  = ConsoleStyle_MessageError;
            Prefix = byte_string_literal("[Fatal] => ");
        } break;

        default: break;

        }

        Result.Message = ByteStringAppend(Message, Prefix, 0, Arena);
        Result.Style   = Style;
    }

    return Result;
}

internal void
ConsolePrintMessage(console_output Output, ui_node BufferNode, console_ui *Console)
{
    Assert(BufferNode.CanUse);
    Assert(IsValidConsoleOutput(Output));

    u32 ChildIndex = Console->MessageHead++;

    ui_node Child = UINodeFindChild(BufferNode, ChildIndex);
    {
        UINodeSetStyle(Child, Output.Style);
        UINodeSetText(Child, Output.Message);
    }
}

// -----------------------------------------------------------------------------------
// Console Prompt Internal Implementation

internal void
HandleConsoleFlag(byte_string Flag, console_ui *Console)
{
    byte_string InspectFlag = byte_string_compile("inspect");
    if(ByteStringMatches(InspectFlag, Flag, NoFlag))
    {
        Console->Mode = ConsoleMode_Inspect;

        ui_node ContentWindows = UIFindNodeById(ui_id("Console_ContentWindows"), Console->Pipeline->IdTable);
        if(ContentWindows.CanUse)
        {
            ui_node InspectWindow = {0};
            UIBlock(InspectWindow = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(InspectWindow, ConsoleStyle_InspectWindow);

                UILabel(str8_lit("Testing child append and creating windows"), ConsoleStyle_HeaderStatus);
            }

            UINodeAppendChild(ContentWindows, InspectWindow);
        }
    }

    byte_string QuitFlag = byte_string_compile("quit");
    if(ByteStringMatches(QuitFlag, Flag, NoFlag))
    {
        Console->Mode = ConsoleMode_Default;
    }
}

internal void
ReadConsolePrompt(console_ui *Console)
{
    Assert(Console);

    u8 *Start = Console->PromptBuffer;
    u32 Size  = Console->PromptBufferSize;
    u8 *End   = Start + Size;

    while(Start < End)
    {
        while(Start < End && IsWhiteSpace(*Start))
        {
            ++Start;
        }

        if(Start >= End)
        {
            break;
        }

        if(*Start == '/')
        {
            ++Start;

            byte_string Flag = ByteString(0, 0);
            Flag.String = PushArray(Console->FrameArena, u8, ConsoleConstant_FlagSizeLimit);

            while((Start < End) && (Flag.Size < ConsoleConstant_FlagSizeLimit) && !IsWhiteSpace(*Start))
            {
                Flag.String[Flag.Size++] = ToLowerChar(*Start++);
            }

            while(Start < End && IsWhiteSpace(*Start))
            {
                ++Start;
            }

            HandleConsoleFlag(Flag, Console);
        }
        else
        {
            Assert(!"Unsure");
        }
    }

    ui_node PromptNode = UIFindNodeById(ui_id("Console_Prompt"), Console->Pipeline->IdTable);
    if(PromptNode.CanUse)
    {
        UINodeClearTextInput(PromptNode);
    }
}

internal UIEvent_State
ConsolePrompt_PressedKey(OSInputKey_Type Key, void *UserData)
{
    console_ui *Console = (console_ui *)UserData;
    Assert(Console);

    if(Key == OSInputKey_Enter)
    {
        ReadConsolePrompt(Console);
        return UIEvent_Handled;
    }

    return UIEvent_Untouched;
}

// ------------------------------------------------------------------------------------
// Console Context Implementation

internal b32
InitializeConsoleUI(console_ui *Console)
{
    Assert(!Console->IsInitialized);

    Console->StatusText       = str8_lit("We should display useful information here.");
    Console->MessageLimit     = ConsoleConstant_MessageCountLimit;
    Console->PromptBufferSize = ConsoleConstant_PromptBufferSize;

    memory_arena_params ArenaParams = {0};
    {
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Kilobyte(64);
        ArenaParams.CommitSize        = Kilobyte(4);
    }
    Console->FrameArena   = AllocateArena(ArenaParams);

    ui_pipeline_params PipelineParams =
    {
        .ThemeFile = byte_string_literal("styles/console.cim"),
    };
    Console->Pipeline = UICreatePipeline(PipelineParams);

    ui_subtree_params SubtreeParams =
    {
        .CreateNew = 1,
        .NodeCount = 256,
    };

    UISubtree(SubtreeParams)
    {
        UIBlock(UIWindow(ConsoleStyle_Window))
        {
            ui_node Header = {0};
            UIBlock(Header = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(Header, ConsoleStyle_Header);

                UILabel(Console->StatusText, ConsoleStyle_HeaderStatus);
            }

            ui_node ContentWindows = {0};
            UIBlock(ContentWindows = UINode(UILayoutNode_IsParent))
            {
                UINodeSetId(ContentWindows, ui_id("Console_ContentWindows"));
                UINodeSetStyle(ContentWindows, ConsoleStyle_WindowContainer);

                ui_node ScrollBuffer = {0};
                UIBlock(ScrollBuffer = UIScrollableContent(UIAxis_Y, ConsoleStyle_ScrollWindow))
                {
                    UINodeSetId(ScrollBuffer, ui_id("Console_Buffer"));
                    UINodeReserveChildren(ScrollBuffer, Console->MessageLimit);
                }
            }

            ui_node Input = UITextInput(Console->PromptBuffer, Console->PromptBufferSize, ConsoleStyle_Prompt);
            {
                UINodeSetId(Input, ui_id("Console_Prompt"));
                UINodeListenOnKey(Input, ConsolePrompt_PressedKey, Console);
            }
        }
    }

    ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"));

    return 1;
}

internal void
ShowConsoleUI(void)
{
    static console_ui Console;

    if(!Console.IsInitialized)
    {
        Console.IsInitialized = InitializeConsoleUI(&Console);
    }

    UIBeginAllSubtrees(Console.Pipeline);
    ClearArena(Console.FrameArena);

    // Drain The Queue
    {
        ui_node BufferNode = UIFindNodeById(ui_id("Console_Buffer"), Console.Pipeline->IdTable);
        if (BufferNode.CanUse)
        {
            console_queue_node *Node = 0;
            while ((Node = PopConsoleMessageQueue(&UIState.Console)))
            {
                byte_string    RawMessage = ByteString(Node->Value.Text, Node->Value.TextSize);
                console_output Output     = FormatConsoleOutput(RawMessage, Node->Value.Severity, Console.FrameArena);
                ConsolePrintMessage(Output, BufferNode, &Console);

                FreeConsoleNode(Node);
            }
        }
    }

    UIExecuteAllSubtrees(Console.Pipeline);
}
