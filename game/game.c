internal void 
GameEntryPoint()
{
    game_state State = {0};

    // Game Memory
    {
        memory_arena_params StaticArenaParams = { 0 };
        StaticArenaParams.CommitSize        = ArenaDefaultCommitSize;
        StaticArenaParams.ReserveSize       = ArenaDefaultReserveSize;
        StaticArenaParams.AllocatedFromFile = __FILE__;
        StaticArenaParams.AllocatedFromLine = __LINE__;

        State.StaticArena = AllocateArena(StaticArenaParams);
    }

    // Renderer Initialization (BUILD DEFINED)
    render_handle RendererHandle = InitializeRenderer(State.StaticArena);
    if (!IsValidRenderHandle(RendererHandle))
    {
        return;
    }
    State.RendererHandle = RendererHandle;

    while(OSUpdateWindow())
    {
        BeginRendereringContext(&State.RenderContext);

        SubmitRenderCommands(&State.RenderContext, State.RendererHandle);

        OSSleep(5);
    }
}
