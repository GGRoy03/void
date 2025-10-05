external void
ConsolePrintMessage(byte_string Message, ConsoleMessage_Severity Severity, ui_state *UIState)
{
    editor_console_ui *ConsoleUI = &EditorUI.ConsoleUI;

    byte_string Output = ByteStringCopy(Message, ConsoleUI->Arena);

    // NOTE: This sould not be done explicitly by the user.
    // Also, how do we want to set the text color? Just an attribute
    // surely...

    PushLayoutNodeParent(ConsoleUI->MessageScrollViewNode, ConsoleUI->Pipeline.LayoutTree);
    UILabel(ConsoleUI->MessageStyle, Output, &ConsoleUI->Pipeline, UIState);
    PopLayoutNodeParent(ConsoleUI->Pipeline.LayoutTree);
}

internal void
ConsoleUI(editor_console_ui *ConsoleUI, game_state *GameState, ui_state *UIState)
{
    if(!ConsoleUI->IsInitialized)
    {
        ui_pipeline_params PipelineParams = {0};
        {
            byte_string ThemeFiles[] =
            {
                byte_string_literal("styles/editor_console.cim"),
            };

            PipelineParams.ThemeFiles     = ThemeFiles;
            PipelineParams.ThemeCount     = ArrayCount(ThemeFiles);
            PipelineParams.TreeDepth      = 4;
            PipelineParams.TreeNodeCount  = 16;
            PipelineParams.RendererHandle = GameState->Renderer; // NOTE: Why do we do this?
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

        // Styles
        {
            ConsoleUI->WindowStyle       = UIGetCachedName(byte_string_literal("Console_Window")     , ConsoleUI->Pipeline.Registry);
            ConsoleUI->MessageStyle      = UIGetCachedName(byte_string_literal("Console_Message")    , ConsoleUI->Pipeline.Registry);
            ConsoleUI->MessageScrollView = UIGetCachedName(byte_string_literal("Console_MessageView"), ConsoleUI->Pipeline.Registry);
        }

        // Layout
        UIWindow(ConsoleUI->WindowStyle, &ConsoleUI->Pipeline);
        {
            UIScrollViewEx(ConsoleUI->MessageScrollViewNode, ConsoleUI->MessageScrollView, (&ConsoleUI->Pipeline))
            {
            }
        }

        ConsoleUI->IsInitialized = 1;
    }

    UIPipelineBegin(&ConsoleUI->Pipeline);

    // Drain The Queue
    {
        console_queue_node *Node = 0;
        while((Node = PopConsoleMessageQueue(&Console)))
        {
            ConsolePrintMessage(ByteString(Node->Value.Text, Node->Value.TextSize), Node->Value.Severity, UIState);
            FreeConsoleNode(Node);
        }
    }

    UIPipelineExecute(&ConsoleUI->Pipeline, &GameState->RenderPassList);
}
