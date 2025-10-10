// [FORWARD DECLARATION]

typedef struct game_state game_state;

// [CORE TYPES]

typedef struct editor_console_ui
{
    memory_arena *Arena;
    ui_pipeline   Pipeline;
    b32           IsInitialized;
} editor_console_ui;

internal void ConsoleUI            (editor_console_ui *Console, game_state *GameState);
external void ConsolePrintMessage  (byte_string Message, ConsoleMessage_Severity Severity);
