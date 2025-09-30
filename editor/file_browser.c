// [Handlers]

internal void
FileBrowserForwardHistory(ui_node *Node, ui_pipeline *Pipeline)
{   UNUSED(Node); UNUSED(Pipeline);

}

internal void
FileBrowserBackwardHistory(ui_node *Node, ui_pipeline *Pipeline)
{   UNUSED(Node); UNUSED(Pipeline);

}

// [UI]

internal void
FileBrowserUI(file_browser_ui *UI, render_pass_list *PassList, render_handle Renderer, ui_state *UIState)
{
    UNUSED(UIState);

    if(!UI->IsInitialized)
    {

        ui_pipeline_params PipelineParams = {0};
        {
            byte_string ThemeFiles[] = 
            {
                byte_string_literal("styles/file_browser.cim"),
            };

            PipelineParams.ThemeFiles     = ThemeFiles;
            PipelineParams.ThemeCount     = ArrayCount(ThemeFiles);
            PipelineParams.TreeDepth      = 4;
            PipelineParams.TreeNodeCount  = 16;
            PipelineParams.RendererHandle = Renderer;
        }

        UI->Pipeline = UICreatePipeline(PipelineParams);

        memory_arena_params ArenaParams = {0};
        {
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Kilobyte(10);
            ArenaParams.CommitSize        = Kilobyte(1);
        }

        UI->Arena = AllocateArena(ArenaParams);

        // Styles
        {
            UI->MainWindowStyle   = UIGetCachedName(byte_string_literal("FileBrowser_MainWindow")  , UI->Pipeline.StyleRegistery);
            UI->MainHeaderStyle   = UIGetCachedName(byte_string_literal("FileBrowser_Header")      , UI->Pipeline.StyleRegistery);
            UI->HeaderButtonStyle = UIGetCachedName(byte_string_literal("FileBrowser_HeaderButton"), UI->Pipeline.StyleRegistery);
        }

        // Layout
        UIWindow(UI->MainWindowStyle, &UI->Pipeline);
        {
            UIHeaderEx(UI->MainHeaderStyle, (&UI->Pipeline))
            {
                UIButton(UI->HeaderButtonStyle, FileBrowserForwardHistory , &UI->Pipeline);
                UIButton(UI->HeaderButtonStyle, FileBrowserBackwardHistory, &UI->Pipeline);
            }
        }

        UI->IsInitialized = 1;
    }

    UIPipelineBegin  (&UI->Pipeline);
    UIPipelineExecute(&UI->Pipeline, PassList);
}
