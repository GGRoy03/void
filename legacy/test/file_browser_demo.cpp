// [Constants]

#define FileBrowser_MaxPath    260
#define FileBrowser_EntryCount 128
#define FileBrowser_HistoryDepth 32
#define FileBrowser_InvalidIndex 0xFFFFFFFF

#include "assert.h"
#define FileBrowser_Assert assert

// [Types]

typedef struct directory_entry
{
    char Name[FileBrowser_MaxPath];
    char FullPath[FileBrowser_MaxPath];
    bool IsFolder;

    uint32_t Parent;
    uint32_t Sibling;
    uint32_t FirstChild;
    uint32_t Id;
} directory_entry;

typedef struct browsing_history_stack
{
    uint32_t FolderIndices[FileBrowser_HistoryDepth];
    uint32_t Top;
} browsing_history_stack;

typedef struct file_browser_ui
{
    // UI_STORAGE
    directory_entry Entries[FileBrowser_EntryCount];
    uint32_t        EntryCount;
 
    // UI_LOGIC
    uint32_t               ActiveFolderIndex;
    browsing_history_stack Forward;
    browsing_history_stack Backward;
    bool                   IsInitialized;

    // CIM_UI_STATES
    ui_font           *Font;
    ui_component_state BackwardButton;
    ui_component_state ForwardButton;
    ui_component_state HistoryButton;
} file_browser_ui;

// [Helpers]

static directory_entry *
AllocateDirectoryEntry(file_browser_ui *FileBrowser)
{
    FileBrowser_Assert(FileBrowser);

    directory_entry *Result = NULL;

    if (FileBrowser->EntryCount < FileBrowser_EntryCount)
    {
        uint32_t EntryId = FileBrowser->EntryCount++;

        Result             = FileBrowser->Entries + EntryId;
        Result->Id         = EntryId;
        Result->FirstChild = FileBrowser_InvalidIndex;
        Result->Sibling    = FileBrowser_InvalidIndex;
        Result->Parent     = FileBrowser_InvalidIndex;
    }

    return Result;
}

#ifdef _WIN32

static void
Win32UTF8ToWide(const char *Str, wchar_t *OutBuffer, int OutBufferSize)
{
    auto SizeNeeded = MultiByteToWideChar(CP_UTF8, 0, Str, -1, nullptr, 0);

    if (SizeNeeded > OutBufferSize || SizeNeeded <= 0)
    {
        assert(!"Invalid conversion.");
        return;
    }

    int WrittenSize = MultiByteToWideChar(CP_UTF8, 0, Str, -1, OutBuffer, SizeNeeded);
    assert(WrittenSize == SizeNeeded);
}

static void
Win32WideToUTF8(const wchar_t *Str, char *OutBuffer, int OutBufferSize)
{
    auto SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, Str, -1, NULL, 0, NULL, NULL);
    if (SizeNeeded > OutBufferSize || SizeNeeded <= 0)
    {
        assert(!"Invalid conversion.");
        return;
    }

    int WrittenSize = WideCharToMultiByte(CP_UTF8, 0, Str, -1, OutBuffer, SizeNeeded, NULL, NULL);
    assert(WrittenSize == SizeNeeded);
}

