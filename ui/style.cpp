// ------------------------------------------------------------------------------------
// Style Manipulation Public API

static ui_paint_properties *
GetPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->PaintArray);

    ui_paint_properties *Result = 0;

    if(NodeIndex < Subtree->NodeCount)
    {
        Result = Subtree->PaintArray + NodeIndex;
    }

    return Result;
}

static void
ClearPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(!"Not Implemented");
}

static void
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_subtree *Subtree)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_cached_style *Cached = 0;
    VOID_ASSERT(Cached);

    ui_paint_properties *Paint = GetPaintProperties(NodeIndex, Subtree);
    VOID_ASSERT(Paint);

    Paint->Color        = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].Color;
    Paint->BorderColor  = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].BorderColor;
    Paint->TextColor    = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].TextColor;
    Paint->CaretColor   = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].CaretColor;
    Paint->Softness     = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].Softness;
    Paint->BorderWidth  = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].BorderWidth;
    Paint->CaretWidth   = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].CaretWidth;
    Paint->CornerRadius = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].CornerRadius;
    Paint->Font         = Cached->Properties[static_cast<uint32_t>(StyleState::Default)].Font;
    Paint->CachedIndex  = StyleIndex;

    SetNodeProperties(NodeIndex, Subtree->LayoutTree, Cached);
}

// [Helpers]

static bool
IsVisibleColor(ui_color Color)
{
    bool Result = (Color.A > 0.f);
    return Result;
}

static ui_color
NormalizeColor(ui_color Color)
{
    float Inverse = 1.f / 255.f;

    ui_color Result =
    {
        .R = Color.R * Inverse,
        .G = Color.G * Inverse,
        .B = Color.B * Inverse,
        .A = Color.A * Inverse,
    };

    return Result;
}
