#pragma once

// [GLOBALS]

read_only global u32 FileBrowser_HistoryDepth      = 32;
read_only global u32 FileBrowser_InvalidEntryIndex = 0xFFFFFFFF;
read_only global u32 FileBrowser_MaximumEntryCount = 128;

// [CORE TYPES]

typedef struct directory_entry directory_entry;
struct directory_entry
{
    byte_string Name;
    byte_string FullPath;
    b32         IsFolder;

    directory_entry *Parent;
    directory_entry *Next;
    directory_entry *First;
};

typedef struct file_browser_ui
{
    // Misc
    ui_pipeline   Pipeline;
    memory_arena *Arena;
    b32           IsInitialized;

    // Entries
    directory_entry *RootEntry;
    u32              EntryCount;

    // Styles
    ui_style_name MainWindowStyle;
    ui_style_name MainHeaderStyle;
    ui_style_name HeaderButtonStyle;
} file_browser_ui;

// [CORE API]

// [Handlers]

internal void FileBrowserForwardHistory   (ui_node *Node, ui_pipeline *Pipeline);
internal void FileBrowserBackwardHistory  (ui_node *Node, ui_pipeline *Pipeline);

// [UI]

internal void FileBrowserUI               (file_browser_ui *UI, render_pass_list *PassList, render_handle Renderer, ui_state *UIState);
