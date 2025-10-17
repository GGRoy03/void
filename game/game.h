// [Core Types]

typedef struct game_state
{
    // Memory
    memory_arena *StaticData;
    memory_arena *FrameArena;

    // Rendering
    render_pass_list RenderPassList;
} game_state;

// [Global]

global game_state GameState;
