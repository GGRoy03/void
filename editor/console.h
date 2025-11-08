// [CONSTANTS]

typedef enum ConsoleStyle_Type
{
    ConsoleStyle_Window      = 1,
    ConsoleStyle_MessageView = 2,
    ConsoleStyle_Message     = 3,
    ConsoleStyle_Prompt      = 4,
} ConsoleStyle_Type;

typedef enum ConsoleConstant_Type
{
    ConsoleConstant_MessageCountLimit = 60,
    ConsoleConstant_MessageSizeLimit  = Kilobyte(2),
    ConsoleConstant_PromptBufferSize  = 128,
} ConsoleConstant_Type;

// [FORWARD DECLARATION]

typedef struct game_state game_state;

// [CORE TYPES]

typedef struct console_ui
{
    memory_arena *Arena;
    ui_pipeline  *Pipeline;
    b32           IsInitialized;

    // Prompt
    u8 *PromptBuffer;
    u64 PromptBufferSize;

    // Scroll Buffer State
    u32 MessageLimit;
    u32 MessageHead;
    u32 MessageTail;
} console_ui;

internal void ConsoleUI  (console_ui *Console);
