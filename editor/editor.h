#pragma once

typedef struct editor_ui
{
    console_ui ConsoleUI;
    b32        ShowFileBrowser;
} editor_ui;

global editor_ui EditorUI; // TODO: Move this as part of the UI state

internal void ShowEditorUI  (void);
