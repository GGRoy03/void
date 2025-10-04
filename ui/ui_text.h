// [CONSTANTS]

typedef enum UIFontCoverage_Type
{
    UIFontCoverage_None      = 0,
    UIFontCoverage_ASCIIOnly = 1,
} UIFontCoverage_Type;

// [CORE TYPES]

typedef struct glyph_state
{
    os_glyph_layout      Layout;
    os_glyph_raster_info RasterInfo;
} glyph_state;

typedef struct glyph_table_params
{
    u32 EntryCount;
} glyph_table_params;

typedef struct glyph_table_stats
{
    u64 HitCount;
    u64 MissCount;
} glyph_table_stats;

typedef struct direct_glyph_table
{
    glyph_table_stats Stats;

    glyph_state *Entries;
    u32          EntryCount;
} direct_glyph_table;

typedef struct ui_character
{
    os_glyph_layout Layout;
    rect_f32        SampleSource;
    rect_f32        Position;
} ui_character;

typedef struct ui_text
{
    f32           LineHeight;
    vec2_f32      AtlasTextureSize;
    render_handle AtlasTexture;
    ui_character *Characters;
    u32           Size;
} ui_text;

// NOTE: Should we just hold on to handles?

typedef struct ui_font ui_font;
struct ui_font
{
    // LLC
    ui_font *Next;

    // Backend
    gpu_font_objects GPUFontObjects;
    os_font_objects  OSFontObjects;

    // Info
    vec2_f32            TextureSize;
    f32                 LineHeight;
    f32                 Size;
    byte_string         Name;
    UIFontCoverage_Type Coverage;

    // 2D Allocator
    stbrp_context AtlasContext;
    stbrp_node   *AtlasNodes;

    // Tables
    direct_glyph_table *GlyphTable;
};

// [API]

// [Fonts]

internal ui_font * UILoadFont  (byte_string Name, f32 Size, render_handle Renderer, UIFontCoverage_Type Coverage, ui_state *UIState);
internal ui_font * UIFindFont  (byte_string Name, f32 Size, ui_state *UIState);
internal ui_font * UIGetFont   (ui_cached_style *Style, render_handle Renderer, ui_state *UIState);

// [Glyphs]

internal size_t               GetDirectGlyphTableFootprint   (glyph_table_params Params);
internal glyph_state          FindGlyphEntryByDirectAccess   (u32 CodePoint, direct_glyph_table *Table);
internal direct_glyph_table * PlaceDirectGlyphTableInMemory  (glyph_table_params Params, void *Memory);
internal void                 UpdateDirectGlyphTableEntry    (u32 CodePoint, os_glyph_layout NewLayout, os_glyph_raster_info NewRasterInfo, direct_glyph_table *Table);
