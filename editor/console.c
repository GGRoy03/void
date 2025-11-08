typedef struct console_output
{
    byte_string Message;
    ui_color    TextColor;
} console_output;

internal b32
IsValidConsoleOutput(console_output Output)
{
    b32 Result = IsValidByteString(Output.Message) && IsVisibleColor(Output.TextColor);
    return Result;
}

internal console_output
FormatConsoleOutput(byte_string Message, ConsoleMessage_Severity Severity, memory_arena *Arena)
{
    console_output Result = {0};

    if(IsValidByteString(Message))
    {
        ui_color    TextColor = UIColor(1.f, 1.f, 1.f, 0.f);
        byte_string Prefix    = ByteString(0, 0);

        switch(Severity)
        {

        case ConsoleMessage_Info:
        {
            TextColor = UIColor(0.01f, 0.71f, 0.99f, 1.f);
            Prefix    = byte_string_literal("[Info] => ");
        } break;

        case ConsoleMessage_Warn:
        {
            TextColor = UIColor(0.99f, 0.62f, 0.01f, 1.f);
            Prefix    = byte_string_literal("[Warn] => ");
        } break;

        case ConsoleMessage_Error:
        {
            TextColor = UIColor(0.99f, 0.01f, 0.11f, 1.f);
            Prefix    = byte_string_literal("[Error] => ");
        } break;

        case ConsoleMessage_Fatal:
        {
            TextColor = UIColor(0.99f, 0.01f, 0.11f, 1.f);
            Prefix    = byte_string_literal("[Fatal] => ");
        } break;

        default: break;

        }

        Result.Message   = ByteStringAppend(Message, Prefix, 0, Arena);
        Result.TextColor = TextColor;
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
        UINodeSetStyle(Child, ConsoleStyle_Message);
        UINodeSetTextColor(Child, Output.TextColor);
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
        .NodeCount = 1024,
    };

    UISubtree(SubtreeParams)
    {
        UIBlock(UIWindow(ConsoleStyle_Window))
        {
            ui_node ScrollBuffer = {0};
            UIBlock(ScrollBuffer = UIScrollableContent(UIAxis_Y, ConsoleStyle_MessageView))
            {
                UINodeSetId(ScrollBuffer, ui_id("Console_Buffer"));
                UINodeReserveChildren(ScrollBuffer, Console->MessageLimit);
            }

            ui_node Input = UITextInput(Console->PromptBuffer, Console->PromptBufferSize, ConsoleStyle_Prompt);
            {
                UINodeSetId(Input, ui_id("Console_Prompt"));
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
