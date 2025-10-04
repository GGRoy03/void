#pragma once

// [CONSTANTS]

typedef enum UIStyleNode_Flag
{
    UIStyleNode_NoFlag            = 0,
    UIStyleNode_HasSize           = 1 << 0,
    UIStyleNode_HasColor          = 1 << 1,
    UIStyleNode_HasPadding        = 1 << 2,
    UIStyleNode_HasSpacing        = 1 << 3,
    UIStyleNode_HasFontName       = 1 << 4,
    UIStyleNode_HasFontSize       = 1 << 5,
    UIStyleNode_HasSoftness       = 1 << 6,
    UIStyleNode_HasBorderColor    = 1 << 7,
    UIStyleNode_HasBorderWidth    = 1 << 8,
    UIStyleNode_HasCornerRadius   = 1 << 9,
    UIStyleNode_DirtySubtree      = 1 << 10,
    UIStyleNode_DirtySpine        = 1 << 11,
    UIStyleNode_StyleSetAtRuntime = 1 << 12,
} UIStyleNode_Flag;

typedef enum SetLayoutNodeStyle_Flag
{
    SetLayoutNodeStyle_NoFlag        = 0,
    SetLayoutNodeStyle_OmitReference = 1,
} SetLayoutNodeStyle_Flag;

// [CORE TYPES]

typedef struct ui_style
{
    // Color
    ui_color Color;
    f32      Opacity;

    // Layout
    vec2_unit  Size;
    ui_padding Padding;
    ui_spacing Spacing;

    // Borders/Corners
    ui_color         BorderColor;
    ui_corner_radius CornerRadius;
    f32              Softness;
    f32              BorderWidth;

    // Font
    f32 FontSize;
    union
    {
        ui_font *Ref;
        byte_string Name;
    } Font;

    // Effects
    ui_style *ClickOverride;
    ui_style *HoverOverride;

    // Misc
    u32       BaseVersion;
    bit_field Flags;
} ui_style;

typedef struct ui_cached_style ui_cached_style;
struct ui_cached_style
{
    u32              Index;
    ui_cached_style *Next;
    ui_style         Value;

    // Nodes References
    ui_layout_node *First;
    ui_layout_node *Last;
    u32             RefCount;

    // Misc
    b32 FontReferenceIsSet;
};

typedef struct ui_style_subregistry ui_style_subregistry;
typedef struct ui_style_subregistry
{
    // Misc
    b32                   HadError;
    u32                   StyleCount;
    ui_style_subregistry *Next;
    u8                    FileName[OS_MAX_PATH];
    u64                   FileNameSize;

    // Heap-Data (Styles, Names)
    ui_style_name *CachedNames;
    ui_cached_style *CachedStyles;
    ui_cached_style *Sentinels;
    u8 *StringBuffer;
    u64              StringBufferOffset;
} ui_style_subregistry;

typedef struct ui_style_registry
{
    u32                   Count;
    ui_style_subregistry *First;
    ui_style_subregistry *Last;
} ui_style_registry;

// [API]

internal void SetLayoutNodeStyle(ui_cached_style *CachedStyle, ui_layout_node *Node, bit_field Flags);

internal ui_cached_style * UIGetStyleSentinel               (byte_string   Name, ui_style_subregistry *Registry);
internal ui_style_name      UIGetCachedName                 (byte_string   Name, ui_style_registry    *Registry);
internal ui_style_name      UIGetCachedNameFromSubregistry  (byte_string   Name, ui_style_subregistry *Registry);
internal ui_cached_style * UIGetStyle                       (ui_style_name Name, ui_style_registry    *Registry);
internal ui_cached_style * UIGetStyleFromSubregistry        (ui_style_name Name, ui_style_subregistry *Registry);