static void
BuildFileTree(const char *RootFolderPath, file_browser_ui *FileBrowser)
{
    FileBrowser_Assert(RootFolderPath && FileBrowser);

    directory_entry *Root = AllocateDirectoryEntry(FileBrowser);
    Root->IsFolder   = true;
    Root->Parent     = FileBrowser_InvalidIndex;
    Root->Sibling    = FileBrowser_InvalidIndex;
    Root->FirstChild = FileBrowser_InvalidIndex;
    assert(strncpy_s(Root->Name    , FileBrowser_MaxPath, "Root"        , FileBrowser_MaxPath - 1) == 0);
    assert(strncpy_s(Root->FullPath, FileBrowser_MaxPath, RootFolderPath, FileBrowser_MaxPath - 1) == 0);

    for (uint32_t Idx = 0; Idx < FileBrowser->EntryCount; Idx++)
    {
        directory_entry *ParentEntry = FileBrowser->Entries + Idx;

        if (!ParentEntry->IsFolder)
        {
            continue;
        }

        wchar_t WidePath[FileBrowser_MaxPath];
        Win32UTF8ToWide(ParentEntry->FullPath, WidePath, FileBrowser_MaxPath);
        wcscat_s(WidePath, FileBrowser_MaxPath, L"/*");

        WIN32_FIND_DATAW FindData;
        HANDLE           Handle = FindFirstFileW(WidePath, &FindData);
        if (Handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        do
        {
            if (wcscmp(FindData.cFileName, L".") == 0 || wcscmp(FindData.cFileName, L"..") == 0)
            {
                continue;
            }

            directory_entry *ChildEntry = AllocateDirectoryEntry(FileBrowser); 
            assert(ChildEntry && "Too many files.");

            ChildEntry->IsFolder = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            ChildEntry->Parent   = Idx;

            Win32WideToUTF8(FindData.cFileName, ChildEntry->Name, FileBrowser_MaxPath);
            snprintf(ChildEntry->FullPath, FileBrowser_MaxPath, "%s/%s", ParentEntry->FullPath, ChildEntry->Name);

            ChildEntry->Sibling     = ParentEntry->FirstChild;
            ParentEntry->FirstChild = ChildEntry->Id;

        } while (FindNextFileW(Handle, &FindData));

        FindClose(Handle);
    }
}

#endif

static bool
IsHistoryEmpty(browsing_history_stack *Stack)
{
    bool Result = Stack->Top == 0;
    return Result;
}

static void
PushHistory(uint32_t FolderIndex, browsing_history_stack *Stack)
{
    if (Stack->Top < FileBrowser_HistoryDepth)
    {
        Stack->FolderIndices[Stack->Top++] = FolderIndex;
    }
}

static uint32_t
PopHistory(browsing_history_stack *Stack)
{
    uint32_t Result = FileBrowser_InvalidIndex;

    if (Stack->Top > 0)
    {
        Result = Stack->FolderIndices[--Stack->Top];
    }

    return Result;
}

static uint32_t
GetHistoryDepth(browsing_history_stack *Stack)
{
    uint32_t Result = Result = Stack->Top;; 
    return Result;
}

static directory_entry *
GetDirectoryEntry(uint32_t Index, file_browser_ui *FileBrowser)
{
    assert(FileBrowser);

    directory_entry *Result = NULL;

    if (Index < FileBrowser->EntryCount)
    {
        Result = FileBrowser->Entries + Index;
    }

    return Result;
}

// [UI]

static void
FileBrowser()
{
    static file_browser_ui FileBrowser;

    if (!FileBrowser.IsInitialized)
    {
        FileBrowser.Font = UILoadFont("Consolas", 20, UIFont_ExtendedASCIIDirectMapping);

        BuildFileTree("D:/Work/CIM/test/root_dir", &FileBrowser);
        FileBrowser.ActiveFolderIndex = 0; // Let's try something.

        PushHistory(1, &FileBrowser.Backward);
        PushHistory(2, &FileBrowser.Backward);

        FileBrowser.IsInitialized = true;
    }

    UIBeginContext()
    {
        switch (Pass)
        {

        case UIPass_Layout:
        {
            UISetFont(FileBrowser.Font);

            if (UIWindow("FileBrowser_Window", "FileBrowser_Window", NULL))
            {
                // NOTE: These markers are quite annoying. Loop macro would help, but let's think more about
                // layouts. Maybe it's fine for now, until I hit limitations.
                UIRowBlock()
                {
                    UIButton("FileBrowser_Backward", "FileBrowser_HeaderButtons", "<--", &FileBrowser.BackwardButton);
                    UIButton("FileBrowser_Forward" , "FileBrowser_HeaderButtons", "-->", &FileBrowser.ForwardButton);
                }

                if (!IsHistoryEmpty(&FileBrowser.Backward))
                {
                    UIRowBlock()
                    {
                        uint32_t Count = GetHistoryDepth(&FileBrowser.Backward);
                        for (uint32_t Idx = 0; Idx < Count; Idx++)
                        {
                            uint32_t         FolderIndex = FileBrowser.Backward.FolderIndices[Idx];
                            directory_entry *Entry       = GetDirectoryEntry(FolderIndex, &FileBrowser);
                            FileBrowser_Assert(Entry);

                            UIIndexedButton("FileBrowser_HistoryButtons", Entry->Name, &FileBrowser.HistoryButton, FolderIndex);

                            // Draw a '>' if it is not the last entry in the stack.
                            if (Idx != Count - 1)
                            {
                                UIText(">");
                            }
                        }
                    }
                }
                else
                {
                    uint32_t         FolderIndex = FileBrowser.ActiveFolderIndex;
                    directory_entry *Entry       = GetDirectoryEntry(FolderIndex, &FileBrowser);
                    FileBrowser_Assert(Entry);
                    
                    // NOTE: Here's another pain point, how come we can't infer the layout at all?
                    // If there's no layout can we just assume? How does that work? I mean.. Like since 
                    // it's already a column how come it is incorrectly placed?

                    // BUG: It is not aligned with the above buttons.
                    UIIndexedButton("FileBrowser_HistoryButtons", Entry->Name, &FileBrowser.HistoryButton, FolderIndex);
                }

                // TODO: Add separators.
            }

        } break;

        case UIPass_Event:
        {
            if (FileBrowser.BackwardButton.Clicked)
            {
                if (!IsHistoryEmpty(&FileBrowser.Backward))
                {
                    PushHistory(FileBrowser.ActiveFolderIndex, &FileBrowser.Forward);
                    FileBrowser.ActiveFolderIndex = PopHistory(&FileBrowser.Backward);
                }

                FileBrowser.BackwardButton.Clicked = false;
                FileBrowser.BackwardButton.Hovered = false;

                break;
            }

            if (FileBrowser.ForwardButton.Clicked)
            {
                if (!IsHistoryEmpty(&FileBrowser.Forward))
                {
                    PushHistory(FileBrowser.ActiveFolderIndex, &FileBrowser.Backward);
                    FileBrowser.ActiveFolderIndex = PopHistory(&FileBrowser.Forward);

                    FileBrowser.ForwardButton.Clicked = false;
                    FileBrowser.ForwardButton.Hovered = false;
                }

                break;
            }

            if (FileBrowser.HistoryButton.Clicked)
            {
                if (!IsHistoryEmpty(&FileBrowser.Backward))
                {
                    uint32_t ClickedIndex = FileBrowser.HistoryButton.ActiveIndex;
                    while (ClickedIndex != PopHistory(&FileBrowser.Backward)) {};
                    FileBrowser.ActiveFolderIndex = ClickedIndex;
                }

                FileBrowser.HistoryButton.Clicked = false;
                FileBrowser.HistoryButton.Hovered = false;
            }

        } break;

        default: break;

        }

        UIEndContext()
    }
}
