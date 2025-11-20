typedef enum ConsoleStyle_Type
{
    ConsoleStyle_None            = 0 ,
    ConsoleStyle_Window              ,
    ConsoleStyle_Header              ,
    ConsoleStyle_HeaderStatus        ,
    ConsoleStyle_WindowContainer     ,
    ConsoleStyle_ScrollWindow        ,
    ConsoleStyle_InspectWindow       ,
    ConsoleStyle_BoxModelDiagram     ,
    ConsoleStyle_BoxModelDiagramLabel,
    ConsoleStyle_MessageInfo         ,
    ConsoleStyle_MessageWarning      ,
    ConsoleStyle_MessageError        ,
    ConsoleStyle_Prompt              ,
    ConsoleStyle_Image               ,
} ConsoleStyle_Type;

typedef enum ConsoleMode_Type
{
    ConsoleMode_Default = 0,
    ConsoleMode_Inspect = 1,
    ConsoleMode_Count   = 2,
} ConsoleMode_Type;

typedef enum ConsoleConstant_Type
{
    ConsoleConstant_MessageCountLimit = 60,
    ConsoleConstant_MessageSizeLimit  = Kilobyte(2),
    ConsoleConstant_PromptBufferSize  = 128,
    ConsoleConstant_FlagSizeLimit     = 16,
} ConsoleConstant_Type;

typedef struct console_output
{
    byte_string       Message;
    ConsoleStyle_Type Style;
} console_output;

typedef struct console_inspect_mode
{
    u32         NodeIndex;
    ui_subtree *Subtree;
} console_inspect_mode;

typedef struct console_ui
{
    ui_pipeline  *Pipeline;
    b32           IsInitialized;

    // State
    byte_string StatusText;
    u8          PromptBuffer[ConsoleConstant_PromptBufferSize];
    u64         PromptBufferSize;

    // Modes
    b32                  ActiveModes[ConsoleMode_Count];
    console_inspect_mode Inspect;

    // Transient
    memory_arena *FrameArena;

    // Scroll Buffer State
    u32 MessageLimit;
    u32 MessageHead;
    u32 MessageTail;
} console_ui;

// -----------------------------------------------------------------------------------
// Console State Internal Implementation

internal b32
IsConsoleModeActive(ConsoleMode_Type Mode, console_ui *Console)
{
    b32 Result = Console->ActiveModes[Mode];
    return Result;
}

internal void
SetConsoleModeState(ConsoleMode_Type Mode, b32 State, console_ui *Console)
{
    Assert(Mode > ConsoleMode_Default && Mode < ConsoleMode_Count);

    Console->ActiveModes[Mode] = State;
}

internal b32
IsSameInspectedNode(u32 NodeIndex, ui_subtree *Subtree, console_ui *Console)
{
    b32 Result = (Console->Inspect.NodeIndex == NodeIndex) && (Console->Inspect.Subtree == Subtree);
    return Result;
}

// -----------------------------------------------------------------------------------

internal b32
IsValidConsoleOutput(console_output Output)
{
    b32 Result = IsValidByteString(Output.Message) && (Output.Style >= ConsoleStyle_MessageInfo && Output.Style <= ConsoleStyle_MessageError);
    return Result;
}

internal console_output
FormatConsoleOutput(byte_string Message, ConsoleMessage_Severity Severity, memory_arena *Arena)
{
    console_output Result = {};

    if(IsValidByteString(Message))
    {
        ConsoleStyle_Type Style  = ConsoleStyle_MessageInfo;
        byte_string       Prefix = ByteString(0, 0);

        switch(Severity)
        {

        case ConsoleMessage_Info:
        {
            Style  = ConsoleStyle_MessageInfo;
            Prefix = byte_string_literal("[Info] => ");
        } break;

        case ConsoleMessage_Warn:
        {
            Style  = ConsoleStyle_MessageWarning;
            Prefix = byte_string_literal("[Warn] => ");
        } break;

        case ConsoleMessage_Error:
        {
            Style  = ConsoleStyle_MessageError;
            Prefix = byte_string_literal("[Error] => ");
        } break;

        case ConsoleMessage_Fatal:
        {
            Style  = ConsoleStyle_MessageError;
            Prefix = byte_string_literal("[Fatal] => ");
        } break;

        default: break;

        }

        Result.Message = ByteStringAppend(Message, Prefix, 0, Arena);
        Result.Style   = Style;
    }

    return Result;
}

internal void
ConsolePrintMessage(console_output Output, ui_node BufferNode, console_ui *Console)
{
    Assert(BufferNode.CanUse);
    Assert(IsValidConsoleOutput(Output));

    u32 ChildIndex = Console->MessageHead++;

    ui_node Child = UINodeFindChild(BufferNode, ChildIndex);
    {
        UINodeSetStyle(Child, Output.Style);
        UINodeSetText(Child, Output.Message);
    }
}

