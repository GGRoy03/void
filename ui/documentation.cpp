// TODO:
// Figure out what we even want to do with this
// OOP the UI itself? Could be nice.

// -----------------------------------------------------------------------------------
// @Internal Private : VoidDocumentation Strings

read_only global byte_string DocumentationIntroTitle              = str8_lit("Void Engine Documentation");
read_only global byte_string DocumentationPurposeSubtitle         = str8_lit("Why:");
read_only global byte_string DocumentationPurposeText             = str8_lit("Building game UI shouldn't be frustrating. Void is a lightweight UI engine designed to make shipping polished interfaces fast without sacrificing control. You manage allocations, flow, and rendering Void handles layout, styling and interaction efficiently.");

read_only global byte_string DocumentationKeyFeaturesSubtitle     = str8_lit("Key Features:");
read_only global byte_string DocumentationKeyFeaturesText         = str8_lit("Semi-retained mode API with zero lifetime management overhead. DSL-based styling that completely decouples presentation from logic. Component-first design: create custom components easily while leveraging a solid foundation of basic UI primitives. Built with performance in mind, Void never becomes your bottleneck.");

read_only global byte_string DocumentationAudienceSubtitle        = str8_lit("Who It's For:");
read_only global byte_string DocumentationAudienceText            = str8_lit("Game developers who want fine-grained control over their UI implementation without reinventing the wheel. Teams that need flexible styling without coupling logic to presentation. Anyone building shipped game experiences where both developer velocity and runtime performance matter.");

read_only global byte_string DocumentationQuickstartSubtitle      = str8_lit("Quick Start:");
read_only global byte_string DocumentationQuickstartText          = str8_lit("Coming soon. For now, explore the API Reference to understand the core concepts: UIBlock, UILabel, styling system, and component composition.");

read_only global byte_string DocumentationRequirementsSubtitle    = str8_lit("System Requirements:");
read_only global byte_string DocumentationRequirementsText        = str8_lit("C++20 compiler. Minimal dependencies designed to integrate with any game engine or graphics backend. Currently in early development.");

read_only global byte_string DocumentationComponentsSubtitle      = str8_lit("What's Included:");
read_only global byte_string DocumentationComponentsText          = str8_lit("Core layout engine with flexbox-style positioning. Basic UI primitives: buttons, labels, text input, scroll regions. DSL styling system. Component authoring tools. Accessibility framework (in development). Text shaping and rendering integration.");

read_only global byte_string DocumentationNavigationSubtitle      = str8_lit("How to Navigate These Docs:");
read_only global byte_string DocumentationNavigationText          = str8_lit("Start with Concepts to understand the core architecture. The API Reference documents all major types and functions. Components shows how to build custom UI elements. Styling Guide covers the DSL and visual properties. Examples demonstrate real-world patterns.");

read_only global byte_string DocumentationNextStepsSubtitle       = str8_lit("Next Steps:");
read_only global byte_string DocumentationNextStepsText           = str8_lit("New to Void? Start with Concepts. Want to build something? Jump to Components. Need to style UI? See the Styling Guide. Looking for specific functionality? Check the API Reference.");

// -----------------------------------------------------------------------------------
// @Internal Private : VoidDocumentation Core Types

enum VoidDocumentation_Style : u32
{
    VoidDocumentation_None         = 0,
    VoidDocumentation_MainWindow   = 1,
    VoidDocumentation_TextTitle    = 2,
    VoidDocumentation_TextSubtitle = 3,
    VoidDocumentation_TextBody     = 4,
};

struct ui_void_documentation
{
    ui_pipeline *UIPipeline;
    b32          IsInitialized;
};

// -----------------------------------------------------------------------------------
// @Internal Private : VoidDocumentation Context Management

internal bool
InitializeVoidDocumentation(ui_void_documentation *Doc)
{
    Assert(Doc);
    Assert(!Doc->IsInitialized);

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
            Assert(Window);
            Window->SetScroll(32.f, UIAxis_Y);
            Window->DebugBox(UILayoutNode_DebugOuterBox  , 1);
            Window->DebugBox(UILayoutNode_DebugInnerBox  , 1);
            Window->DebugBox(UILayoutNode_DebugContentBox, 1);

            ui_node *Title    = UILabel(DocumentationIntroTitle     , VoidDocumentation_TextTitle);
            ui_node *Subtitle = UILabel(DocumentationPurposeSubtitle, VoidDocumentation_TextSubtitle);
            ui_node *Text     = UILabel(DocumentationPurposeText    , VoidDocumentation_TextBody);

            Title->DebugBox(UILayoutNode_DebugContentBox, 1);
            Subtitle->DebugBox(UILayoutNode_DebugContentBox, 1);
            Text->DebugBox(UILayoutNode_DebugContentBox, 1);

            UILabel(DocumentationKeyFeaturesSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationKeyFeaturesText    , VoidDocumentation_TextBody);

            UILabel(DocumentationAudienceSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationAudienceText    , VoidDocumentation_TextBody);

            UILabel(DocumentationQuickstartSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationQuickstartText    , VoidDocumentation_TextBody);

            UILabel(DocumentationRequirementsSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationRequirementsText    , VoidDocumentation_TextBody);

            UILabel(DocumentationComponentsSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationComponentsText    , VoidDocumentation_TextBody);

            UILabel(DocumentationNavigationSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationNavigationText    , VoidDocumentation_TextBody);

            UILabel(DocumentationNextStepsSubtitle, VoidDocumentation_TextSubtitle);
            UILabel(DocumentationNextStepsText    , VoidDocumentation_TextBody);
        }
    }

    return true;
}

internal void
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
