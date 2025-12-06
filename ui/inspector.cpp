namespace Inspector
{

enum class InspectorStyle
{
    Window = 0,
};

constexpr ui_color Black = ui_color(0.f, 0.f, 0.f, 1.f);
constexpr ui_color White = ui_color(1.f, 1.f, 1.f, 1.f);

static ui_cached_style InspectorStyleArray[] =
{

    // Window
    {
        .Default =
        {
            .SizingX   = UIFixedSizing(500.f),
            .SizingY   = UIFixedSizing(500.f),
            .MinSize   = ui_size(400.f, 400.f),
            .MaxSize   = ui_size(600.f, 600.f),
            .Direction = LayoutDirection::Vertical,
            .AlignX    = Alignment::Center,
            .AlignY    = Alignment::Center,

            .Padding = ui_padding(0.f, 0.f, 0.f, 0.f),
            .Spacing = 0.f,
            .Grow    = 0.f,
            .Shrink  = 0.f,

            .Color       = White,
            .BorderColor = ui_color(0.f, 0.f, 0.f, 0.f),
            .TextColor   = ui_color(0.f, 0.f, 0.f, 0.f),
            .CaretColor  = ui_color(0.f, 0.f, 0.f, 0.f),

            .BorderWidth  = 0.f,
            .Softness     = 0.f,
            .CornerRadius = ui_corner_radius(0.f, 0.f, 0.f, 0.f),

            .Font       = nullptr,
            .FontSize   = 0.f,
            .CaretWidth = 0.f,
        },

        .Hovered =
        {
        },

        .Focused =
        {
        },
    },
};

struct inspector_ui
{
    bool IsInitialized;
};

// ------------------------------------------------------------------------------------
// @Private : Inspector Helpers

static bool
InitializeInspector(inspector_ui &Inspector)
{
    // UI Pipeline
    {
        ui_pipeline_params Params = UIGetDefaultPipelineParams();
        Params.NodeCount     = 64;
        Params.FrameBudget   = VOID_KILOBYTE(10);
        Params.Pipeline      = UIPipeline::Default;
        Params.StyleArray    = InspectorStyleArray;
        Params.StyleIndexMin = static_cast<uint32_t>(InspectorStyle::Window);
        Params.StyleIndexMax = static_cast<uint32_t>(InspectorStyle::Window);

        UICreatePipeline(Params);
    }

    // Base Layout
    {
        ui_pipeline &Pipeline = UIBindPipeline(UIPipeline::Default);

        ui_node Window = UIWindow(static_cast<uint32_t>(InspectorStyle::Window), Pipeline);
        {
        }
        UIEndWindow(Window, Pipeline);

        UIUnbindPipeline(UIPipeline::Default);
    }


    return true;
}

// ------------------------------------------------------------------------------------
// @Public : Inspector API

static void
ShowUI(void)
{
    static inspector_ui Inspector;

    if(!Inspector.IsInitialized)
    {
        Inspector.IsInitialized = InitializeInspector(Inspector);
    }

    if(Inspector.IsInitialized)
    {
    }
}

}
