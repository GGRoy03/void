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
    f32      AdvanceX;
} glyph_state;

typedef struct ui_font ui_font;
struct ui_font
{
    // LLC
    ui_font *Next;

    // Backend
    gpu_font_objects GPUFontObjects;
    os_font_objects  OSFontObjects;

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

typedef struct ui_glyph
{
    rect_f32 Position;
    rect_f32 Source;
    vec2_f32 Offset;
    f32      AdvanceX;
} ui_glyph;

typedef struct ui_glyph_run
{
    render_handle Atlas;
    vec2_f32      AtlasSize;
    f32           LineHeight;
    rect_f32      BoundsLastFrame;
    ui_glyph     *Glyphs;
    u32           GlyphCount;
} ui_glyph_run;

// [Fonts]

internal ui_font * UILoadFont   (byte_string Name, f32 Size);
internal ui_font * UIQueryFont  (byte_string Name, f32 Size);

// [Glyphs]

internal glyph_entry * GetGlyphEntry          (glyph_table *Table, u32 Index);
internal u32         * GetSlotPointer         (glyph_table *Table, glyph_hash Hash);
internal glyph_entry * GetSentinel            (glyph_table *Table);
internal void          UpdateGlyphCacheEntry  (glyph_table *Table, glyph_state New);
internal ui_glyph_run  CreateGlyphRun         (byte_string Text, ui_font *Font, memory_arena *Arena);
