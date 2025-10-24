// [Helpers]

internal ui_color
UIColor(f32 R, f32 G, f32 B, f32 A)
{
    ui_color Result = { R, G, B, A };
    return Result;
}

internal ui_spacing
UISpacing(f32 Horizontal, f32 Vertical)
{
    ui_spacing Result = { Horizontal, Vertical };
    return Result;
}

internal ui_padding
UIPadding(f32 Left, f32 Top, f32 Right, f32 Bot)
{
    ui_padding Result = { Left, Top, Right, Bot };
    return Result;
}

internal ui_corner_radius
UICornerRadius(f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight)
{
    ui_corner_radius Result = { TopLeft, TopRight, BotLeft, BotRight };
    return Result;
}

internal vec2_unit
Vec2Unit(ui_unit U0, ui_unit U1)
{
    vec2_unit Result = { .X = U0, .Y = U1 };
    return Result;
}

internal b32
IsNormalizedColor(ui_color Color)
{
    b32 Result = (Color.R >= 0.f && Color.R <= 1.f) &&
                 (Color.G >= 0.f && Color.G <= 1.f) &&
                 (Color.B >= 0.f && Color.B <= 1.f) &&
                 (Color.A >= 0.f && Color.A <= 1.f);

    return Result;
}

// -------------------------------------------------------------
// UI Context Public API Implementation

internal void
UIBeginFrame()
{
    ui_state *State = &UIState;

    // Clear the state if needed
    b32 MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    if(MouseReleased)
    {
        State->CapturedNode = 0;
    }
}

// -------------------------------------------------------------
// UI Node Private API Implementation

internal ui_node
GetNodeFromChain(ui_node_chain Chain)
{
    ui_node Node = Chain.Node;
    return Node;
}

internal ui_subtree *
GetSubtreeFromChain(ui_node_chain Chain, ui_pipeline *Pipeline)
{
    ui_subtree *Result = 0;

    if(Chain.Node.CanUse)
    {
        if(Chain.Subtree)
        {
            Result = Chain.Subtree;
        }
        else
        {
            Result = FindSubtree(Chain.Node, Pipeline);
        }
    }

    return Result;
}

internal ui_node
SetTextColorInChain(ui_color Color, ui_pipeline *Pipeline)
{
    ui_node     Node    = GetNodeFromChain(Pipeline->Chain);
    ui_subtree *Subtree = GetSubtreeFromChain(Pipeline->Chain, Pipeline); // NOTE: Not great.

    if(Subtree)
    {
        SetUITextColor(Node, Color, Subtree, Pipeline->StaticArena);
    }

    return Node;
}

internal ui_node
FindChildInChain(u32 Index, ui_pipeline *Pipeline)
{
    ui_node     Node    = GetNodeFromChain(Pipeline->Chain);
    ui_subtree *Subtree = GetSubtreeFromChain(Pipeline->Chain, Pipeline); // NOTE: Not great.

    if(Subtree)
    {
        ui_layout_tree *LayoutTree = Subtree->LayoutTree;
        if(LayoutTree)
        {
            // WARN: The problem is that I never set the subtree?
            // Which causes us to query it again and again.

            BeginNodeChain(UIFindChild(Node, Index, LayoutTree), Pipeline); // NOTE: Not great.
        }
    }

    return Node;
}

internal ui_node
SetNodeIdInChain(byte_string Id, ui_pipeline *Pipeline)
{
    ui_node     Node    = GetNodeFromChain(Pipeline->Chain);
    ui_subtree *Subtree = GetSubtreeFromChain(Pipeline->Chain, Pipeline);

    if(Subtree)
    {
        UISetNodeId(Id, Node, Pipeline->IdTable);
    }

    return Node;
}

internal void
BeginNodeChain(ui_node Node, ui_pipeline *Pipeline)
{
    ui_node_chain Chain = {0};
    Chain.Node              = Node;
    Chain.Node.SetTextColor = SetTextColorInChain;
    Chain.Node.FindChild    = FindChildInChain;
    Chain.Node.SetId        = SetNodeIdInChain;

    Pipeline->Chain = Chain;
}

// -------------------------------------------------------------
// UI Node Public API Implementation

internal ui_node
UIGetLast(ui_pipeline *Pipeline)
{
    ui_node_chain Chain = Pipeline->Chain;
    ui_node       Node  = Chain.Node;
    return Node;
}

// -------------------------------------------------------------
// Events Public API Implementation

internal void
ProcessUIEventList(ui_event_list *Events)
{
    IterateLinkedList(Events, ui_event_node *, Node)
    {
        ui_event *Event = &Node->Value;

        switch(Event->Type)
        {

        case UIEvent_Hover:
        {
            ui_hover_event Hover = Event->Hover;

            style_property *HoverStyle = GetHoverStyle(Hover.CachedStyleIndex, Hover.Source->Registry);
            if(HoverStyle)
            {
            }
        } break;

        case UIEvent_Click:
        {
        } break;

        case UIEvent_Resize:
        {
        } break;

        case UIEvent_Drag:
        {
        } break;

        }
    }
}

