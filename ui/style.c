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

    // WARN: Doesn't make any sense. Probably just remove this flag?

    if(HasFlag(Cached->Flags, CachedStyle_FontIsLoaded))
    {
        Result = Cached->Properties[StyleEffect_Base][StyleProperty_Font].Pointer;
    }

    return Result;
}

// [Styles]

internal ui_style_override_node *
AllocateStyleOverrideNode(StyleProperty_Type Type, memory_arena *Arena)
{
    ui_style_override_node *Result = PushArray(Arena, ui_style_override_node, 1);
    if(Result)
    {
        Result->Value.IsSet = 1;
        Result->Value.Type  = Type;
    }

    return Result;
}

internal void
UISetStyleProperty(StyleProperty_Type Type, style_property Value, ui_pipeline *Pipeline)
{
    ui_layout_node *Node = GetLastAddedLayoutNode(Pipeline->LayoutTree);
    if(Node)
    {
        ui_node_style *NodeStyle = GetNodeStyle(Node->Index, Pipeline);
        if(NodeStyle)
        {
            ui_style_override_node *Override = AllocateStyleOverrideNode(Type, Pipeline->StaticArena);
            if(Override)
            {
                Override->Value       = Value;
                Override->Value.IsSet = 1;
                AppendToLinkedList((&NodeStyle->Overrides), Override, NodeStyle->Overrides.Count);
            }
        }
    }
}

internal void
UISetTextColor(ui_color Color, ui_pipeline *Pipeline)
{
    UISetStyleProperty(StyleProperty_TextColor, (style_property){.Type = StyleProperty_TextColor, .Color = Color}, Pipeline);
}

internal void
SuperposeStyle(style_property *BaseProperties, style_property *LayerProperties)
{
    ForEachEnum(StyleProperty_Type, StyleProperty_Count, Type)
    {
        if(LayerProperties[Type].IsSet)
        {
            BaseProperties[Type] = LayerProperties[Type];
        }
    }
}

internal ui_node_style *
GetNodeStyle(u32 Index, ui_pipeline *Pipeline)
{
    ui_node_style *Result = 0;

    // WARN:
    // Will become faulty logic once we implement some sort of free-list
    // for the tree. Maybe we just check against the node capacity?

    if(Index < Pipeline->LayoutTree->NodeCount)
    {
        Result = Pipeline->NodesStyle + Index;
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

internal style_property *
UIGetStyleEffect(ui_cached_style *Cached, StyleEffect_Type Effect)
{
    style_property *Result = Cached->Properties[Effect];
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
