// [Helpers]

internal void
AlignGlyph(vec2_f32 Position, ui_glyph *Glyph)
{
    Glyph->Position.Min.X = roundf(Position.X + Glyph->Offset.X);
    Glyph->Position.Min.Y = roundf(Position.Y + Glyph->Offset.Y);
    Glyph->Position.Max.X = roundf(Glyph->Position.Min.X + Glyph->Size.X);
    Glyph->Position.Max.Y = roundf(Glyph->Position.Min.Y + Glyph->Size.Y);
}

internal f32
ConvertUnitToFloat(ui_unit Unit, f32 AvSpace)
{
    f32 Result = 0;

    if(Unit.Type == UIUnit_Float32)
    {
        Result = Unit.Float32 <= AvSpace ? Unit.Float32 : AvSpace;
    } else
    if(Unit.Type == UIUnit_Percent)
    {
        Result = (Unit.Percent / 100.f) * AvSpace;
    }

    return Result;
}

// -----------------------------------------------
// Tree/Node internal implementation (Helpers/Types)

typedef struct ui_parent_stack
{
    u64 NextWrite;
    u64 Size;

    ui_layout_node **Nodes;
} ui_parent_stack;

typedef struct ui_layout_box
{
    // Output
    f32 FinalX;
    f32 FinalY;
    f32 FinalWidth;
    f32 FinalHeight;

    // Layout Info
    ui_unit        Width;
    ui_unit        Height;
    ui_spacing     Spacing;
    ui_padding     Padding;
    UIDisplay_Type Display;

    // Resources ( TODO: Make this a resource?)
    ui_scroll_context *ScrollContext;
} ui_layout_box;

struct ui_layout_node
{
    // Hierarchy
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    // Extra Info
    u32 SubIndex;
    u32 Index;
    u32 ChildCount;

    // Value
    ui_layout_box Value;
    bit_field     Flags;
};

typedef struct ui_layout_tree
{
    // Nodes
    u64             NodeCapacity;
    u64             NodeCount;
    ui_layout_node *Nodes;

    // Depth
    ui_parent_stack ParentStack;
} ui_layout_tree;

internal vec2_f32
GetInnerBoxSize(ui_layout_box *Box)
{
    vec2_f32 Result = Vec2F32(Box->FinalWidth - (Box->Padding.Left + Box->Padding.Right), Box->FinalHeight - (Box->Padding.Top + Box->Padding.Bot));
    return Result;
}

internal rect_f32
MakeRectFromNode(ui_layout_node *Node, vec2_f32 Translation)
{
    rect_f32 NodeRect = RectF32(Node->Value.FinalX, Node->Value.FinalY, Node->Value.FinalWidth, Node->Value.FinalHeight);
    rect_f32 Result   = TranslatedRectF32(NodeRect, Translation);

    return Result;
}

internal b32 
IsValidLayoutNode(ui_layout_node *Node)
{
    b32 Result = Node && Node->Index != InvalidUINodeIndex;
    return Result;
}

internal ui_layout_node *
GetLayoutNode(u32 Index, ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if(Index < Tree->NodeCount)
    {
        Result = Tree->Nodes + Index;
    }

    return Result;
}

internal ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree)
{
    Assert(Tree && "Calling GetFreeLayoutNode with a NULL tree");

    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

internal ui_layout_node *
NodeToLayoutNode(ui_node Node, ui_layout_tree *Tree)
{
    ui_layout_node *Result = GetLayoutNode(Node.IndexInTree, Tree);
    return Result;
}

internal ui_parent_stack
BeginParentStack(u64 Size, memory_arena *Arena)
{
    ui_parent_stack Result = {0};
    Result.Nodes = PushArray(Arena, ui_layout_node *, Size);
    Result.Size  = Size;

    return Result;
}

internal b32
IsValidParentStack(ui_parent_stack *Stack)
{
    b32 Result = (Stack) && (Stack->Size);
    return Result;
}

internal void
PushParentStack(ui_layout_node *Node, ui_parent_stack *Stack)
{
    Assert(IsValidParentStack(Stack));

    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Nodes[Stack->NextWrite++] = Node;
    }
}

internal ui_layout_node *
PeekParentStack(ui_parent_stack *Stack)
{
    Assert(IsValidParentStack(Stack));

    ui_layout_node *Result = 0;

    if(Stack->NextWrite > 0)
    {
        Result = Stack->Nodes[Stack->NextWrite - 1];
    }

    return Result;
}

internal void
PopParentStack(ui_parent_stack *Stack)
{
    Assert(IsValidParentStack(Stack));

    if(Stack->NextWrite > 0)
    {
        --Stack->NextWrite;
    }
}

