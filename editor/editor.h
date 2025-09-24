#pragma once

typedef struct editor_ui
{
	file_browser_ui FileBrowserUI;
	b32             ShowFileBrowser;
} editor_ui;

internal void ShowEditorUI  (render_pass_list *PassList, render_handle Renderer);