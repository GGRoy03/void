typedef struct ui_test
{
	// UI Objects
	ui_layout_tree LayoutTree;

	// Styles
	ui_style_name WindowStyle;
	ui_style_name ButtonStyle;

	// Misc
	b32 IsInitialized;
} ui_test;

internal void
TestUI(ui_state *UIState, render_context *RenderContext)
{
	local_persist ui_test UITest;

	if (!UITest.IsInitialized)
	{
		ui_layout_tree_params Params = { 0 };
		Params.NodeCount = 16;
		Params.Depth     = 4;

		// Allocations/Caching
		UITest.LayoutTree  = UIAllocateLayoutTree(Params);
		UITest.WindowStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestWindow"), &UIState->StyleRegistery);
		UITest.ButtonStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestButton"), &UIState->StyleRegistery);

		// Layout
		UIWindow(UITest.WindowStyle, &UITest.LayoutTree, &UIState->StyleRegistery);
		UIButton(UITest.ButtonStyle, &UITest.LayoutTree, &UIState->StyleRegistery);
		UIButton(UITest.ButtonStyle, &UITest.LayoutTree, &UIState->StyleRegistery);

		UITest.IsInitialized = 1;
	}

	UIComputeLayout(&UITest.LayoutTree, RenderContext);
}