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

internal byte_string
UIGetFontName(ui_cached_style *Cached)
{
    byte_string Result = ByteString(0, 0);

    if(!(HasFlag(Cached->Flags, CachedStyle_FontIsLoaded)))
    {
        Result = Cached->Properties[StyleEffect_Base][StyleProperty_Font].String;
    }

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
