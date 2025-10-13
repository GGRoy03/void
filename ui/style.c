// [Properties]

internal f32 
UIGetBorderWidth(ui_cached_style *Cached)
{
    f32 Result = Cached->Properties[StyleEffect_Base][StyleProperty_BorderWidth].Float32;
    return Result;
}

internal f32
UIGetSoftness(ui_cached_style *Cached)
{
    f32 Result = Cached->Properties[StyleEffect_Base][StyleProperty_Softness].Float32;
    return Result;
}

internal f32
UIGetFontSize(ui_cached_style *Cached)
{
    f32 Result = Cached->Properties[StyleEffect_Base][StyleProperty_FontSize].Float32;
    return Result;
}

internal vec2_unit
UIGetSize(ui_cached_style *Cached)
{
    vec2_unit Result = Cached->Properties[StyleEffect_Base][StyleProperty_Size].Vec2;
    return Result;
}

internal ui_color
UIGetColor(ui_cached_style *Cached)
{
    ui_color Result = Cached->Properties[StyleEffect_Base][StyleProperty_Color].Color;
    return Result;
}

internal ui_color
UIGetBorderColor(ui_cached_style *Cached)
{
    ui_color Result = Cached->Properties[StyleEffect_Base][StyleProperty_BorderColor].Color;
    return Result;
}

internal ui_color
UIGetTextColor(ui_cached_style *Cached)
{
    ui_color Result = Cached->Properties[StyleEffect_Base][StyleProperty_TextColor].Color;
    return Result;
}

internal ui_padding
UIGetPadding(ui_cached_style *Cached)
{
    ui_padding Result = Cached->Properties[StyleEffect_Base][StyleProperty_Padding].Padding;
    return Result;
}

internal ui_spacing
UIGetSpacing(ui_cached_style *Cached)
{
    ui_spacing Result = Cached->Properties[StyleEffect_Base][StyleProperty_Spacing].Spacing;
    return Result;
}

internal ui_corner_radius
UIGetCornerRadius(ui_cached_style *Cached)
{
    ui_corner_radius Result = Cached->Properties[StyleEffect_Base][StyleProperty_CornerRadius].CornerRadius;
    return Result;
}

internal ui_font *
UIGetFont(ui_cached_style *Cached)
{
    ui_font *Result = 0;

    if(HasFlag(Cached->Flags, CachedStyle_FontIsLoaded))
    {
        Result = Cached->Properties[StyleEffect_Base][StyleProperty_Font].Pointer;
    }

    return Result;
}

internal void
UISetFont(ui_cached_style *Cached, ui_font *Font)
{
    Cached->Properties[StyleEffect_Base][StyleProperty_Font].Pointer = Font;
}

// [Styles]

internal ui_cached_style
SuperposeStyle(ui_cached_style *Style, StyleEffect_Type Effect)
{
    Assert(Effect != StyleEffect_None);
    Assert(Effect != StyleEffect_Count);
    Assert(Effect != StyleEffect_Base);

    ui_cached_style Result = *Style;

    ForEachEnum(StyleProperty_Type, StyleProperty_Count, Type)
    {
        // If the layer to superpose is set, override the result with that value.
        // Otherwise we just use whatever is stored in the base style.

        if(Style->Properties[Effect][Type].IsSet)
        {
            Result.Properties[StyleEffect_Base][Type] = Style->Properties[Effect][Type];
        }
    }

    return Result;
}

internal ui_cached_style *
GetCachedStyle(ui_style_registry *Registry, u32 Index)
{
    ui_cached_style *Result = 0;

    if(Index < Registry->StylesCount)
    {
        Result = Registry->Styles + Index;
    }

    return Result;
}

// [Helpers]

internal b32
IsVisibleColor(ui_color Color)
{
    b32 Result = (Color.A > 0.f);
    return Result;
}

internal b32
PropertyIsSet(ui_cached_style *Style, StyleEffect_Type Effect, StyleProperty_Type Property)
{
    b32 Result = Style->Properties[Effect][Property].IsSet;
    return Result;
}
