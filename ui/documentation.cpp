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
        .ThemeFile = byte_string_literal("styles/void_documentation.void"),
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
