#pragma once

#define DEBUG_VALIDATE_LRU 0

typedef struct glyph_size
{
    cim_u16 Width;
    cim_u16 Height;
}  glyph_size;

typedef struct glyph_info
{
    cim_u32    MapId;
    bool       IsInAtlas;
    cim_f32    U0, V0, U1, V1;
    glyph_size Size;
} glyph_info;

typedef struct glyph_layout_info
{
    cim_u32 MapId;   // Easy retrieve.
    cim_u32 GlyphId; // Code point for hashing

    // Layout
    cim_f32 OffsetX;
    cim_f32 OffsetY;
    cim_f32 AdvanceX;
} glyph_layout_info;

// General layout for a glyph run, maybe rename?
typedef struct text_layout_info
{
    // Global
    void   *BackendLayout;
    cim_u16 Width;
    cim_u16 Height;

    // Per glyph
    glyph_layout_info *GlyphLayoutInfo;
    cim_u32            GlyphCount;
} text_layout_info;

// ====================================
// New API.

// Opaque types.
typedef struct glyph_entry glyph_entry;
typedef struct glyph_table glyph_table;
typedef struct os_font_objects os_font_objects;
typedef struct renderer_font_objects renderer_font_objects;

// Types to be used by the user
typedef struct glyph_table_params
{
    cim_u32 HashCount;
    cim_u32 EntryCount;
} glyph_table_params;

// NOTE: Perhaps we don't want a LRU per font, or rather, maybe we want a simpler
// scheme? Not really sure. I need to think about it more, but it's good to note it.
typedef struct ui_font
{
    bool IsValid;

    // Layout data
    cim_f32 LineHeight;

    // 2D Allocator
    stbrp_context  AtlasContext;
    stbrp_node    *AtlasNodes;
    cim_u32        AtlasHeight;
    cim_u32        AtlasWidth;

    renderer_font_objects *FontObjects;
    os_font_objects       *OSFontObjects;
    glyph_table           *Table;
    char                  *HeapBase;
} ui_font;

typedef struct glyph_hash
{
    cim_u32 Value;
} glyph_hash;