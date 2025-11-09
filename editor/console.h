// [CONSTANTS]

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

    // Text
    byte_string StatusText;
    byte_string FooterText;

    // Scroll Buffer State
    u32 MessageLimit;
    u32 MessageHead;
    u32 MessageTail;
} console_ui;

internal void ConsoleUI  (console_ui *Console);