internal ui_layout_node *
CreateAndInsertLayoutNode(ui_subtree *Subtree, bit_field Flags)
{
    Assert(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_layout_node *Result = GetFreeLayoutNode(Tree);
    if(Result)
    {
        Result->Flags    = Flags;
        Result->SubIndex = Subtree->Id;

        if (!IsValidParentStack(&Tree->ParentStack))
        {
            Tree->ParentStack = BeginParentStack(Tree->NodeCapacity, Subtree->FrameData);
        }

        Result->Last   = 0;
        Result->Next   = 0;
        Result->First  = 0;
        Result->Parent = PeekParentStack(&Tree->ParentStack);

        if (Result->Parent)
        {
            AppendToDoublyLinkedList(Result->Parent, Result, Result->Parent->ChildCount);
        }

        if (HasFlag(Result->Flags, UILayoutNode_IsParent))
        {
            PushParentStack(Result, &Tree->ParentStack);
        }
    }

    return Result;
}

// -------------------------------------------------------------
// Tree/Node Public API Implementation

internal u64
GetLayoutTreeFootprint(u64 NodeCount)
{
    u64 ArraySize = NodeCount * sizeof(ui_layout_node);
    u64 Result    = sizeof(ui_layout_tree) + ArraySize;

    return Result;
}

internal ui_layout_tree *
PlaceLayoutTreeInMemory(u64 NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        Result = (ui_layout_tree *)Memory;
        Result->NodeCapacity = NodeCount;
        Result->Nodes        = (ui_layout_node *)(Result + 1);

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidUINodeIndex;
        }
    }

    return Result;
}

internal ui_node
FindLayoutChild(ui_node Node, u32 Index, ui_subtree *Subtree)
{
    Assert(Node.CanUse);
    Assert(IsValidSubtree(Subtree));

    ui_node Result = {0};

    ui_layout_node *LayoutNode = NodeToLayoutNode(Node, Subtree->LayoutTree);
    if(IsValidLayoutNode(LayoutNode))
    {
        ui_layout_node *Child = LayoutNode->First;
        while(Index--)
        {
            Child = Child->Next;
        }

        if(Child)
        {
            Result.CanUse      = 1;
            Result.SubtreeId   = Subtree->Id;
            Result.IndexInTree = Child->Index;
        }
    }

    return Result;
}

internal void
ReserveLayoutChildren(ui_node Node, u32 Amount, ui_subtree *Subtree)
{
    Assert(Node.CanUse);
    Assert(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    b32 NeedPush = (PeekParentStack(&Tree->ParentStack)->Index != Node.IndexInTree);

    if(NeedPush)
    {
        ui_layout_node *LayoutNode = GetLayoutNode(Node.IndexInTree, Tree);
        Assert(LayoutNode);

        PushParentStack(LayoutNode, &Tree->ParentStack);
    }

    for(u32 Idx = 0; Idx < Amount; ++Idx)
    {
        AllocateUINode(0, 0, Subtree);
    }

    if(NeedPush)
    {
        PopParentStack(&Tree->ParentStack);
    }
}

internal ui_node
AllocateUINode(style_property Properties[StyleProperty_Count], bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node Result = {.CanUse = 0};

    ui_layout_node *Node = CreateAndInsertLayoutNode(Subtree, Flags);
    if (Node)
    {
        Result = (ui_node){ .IndexInTree = Node->Index, .SubtreeId = Subtree->Id, .CanUse = 1 };
    }

    return Result;
}

internal void
BindTextResource(ui_node Node, ui_subtree *Subtree, ui_resource_key Key)
{
    Assert(Subtree);
}

internal void
UIEnd()
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    Assert(Subtree);

    ui_layout_tree *LayoutTree = Subtree->LayoutTree;
    Assert(LayoutTree);

    PopParentStack(&LayoutTree->ParentStack);
}

// -----------------------------------------------------------
// Layout Pass Internal Helpers/Types

typedef struct draw_stack_frame
{
    ui_layout_node *Node;
    vec2_f32        AccScroll;
    rect_f32        Clip;
} draw_stack_frame;

typedef struct draw_stack
{
    u64 NextWrite;
    u64 Size;

    draw_stack_frame *Frames;
} draw_stack;

internal draw_stack
BeginDrawStack(u64 Size, memory_arena *Arena)
{
    draw_stack Result = {0};
    Result.Frames = PushArray(Arena, draw_stack_frame, Size);
    Result.Size   = Size;

    return Result;
}

internal b32
IsValidDrawStack(draw_stack *Stack)
{
    b32 Result = (Stack) && (Stack->Frames) && (Stack->Size);
    return Result;
}

internal b32
IsDrawStackEmpty(draw_stack *Stack)
{
    b32 Result = Stack->NextWrite == 0;
    return Result;
}

internal void
PushDrawStack(draw_stack_frame Value, draw_stack *Stack)
{
    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Frames[Stack->NextWrite++] = Value;
    }
}

internal draw_stack_frame
PopDrawStack(draw_stack *Stack)
{
    Assert(Stack->NextWrite != 0);

    draw_stack_frame Result = Stack->Frames[--Stack->NextWrite];
    return Result;
}

internal void
EnsureNodeStyle(ui_layout_node *Node, ui_subtree *Subtree)
{
    ui_node_style *Style = GetNodeStyle(Node->Index, Subtree);
    {
        Assert(Style);

        ui_pipeline *Pipeline = GetCurrentPipeline();
        Assert(Pipeline);

        style_property *Properties = GetBaseStyle(Style->CachedStyleIndex, Pipeline->Registry);
        Assert(Properties);

        vec2_unit  Size    = UIGetSize(Properties);
        ui_spacing Spacing = UIGetSpacing(Properties);
        ui_padding Padding = UIGetPadding(Properties);

        Node->Value.Width   = Size.X;
        Node->Value.Height  = Size.Y;
        Node->Value.Padding = Padding;
        Node->Value.Spacing = Spacing;

        MemoryCopy(Style->Properties, Properties, sizeof(style_property) * StyleProperty_Count);

        Style->LayoutInfoIsBound = 1;
    }
}

