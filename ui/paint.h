#pragma once

// ===================================================================================
// @Public: Helper Macros

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

template<typename T>
struct ui_property
{
    T    Value;
    bool IsSet;

    ui_property() = default;
    constexpr ui_property(const T& Value_)              : Value(Value_), IsSet(true) {}
    constexpr ui_property(const T& Value_, bool IsSet_) : Value(Value_), IsSet(IsSet_) {}

    ui_property& operator=(const T& Value_) { Value = Value_; IsSet = true; return *this; }
};

constexpr ui_property<ui_sizing_axis> ui_fixed_sizing(float Size)
{
    return ui_property<ui_sizing_axis>{ ui_sizing_axis{Sizing::Fixed, {Size}}, true};
}

struct ui_paint_properties
{
    ui_property<ui_color>         Color;
    ui_property<ui_color>         BorderColor;
    ui_property<ui_color>         TextColor;
    ui_property<ui_color>         CaretColor;

    ui_property<float>            BorderWidth;
    ui_property<float>            Softness;
    ui_property<ui_corner_radius> CornerRadius;

    ui_property<float>            CaretWidth;
};

struct ui_default_properties
{
    ui_property<ui_sizing_axis>   SizingX;
    ui_property<ui_sizing_axis>   SizingY;
    ui_property<ui_size>          MinSize;
    ui_property<ui_size>          MaxSize;
    ui_property<LayoutDirection>  Direction;
    ui_property<Alignment>        AlignX;
    ui_property<Alignment>        AlignY;

    ui_property<ui_padding>       Padding;
    ui_property<float>            Spacing;
    ui_property<float>            Grow;
    ui_property<float>            Shrink;

    ui_property<ui_color>         Color;
    ui_property<ui_color>         BorderColor;
    ui_property<ui_color>         TextColor;

    ui_property<float>            BorderWidth;
    ui_property<float>            Softness;
    ui_property<ui_corner_radius> CornerRadius;

    ui_paint_properties MakePaintProperties(void)
    {
        ui_paint_properties Result = {};

        // Defaults
        Result.Color        = Color;
        Result.BorderColor  = BorderColor;
        Result.TextColor    = TextColor;
        Result.BorderWidth  = BorderWidth;
        Result.Softness     = Softness;
        Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct ui_hovered_properties
{
    ui_property<ui_color>         Color;
    ui_property<ui_color>         BorderColor;
    ui_property<float>            Softness;
    ui_property<ui_corner_radius> CornerRadius;

    ui_paint_properties InheritPaintProperties(const ui_paint_properties &Default)
    {
        ui_paint_properties Result = Default;

        if (Color.IsSet)        Result.Color        = Color;        else Result.Color        = Default.Color;
        if (BorderColor.IsSet)  Result.BorderColor  = BorderColor;  else Result.BorderColor  = Default.BorderColor;
        if (Softness.IsSet)     Result.Softness     = Softness;     else Result.Softness     = Default.Softness;
        if (CornerRadius.IsSet) Result.CornerRadius = CornerRadius; else Result.CornerRadius = Default.CornerRadius;

        return Result;
    }
};

struct ui_focused_properties
{
    ui_property<ui_color>         Color;
    ui_property<ui_color>         BorderColor;
    ui_property<ui_color>         TextColor;
    ui_property<ui_color>         CaretColor;
    ui_property<float>            CaretWidth;
    ui_property<float>            Softness;
    ui_property<ui_corner_radius> CornerRadius;

    ui_paint_properties InheritPaintProperties(const ui_paint_properties &Default)
    {
        ui_paint_properties Result = Default;

        if (Color.IsSet)        Result.Color        = Color;        else Result.Color        = Default.Color;
        if (BorderColor.IsSet)  Result.BorderColor  = BorderColor;  else Result.BorderColor  = Default.BorderColor;
        if (TextColor.IsSet)    Result.TextColor    = TextColor;    else Result.TextColor    = Default.TextColor;
        if (CaretColor.IsSet)   Result.CaretColor   = CaretColor;   else Result.CaretColor   = Default.CaretColor;
        if (CaretWidth.IsSet)   Result.CaretWidth   = CaretWidth;   else Result.CaretWidth   = Default.CaretWidth;
        if (Softness.IsSet)     Result.Softness     = Softness;     else Result.Softness     = Default.Softness;
        if (CornerRadius.IsSet) Result.CornerRadius = CornerRadius; else Result.CornerRadius = Default.CornerRadius;
        return Result;
    }
};

struct ui_cached_style
{
    ui_default_properties Default;
    ui_hovered_properties Hovered;
    ui_focused_properties Focused;
};

// ===================================================================================
// @Internal: Small Helpers

static bool     IsVisibleColor  (ui_color Color);
static ui_color NormalizeColor  (ui_color Color);

static void ExecutePaintCommands(ui_paint_buffer Buffer, memory_arena *Arena);
