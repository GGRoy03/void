// --------------------------------------------------------------------------------------------------
// Public API Getters Implementation

internal f32 
UIGetBorderWidth(style_property Properties[StyleProperty_Count])
{
    f32 Result = Properties[StyleProperty_BorderWidth].Float32;
    return Result;
}

internal f32
UIGetSoftness(style_property Properties[StyleProperty_Count])
{
    f32 Result = Properties[StyleProperty_Softness].Float32;
    return Result;
}

internal f32
UIGetFontSize(style_property Properties[StyleProperty_Count])
{
    f32 Result = Properties[StyleProperty_FontSize].Float32;
    return Result;
}

internal vec2_unit
UIGetSize(style_property Properties[StyleProperty_Count])
{
    vec2_unit Result = Properties[StyleProperty_Size].Vec2;
    return Result;
}

internal ui_color
UIGetColor(style_property Properties[StyleProperty_Count])
{
    ui_color Result = Properties[StyleProperty_Color].Color;
    return Result;
}

internal ui_color
UIGetBorderColor(style_property Properties[StyleProperty_Count])
{
    ui_color Result = Properties[StyleProperty_BorderColor].Color;
    return Result;
}

internal ui_color
UIGetTextColor(style_property Properties[StyleProperty_Count])
{
    ui_color Result = Properties[StyleProperty_TextColor].Color;
    return Result;
}

internal ui_padding
UIGetPadding(style_property Properties[StyleProperty_Count])
{
    ui_padding Result = Properties[StyleProperty_Padding].Padding;
    return Result;
}

internal ui_spacing
UIGetSpacing(style_property Properties[StyleProperty_Count])
{
    ui_spacing Result = Properties[StyleProperty_Spacing].Spacing;
    return Result;
}

internal ui_corner_radius
UIGetCornerRadius(style_property Properties[StyleProperty_Count])
{
    ui_corner_radius Result = Properties[StyleProperty_CornerRadius].CornerRadius;
    return Result;
}

internal ui_font *
UIGetFont(style_property Properties[StyleProperty_Count])
{
    ui_font *Result = Properties[StyleProperty_Font].Pointer;
    return Result;
}

internal UIDisplay_Type
UIGetDisplay(style_property Properties[StyleProperty_Count])
{
    UIDisplay_Type Result = Properties[StyleProperty_Display].Enum;
    return Result;
}

// --------------------------------------------------------------------------------------------------
// Style Manipulation Internal API

internal void
SetStyleProperty(ui_node Node, style_property Value, StyleState_Type State, ui_subtree *Subtree)
{
    ui_node_style *NodeStyle = Subtree->ComputedStyles + Node.IndexInTree;

    if(NodeStyle)
    {
        NodeStyle->Properties[State][Value.Type] = Value;
    }
}

internal ui_cached_style *
GetCachedStyle(u32 Index, ui_style_registry *Registry)
{
    Assert(Registry);

    ui_cached_style *Result = 0;

    if(Index < Registry->StylesCount)
    {
        Result = Registry->Styles + Index;
    }

    return Result;
}

// --------------------------------------------------------------------------------------------------
// Style Manipulation Public API

internal void
UISetDisplay(ui_node Node, UIDisplay_Type Display, ui_subtree *Subtree)
{
    Assert(Node.CanUse);
    Assert(Subtree);

    style_property Property = {.Type = StyleProperty_Display, .Enum = Display};
    SetStyleProperty(Node, Property, StyleState_Basic, Subtree);
}

internal void
UISetTextColor(ui_node Node, ui_color Color, ui_subtree *Subtree)
{
    Assert(Node.CanUse);
    Assert(Subtree);

    style_property Property = {.Type = StyleProperty_TextColor, .Color = Color};
    SetStyleProperty(Node, Property, StyleState_Basic, Subtree);
}

internal u32
ResolveCachedIndex(u32 Index)
{
    Assert(Index);

    u32 Result = Index - 1;
    return Result;
}

//

internal ui_node_style *
GetNodeStyle(u32 Index, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Result = 0;

    if(Index < Subtree->NodeCount)
    {
        Result = Subtree->ComputedStyles + Index;
    }

    return Result;
}

internal style_property *
GetCachedProperties(u32 StyleIndex, StyleState_Type State, ui_style_registry *Registry)
{
    Assert(StyleIndex);
    Assert(StyleIndex <= Registry->StylesCount);

    style_property  *Result = 0;

    u32              ResolvedIndex = ResolveCachedIndex(StyleIndex);
    ui_cached_style *Cached        = GetCachedStyle(ResolvedIndex, Registry);

    if(Cached)
    {
        Result = Cached->Properties[State];
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
