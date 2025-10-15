external void
ConsolePrintMessage(byte_string Message, ConsoleMessage_Severity Severity, ui_layout_node *ScrollBuffer)
{
    editor_console_ui *ConsoleUI = &EditorUI.ConsoleUI;

    ui_color    TextColor = UIColor(1.f, 1.f, 1.f, 1.f);
    byte_string Prefix    = ByteString(0, 0);

    switch(Severity)
    {

    case ConsoleMessage_Info:
    {
        TextColor = UIColor(0.01f, 0.71f, 0.99f, 1.f);
        Prefix    = byte_string_literal("[Info]  => ");
    } break;

    case ConsoleMessage_Warn:
    {
        TextColor = UIColor(0.99f, 0.62f, 0.01f, 1.f);
        Prefix    = byte_string_literal("[Warn]  => ");
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

    memory_region Local = EnterMemoryRegion(ConsoleUI->Arena);

    byte_string FormattedMessage = ByteStringAppend(Message, Prefix, 0, Local.Arena);

    // NOTE: Is there really no way to access the last added node?

    UISubtreeBlock(ScrollBuffer, ConsoleUI->Pipeline)
    {
        UILabel(ConsoleStyle_Message, FormattedMessage, ConsoleUI->Pipeline);
        UISetTextColor(TextColor, ConsoleUI->Pipeline);
    }

    LeaveMemoryRegion(Local);

    Useless(TextColor);
}

internal void
ConsoleUI(editor_console_ui *ConsoleUI)
{
    if(!ConsoleUI->IsInitialized)
    {
        ui_pipeline_params PipelineParams = {0};
        {
            PipelineParams.ThemeFile = byte_string_literal("styles/editor_console.cim");
        }
        ConsoleUI->Pipeline = UICreatePipeline(PipelineParams);

        memory_arena_params ArenaParams = {0};
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Kilobyte(64);
            ArenaParams.CommitSize        = Kilobyte(4);
        }
        ConsoleUI->Arena = AllocateArena(ArenaParams);

        UISubtreeBlock(0, ConsoleUI->Pipeline)
        {
            UIWindowBlock(ConsoleStyle_Window, ConsoleUI->Pipeline)
            {
                UIScrollViewBlockID(ui_id("Console_ScrollBuffer"), ConsoleStyle_MessageView, ConsoleUI->Pipeline)
                {
                }
            }
        }

        ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), &Console);
        ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"), &Console);

        ConsoleUI->IsInitialized = 1;
    }

    UIPipelineBegin(ConsoleUI->Pipeline);

    // Drain The Queue
    {
        ui_layout_node     *ScrollBuffer = UIFindNodeById(ui_id("Console_ScrollBuffer"), ConsoleUI->Pipeline);
        console_queue_node *Node         = 0;

        while((Node = PopConsoleMessageQueue(&Console)))
        {
            ConsolePrintMessage(ByteString(Node->Value.Text, Node->Value.TextSize), Node->Value.Severity, ScrollBuffer);
            FreeConsoleNode(Node);
        }
    }

    UIPipelineExecute(ConsoleUI->Pipeline);
}
