// [CORE TYPES]

typedef struct glyph_hash
{
    u64 Value;
} glyph_hash;

typedef struct glyph_table_params
{
    u32 EntryCount;
    u32 HashCount;
} glyph_table_params;

typedef struct glyph_table_stats
{
    u64 HitCount;
    u64 MissCount;
} glyph_table_stats;

typedef struct glyph_entry
{
    glyph_hash HashValue;

    u32 NextWithSameHash;
    u32 NextLRU;
    u32 PrevLRU;

    b32      IsRasterized;
    rect_f32 Source;
    rect_f32 Position;
    vec2_f32 Offset;
    vec2_i32 Size;
    f32      AdvanceX;
} glyph_entry;

typedef struct glyph_table
{
    glyph_table_stats Stats;

    u32 HashMask;
    u32 HashCount;
    u32 EntryCount;

    u32         *HashTable;
    glyph_entry *Entries;
} glyph_table;

typedef struct glyph_state
{
    u32      Id;
    b32      IsRasterized;
    rect_f32 Source;
    rect_f32 Position;
    vec2_f32 Offset;
    vec2_i32 Size;
    f32      AdvanceX;
} glyph_state;

typedef struct ui_font ui_font;
struct ui_font
{
    ui_font *Next;

    // Backend
    gpu_font_context GPUContext;
    os_font_context  OSContext;

    // Info
    vec2_f32    TextureSize;
    f32         LineHeight;
    f32         Size;
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
    rect_f32 Position;
    rect_f32 Source;
    vec2_f32 Offset;
    vec2_i32 Size;
    f32      AdvanceX;
} ui_shaped_glyph;

typedef struct ui_text
{
    render_handle    Atlas;
    vec2_f32         AtlasSize;
    f32              LineHeight; // NOTE: Weird..
    ui_shaped_glyph *Shaped;
    u32              ShapedCount;
    u32              ShapedLimit;
    f32              TotalHeight;

} ui_text;

typedef struct ui_text_input
{
    byte_string UserBuffer;
    u64         InternalCount;

    i32 CaretAnchor;
    f32 CaretTimer;

    // User Callbacks
    ui_text_input_onchar OnChar;
    ui_text_input_onkey  OnKey;
    void                *OnKeyUserData;
} ui_text_input;

internal u64       GetUITextFootprint   (u64 TextSize);
internal ui_text * PlaceUITextInMemory  (byte_string Text, u64 BufferSize, ui_font *Font, void *Memory);

internal void UITextAppend_  (byte_string UTF8, ui_font *Font, ui_text *Text);
internal void UITextClear_   (ui_text *Text);


internal void UITextInputMoveCaret_  (ui_text *Text, ui_text_input *Input, i32 Offset);
internal void UITextInputClear_      (ui_text_input *TextInput);

// [Fonts]

internal ui_font * UILoadFont   (byte_string Name, f32 Size);
internal ui_font * UIQueryFont  (byte_string Name, f32 Size);
