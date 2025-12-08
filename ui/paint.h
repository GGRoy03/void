#pragma once

// ====================================================================================
// @Public: Helper Macros

#define UIFixedSizing(SizeInFloat) {.Type = Sizing::Fixed, .Fixed = SizeInFloat}
#define UISize(Width, Height)      {Width, Height}

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

struct ui_sizing_axis
{
    Sizing Type;
    union
    {
        float Fixed;
        float Percent;
    };
};

struct ui_default_properties
{
    ui_sizing_axis   SizingX;
    ui_sizing_axis   SizingY;
    ui_size          MinSize;
    ui_size          MaxSize;
    LayoutDirection  Direction;
    Alignment        AlignX;
    Alignment        AlignY;

    ui_padding       Padding;
    float            Spacing;
    float            Grow;
    float            Shrink;

    ui_color         Color;
    ui_color         BorderColor;
    ui_color         TextColor;

    float            BorderWidth;
    float            Softness;
    ui_corner_radius CornerRadius;

    ui_font         *Font;
    float            FontSize;
};

struct ui_hovered_properties
{
    ui_color Color;
    ui_color BorderColor;
};

struct ui_focused_properties
{
    ui_color Color;
    ui_color BorderColor;
    ui_color TextColor;
    ui_color CaretColor;
    float    CaretWidth;
};

struct ui_cached_style
{
    ui_default_properties Default;
    ui_hovered_properties Hovered;
    ui_focused_properties Focused;
};

// TODO: Can we be smarter about what a command really is?
// Right now it's mostly tied to a node in the tree so we need some fat struct.

struct ui_paint_command
{
    rect_float       Rectangle;
    rect_float       RectangleClip;

    ui_resource_key  TextKey;
    ui_resource_key  ImageKey;

    ui_color         Color;
    ui_color         BorderColor;
    ui_corner_radius CornerRadius;
    float            Softness;
    float            BorderWidth;
};

struct ui_paint_buffer
{
    ui_paint_command *Commands;
    uint32_t          Size;
};

// ===================================================================================
// @Internal: Small Helpers

static bool     IsVisibleColor  (ui_color Color);
static ui_color NormalizeColor  (ui_color Color);

static void ExecutePaintCommands(ui_paint_buffer Buffer, memory_arena *Arena);
