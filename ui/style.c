// --------------------------------------------------------------------------------------------------
// Style Manipulation Internal API

internal void
SetStyleProperty(u32 NodeIndex, style_property Value, StyleState_Type State, ui_subtree *Subtree)
{
    ui_node_style *NodeStyle = Subtree->ComputedStyles + NodeIndex;

    if(NodeStyle)
    {
        NodeStyle->Properties[State][Value.Type]              = Value;
        NodeStyle->Properties[State][Value.Type].IsSetRuntime = 1;

        NodeStyle->IsLastVersion = 0;
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
UISetSize(u32 NodeIndex, vec2_unit Size, ui_subtree *Subtree)
{
    Assert(Subtree);

    style_property Property = {.Type = StyleProperty_Size, .Vec2 = Size};
    SetStyleProperty(NodeIndex, Property, StyleState_Default, Subtree);
}

internal void
UISetDisplay(u32 NodeIndex, UIDisplay_Type Display, ui_subtree *Subtree)
{
    Assert(Subtree);

    style_property Property = {.Type = StyleProperty_Display, .Enum = Display};
    SetStyleProperty(NodeIndex, Property, StyleState_Default, Subtree);
}

internal void
UISetTextColor(u32 NodeIndex, ui_color Color, ui_subtree *Subtree)
{
    Assert(Subtree);

    style_property Property = {.Type = StyleProperty_TextColor, .Color = Color};
    SetStyleProperty(NodeIndex, Property, StyleState_Default, Subtree);
}

internal u32
ResolveCachedIndex(u32 Index)
{
    Assert(Index);

    u32 Result = Index - 1;
    return Result;
}

//

internal void
SetNodeStyleState(StyleState_Type State, u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    Style->State = State;
}

internal style_property *
GetPaintProperties(u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    style_property *Result = PushArray(Subtree->Transient, style_property, StyleProperty_Count);
    Assert(Result);

    ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
    {
        if(Style->Properties[Style->State][Prop].IsSet)
        {
            Result[Prop] = Style->Properties[Style->State][Prop];
        }
        else
        {
            Result[Prop] = Style->Properties[StyleState_Default][Prop];
        }
    }

    // WARN: Unsure.
    Style->State = StyleState_Default;

    return Result;
}

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
