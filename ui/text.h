// NOTE:
// This file needs cleanups. I don't really know which types I want to expose in the header.
// Also: Consider adding a name space.

// [CORE TYPES]

typedef struct glyph_hash
{
    uint64_t Value;
} glyph_hash;

typedef struct glyph_table_params
{
    uint32_t EntryCount;
    uint32_t HashCount;
} glyph_table_params;

typedef struct glyph_table_stats
{
    uint64_t HitCount;
    uint64_t MissCount;
} glyph_table_stats;

typedef struct glyph_entry
{
    glyph_hash HashValue;

    uint32_t NextWithSameHash;
    uint32_t NextLRU;
    uint32_t PrevLRU;

    bool      IsRasterized;
    rect_float Source;
    rect_float Position;
    vec2_float Offset;
    vec2_int Size;
    float      AdvanceX;
} glyph_entry;

typedef struct glyph_table
{
    glyph_table_stats Stats;

    uint32_t HashMask;
    uint32_t HashCount;
    uint32_t EntryCount;

    uint32_t         *HashTable;
    glyph_entry *Entries;
} glyph_table;

typedef struct glyph_state
{
    uint32_t      Id;
    bool      IsRasterized;
    rect_float Source;
    rect_float Position;
    vec2_float Offset;
    vec2_int Size;
    float      AdvanceX;
} glyph_state;

typedef struct ui_font ui_font;
struct ui_font
{
    ui_font *Next;

    // Backend
    gpu_font_context GPUContext;
    os_font_context  OSContext;

    // Info
    vec2_float    TextureSize;
    float         LineHeight;
    float         Size;
    byte_string Name;

    // 2D Allocator
    stbrp_context AtlasContext;
    stbrp_node   *AtlasNodes;

    // Tables
    glyph_table *Cache;        // Handles any codepoint
    glyph_state  Direct[0x7F]; // Handles ASCII
};

typedef struct ui_shaped_glyph
{
    rect_float Position;
    rect_float Source;
    vec2_float Offset;
    vec2_int Size;
    float      AdvanceX;
} ui_shaped_glyph;

typedef struct ui_text
{
    render_handle    Atlas;
    vec2_float         AtlasSize;
    float              LineHeight; // NOTE: Weird..
    ui_shaped_glyph *Shaped;
    uint32_t              ShapedCount;
    uint32_t              ShapedLimit;
    float              TotalHeight;

} ui_text;

typedef struct ui_text_input
{
    byte_string UserBuffer;
    uint64_t         internalCount;

    int CaretAnchor;
    float CaretTimer;

    // User Callbacks
    ui_text_input_onchar OnChar;
    ui_text_input_onkey  OnKey;
    void                *OnKeyUserData;
} ui_text_input;

static uint64_t       GetUITextFootprint   (uint64_t TextSize);
static ui_text * PlaceUITextInMemory  (byte_string Text, uint64_t BufferSize, ui_font *Font, void *Memory);

static void UITextAppend_  (byte_string UTF8, ui_font *Font, ui_text *Text);
static void UITextClear_   (ui_text *Text);


static void UITextInputMoveCaret_  (ui_text *Text, ui_text_input *Input, int Offset);
static void UITextInputClear_      (ui_text_input *TextInput);

// [Fonts]

static ui_font * UILoadFont   (byte_string Name, float Size);
static ui_font * UIQueryFont  (byte_string Name, float Size);
