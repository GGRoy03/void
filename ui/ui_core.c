// [API]

// [Components]

internal void
UIWindow(ui_style_name StyleName, ui_state *UIState)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, &UIState->StyleRegistery);
    UNUSED(Style);

    // NOTE: Then create some sort of layout box with the given style information.


    // NOTE: I'm not really sure what information I need to pass along. Maybe I pass
    // along the style, because it will be used further in the pipeline.
}

internal void
UIButton(ui_style_name StyleName, ui_state *UIState)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, &UIState->StyleRegistery);
    UNUSED(Style);

    // NOTE: Then create some sort of layout box with the given style information.


    // NOTE: I'm not really sure what information I need to pass along. Maybe I pass
    // along the style, because it will be used further in the pipeline.
}

// [Style]

internal ui_cached_style *
UIGetStyleSentinel(byte_string Name, ui_style_registery *Registery)
{
    ui_cached_style *Result = 0;

    if (Registery)
    {
        Result = Registery->Sentinels + (Name.Size - 1);
    }

    return Result;
}

internal ui_style_name *
UIGetCachedNameFromStyleName(byte_string Name, ui_style_registery *Registery)
{
    ui_style_name *Result = 0;

    if (Registery)
    {
        ui_cached_style *CachedStyle = 0;
        ui_cached_style *Sentinel    = UIGetStyleSentinel(Name, Registery);

        for (CachedStyle = Sentinel->Next; CachedStyle != 0; CachedStyle = CachedStyle->Next)
        {
            byte_string CachedString = Registery->CachedName[CachedStyle->Index].Value;
            if (ByteStringMatches(Name, CachedString))
            {
                Result = &Registery->CachedName[CachedStyle->Index];
                break;
            }
        }
    }

    return Result;
}

internal ui_style
UIGetStyleFromCachedName(ui_style_name Name, ui_style_registery *Registery)
{
    ui_style Result = { 0 };

    if (Registery)
    {
        u64              SentinelIndex = Name.Value.Size - 1;
        ui_cached_style *Sentinel      = Registery->Sentinels + SentinelIndex;

        for (ui_cached_style *CachedStyle = Sentinel->Next; CachedStyle != 0; CachedStyle = CachedStyle->Next)
        {
            byte_string CachedString = Registery->CachedName[CachedStyle->Index].Value;
            if (Name.Value.String == CachedString.String)
            {
                Result = CachedStyle->Style;
                break;
            }
        }
    }

    return Result;
}