#pragma once

enum class StyleProperty : uint32_t
{
    // Layout Properties
    Size               = 0,
    MinSize            = 1,
    MaxSize            = 2,
    Padding            = 3,
    Spacing            = 4,
    BorderWidth        = 5,
    Grow               = 6,
    Shrink             = 7,
    LayoutDirection    = 8,
    AlignmentM         = 9,
    AlignmentC         = 10,

    // Style Properties
    Color              = 11,
    BorderColor        = 12,
    Softness           = 13,
    CornerRadius       = 14,

    // Text Properties
    Font               = 15,
    FontSize           = 16,
    TextAlign          = 17,
    TextColor          = 18,
    CaretColor         = 19,
    CaretWidth         = 20,

    // Misc
    Count              = 21,
    None               = 666,
};

constexpr uint32_t StylePropertyCount = static_cast<uint32_t>(StyleProperty::Count);

enum class StyleState
{
    Default = 0,
    Hovered = 1,
    Focused = 2,

    None  = 3,
    Count = 3,
};

constexpr uint32_t StyleStateCount = static_cast<uint32_t>(StyleState::Count);

// [CORE TYPES]

#define InvalidCachedStyleIndex 0

enum class Alignment
{
    None   = 0,
    Start  = 1,
    Center = 2,
    End    = 3,
};

enum class LayoutDirection
{
    None       = 0,
    Horizontal = 1,
    Vertical   = 2,
};

enum class Sizing
{
    None    = 0,
    Fit     = 1,
    Grow    = 2,
    Percent = 3,
    Fixed   = 4,
    Stretch = 5,
};

struct ui_size
{
    float Width;
    float Height;
};

struct ui_sizing_bounds
{
    float Min;
    float Max;
};

struct ui_sizing_axis
{
    Sizing Type;
    union
    {
        float Fixed;
        float Percent;
    };
};

struct ui_sizing
{
    ui_sizing_axis Horizontal;
    ui_sizing_axis Vertical;
};

struct ui_style_properties
{
    ui_sizing        Sizing;
    ui_size          MinSize;
    ui_size          MaxSize;
    LayoutDirection  LayoutDirection;
    Alignment        AlignmentM;
    Alignment        AlignmentC;

    ui_padding       Padding;
    float            Spacing;
    float            Grow;
    float            Shrink;

    ui_color         Color;
    ui_color         BorderColor;
    ui_color         TextColor;
    ui_color         CaretColor;

    float            BorderWidth;
    float            Softness;
    ui_corner_radius CornerRadius;

    ui_font         *Font;
    float            FontSize;
    float            CaretWidth;
    Alignment        TextAlign[2];
};

struct ui_cached_style
{
    ui_style_properties Properties[StyleStateCount];

    ui_style_properties Default;
    ui_style_properties Hovered;
    ui_style_properties Focused;
};

struct ui_paint_properties
{
    ui_color         Color;
    ui_color         BorderColor;
    ui_color         TextColor;
    ui_color         CaretColor;

    float            Softness;
    float            BorderWidth;
    float            CaretWidth;
    ui_corner_radius CornerRadius;

    ui_font         *Font;

    uint32_t         CachedIndex;
};

static void
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_subtree *Subtree);

static ui_paint_properties *
GetPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree);

static void
ClearPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree);

// ===================================================================================
// @Internal: Small Helpers

static bool     IsVisibleColor  (ui_color Color);
static ui_color NormalizeColor  (ui_color Color);
