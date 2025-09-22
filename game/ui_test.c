typedef struct ui_test
{
	// UI Objects
	ui_pipeline Pipeline;

	// Styles
	ui_style_name WindowStyle;
	ui_style_name ButtonStyle;
	ui_style_name LabelStyle;

	// Misc
	b32 IsInitialized;
} ui_test;

internal void
TestUI(render_pass_list *PassList, render_handle RendererHandle)
{
    local_persist ui_test UITest;

    // Unpacking
    ui_pipeline   *Pipeline    = &UITest.Pipeline;
    ui_style_name *WindowStyle = &UITest.WindowStyle;
	ui_style_name *ButtonStyle = &UITest.ButtonStyle;
	ui_style_name *LabelStyle  = &UITest.LabelStyle;

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
			*WindowStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestWindow"), &Pipeline->StyleRegistery);
			*ButtonStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestButton"), &Pipeline->StyleRegistery);
			*LabelStyle  = UIGetCachedNameFromStyleName(byte_string_literal("TestLabel") , &Pipeline->StyleRegistery);
		}

		// Layout
		UIWindow(*WindowStyle, Pipeline);
		UIButton(*ButtonStyle, Pipeline);
		UIButton(*ButtonStyle, Pipeline);
		UILabel (*LabelStyle , byte_string_literal("Texxxxt"), Pipeline);

		UITest.IsInitialized = 1;
	}

	UIPipelineBegin(Pipeline);
	UIPipelineExecute(Pipeline, PassList);
}
