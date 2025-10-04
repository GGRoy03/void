// [FORWARD DECLARATION]

typedef struct game_state game_state;

// [CORE TYPES]

typedef struct editor_console_ui
{
    memory_arena *Arena;
    ui_pipeline   Pipeline;
    b32           IsInitialized;

    // Styles Reference
    ui_style_name WindowStyle;
    ui_style_name MessageStyle;
    ui_style_name MessageScrollView;

    // Nodes Reference
    ui_layout_node *MessageScrollViewNode;
} editor_console_ui;

internal void ConsoleUI            (editor_console_ui *Console, game_state *GameState, ui_state *UIState);
external void ConsolePrintMessage  (byte_string Message, ConsoleMessage_Severity Severity, ui_state *UIState);
