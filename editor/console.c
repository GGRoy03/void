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
ConsolePrintMessage(console_output Output, ui_node BufferNode, editor_console_ui *Console)
{
    if(IsValidConsoleOutput(Output))
    {
        u32 ChildIndex = Console->MessageHead++;

        UIChain(BufferNode)
            ->FindChild(ChildIndex)
            ->SetStyle(ConsoleStyle_Message)
            ->SetTextColor(Output.TextColor)
            ->SetText(Output.Message);
    }
}

internal b32
InitializeConsoleUI(editor_console_ui *Console)
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
            UIBlock(
            UIScrollView(ConsoleStyle_MessageView)
                ->SetId(ui_id("Console_Buffer"))
                ->ReserveChildren(Console->MessageLimit))
            {
            }

            UITextInput(Console->PromptBuffer, Console->PromptBufferSize, ConsoleStyle_Prompt);
        }
    }

    ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"));

    return 1;
}

internal void
ConsoleUI(editor_console_ui *Console)
{
    if(!Console->IsInitialized)
    {
        Console->IsInitialized = InitializeConsoleUI(Console);
    }

    UIBeginAllSubtrees(Console->Pipeline);
    ClearArena(Console->Arena);

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
