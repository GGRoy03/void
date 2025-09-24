internal void 
GameEntryPoint()
{
    game_state GameState = {0};

    // Game Memory
    {
        memory_arena_params StaticArenaParams = { 0 };
        StaticArenaParams.CommitSize        = ArenaDefaultCommitSize;
        StaticArenaParams.ReserveSize       = ArenaDefaultReserveSize;
        StaticArenaParams.AllocatedFromFile = __FILE__;
        StaticArenaParams.AllocatedFromLine = __LINE__;

        GameState.StaticArena = AllocateArena(StaticArenaParams);
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
        BeginRendereringContext(&GameState.RenderPassList);

        ShowEditorUI(&GameState.RenderPassList, RendererHandle);

        SubmitRenderCommands(&GameState.RenderPassList, GameState.RendererHandle);

        OSClearInputs();
        OSSleep(5);
    }
}
