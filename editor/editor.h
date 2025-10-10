#pragma once

typedef struct editor_ui
{
    editor_console_ui ConsoleUI;
    b32               ShowFileBrowser;
} editor_ui;

global editor_ui EditorUI;

internal void ShowEditorUI  (game_state *GameState);
