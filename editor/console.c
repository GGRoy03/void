typedef enum ConsoleStyle_Type
{
    ConsoleStyle_Window         = 1,
    ConsoleStyle_Header         = 2,
    ConsoleStyle_HeaderTitle    = 3,
    ConsoleStyle_HeaderStatus   = 4,
    ConsoleStyle_ScrollBuffer   = 5,
    ConsoleStyle_MessageInfo    = 6,
    ConsoleStyle_MessageWarning = 7,
    ConsoleStyle_MessageError   = 8,
    ConsoleStyle_Prompt         = 9,
    ConsoleStyle_Footer         = 10,
    ConsoleStyle_FooterText     = 11,
} ConsoleStyle_Type;

typedef struct console_output
{
    byte_string       Message;
    ConsoleStyle_Type Style;
} console_output;

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

            u8  FlagName[8]    = {0};
            u32 FlagNameLength = 0;

            while((Start < End) && (FlagNameLength < ArrayCount(FlagName) - 1) && !IsWhiteSpace(*Start))
            {
                FlagName[FlagNameLength++] = ToLowerChar(*Start++);
            }

            while(Start < End && IsWhiteSpace(*Start))
            {
                ++Start;
            }

            if(FlagNameLength > 0 && FlagName[0] == 'm')
            {
                u8  ArgBuffer[Kilobyte(1)];
                u32 ArgLength = 0;

                while ((Start < End) && !IsWhiteSpace(*Start) && (ArgLength < ArrayCount(ArgBuffer) - 1))
                {
                    ArgBuffer[ArgLength++] = *Start++;
                }
                ArgBuffer[ArgLength] = 0;

                if(ArgLength > 0)
                {
                    byte_string    Message = ByteString(0, ArgLength);
                    console_output Output  = FormatConsoleOutput(Message, ConsoleMessage_Info, Console->Arena);

                    // NOTE:
                    // We do not want to access the pipeline here.

                    ui_node BufferNode = FindNodeById(ui_id("Console_Buffer"), Console->Pipeline->IdTable);
                    if(BufferNode.CanUse)
                    {
                        ConsolePrintMessage(Output, BufferNode, Console);
                    }
                }
            }
        }
        else
        {
            // NOTE: Idk.

            ++Start;
        }
    }

    ui_node PromptNode = FindNodeById(ui_id("Console_Prompt"), Console->Pipeline->IdTable);
    if(PromptNode.CanUse)
    {
        UINodeClearText(PromptNode);
    }
}

// ------------------------------------------------------------------------------------
// Console Context Implementation

internal b32
InitializeConsoleUI(console_ui *Console)
{
    Assert(!Console->IsInitialized);

    Console->StatusText       = str8_lit("Hello?");
    Console->FooterText       = str8_lit("Hello!?");
    Console->MessageLimit     = ConsoleConstant_MessageCountLimit;
    Console->PromptBufferSize = ConsoleConstant_PromptBufferSize;

    memory_arena_params ArenaParams = {0};
    {
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Kilobyte(64);
        ArenaParams.CommitSize        = Kilobyte(4);
    }
    Console->Arena        = AllocateArena(ArenaParams);
    Console->PromptBuffer = PushArray(Console->Arena, u8, Console->PromptBufferSize);

    ui_pipeline_params PipelineParams =
    {
        .ThemeFile = byte_string_literal("styles/console.cim"),
    };
    Console->Pipeline = UICreatePipeline(PipelineParams);

    ui_subtree_params SubtreeParams =
    {
        .CreateNew = 1,
        .NodeCount = 128,
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

            ui_node ScrollBuffer = {0};
            UIBlock(ScrollBuffer = UIScrollableContent(UIAxis_Y, ConsoleStyle_ScrollBuffer))
            {
                UINodeSetId(ScrollBuffer, ui_id("Console_Buffer"));
                UINodeReserveChildren(ScrollBuffer, Console->MessageLimit);

            }

            ui_node Input = UITextInput(Console->PromptBuffer, Console->PromptBufferSize, ConsoleStyle_Prompt);
            {
                UINodeSetId(Input, ui_id("Console_Prompt"));
            }

            ui_node Footer = {0};
            UIBlock(Footer = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(Footer, ConsoleStyle_Footer);

                UILabel(Console->FooterText, ConsoleStyle_FooterText);
            }
        }
    }

    ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"));

    return 1;
}

internal void
ConsoleUI(console_ui *Console)
{
    if(!Console->IsInitialized)
    {
        Console->IsInitialized = InitializeConsoleUI(Console);
    }

    UIBeginAllSubtrees(Console->Pipeline);
    ClearArena(Console->Arena);

    // Read Inputs
    {
        if(IsKeyClicked(OSInputKey_Enter))
        {
            ReadConsolePrompt(Console);
        }
    }

    // Drain The Queue
    {
        ui_node BufferNode = FindNodeById(ui_id("Console_Buffer"), Console->Pipeline->IdTable);
        if (BufferNode.CanUse)
        {
            console_queue_node *Node = 0;
            while ((Node = PopConsoleMessageQueue(&UIState.Console)))
            {
                byte_string    RawMessage = ByteString(Node->Value.Text, Node->Value.TextSize);
                console_output Output     = FormatConsoleOutput(RawMessage, Node->Value.Severity, Console->Arena);
                ConsolePrintMessage(Output, BufferNode, Console);

                FreeConsoleNode(Node);
            }
        }
    }

    UIExecuteAllSubtrees(Console->Pipeline);
}
