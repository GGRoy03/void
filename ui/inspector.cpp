namespace Inspector
{

enum class InspectorStyle : uint32_t
{
    Window    = 0,
    Something = 1,
};

constexpr ui_color MainOrange        = ui_color(0.9765f, 0.4510f, 0.0863f, 1.0f);
constexpr ui_color WhiteOrange       = ui_color(1.0000f, 0.9686f, 0.9294f, 1.0f);
constexpr ui_color SurfaceOrange     = ui_color(0.9961f, 0.8431f, 0.6667f, 1.0f);
constexpr ui_color HoverOrange       = ui_color(0.9176f, 0.3451f, 0.0471f, 1.0f);
constexpr ui_color SubtleOrange      = ui_color(0.1686f, 0.1020f, 0.0627f, 1.0f);

constexpr ui_color Background        = ui_color(0.0588f, 0.0588f, 0.0627f, 1.0f);
constexpr ui_color SurfaceBackground = ui_color(0.1020f, 0.1098f, 0.1176f, 1.0f);
constexpr ui_color BorderOrDivider   = ui_color(0.1804f, 0.1961f, 0.2118f, 1.0f);

constexpr ui_color TextPrimary       = ui_color(0.9765f, 0.9804f, 0.9843f, 1.0f);
constexpr ui_color TextSecondary     = ui_color(0.6118f, 0.6392f, 0.6863f, 1.0f);

constexpr ui_color Success           = ui_color(0.1333f, 0.7725f, 0.3686f, 1.0f);
constexpr ui_color Error             = ui_color(0.9373f, 0.2667f, 0.2667f, 1.0f);
constexpr ui_color Warning           = ui_color(0.9608f, 0.6196f, 0.0431f, 1.0f);

// TODO:
// Find out how to remove the annoying repetition. Some sort of automatic inheritence.

static ui_cached_style InspectorStyleArray[] =
{
    // Window
    {
        .Default =
        {
            .SizingX     = UIFixedSizing(500.f),
            .SizingY     = UIFixedSizing(500.f),
            .MinSize     = ui_size(400.f, 400.f),
            .MaxSize     = ui_size(600.f, 600.f),
            .Direction   = LayoutDirection::Vertical,
            .AlignX      = Alignment::Center,
            .AlignY      = Alignment::Center,

            .Padding     = ui_padding(10.f, 10.f, 10.f, 10.f),

            .Color       = Background,
            .BorderColor = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = ui_corner_radius(4.f, 4.f, 4.f, 4.f),
        },

        .Hovered =
        {
            .Color       = Background,
            .BorderColor = HoverOrange,
        },

        .Focused =
        {
            .Color       = Success,
            .BorderColor = BorderOrDivider,
        },
    },

    // Something
    {
        .Default =
        {
            .SizingX      = UIFixedSizing(300.f),
            .SizingY      = UIFixedSizing(300.f),
            .MinSize      = ui_size(150.f, 150.f),
            .MaxSize      = ui_size(300.f, 300.f),
            .Direction    = LayoutDirection::Vertical,
            .AlignX       = Alignment::Center,
            .AlignY       = Alignment::Center,
            .Shrink       = 1.f,

            .Color        = SurfaceBackground,
            .BorderColor  = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = ui_corner_radius(4.f, 4.f, 4.f, 4.f),
        },

        .Hovered =
        {
            .Color       = Background,
            .BorderColor = HoverOrange,
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
        Params.Pipeline      = UIPipeline::Default;
        Params.StyleArray    = InspectorStyleArray;
        Params.StyleIndexMin = static_cast<uint32_t>(InspectorStyle::Window);
        Params.StyleIndexMax = static_cast<uint32_t>(InspectorStyle::Something);
        Params.FrameBudget   = VOID_KILOBYTE(50);

        UICreatePipeline(Params);
    }

    // NOTE:
    // This looks nice.

    // Base Layout
    {
        ui_pipeline &Pipeline = UIBindPipeline(UIPipeline::Default);

        ui_node Window = UIWindow(static_cast<uint32_t>(InspectorStyle::Window), Pipeline);
        {
            ui_node Dummy0 = UIDummy(static_cast<uint32_t>(InspectorStyle::Something), Pipeline);
            {
                UIEndDummy(Dummy0, Pipeline);
            }

            ui_node Dummy1 = UIDummy(static_cast<uint32_t>(InspectorStyle::Something), Pipeline);
            {
                UIEndDummy(Dummy1, Pipeline);
            }

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
        UIBindPipeline(UIPipeline::Default);

        UIUnbindPipeline(UIPipeline::Default);
    }
}

}