internal void
RecordUIEvent(ui_event Event, ui_event_list *Events, memory_arena *Arena)
{
    ui_event_node *Node = PushStruct(Arena, ui_event_node);
    if(Node)
    {
        Node->Value = Event;
        AppendToLinkedList(Events, Node, Events->Count);
    }
}

internal void
RecordUIHoverEvent(ui_node Node, ui_pipeline *Source, ui_event_list *Events, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Hover, .Hover.Node = Node, .Hover.Source = Source};
    RecordUIEvent(Event, Events, Arena);
}

// ----------------------------------------------------------------------------------
// Pipeline Public API Implementation

internal void
UIBeginSubtree(ui_subtree_params Params, ui_pipeline *Pipeline)
{
    Assert(Pipeline);

    memory_arena *Persist   = Pipeline->StaticArena; Assert(Persist);
    memory_arena *Transient = Pipeline->FrameArena;  Assert(Transient);

    if(Params.CreateNew)
    {
        ui_subtree_node *SubtreeNode = PushStruct(Persist, ui_subtree_node);
        if(SubtreeNode)
        {
            ui_subtree_list *SubtreeList = &Pipeline->Subtrees;
            AppendToLinkedList(SubtreeList, SubtreeNode, SubtreeList->Count);

            ui_layout_tree *LayoutTree = 0;
            {
                ui_layout_tree_params LayoutTreeParams = {0};
                LayoutTreeParams.NodeCount = Params.LayoutNodeCount;
                LayoutTreeParams.NodeDepth = Params.LayoutNodeDepth;

                u64   Footprint = GetLayoutTreeFootprint(LayoutTreeParams);
                void *Memory    = PushArray(Persist, u8, Footprint);

                LayoutTree = PlaceLayoutTreeInMemory(LayoutTreeParams, Memory);
            }

            SubtreeNode->Value.LayoutTree     = LayoutTree;
            SubtreeNode->Value.ComputedStyles = PushArray(Persist, ui_node_style, 0);
            SubtreeNode->Value.Id             = Pipeline->NextSubtreeId++;

            Pipeline->CurrentSubtree = &SubtreeNode->Value;
        }
    }
    else
    {
        // NOTE: Not sure..
    }
}

internal void
UIEndSubtree(ui_subtree_params Params)
{
    Useless(Params);
}

internal void
UIBeginPipeline(ui_pipeline *Pipeline)
{
    ClearArena(Pipeline->FrameArena);
}

internal ui_pipeline *
UICreatePipeline(ui_pipeline_params Params)
{
    ui_state    *State  = &UIState;
    ui_pipeline *Result = PushArray(State->StaticData, ui_pipeline, 1);

    if(Result)
    {
        // Static Data
        {
            memory_arena_params ArenaParams = { 0 };
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result->StaticArena = AllocateArena(ArenaParams);
        }

        // NOTE: I wonder if we even want this rather than a user supplied arena.
        // Unsure yet, so stick with this.

        // Frame Arena
        {
            memory_arena_params ArenaParams = {0};
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result->FrameArena = AllocateArena(ArenaParams);
        }

        // Node Id Table
        {
            ui_node_id_table_params Params = { 0 };
            Params.GroupSize  = NodeIdTable_128Bits;
            Params.GroupCount = 32;

            u64   Footprint = GetNodeIdTableFootprint(Params);
            void *Memory    = PushArray(Result->StaticArena, u8, Footprint);

            Result->IdTable = PlaceNodeIdTableInMemory(Params, Memory);
        }

        // Registry
        {
            Result->Registry = CreateStyleRegistry(Params.ThemeFile, Result->StaticArena);
        }

        AppendToLinkedList((&State->Pipelines), Result, State->Pipelines.Count);
    }

    return Result;
}

internal void
UIExecuteAllSubtrees(ui_pipeline *Pipeline)
{
    ui_subtree_list   *List     = &Pipeline->Subtrees;
    memory_arena      *Arena    = Pipeline->FrameArena;
    Assert(Arena);

    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree     *Subtree    = &Node->Value;
        ui_layout_tree *LayoutTree = Subtree->LayoutTree;
        Assert(LayoutTree);

        ComputeLayout(LayoutTree, Arena);
        HitTestLayout(LayoutTree, Arena);
        DrawLayout   (Subtree, Arena);
    }
}

internal ui_subtree *
FindSubtree(ui_node Node, ui_pipeline *Pipeline)
{
    ui_subtree *Result = 0;

    IterateLinkedList((&Pipeline->Subtrees), ui_subtree_node *, SubtreeNode)
    {
        ui_subtree *Subtree = &SubtreeNode->Value;
        if(Subtree->Id == Node.SubtreeId)
        {
            Result = Subtree;
            break;
        }
    }

    return Result;
}
