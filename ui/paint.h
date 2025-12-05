#pragma once

// [CORE TYPES]

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
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_pipeline &Pipeline);

// ===================================================================================
// @Internal: Small Helpers

static bool     IsVisibleColor  (ui_color Color);
static ui_color NormalizeColor  (ui_color Color);

static void PaintLayoutTreeFromRoot  (ui_layout_tree *Tree);
