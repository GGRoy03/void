// WARN: WIP

external void
ConsolePrintMessage(byte_string Message, ConsoleMessage_Severity Severity)
{
    editor_console_ui *ConsoleUI = &EditorUI.ConsoleUI;
    ui_pipeline       *Pipeline  = &ConsoleUI->Pipeline;

    ui_color    TextColor = UIColor(1.f, 1.f, 1.f, 1.f);
    byte_string Prefix    = byte_string_literal("[Text]  => ");
    switch(Severity)
    {

    default: break;

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

    }

    Useless(Pipeline);
    Useless(TextColor);
    Useless(Prefix);
}

internal void
ConsoleUI(editor_console_ui *ConsoleUI, game_state *GameState)
{
    if(!ConsoleUI->IsInitialized)
    {
        ui_pipeline_params PipelineParams = {0};
        {
            byte_string ThemeFiles[] =
            {
                byte_string_literal("styles/editor_console.cim"),
            };

            PipelineParams.ThemeFiles    = ThemeFiles;
            PipelineParams.ThemeCount    = ArrayCount(ThemeFiles);
            PipelineParams.TreeDepth     = 4;
            PipelineParams.TreeNodeCount = 16;
        }
        ConsoleUI->Pipeline = UICreatePipeline(PipelineParams);

        memory_arena_params ArenaParams = { 0 };
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Kilobyte(10);
            ArenaParams.CommitSize        = Kilobyte(1);
        }
        ConsoleUI->Arena = AllocateArena(ArenaParams);

        // Layout

        ConsoleUI->IsInitialized = 1;
    }

    UIPipelineBegin(&ConsoleUI->Pipeline);

    // Drain The Queue
    {
        console_queue_node *Node = 0;
        while((Node = PopConsoleMessageQueue(&Console)))
        {
            ConsolePrintMessage(ByteString(Node->Value.Text, Node->Value.TextSize), Node->Value.Severity);
            FreeConsoleNode(Node);
        }
    }

    UIPipelineExecute(&ConsoleUI->Pipeline, &GameState->RenderPassList);
}