// -----------------------------------------------------------------------------------
// Inspect Mode Internal Implementation

internal void
RenderBoxModelDiagram(ui_layout_node *Node)
{
    // rect_f32 OuterBox   = Node->Value.OuterBox;
    // rect_f32 InnerBox   = Node->Value.OuterBox;
    // rect_f32 ContentBox = Node->Value.ContentBox;

    ui_node Outer   = {0};
    ui_node Inner   = {0};
    ui_node Content = {0};

    vec2_unit BoxSize = {};
    BoxSize.X.Type    = UIUnit_Percent;
    BoxSize.X.Percent = 100.f;
    BoxSize.Y.Type    = UIUnit_Percent;
    BoxSize.Y.Percent = 100.f;

    ui_color OuterColor   = UIColor(0.424f, 0.612f, 0.784f, 0.4f);
    ui_color InnerColor   = UIColor(0.345f, 0.651f, 0.608f, 0.4f);
    ui_color ContentColor = UIColor(0.431f, 0.627f, 0.765f, 0.5f);

    UIBlock(Outer = UINode(UILayoutNode_IsParent))
    {
        vec2_unit OuterBoxSize = {};
        OuterBoxSize.X.Type    = UIUnit_Percent;
        OuterBoxSize.X.Percent = 100.f;
        OuterBoxSize.Y.Type    = UIUnit_Float32;
        OuterBoxSize.Y.Float32 = 110.f;

        UINodeSetStyle(Outer, ConsoleStyle_BoxModelDiagram);
        UINodeSetSize (Outer, OuterBoxSize);
        UINodeSetColor(Outer, OuterColor);

        UIBlock(Inner = UINode(UILayoutNode_IsParent))
        {
            UINodeSetStyle(Inner, ConsoleStyle_BoxModelDiagram);
            UINodeSetSize (Inner, BoxSize);
            UINodeSetColor(Inner, InnerColor);

            UIBlock(Content = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(Content, ConsoleStyle_BoxModelDiagram);
                UINodeSetSize (Content, BoxSize);
                UINodeSetColor(Content, ContentColor);

                UILabel(str8_lit("[0 x 0]"), ConsoleStyle_BoxModelDiagramLabel);
            }
        }
    }
}

// -----------------------------------------------------------------------------------
// Console Prompt Internal Implementation

internal void
HandleConsoleFlag(byte_string Flag, console_ui *Console)
{
    byte_string InspectFlag = byte_string_compile("inspect");
    if(ByteStringMatches(InspectFlag, Flag, NoFlag))
    {
        SetConsoleModeState(ConsoleMode_Inspect, 1, Console);

        ui_node ContentWindows = UIFindNodeById(ui_id("Console_ContentWindows"), Console->Pipeline->IdTable);
        if(ContentWindows.CanUse)
        {
            ui_node InspectWindow = {0};
            UIBlock(InspectWindow = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(InspectWindow, ConsoleStyle_InspectWindow);

                RenderBoxModelDiagram(0);


                // TODO:
                // Draw the box model diagram. Start with 3 rectangles. (Border-Padding-Content)
                // Add text.
                // Add numbers.
                // Add clickability.
            }

            UINodeAppendChild(ContentWindows, InspectWindow);
        }
    }
}

internal void
ReadConsolePrompt(console_ui *Console)
{
    Assert(Console);

    u8 *Start = Console->PromptBuffer;
    u32 Size  = Console->PromptBufferSize;
    u8 *End   = Start + Size;

    while(Start < End)
    {
        while(Start < End && IsWhiteSpace(*Start))
        {
            ++Start;
        }

        if(Start >= End)
        {
            break;
        }

        if(*Start == '/')
        {
            ++Start;

            byte_string Flag = ByteString(0, 0);
            Flag.String = PushArray(Console->FrameArena, u8, ConsoleConstant_FlagSizeLimit);

            while((Start < End) && (Flag.Size < ConsoleConstant_FlagSizeLimit) && !IsWhiteSpace(*Start))
            {
                Flag.String[Flag.Size++] = ToLowerChar(*Start++);
            }

            while(Start < End && IsWhiteSpace(*Start))
            {
                ++Start;
            }

            HandleConsoleFlag(Flag, Console);
        }
        else
        {
            Assert(!"Unsure");
        }
    }

    ui_node PromptNode = UIFindNodeById(ui_id("Console_Prompt"), Console->Pipeline->IdTable);
    if(PromptNode.CanUse)
    {
        UINodeClearTextInput(PromptNode);
    }
}

internal UIEvent_State
ConsolePrompt_PressedKey(OSInputKey_Type Key, void *UserData)
{
    console_ui *Console = (console_ui *)UserData;
    Assert(Console);

    if(Key == OSInputKey_Enter)
    {
        ReadConsolePrompt(Console);
        return UIEvent_Handled;
    }

    return UIEvent_Untouched;
}