// NOTE: So this must generate events. Click, Hover, Drag, Resize, ...

internal b32 
Unknown(vec2_f32 MousePosition, ui_layout_node *Root, ui_pipeline *Pipeline)
{
    ui_layout_box *Box = &Root->Value;

    f32 MouseIsOutsideHitbox = 1.f;
    {
        rect_sdf_params Params = {0};
        Params.HalfSize      = Vec2F32(Box->FinalWidth * 0.5f, Box->FinalHeight * 0.5f);
        Params.PointPosition = Vec2F32Sub(MousePosition, Vec2F32Add(Vec2F32(Box->FinalX, Box->FinalY), Params.HalfSize));

        MouseIsOutsideHitbox = RoundedRectSDF(Params);
    }

    if(!MouseIsOutsideHitbox)
    {
        IterateLinkedListBackward(Root, ui_layout_node *, Child)
        {
            b32 Result = Unknown(MousePosition, Child, Pipeline);
            if(Result)
            {
                return Result;
            }
        }

        f32      ScrollDelta  = OSGetScrollDelta();
        vec2_f32 MouseDelta   = OSGetMouseDelta();
        b32      MouseMoved   = (MouseDelta.X != 0 || MouseDelta.Y != 0);
        b32      MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

        // The order of importance goes like: Resize->Drag->Click->Hover->Scroll
        // Uhm, how do we treat the node capture then?

        if(HasFlag(Root->Flags, UILayoutNode_IsResizable) && MouseMoved && MouseClicked)
        {
            // NOTE: Can I not remove the SDF check? I am somewhat confused...
            // Also, what are the conditions so that I can enter this branch?
            // I do not think I care about the cursor. I'll let the user control it.
            // So I do not care about intent, but rather, capture is what I am looking for.

            f32 MouseIsOutsideBorder = 1.f;
            {
                // TODO: I'm pretty sure some of these parameters are wrong...
                // Should the local position be computed from the full half size?
                // What should the radius be given that the 4 corners could be different values?

                vec2_f32 BorderVector = Vec2F32(0.f, 0.f);

                rect_sdf_params Params = {0};
                Params.Radius        = 0.f;
                Params.HalfSize      = Vec2F32Sub(Vec2F32(Box->FinalWidth * 0.5f, Box->FinalHeight * 0.5f), BorderVector);
                Params.PointPosition = Vec2F32Sub(MousePosition, Vec2F32Add(Vec2F32(Box->FinalX, Box->FinalY), Params.HalfSize));

                MouseIsOutsideBorder = RoundedRectSDF(Params);
            }

            if(!MouseIsOutsideBorder)
            {
                f32 Left   = Box->FinalX;
                f32 Top    = Box->FinalY;
                f32 Width  = Box->FinalWidth;
                f32 Height = Box->FinalHeight;

                f32 CornerTolerance = 5.f;
                f32 CornerX         = Left + Width  - CornerTolerance;
                f32 CornerY         = Top  + Height - CornerTolerance;

                // NOTE: Temporary
                f32 BorderWidth = 0.f;

                rect_f32 RightEdge  = RectF32(Left + Width - BorderWidth, Top, BorderWidth, Height);
                rect_f32 BottomEdge = RectF32(Left, Top + Height - BorderWidth, Width, BorderWidth);
                rect_f32 Corner     = RectF32(CornerX, CornerY, CornerTolerance, CornerTolerance);

                if(IsPointInRect(Corner, MousePosition))
                {
                } else
                if(IsPointInRect(RightEdge, MousePosition))
                {
                } else
                if (IsPointInRect(BottomEdge, MousePosition))
                {
                }
            }
        }

        if(HasFlag(Root->Flags, UILayoutNode_IsDraggable) && MouseMoved)
        {
        }

        if(HasFlag(Root->Flags, UILayoutNode_IsClickable) && MouseClicked)
        {
        }

        if(HasFlag(Root->Flags, UILayoutNode_IsHoverable))
        {
            // RecordUIHoverEvent(LayoutNodeToNode(Root), Pipeline, &Pipeline->Events, Pipeline->FrameArena);
        }

        if(HasFlag(Root->Flags, UILayoutNode_IsScrollable) && ScrollDelta != 0.f)
        {
        }
    }

    return 0;
}

// NOTE: These two must be moved. Move to the event system.

internal void
DragUISubtree(vec2_f32 Delta, ui_layout_node *Node, ui_pipeline *Pipeline)
{
    Assert(Delta.X != 0.f || Delta.Y != 0.f);

    // NOTE: Should we allow this mutation? It does work...

    Node->Value.FinalX += Delta.X;
    Node->Value.FinalY += Delta.Y;

    for (ui_layout_node *Child = Node->First; Child != 0; Child = Child->Next)
    {
        DragUISubtree(Delta, Child, Pipeline);
    }
}

