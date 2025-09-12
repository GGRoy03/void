// [CONSTANTS]

// [CORE TYPES]

typedef struct ui_layout_box
{
    u32      Width;
    u32      Height;
    vec2_f32 Spacing;
    vec4_f32 Padding;
} ui_layout_box;

typedef struct ui_layout_node ui_layout_node;
struct ui_layout_node
{
    ui_layout_box LayoutBox;

    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;
};

typedef struct ui_style
{
    vec4_f32 Color;
    vec4_f32 BorderColor;
    u32      BorderWidth;
    vec2_f32 Size;
    vec2_f32 Spacing;
    vec4_f32 Padding;
} ui_style;

typedef struct ui_style_name
{
    byte_string Value;
} ui_style_name;

typedef struct ui_cached_style ui_cached_style;
struct ui_cached_style
{
    u32              Index;
    ui_cached_style *Next;
    ui_style         Style;
};

typedef struct ui_style_registery
{
    u32 Count;
    u32 Capacity;

    ui_style_name   *CachedName;    // Unique String
    ui_cached_style *Sentinels;    // 0..ThemeNameLength - 1
    ui_cached_style *CachedStyles; // ThemeNameLength .. ThemeMaxCount
    memory_arena    *Arena;        // Holds cached style / Interned Strings
} ui_style_registery;

// [API]

// [Components]

internal void UIWindow(ui_style_name StyleName);
internal void UIButton(ui_style_name StyleName);
