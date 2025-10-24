// [CONSTANTS]

typedef enum ConsoleStyle_Type
{
    ConsoleStyle_Window      = 0,
    ConsoleStyle_MessageView = 1,
    ConsoleStyle_Message     = 2,
} ConsoleStyle_Type;

typedef enum ConsoleConstant_Type
{
    ConsoleConstant_MessageCountLimit = 60,
    ConsoleConstant_MessageSizeLimit  = Kilobyte(2),
} ConsoleConstant_Type;

// [FORWARD DECLARATION]

typedef struct game_state game_state;

// [CORE TYPES]

typedef struct editor_console_ui
{
    memory_arena *Arena;
    ui_pipeline  *Pipeline;
    b32           IsInitialized;

    // Scroll Buffer State
    u32 MessageLimit;
    u32 MessageHead;
    u32 MessageTail;
} editor_console_ui;

internal void ConsoleUI  (editor_console_ui *Console);
