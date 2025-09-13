internal void 
GameEntryPoint()
{
    game_state GameState = {0};
    ui_state   UIState   = { 0 };

    // Game Memory
    {
        memory_arena_params StaticArenaParams = { 0 };
        StaticArenaParams.CommitSize        = ArenaDefaultCommitSize;
        StaticArenaParams.ReserveSize       = ArenaDefaultReserveSize;
        StaticArenaParams.AllocatedFromFile = __FILE__;
        StaticArenaParams.AllocatedFromLine = __LINE__;

        GameState.StaticArena = AllocateArena(StaticArenaParams);
    }

    // Styles Initialization
    {
        byte_string StyleFiles[1] =
        {
            byte_string_literal("D:/Work/CIM/styles/window.cim"),
        };

        LoadThemeFiles(StyleFiles, ArrayCount(StyleFiles), &UIState.StyleRegistery);
    }

    // Renderer Initialization (BUILD DEFINED)
    render_handle RendererHandle = InitializeRenderer(GameState.StaticArena);
    if (!IsValidRenderHandle(RendererHandle))
    {
        return;
    }
    GameState.RendererHandle = RendererHandle;

    while(OSUpdateWindow())
    {
        BeginRendereringContext(&GameState.RenderContext);

        TestUI(&UIState, &GameState.RenderContext);
        SubmitRenderCommands(&GameState.RenderContext, GameState.RendererHandle);

        OSSleep(5);
    }
}