internal void
ResizeUISubtree(vec2_f32 Delta, ui_layout_node *Node, ui_pipeline *Pipeline)
{
    Assert(Node->Value.Width.Type  != UIUnit_Percent);
    Assert(Node->Value.Height.Type != UIUnit_Percent);

    // NOTE: Should we allow this mutation? It does work...

    Node->Value.Width.Float32  += Delta.X;
    Node->Value.Height.Float32 += Delta.Y;
}

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

internal void
PreOrderPlace(ui_layout_node *Root, memory_arena *Arena, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Arena);
    Assert(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Arena);
    if (Queue.Data)
    {
        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node = PopNodeQueue(&Queue);
            ui_layout_box  *Box  = &Node->Value;

            f32 CursorX = Box->FinalX + Box->Padding.Left;
            f32 CursorY = Box->FinalY + Box->Padding.Top;

            f32 MaxCursorX = CursorX + Box->FinalWidth;
            f32 MaxCursorY = CursorY + Box->FinalHeight;
            if(HasFlag(Node->Flags, UILayoutNode_IsScrollable))
            {
                ui_scroll_context *Scroll = Box->ScrollContext;
                if(Scroll->Axis == ScrollAxis_X)
                {
                    MaxCursorX = FLT_MAX;
                }
                else
                {
                    MaxCursorY = FLT_MAX;
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_PlaceChildrenX))
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    f32 Width = Child->Value.FinalWidth + Box->Spacing.Horizontal;
                    if(CursorX + Width > MaxCursorX)
                    {
                        break;
                    }

                    Child->Value.FinalX = CursorX;
                    Child->Value.FinalY = CursorY;

                    CursorX += Width;

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_PlaceChildrenY))
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    Child->Value.FinalX = CursorX;
                    Child->Value.FinalY = CursorY;

                    f32 Height = Child->Value.FinalHeight + Box->Spacing.Vertical;
                    if(CursorY + Height > MaxCursorY)
                    {
                        break;
                    }

                    CursorY += Height;

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_DrawText))
            {
                ui_resource_key   Key   = GetNodeResource(Node->Index, Subtree);
                ui_resource_state State = FindUIResourceByKey(Key, 0);
                ui_glyph_run     *Run   = (ui_glyph_run *)State.Resource;

                Assert(Run                           && "Node is marked as having text but no glyph run is provided");
                Assert(State.Type == UIResource_Text && "Queried resource is not the type we expect");

                u32 FilledLine = 0.f;
                f32 LineWidth  = 0.f;
                f32 LineStartX = CursorX;
                f32 LineStartY = CursorY;

                for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
                {
                    ui_glyph *Glyph = &Run->Glyphs[Idx];

                    f32      GlyphWidth    = Glyph->AdvanceX;
                    vec2_f32 GlyphPosition = Vec2F32(LineStartX + LineWidth, LineStartY + (FilledLine * Run->LineHeight));

                    AlignGlyph(GlyphPosition, Glyph);

                    if(LineWidth + GlyphWidth > Box->FinalWidth)
                    {
                        if(LineWidth > 0.f)
                        {
                            ++FilledLine;
                        }
                        LineWidth = GlyphWidth;

                        GlyphPosition = Vec2F32(LineStartX, LineStartY + (FilledLine * Run->LineHeight));
                        AlignGlyph(GlyphPosition, Glyph);
                    }
                    else
                    {
                        LineWidth += GlyphWidth;
                    }
                }
            }
        }
    }
}

internal void
PreOrderMeasure(ui_layout_node *Root, memory_arena *Arena, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Arena);
    Assert(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Arena);

    if(Queue.Data)
    {
        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node = PopNodeQueue(&Queue);
            Assert(Node);

            ui_node_style *Style = GetNodeStyle(Node->Index, Subtree);
            Assert(Style);

            if(!Node->Parent)
            {
                EnsureNodeStyle(Node, Subtree);

                Assert(Node->Value.Width.Type  == UIUnit_Float32);
                Assert(Node->Value.Height.Type == UIUnit_Float32);
                Node->Value.FinalWidth  = Node->Value.Width.Float32;
                Node->Value.FinalHeight = Node->Value.Height.Float32;
            }

            ui_layout_box *Box = &Node->Value;

            f32 AvWidth  = Box->FinalWidth  - (Box->Padding.Left + Box->Padding.Right);
            f32 AvHeight = Box->FinalHeight - (Box->Padding.Top  + Box->Padding.Bot);

            if (HasFlag(Node->Flags, UILayoutNode_IsScrollable))
            {
                Assert(Box->ScrollContext);
                ui_scroll_context *Scroll = Box->ScrollContext;
                if (Scroll->Axis == ScrollAxis_X)
                {
                    AvWidth = FLT_MAX;
                }
                else if (Scroll->Axis == ScrollAxis_Y)
                {
                    AvHeight = FLT_MAX;
                }

                Scroll->ContentWindowSize = GetInnerBoxSize(Box);
            }

            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                EnsureNodeStyle(Child, Subtree);

                Box = &Child->Value;
                Box->FinalWidth  = ConvertUnitToFloat(Box->Width , AvWidth );
                Box->FinalHeight = ConvertUnitToFloat(Box->Height, AvHeight);

                PushNodeQueue(&Queue, Child);
            }
        }
    }
}