// ------------------------------------------------------------------------------------
// Console Context Implementation

internal b32
InitializeConsoleUI(console_ui *Console)
{
    Assert(Console);
    Assert(!Console->IsInitialized);

    Console->StatusText       = str8_lit("We should display useful information here.");
    Console->MessageLimit     = ConsoleConstant_MessageCountLimit;
    Console->PromptBufferSize = ConsoleConstant_PromptBufferSize;

    memory_arena_params ArenaParams = {0};
    {
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Kilobyte(64);
        ArenaParams.CommitSize        = Kilobyte(4);
    }
    Console->FrameArena = AllocateArena(ArenaParams);

    ui_pipeline_params PipelineParams =
    {
        .ThemeFile = byte_string_literal("styles/console.cim"),
    };
    Console->Pipeline = UICreatePipeline(PipelineParams);

    UICreateImageGroup(str8_lit("ImageGroup"), 1024, 1024);

    // NOTE:
    // This whole subtree thing is quite bad.

    ui_subtree_params SubtreeParams =
    {
        .CreateNew = 1,
        .NodeCount = 256,
    };

    UISubtree(SubtreeParams)
    {
        UIBlock(UIWindow(ConsoleStyle_Window))
        {
            UIImage(str8_lit("resources/images/badge-alert.png"), str8_lit("ImageGroup"), ConsoleStyle_Image);

            ui_node Header = {0};
            UIBlock(Header = UINode(UILayoutNode_IsParent))
            {
                UINodeSetStyle(Header, ConsoleStyle_Header);

                UILabel(Console->StatusText, ConsoleStyle_HeaderStatus);
            }

            ui_node ContentWindows = {0};
            UIBlock(ContentWindows = UINode(UILayoutNode_IsParent))
            {
                UINodeSetId(ContentWindows, ui_id("Console_ContentWindows"));
                UINodeSetStyle(ContentWindows, ConsoleStyle_WindowContainer);

                ui_node ScrollBuffer = {0};
                UIBlock(ScrollBuffer = UIScrollableContent(UIAxis_Y, ConsoleStyle_ScrollWindow))
                {
                    UINodeSetId(ScrollBuffer, ui_id("Console_Buffer"));
                    UINodeReserveChildren(ScrollBuffer, Console->MessageLimit);
                }
            }

            ui_node Input = UITextInput(Console->PromptBuffer, Console->PromptBufferSize, ConsoleStyle_Prompt);
            {
                UINodeSetId(Input, ui_id("Console_Prompt"));
                UINodeListenOnKey(Input, ConsolePrompt_PressedKey, Console);
            }
        }
    }

    ConsoleWriteMessage(info_message("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ConsoleWriteMessage(info_message("abcdefghijklmnopqrstuvwxyz"));

    return 1;
}

// ------------------------------------------------------------------------------------
// Console Public API Implementation

internal void
ShowConsoleUI(void)
{
    static console_ui Console;

    if(!Console.IsInitialized)
    {
        Console.IsInitialized = InitializeConsoleUI(&Console);
    }

    UIBeginAllSubtrees(Console.Pipeline);
    ClearArena(Console.FrameArena);

    ui_node BufferNode = UIFindNodeById(ui_id("Console_Buffer"), Console.Pipeline->IdTable);
    if (BufferNode.CanUse)
    {
        console_queue_node *Node = 0;
        while ((Node = PopConsoleMessageQueue(&UIState.Console)))
        {
           byte_string    RawMessage = ByteString(Node->Value.Text, Node->Value.TextSize);
           console_output Output     = FormatConsoleOutput(RawMessage, Node->Value.Severity, Console.FrameArena);

           ConsolePrintMessage(Output, BufferNode, &Console);

           FreeConsoleNode(Node);
        }
    }

    // BUG:
    // If the hover check hasn't been done, then we have no idea if the node might
    // appear or not. We might just cheat and always check for hover, but it's a
    // bandaid. We do not have access to every subtree. Well we do... Uh.

    if (IsConsoleModeActive(ConsoleMode_Inspect, &Console) && UIHasNodeHover())
    {
        ui_hovered_node Hovered = UIGetNodeHover();
        if(!IsSameInspectedNode(Hovered.Index, Hovered.Subtree, &Console))
        {
            ui_layout_node *Node  = GetLayoutNode(Hovered.Index, Hovered.Subtree->LayoutTree);
            style_property *Props = GetPaintProperties(Hovered.Index, 0, Hovered.Subtree);

            Assert(Node);
            Assert(Props);

            // NOTE:
            // We can unpack content boxes and whatnot.
            // As well as the styling. We basically have all the information.
            // What do we do and how.

            // TODO:
            // Display all the valuable information!!!
        }
    }

    UIExecuteAllSubtrees(Console.Pipeline);
}
