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

        // NOTE: This code is quite bad. Need to figure this out. Probably will do
        // as we re-implement the UI.
        {
            render_batch_list *BatchList = GetBatchList(&GameState.RenderContext, RenderPass_UI);
            render_batch      *Batch     = BeginRenderBatch(10 * sizeof(render_rect), BatchList, GameState.RenderContext.PassArena[RenderPass_UI]);

            render_rect *Rect = AllocateRect(Batch);
            Rect->Color      = NormalizedColor(Vec4F32(173.f, 184.f, 154.f, 255.f));
            Rect->RectBounds = Vec4F32(400.f, 400.f, 550.f, 750.f);

            BatchList->ByteCount += sizeof(render_rect); // WARN: Do not do this! Make smarter functions.
        }

        SubmitRenderCommands(&GameState.RenderContext, GameState.RendererHandle);

        OSSleep(5);
    }
}