internal void
PostOrderMeasure(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasure(Child, Subtree);
    }

    ui_layout_box *Box = &Root->Value;

    if(HasFlag(Root->Flags, UILayoutNode_DrawText))
    {
        ui_resource_key   Key   = GetNodeResource(Root->Index, Subtree);
        ui_resource_state State = FindUIResourceByKey(Key, UIState.ResourceTable);
        ui_glyph_run     *Run   = (ui_glyph_run *)State.Resource;

        Assert(Run);
        Assert(State.Type == UIResource_Text);

        // TODO: Handle other units
        f32 InnerAvWidth = 0.f;
        if (Box->Width.Type == UIUnit_Auto)
        {
            ui_layout_node *Parent = Root->Parent;
            if(Parent)
            {
                // TODO: Unsure how we want to handle height?
                InnerAvWidth = GetInnerBoxSize(&Parent->Value).X;
            }
        }

        f32 LineWidth = 0.f;
        u32 LineWrapCount = 0;
        if(Run->GlyphCount)
        {
            for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
            {
                ui_glyph *Glyph      = &Run->Glyphs[Idx];
                f32       GlyphWidth = Glyph->AdvanceX;

                if(LineWidth + GlyphWidth > InnerAvWidth)
                {
                    if(LineWidth > 0.f)
                    {
                        ++LineWrapCount;
                    }

                    LineWidth = GlyphWidth;
                }
                else
                {
                    LineWidth += GlyphWidth;
                }
            }

        }

        if(Box->Width.Type == UIUnit_Auto)
        {
            if(LineWrapCount == 0)
            {
                Box->FinalWidth = LineWidth;
            }
            else
            {
                Box->FinalWidth = InnerAvWidth;
            }

            Box->FinalHeight = (LineWrapCount + 1) * Run->LineHeight;
        }
    }

    // WARN: Still a work in progress.

    if(HasFlag(Root->Flags, UILayoutNode_IsScrollable))
    {
        ui_scroll_context *Scroll = Root->Value.ScrollContext;
        Assert(Scroll);

        f32 TotalWidth  = 0.f;
        f32 TotalHeight = 0.f;
        f32 MaxWidth    = 0.f;
        f32 MaxHeight   = 0.f;

        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            TotalWidth  += Child->Value.FinalWidth;
            TotalHeight += Child->Value.FinalHeight;

            if (Child->Value.FinalWidth > MaxWidth)
            {
                MaxWidth = Child->Value.FinalWidth;
            }

            if (Child->Value.FinalHeight > MaxHeight)
            {
                MaxHeight = Child->Value.FinalHeight;
            }
        }

        if (Scroll->Axis == ScrollAxis_X)
        {
            Scroll->ContentSize = Vec2F32(TotalWidth, MaxHeight);
        } else
        if (Scroll->Axis == ScrollAxis_Y)
        {
            Scroll->ContentSize = Vec2F32(MaxWidth, TotalHeight);
        }
    }
}

