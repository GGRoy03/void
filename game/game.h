// [Core Types]

typedef struct game_state
{
    // Memory
    memory_arena *StaticArena;
    memory_arena *FrameArena;

    // Rendering
    render_context RenderContext;
    render_handle  RendererHandle;
} game_state;

// [Core API]

internal void GameEntryPoint();
