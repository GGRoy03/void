external void
ConsolePrintMessage(byte_string Message, ConsoleMessage_Severity Severity, ui_state *UIState)
{
    editor_console_ui *Console = &EditorUI.ConsoleUI;

    byte_string Output = ByteStringCopy(Message, Console->Arena);

    // NOTE: This sould not be done explicitly by the user.
    // Also, how do we want to set the text color? Just an attribute
    // surely...

    PushLayoutNodeParent(Console->MessageScrollViewNode, Console->Pipeline.LayoutTree);
    UILabel(Console->MessageStyle, Output, &Console->Pipeline, UIState);
    PopLayoutNodeParent(Console->Pipeline.LayoutTree);
}

internal void
ConsoleUI(editor_console_ui *Console, game_state *GameState, ui_state *UIState)
{
    if(!Console->IsInitialized)
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
        Console->Pipeline = UICreatePipeline(PipelineParams);

        memory_arena_params ArenaParams = { 0 };
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Kilobyte(10);
            ArenaParams.CommitSize        = Kilobyte(1);
        }
        Console->Arena = AllocateArena(ArenaParams);

        // Styles
        {
            Console->WindowStyle       = UIGetCachedName(byte_string_literal("Console_Window")     , Console->Pipeline.Registry);
            Console->MessageStyle      = UIGetCachedName(byte_string_literal("Console_Message")    , Console->Pipeline.Registry);
            Console->MessageScrollView = UIGetCachedName(byte_string_literal("Console_MessageView"), Console->Pipeline.Registry);
        }

        // Layout
        UIWindow(Console->WindowStyle, &Console->Pipeline);
        {
            UIScrollViewEx(Console->MessageScrollViewNode, Console->MessageScrollView, (&Console->Pipeline))
            {
            }
        }

        Console->IsInitialized = 1;
    }

    UIPipelineBegin(&Console->Pipeline);

    // Drain The Queue
    {
        console_queue_node *Node = 0;
        while((Node = PopConsoleMessageQueue(&UIState->Console)))
        {
            ConsolePrintMessage(ByteString(Node->Value.Text, Node->Value.TextSize), Node->Value.Severity, UIState);
            FreeConsoleNode(Node);
        }
    }

    UIPipelineExecute(&Console->Pipeline, &GameState->RenderPassList);
}