internal void
DrawSubtree(ui_layout_node *Root, u64 NodeLimit, ui_subtree *Subtree, memory_arena *Arena)
{
    render_pass *Pass  = GetRenderPass(Arena, RenderPass_UI);
    draw_stack   Stack = BeginDrawStack(NodeLimit, Arena);

    if(Pass && IsValidDrawStack(&Stack))
    {
        PushDrawStack((draw_stack_frame){.Node = Root, .AccScroll = Vec2F32(0.f ,0.f), .Clip = RectF32(0, 0, 0, 0)}, &Stack);

        while(!IsDrawStackEmpty(&Stack))
        {
            draw_stack_frame Frame          = PopDrawStack(&Stack);
            rect_f32         ChildClip      = Frame.Clip;
            vec2_f32         ChildAccScroll = Frame.AccScroll;

            ui_node_style *Style = GetNodeStyle(Frame.Node->Index, Subtree);

            render_batch_list *BatchList = 0;
            {
                rect_group_params RootParams = {0};
                RootParams.Transform = Mat3x3Identity();
                RootParams.Clip      = Frame.Clip;

                render_pass_params_ui *UIParams = &Pass->Params.UI.Params;
                rect_group_node       *Node     = UIParams->Last;

                b32 CanMerge = 0;
                if(Node)
                {
                    if(HasFlag(Frame.Node->Flags, UILayoutNode_DrawText))
                    {
                        ui_resource_key   Key   = GetNodeResource(Frame.Node->Index, Subtree);
                        ui_resource_state State = FindUIResourceByKey(Key, UIState.ResourceTable);
                        ui_glyph_run     *Run   = (ui_glyph_run *)State.Resource;

                        Assert(Run                           && "Node is marked as having text but no glyph run is provided");
                        Assert(State.Type == UIResource_Text && "Queried resource is not the type we expect");

                        RootParams.Texture     = Run->Atlas;
                        RootParams.TextureSize = Run->AtlasSize;
                    }

                    CanMerge = CanMergeRectGroupParams(&Node->Params, &RootParams);
                }

                if(!CanMerge)
                {
                    Node = PushStruct(Arena, rect_group_node);
                    Node->BatchList.BytesPerInstance = sizeof(ui_rect);

                    AppendToLinkedList(UIParams, Node, UIParams->Count);
                }

                // BUG: Wrong in some cases.
                Node->Params = RootParams;
                BatchList    = &Node->BatchList;
            }

            // Draw Flags
            {
                if (HasFlag(Frame.Node->Flags, UILayoutNode_DrawBackground))
                {
                    ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                    Rect->RectBounds  = MakeRectFromNode(Frame.Node, Frame.AccScroll);
                    Rect->Color       = UIGetColor(Style->Properties);
                    Rect->CornerRadii = UIGetCornerRadius(Style->Properties);
                    Rect->Softness    = UIGetSoftness(Style->Properties);
                }

                if (HasFlag(Frame.Node->Flags, UILayoutNode_DrawBorders))
                {
                    ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                    Rect->RectBounds  = MakeRectFromNode(Frame.Node, Frame.AccScroll);
                    Rect->Color       = UIGetBorderColor(Style->Properties);
                    Rect->CornerRadii = UIGetCornerRadius(Style->Properties);
                    Rect->BorderWidth = UIGetBorderWidth(Style->Properties);
                    Rect->Softness    = UIGetSoftness(Style->Properties);
                }

                if(HasFlag(Frame.Node->Flags, UILayoutNode_DrawText))
                {
                    // NOTE: Insane amount of code repetition.
                    ui_resource_key   Key   = GetNodeResource(Frame.Node->Index, Subtree);
                    ui_resource_state State = FindUIResourceByKey(Key, UIState.ResourceTable);
                    ui_glyph_run     *Run   = (ui_glyph_run *)State.Resource;

                    Assert(Run                           && "Node is marked as having text but no glyph run is provided");
                    Assert(State.Type == UIResource_Text && "Queried resource is not the type we expect");

                    ui_color TextColor = UIGetTextColor(Style->Properties);
                    for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
                    {
                        ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                        Rect->SampleTexture = 1.f;
                        Rect->TextureSource = Run->Glyphs[Idx].Source;
                        Rect->RectBounds    = TranslatedRectF32(Run->Glyphs[Idx].Position, Frame.AccScroll);
                        Rect->Color         = TextColor;
                    }
                }
            }

            // Stack Frame
            {
                if (HasFlag(Frame.Node->Flags, UILayoutNode_IsScrollable))
                {
                    PruneScrollContextNodes(Frame.Node);
                    vec2_f32 NodeScroll = GetScrollTranslation(Frame.Node);

                    ChildAccScroll.X += NodeScroll.X;
                    ChildAccScroll.Y += NodeScroll.Y;
                }

                if(HasFlag(Frame.Node->Flags, UILayoutNode_HasClip))
                {
                    ChildClip = InsetRectF32(MakeRectFromNode(Frame.Node, Vec2F32(0.f, 0.f)), UIGetBorderWidth(Style->Properties));

                    rect_f32 EmptyClip     = RectF32Zero();
                    b32      ParentHasClip = (MemoryCompare(&Frame.Clip, &EmptyClip, sizeof(rect_f32)) != 0);

                    if(ParentHasClip)
                    {
                        ChildClip = IntersectRectF32(Frame.Clip, ChildClip);
                    }
                }
            }

            IterateLinkedListBackward(Frame.Node, ui_layout_node *, Child)
            {
                if (!(HasFlag(Child->Flags, UILayoutNode_DoNotDraw)))
                {
                    PushDrawStack((draw_stack_frame){.Node = Child, .AccScroll = ChildAccScroll, .Clip = ChildClip}, &Stack);
                }
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------
// Layout Pass Public API implementation

internal void
HitTestLayout(ui_subtree *Subtree, memory_arena *Arena)
{
    Assert(Subtree);
    Assert(Arena);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);
    Assert(Root);
}

internal void
ComputeLayout(ui_subtree *Subtree, memory_arena *Arena)
{
    Assert(Subtree);
    Assert(Arena);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);

    memory_region Local = EnterMemoryRegion(Arena);
    {
        PreOrderMeasure     (Root, Local.Arena, Subtree);
        PostOrderMeasure    (Root             , Subtree);
        PreOrderPlace       (Root, Local.Arena, Subtree);
    }
    LeaveMemoryRegion(Local);
}

internal void 
DrawLayout(ui_subtree *Subtree, memory_arena *Arena)
{
    Assert(Subtree);
    Assert(Arena);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);
    Assert(Root);

    DrawSubtree(Root, Tree->NodeCount, Subtree, Arena);
}

// [Binds]

internal void
BindScrollContext(ui_node Node, ScrollAxis_Type Axis, ui_layout_tree *Tree, memory_arena *Arena)
{
    Assert(Node.CanUse);
    Assert(Arena);

    ui_scroll_context *Context = PushArena(Arena, sizeof(ui_scroll_context), AlignOf(ui_scroll_context));
    if(Context)
    {
        ui_layout_node *LayoutNode = NodeToLayoutNode(Node, Tree);
        if (IsValidLayoutNode(LayoutNode))
        {
            LayoutNode->Value.ScrollContext = Context;
        }

        Context->Axis         = Axis;
        Context->PixelPerLine = 16.f;
    }
}

// [Node ID]

#define NodeIdTable_EmptyMask  1 << 0       // 1st bit
#define NodeIdTable_DeadMask   1 << 1       // 2nd bit
#define NodeIdTable_TagMask    0xFF & ~0x03 // High 6 bits

typedef struct ui_node_id_hash
{
    u64 Value;
} ui_node_id_hash;

typedef struct ui_node_id_entry
{
    ui_node_id_hash Hash;
    ui_node         Node;
} ui_node_id_entry;

typedef struct ui_node_id_table
{
    u8               *MetaData;
    ui_node_id_entry *Buckets;

    u64 GroupSize;
    u64 GroupCount;

    u64 HashMask;
} ui_node_id_table;

internal b32
IsValidNodeIdTable(ui_node_id_table *Table)
{
    b32 Result = (Table && Table->MetaData && Table->Buckets && Table->GroupCount);
    return Result;
}

internal u32
GetNodeIdGroupIndexFromHash(ui_node_id_hash Hash, ui_node_id_table *Table)
{
    u32 Result = Hash.Value & Table->HashMask;
    return Result;
}

internal u8
GetNodeIdTagFromHash(ui_node_id_hash Hash)
{
    u8 Result = Hash.Value & NodeIdTable_TagMask;
    return Result;
}

internal ui_node_id_hash
ComputeNodeIdHash(byte_string Id)
{
    // WARN: Again, unsure if this hash is strong enough since hash-collisions
    // are fatal. Fatal in the sense that they will return invalid data.

    ui_node_id_hash Result = {HashByteString(Id)};
    return Result;
}

internal ui_node_id_entry *
FindNodeIdEntry(ui_node_id_hash Hash, ui_node_id_table *Table)
{
    u32 ProbeCount = 0;
    u32 GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        u8 *Meta = Table->MetaData + (GroupIndex * Table->GroupSize);
        u8  Tag  = GetNodeIdTagFromHash(Hash);

        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);
        __m128i TagVector  = _mm_set1_epi8(Tag);

        // Uses a 6 bit tags to search a matching tag thorugh the meta-data
        // using vectors of bytes instead of comparing full hashes directly.

        i32 TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, TagVector));
        while(TagMask)
        {
            u32 Lane  = FindFirstBit(TagMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            if(Hash.Value == Entry->Hash.Value)
            {
                return Entry;
            }

            TagMask &= TagMask - 1;
        }

        // Check for slots that have never been used. If we find any it means that
        // our value has never been inserted in the map, since, otherwise, it would
        // have been inserted in that free slot. Keep proving if no never-used slot
        // have been found.

        __m128i EmptyVector = _mm_set1_epi8(NodeIdTable_EmptyMask);
        i32     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

        if(!EmptyMask)
        {
            ProbeCount++;
            GroupIndex = (GroupIndex + (ProbeCount * ProbeCount)) & (Table->GroupCount - 1);
        }
        else
        {
            break;
        }
    }

    return 0;
}

