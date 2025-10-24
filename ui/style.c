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

// --------------------------------------------------------------------------------------------------
// Style Manipulation Internal API

internal void
SetStyleProperty(ui_node Node, style_property Value, ui_node_style *Computed, memory_arena *Arena)
{
    // BUG: Could read garbage. Just need to check if it's a valid node, but its validity depends
    // on the tree. Uhm... What do we really want to store on the node?

    ui_node_style *NodeStyle = Computed + Node.IndexInTree;
    if(NodeStyle)
    {
        ui_style_override_node *Override = PushStruct(Arena, ui_style_override_node);
        if(Override)
        {
            Override->Value       = Value;
            Override->Value.IsSet = 1;
            AppendToLinkedList((&NodeStyle->Overrides), Override, NodeStyle->Overrides.Count);
        }
    }
}

internal ui_cached_style *
GetCachedStyle(u32 Index, ui_style_registry *Registry)
{
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
SetUITextColor(ui_node Node, ui_color Color, ui_subtree *Subtree, memory_arena *Arena)
{
    Assert(Node.CanUse);

    SetStyleProperty(Node, (style_property){.Type = StyleProperty_TextColor, .Color = Color}, Subtree->ComputedStyles, Arena);
}

//

internal style_property *
GetBaseStyle(u32 StyleIndex, ui_style_registry *Registry)
{
    style_property  *Result = 0;
    ui_cached_style *Cached = GetCachedStyle(StyleIndex, Registry);

    if(Cached)
    {
        Result = Cached->Properties[StyleEffect_Base];
    }

    return Result;
}

internal style_property *
GetHoverStyle(u32 StyleIndex, ui_style_registry *Registry)
{
    style_property  *Result = 0;
    ui_cached_style *Cached = GetCachedStyle(StyleIndex, Registry);

    if(Cached)
    {
        Result = Cached->Properties[StyleEffect_Hover];
    }

    return Result;
}

internal style_property *
GetComputedStyle(u32 NodeIndex, ui_subtree *Subtree)
{
    ui_node_style  *NodeStyle = Subtree->ComputedStyles + NodeIndex;
    style_property *Result    = NodeStyle->ComputedProperties;
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
