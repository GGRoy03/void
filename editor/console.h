// [CONSTANTS]

typedef enum ConsoleStyle_Type
{
    ConsoleStyle_Window      = 0,
    ConsoleStyle_MessageView = 1,
    ConsoleStyle_Message     = 2,
} ConsoleStyle_Type;

// [FORWARD DECLARATION]

typedef struct game_state game_state;

// [CORE TYPES]

typedef struct editor_console_ui
{
    memory_arena *Arena;
    ui_pipeline   Pipeline;
    b32           IsInitialized;
} editor_console_ui;

internal void ConsoleUI            (editor_console_ui *Console);
external void ConsolePrintMessage  (byte_string Message, ConsoleMessage_Severity Severity);
