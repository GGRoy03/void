//-------------------------------------------------------------------------------------
// @internal: Style Helpers

static uint32_t
ResolveCachedIndex(uint32_t Index)
{
    VOID_ASSERT(Index);

    uint32_t Result = Index - 1;
    return Result;
}

static ui_cached_style *
GetCachedStyle(uint32_t Index, ui_style_registry *Registry)
{
    VOID_ASSERT(Registry);

    ui_cached_style *Result = 0;

    if(Index < Registry->StylesCount)
    {
        Result = Registry->Styles + Index;
    }

    return Result;
}

static style_property *
GetCachedProperties(uint32_t StyleIndex, StyleState_Type State, ui_style_registry *Registry)
{
    VOID_ASSERT(StyleIndex);
    VOID_ASSERT(StyleIndex <= Registry->StylesCount);

    style_property  *Result = 0;

    uint32_t              ResolvedIndex = ResolveCachedIndex(StyleIndex);
    ui_cached_style *Cached        = GetCachedStyle(ResolvedIndex, Registry);

    if(Cached)
    {
        Result = Cached->Properties[State];
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// Style Manipulation Public API

static void
SetNodeStyleState(StyleState_Type State, uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    VOID_ASSERT(Style);

    Style->State = State;
}

static void
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_subtree *Subtree)
{
    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    VOID_ASSERT(Style);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    style_property *Cached[StyleState_Count] = {0};
    Cached[StyleState_Default] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Default, Pipeline->Registry);
    Cached[StyleState_Hovered] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Hovered, Pipeline->Registry);
    Cached[StyleState_Focused] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Focused, Pipeline->Registry);

    ForEachEnum(StyleState_Type, StyleState_Count, State)
    {
        VOID_ASSERT(Cached[State]);

        ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
        {
            if(!Style->Properties[State][Prop].IsSetRuntime)
            {
                 Style->Properties[State][Prop] = Cached[State][Prop];
            }
        }
    }

    Style->CachedStyleIndex = StyleIndex;
}

static style_property *
GetPaintProperties(uint32_t NodeIndex, bool ClearState, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    VOID_ASSERT(Style);

    style_property *Result = PushArray(Subtree->Transient, style_property, StyleProperty_Count);
    VOID_ASSERT(Result);

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

static ui_node_style *
GetNodeStyle(uint32_t Index, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_node_style *Result = 0;

    if(Index < Subtree->NodeCount)
    {
        Result = Subtree->ComputedStyles + Index;
    }

    return Result;
}

// [Helpers]

static bool
IsVisibleColor(ui_color Color)
{
    bool Result = (Color.A > 0.f);
    return Result;
}
