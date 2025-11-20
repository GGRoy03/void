// -------------------------------------------------------------------------------------
// Style Manipulation Internal API

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

// ------------------------------------------------------------------------------------
// Style Manipulation Public API

internal u32
ResolveCachedIndex(u32 Index)
{
    Assert(Index);

    u32 Result = Index - 1;
    return Result;
}

internal void
SetNodeStyleState(StyleState_Type State, u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    Style->State = State;
}

internal style_property *
GetPaintProperties(u32 NodeIndex, b32 ClearState, ui_subtree *Subtree)
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

    if(ClearState)
    {
        Style->State = StyleState_Default;
    }

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