internal void
InsertNodeId(ui_node_id_hash Hash, ui_node Node, ui_node_id_table *Table)
{
    u32 ProbeCount = 0;
    u32 GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        u8     *Meta       = Table->MetaData + (GroupIndex * Table->GroupSize);
        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);

        // First check for empty entries (never-used), if one is found insert it and
        // set the meta-data to the tag (which will clear all state bits)

        __m128i EmptyVector = _mm_set1_epi8(NodeIdTable_EmptyMask);
        i32     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

        if(EmptyMask)
        {
            u32 Lane  = FindFirstBit(EmptyMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash = Hash;
            Entry->Node = Node;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        // Then check for dead entries in the meta-vector, if one is found insert it and
        // set the meta-data to the tag which will clear all state bits

        __m128i DeadVector = _mm_set1_epi8(NodeIdTable_DeadMask);
        i32     DeadMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, DeadVector));

        if(DeadMask)
        {
            u32 Lane  = FindFirstBit(DeadMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash = Hash;
            Entry->Node = Node;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        ProbeCount++;
        GroupIndex = (GroupIndex + (ProbeCount * ProbeCount)) & (Table->GroupCount - 1);
    }
}

internal u64
GetNodeIdTableFootprint(ui_node_id_table_params Params)
{
    u64 GroupTotal = Params.GroupSize * Params.GroupCount;

    u64 MetaDataSize = GroupTotal * sizeof(u8);
    u64 BucketsSize  = GroupTotal * sizeof(ui_node_id_entry);
    u64 Result       = sizeof(ui_node_id_table) + MetaDataSize + BucketsSize;

    return Result;
}

internal ui_node_id_table *
PlaceNodeIdTableInMemory(ui_node_id_table_params Params, void *Memory)
{
    Assert(Params.GroupSize == 16);
    Assert(Params.GroupCount > 0 && IsPowerOfTwo(Params.GroupCount));

    ui_node_id_table *Result = 0;

    if(Memory)
    {
        u64 GroupTotal = Params.GroupSize * Params.GroupCount;

        u8 *              MetaData = (u8 *)Memory;
        ui_node_id_entry *Entries  = (ui_node_id_entry *)(MetaData + GroupTotal);

        Result = (ui_node_id_table *)(Entries + GroupTotal);
        Result->MetaData   = MetaData;
        Result->Buckets    = Entries;
        Result->GroupSize  = Params.GroupSize;
        Result->GroupCount = Params.GroupCount;
        Result->HashMask   = Params.GroupCount - 1;

        for(u32 Idx = 0; Idx < GroupTotal; ++Idx)
        {
            Result->MetaData[Idx] = NodeIdTable_EmptyMask;
        }
    }

    return Result;
}

