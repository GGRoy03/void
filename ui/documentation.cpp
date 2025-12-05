// -----------------------------------------------------------------------------------
// @internal Private : VoidDocumentation Strings

const static byte_string DocumentationIntroTitle = str8_lit("Void Documentation");

// -----------------------------------------------------------------------------------
// @internal Private : VoidDocumentation Core Types

enum VoidDocumentation_Style : uint32_t
{
    VoidDocumentation_None       = 0,
    VoidDocumentation_MainWindow = 1,
    VoidDocumentation_SubWindow  = 2 ,
};

static ui_cached_style StyleArray[] =
{
    // Main Window
    {
        .Default =
        {
            .Sizing          = {{Sizing::Fixed, {500.f}}, {Sizing::Fixed, {500.f}}}, // NOTE: Bad.
            .MinSize         = {400.f, 400.f},
            .MaxSize         = {600.f, 600.f},
            .LayoutDirection = LayoutDirection::Vertical,
            .AlignmentM      = Alignment::Center,
            .AlignmentC      = Alignment::Center,

            .Padding         = {8, 8, 8, 8},
            .Spacing         = 4.f,

            .Color           = ui_color{0.15f, 0.15f, 0.15f, 1.0f},
            .BorderColor     = ui_color{0.30f, 0.30f, 0.30f, 1.0f},
            .TextColor       = ui_color{1.0f , 1.0f , 1.0f , 1.0f},

            .BorderWidth     = 3.f,
            .Softness        = 2.f,
            .CornerRadius    = {4.f, 4.f, 4.f, 4.f},

            .FontSize        = 14.f,
        },

        .Hovered = {},
        .Focused = {},
    },

    // SubWindow
    {
        .Default =
        {
            .Sizing          = {{Sizing::Fixed, {200.f}}, {Sizing::Fixed, {300.f}}}, // NOTE: Bad.
            .LayoutDirection = LayoutDirection::Horizontal,
            .AlignmentM      = Alignment::Center,
            .AlignmentC      = Alignment::Center,

            .Grow            = 1.f,
            .Shrink          = 1.f,

            .Color           = ui_color{1.f, 0.15f, 0.15f, 1.0f},
            .BorderColor     = ui_color{0.30f, 0.30f, 0.30f, 1.0f},

            .BorderWidth     = 3.f,
            .Softness        = 2.f,
            .CornerRadius    = {4.f, 4.f, 4.f, 4.f},

            .FontSize        = 14.f,
        },

        .Hovered = {},
        .Focused = {},
    },
};

struct ui_void_documentation
{
    ui_pipeline *UIPipeline;
    bool         IsInitialized;
};

// -----------------------------------------------------------------------------------
// @internal Private : VoidDocumentation Context Management

static bool
InitializeVoidDocumentation(ui_void_documentation *Doc)
{
    VOID_ASSERT(Doc);
    VOID_ASSERT(!Doc->IsInitialized);

    ui_pipeline_params PipelineParams =
    {
        .ThemeFile = byte_string_literal("styles/void_parser.void"),
    };
    Doc->UIPipeline = UICreatePipeline(PipelineParams);

    ui_subtree_params SubtreeParams =
    {
        .CreateNew = true,
        .NodeCount = 256,
    };

    UISubtree(SubtreeParams)
    {
        ui_node *Window = 0;
        UIBlock(Window = UIWindow(VoidDocumentation_MainWindow))
        {
            Window->DebugBox(UILayoutNode_DebugOuterBox  , 1);
            Window->DebugBox(UILayoutNode_DebugInnerBox  , 1);
            Window->DebugBox(UILayoutNode_DebugContentBox, 1);

            ui_node *Child0 = UICreateNode(0, 0);
            ui_node *Child1 = UICreateNode(0, 0);

            Child0->SetStyle(VoidDocumentation_SubWindow);
            Child1->SetStyle(VoidDocumentation_SubWindow);
        }
    }

    return true;
}

static void
ShowVoidDocumentationUI(void)
{
    static ui_void_documentation Doc;

    if(!Doc.IsInitialized)
    {
        Doc.IsInitialized = InitializeVoidDocumentation(&Doc);
    }

    UIBeginAllSubtrees(Doc.UIPipeline);
    UIExecuteAllSubtrees(Doc.UIPipeline);
}
