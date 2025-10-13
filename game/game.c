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
        UIBeginFrame();

        ShowEditorUI();

        UIEndFrame();

        OSClearInputs();
        OSSleep(5);
    }

    // NOTE: This shouldn't be needed but the debug layer for D3D is triggering
    // an error and preventing the window from closing if the resources
    // related to fonts aren't released. So this is for convenience :)
    {
        ui_font_list *FontList = &UIState.Fonts;

        IterateLinkedList(FontList->First, ui_font *, Font)
        {
            OSReleaseFontObjects(&Font->OSFontObjects);
        }
    }
}
