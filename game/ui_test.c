typedef struct ui_test
{
	// UI Objects
	ui_pipeline Pipeline;

	// Styles
	ui_style_name WindowStyle;
	ui_style_name ButtonStyle;

	// Misc
	b32 IsInitialized;
} ui_test;

internal void
TestUI(render_pass_list *PassList, render_handle RendererHandle)
{
	local_persist ui_test UITest;

	// Unpacking
	ui_pipeline *Pipeline = &UITest.Pipeline;

	if (!UITest.IsInitialized)
	{
		// Pipeline
		{
			byte_string ThemeFiles[] =
			{
				byte_string_literal("D:/Work/CIM/styles/window.cim"),
			};

			ui_pipeline_params Params = {0};
			Params.ThemeFiles     = ThemeFiles;
			Params.ThemeCount     = ArrayCount(ThemeFiles);
			Params.TreeDepth      = 4;
			Params.TreeNodeCount  = 16;
			Params.FontCount      = 1;
			Params.RendererHandle = RendererHandle;

			*Pipeline = UICreatePipeline(Params);
		}

		// Styles
		{
			UITest.WindowStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestWindow"), &Pipeline->StyleRegistery);
			UITest.ButtonStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestButton"), &Pipeline->StyleRegistery);
		}

		// Fonts
		{
			ui_font *MainFont = UILoadFont(byte_string_literal("Consolas"), 15, RendererHandle, UIFontCoverage_ASCIIOnly);
			UIPushFont(Pipeline, MainFont);
		}

		// Layout
		UIWindow(UITest.WindowStyle, Pipeline);
		UIButton(UITest.ButtonStyle, Pipeline);
		UIButton(UITest.ButtonStyle, Pipeline);
		UILabel(byte_string_literal("Hello. This is text."), Pipeline);

		UITest.IsInitialized = 1;
	}

	UIPipelineBegin(Pipeline);
	UIPipelineExecute(Pipeline, PassList);
}