internal void 
GameEntryPoint()
{
    game_state GameState = {0};
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.ReserveSize       = Megabyte(1);
        Params.CommitSize        = Kilobyte(64);

        GameState.StaticData = AllocateArena(Params);
    }

    ui_state UIState = {0}; 
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.ReserveSize       = Megabyte(1);
        Params.CommitSize        = Kilobyte(64);

        UIState.StaticData = AllocateArena(Params);
    }

    // Renderer Initialization (BUILD DEFINED)
    render_handle Renderer = InitializeRenderer(GameState.StaticData);
    if (!IsValidRenderHandle(Renderer))
    {
        return;
    }

    while(OSUpdateWindow())
    {
        BeginRendereringContext(&GameState.RenderPassList);

        ShowEditorUI(&GameState.RenderPassList, Renderer, &UIState);

        SubmitRenderCommands(&GameState.RenderPassList, Renderer);

        OSClearInputs();
        OSSleep(5);
    }
}
