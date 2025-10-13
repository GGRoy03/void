internal void
GameEntryPoint()
{
    // Game State
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.ReserveSize       = Megabyte(1);
        Params.CommitSize        = Kilobyte(64);

        GameState.StaticData = AllocateArena(Params);
    }

    // UI State
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.ReserveSize       = Megabyte(1);
        Params.CommitSize        = Kilobyte(64);

        UIState.StaticData = AllocateArena(Params);
    }

    // Render State
    {
        RenderState.Renderer = InitializeRenderer(GameState.StaticData);
    }

    InitializeConsoleMessageQueue(&Console);

    while(OSUpdateWindow())
    {
        ShowEditorUI();

        SubmitRenderCommands();

        OSClearInputs();
        OSSleep(5);
    }

    // NOTE: This shouldn't be needed but the debug layer for D3D is triggering
    // an error if the resources aren't released. So this is for convenience.
    // There are warnings still, but at least it doesn't crash.
    for (ui_font *Font = UIState.First; Font != 0; Font = Font->Next)
    {
        OSReleaseFontObjects(&Font->OSFontObjects);
    }
}
