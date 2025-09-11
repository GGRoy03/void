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

    // Styles Initialization
    {
        byte_string StyleFiles[1] =
        {
            byte_string_literal("D:/Work/CIM/styles/window.cim"),
        };

        LoadThemeFiles(StyleFiles, ArrayCount(StyleFiles));
    }

    // Renderer Initialization (BUILD DEFINED)
    render_handle RendererHandle = InitializeRenderer(State.StaticArena);
    if (!IsValidRenderHandle(RendererHandle))
    {
        return;
    }
    State.RendererHandle = RendererHandle;

    OSLogMessage(byte_string_literal("Logging Info")   , OSMessage_Info);
    OSLogMessage(byte_string_literal("Logging Warning"), OSMessage_Warn);
    OSLogMessage(byte_string_literal("Logging Error")  , OSMessage_Error);
    OSLogMessage(byte_string_literal("Logging Fatal")  , OSMessage_Fatal);

    while(OSUpdateWindow())
    {
        BeginRendereringContext(&State.RenderContext);

        // NOTE: This code is quite bad. Need to figure this out. Probably will do
        // as we re-implement the UI.
        {
            render_batch_list *BatchList = GetBatchList(&State.RenderContext, RenderPass_UI);
            render_batch      *Batch     = BeginRenderBatch(10 * sizeof(render_rect), BatchList, State.RenderContext.PassArena[RenderPass_UI]);

            render_rect *Rect = AllocateRect(Batch);
            Rect->Color      = Vec4F32(0.f, 1.f, 0.f, 1.f);
            Rect->RectBounds = Vec4F32(400.f, 400.f, 550.f, 750.f);

            BatchList->ByteCount += sizeof(render_rect); // WARN: Do not do this! Make smarter functions.
        }

        SubmitRenderCommands(&State.RenderContext, State.RendererHandle);

        OSSleep(5);
    }
}