internal void
SetNodeId(byte_string Id, ui_node Node, ui_node_id_table *Table)
{
    if(!IsValidNodeIdTable(Table))
    {
        return;
    }

    ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
    ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

    if(!Entry)
    {
        InsertNodeId(Hash, Node, Table);
    }
    else
    {
        // TODO: Show which ID is faulty.

        ConsoleWriteMessage(warn_message("ID could not be set because it already exists for this pipeline"));
    }
}

internal ui_node
FindNodeById(byte_string Id, ui_node_id_table *Table)
{
    ui_node Result = {0};

    if(IsValidByteString(Id) && IsValidNodeIdTable(Table))
    {
        ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
        ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

        if(Entry)
        {
            Result = Entry->Node;
        }
        else
        {
            // TODO: Show which ID is faulty.
            ConsoleWriteMessage(warn_message("Could not find queried node."));
        }
    }
    else
    {
        ConsoleWriteMessage(warn_message("Invalid Arguments Provided. See ui/layout.h for information."));
    }

    return Result;
}

// [Scrolling]

internal void
ApplyScrollToContext(f32 ScrollDeltaInLines, ui_scroll_context *Scroll)
{
    f32 ScrolledPixels = ScrollDeltaInLines * Scroll->PixelPerLine;

    f32 ScrollLimit = 0.f;
    if (Scroll->Axis == ScrollAxis_X)
    {
        ScrollLimit = -(Scroll->ContentSize.X - Scroll->ContentWindowSize.X);
    } else
    if (Scroll->Axis == ScrollAxis_Y)
    {
        ScrollLimit = -(Scroll->ContentSize.Y - Scroll->ContentWindowSize.Y);
    }

    Assert(ScrollLimit <= 0.f);

    Scroll->ScrollOffset += ScrolledPixels;

    if(Scroll->ScrollOffset < ScrollLimit)
    {
        Scroll->ScrollOffset = ScrollLimit;
    }
    else if(Scroll->ScrollOffset > 0.f)
    {
        Scroll->ScrollOffset = 0.f;
    }
}

internal void
PruneScrollContextNodes(ui_layout_node *Node)
{
    ui_scroll_context *Context = Node->Value.ScrollContext;
    Assert(Context);

    f32 MinX   = Node->Value.FinalX;
    f32 MinY   = Node->Value.FinalY; 
    f32 Width  = Context->ContentWindowSize.X;
    f32 Height = Context->ContentWindowSize.Y;
    if (Context->Axis == ScrollAxis_X)
    {
        MinX -= Context->ScrollOffset;
    }
    else if (Context->Axis == ScrollAxis_Y)
    {
        MinY -= Context->ScrollOffset;
    }

    rect_f32 ContentWindowBounds = RectF32(MinX, MinY, Width, Height);

    // WARN: 
    // This only checks first childrens. So if we have absolute nodes/animations, this will fail.
    // Only draws things that are fully inside? (This shouldn't be the case right? Since only one of the point's rect must be inside
    // This could probably be faster with some dirty state logic and greedy pruning, this fully iterates the list every frame
    // which is okay for now.

    // TODO: Simplify this code, after we have added the display code.

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        rect_f32 ChildRect;
        if (Child->Value.FinalWidth > 0.0f || Child->Value.FinalHeight > 0.0f) 
        {
            f32 ChildMinX   = Child->Value.FinalX;
            f32 ChildMinY   = Child->Value.FinalY;
            f32 ChildWidth  = Child->Value.FinalWidth;
            f32 ChildHeight = Child->Value.FinalHeight;

            ChildRect = RectF32(ChildMinX, ChildMinY, ChildWidth, ChildHeight);
        } else
        {
            f32 PointX = Child->Value.FinalX;
            f32 PointY = Child->Value.FinalY;

            ChildRect = RectF32(PointX, PointY, 1.f, 1.f);
        }

        if (RectsIntersect(ContentWindowBounds, ChildRect))
        {
            ClearFlag(Child->Flags, UILayoutNode_DoNotDraw);
        }
        else
        {
            SetFlag(Child->Flags, UILayoutNode_DoNotDraw);
        }
    }
}

internal vec2_f32
GetScrollTranslation(ui_layout_node *Node)
{
    vec2_f32 Result = Vec2F32(0.f, 0.f);

    ui_scroll_context *Context = Node->Value.ScrollContext;
    Assert(Context);

    if (Context->Axis == ScrollAxis_X)
    {
        Result = Vec2F32(Context->ScrollOffset, 0.f);
    } else
    if (Context->Axis == ScrollAxis_Y)
    {
        Result = Vec2F32(0.f, Context->ScrollOffset);
    }

    return Result;
}
