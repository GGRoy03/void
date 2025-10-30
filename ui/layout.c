// [Helpers]

internal void
AlignGlyph(vec2_f32 Position, ui_glyph *Glyph)
{
    Glyph->Position.Min.X = roundf(Position.X + Glyph->Offset.X);
    Glyph->Position.Min.Y = roundf(Position.Y + Glyph->Offset.Y);
    Glyph->Position.Max.X = roundf(Glyph->Position.Min.X + Glyph->Size.X);
    Glyph->Position.Max.Y = roundf(Glyph->Position.Min.Y + Glyph->Size.Y);
}

typedef struct resolved_unit
{
    f32 Value;
    b32 Resolved;
} resolved_unit;

internal resolved_unit
TryConvertUnitToFloat(ui_unit Unit, f32 AvSpace)
{
    resolved_unit Result = {0};

    if(Unit.Type == UIUnit_Float32)
    {
        Result.Value    = Unit.Float32 <= AvSpace ? Unit.Float32 : AvSpace;
        Result.Resolved = 1;
    } else
    if(Unit.Type == UIUnit_Percent)
    {
        Result.Value    = (Unit.Percent / 100.f) * AvSpace;
        Result.Resolved = 1;
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

typedef struct ui_flex_box
{
    UIFlexDirection_Type  Direction;
    UIJustifyContent_Type Justify;
    UIAlignItems_Type     Align;
} ui_flex_box;

typedef struct ui_flex_item
{
    UIAlignItems_Type SelfAlign;
} ui_flex_item;

typedef struct ui_layout_box
{
    // Output
    rect_f32 OuterBox;   // Box not accounting for border-width and padding (Is Drawn)
    rect_f32 InnerBox;   // Box not accounting for padding
    rect_f32 ContentBox; // Where children are placed.

    // Layout Info
    f32            BorderWidth; // NOTE: Idk about this.
    ui_unit        Width;
    ui_unit        Height;
    ui_spacing     Spacing;
    ui_padding     Padding;
    UIDisplay_Type Display;

    // How Its Placed
    union
    {
        ui_flex_item FlexItem;
    };

    // How To Place
    union
    {
        ui_flex_box FlexBox;
    };

    // Draw Info
    vec2_f32 VisualOffset;

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
GetContentBoxSize(ui_layout_box *Box)
{
    vec2_f32 Result;
    Result.X = Box->ContentBox.Max.X - Box->ContentBox.Min.X;
    Result.Y = Box->ContentBox.Max.Y - Box->ContentBox.Min.Y;

    return Result;
}

internal vec2_f32
GetContentBoxPosition(ui_layout_box *Box)
{
    vec2_f32 ContentStart = Vec2F32(Box->ContentBox.Min.X, Box->ContentBox.Min.Y);
    vec2_f32 Result       = Vec2F32Add(ContentStart, Box->VisualOffset);
    return Result;
}

internal vec2_f32
GetOuterBoxSize(ui_layout_box *Box)
{
    vec2_f32 Result;
    Result.X = Box->OuterBox.Max.X - Box->OuterBox.Min.X;
    Result.Y = Box->OuterBox.Max.Y - Box->OuterBox.Min.Y;

    return Result;
}

internal vec2_f32
GetOuterBoxPosition(ui_layout_box *Box)
{
    vec2_f32 OuterStart = Vec2F32(Box->OuterBox.Min.X, Box->OuterBox.Min.Y);
    vec2_f32 Result     = Vec2F32Add(OuterStart, Box->VisualOffset);
    return Result;
}

internal vec2_f32
GetInnerBoxSize(ui_layout_box *Box)
{
    vec2_f32 Result;
    Result.X = Box->InnerBox.Max.X - Box->InnerBox.Min.X;
    Result.Y = Box->InnerBox.Max.Y - Box->InnerBox.Min.Y;

    return Result;
}

internal vec2_f32
GetInnerBoxPosition(ui_layout_box *Box)
{
    vec2_f32 InnerStart = Vec2F32(Box->InnerBox.Min.X, Box->InnerBox.Min.Y);
    vec2_f32 Result     = Vec2F32Add(InnerStart, Box->VisualOffset);
    return Result;
}

internal void
ComputeNodeBoxes(ui_layout_node *Node, f32 Width, f32 Height)
{
    ui_layout_box *Box = &Node->Value;

    Box->OuterBox = RectF32(0.f, 0.f, Width, Height);
    Box->InnerBox = InsetRectF32(Box->OuterBox, Box->BorderWidth);

    Box->ContentBox.Min.X = Box->InnerBox.Min.X + Box->Padding.Left;
    Box->ContentBox.Min.Y = Box->InnerBox.Min.Y + Box->Padding.Top;
    Box->ContentBox.Max.X = Box->InnerBox.Max.X - Box->Padding.Right;
    Box->ContentBox.Max.Y = Box->InnerBox.Max.Y - Box->Padding.Bot;
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
CreateAndInsertLayoutNode(bit_field Flags, ui_subtree *Subtree)
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
        ui_layout_node *Node = CreateAndInsertLayoutNode(0, Subtree);
        Node->Value.Display = UIDisplay_None;
    }

    if(NeedPush)
    {
        PopParentStack(&Tree->ParentStack);
    }
}

internal void
UpdateNodeIfNeeded(u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);

    if(Style->CachedStyleIndex != InvalidCachedStyleIndex && !Style->IsLastVersion)
    {
        Assert(Style);

        ui_pipeline *Pipeline = GetCurrentPipeline();
        Assert(Pipeline);

        style_property *Cached[StyleState_Count] = {0};
        Cached[StyleState_Basic] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Basic, Pipeline->Registry);
        Cached[StyleState_Hover] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Hover, Pipeline->Registry);

        ForEachEnum(StyleState_Type, StyleState_Count, State)
        {
            Assert(Cached[State]);

            ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
            {
                if(!Style->Properties[State][Prop].IsSetRuntime)
                {
                    Style->Properties[State][Prop] = Cached[State][Prop];
                }
            }
        }

        ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
        Assert(LayoutNode);
        {
            ui_layout_box  *Box   = &LayoutNode->Value;
            style_property *Basic = Style->Properties[StyleState_Basic];
            Assert(Basic);

            // Defaults
            Box->BorderWidth = UIGetBorderWidth(Basic);
            Box->Width       = UIGetSize(Basic).X;
            Box->Height      = UIGetSize(Basic).Y;
            Box->Padding     = UIGetPadding(Basic);
            Box->Spacing     = UIGetSpacing(Basic);
            Box->Display     = UIGetDisplay(Basic);

            // Flex
            Box->FlexBox.Direction  = UIGetFlexDirection(Basic);
            Box->FlexBox.Justify    = UIGetJustifyContent(Basic);
            Box->FlexBox.Align      = UIGetAlignItems(Basic);
            Box->FlexItem.SelfAlign = UIGetSelfAlign(Basic);
        }

        Style->IsLastVersion = 1;
    }
}

internal void
SetLayoutNodeResource(u32 NodeIndex, ui_resource_key Key, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);
}

internal void
AddLayoutNodeFlag(u32 NodeIndex, bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);

    SetFlag(Node->Flags, Flags);
}

internal void
RemoveLayoutNodeFlag(u32 NodeIndex, bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);

    ClearFlag(Node->Flags, Flags);
}

internal ui_node
AllocateUINode(bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node Result = {.CanUse = 0};

    ui_layout_node *Node = CreateAndInsertLayoutNode(Flags, Subtree);
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
// UI Scrolling Internal Implementation

internal void
ScrollNode(f32 ScrollDelta, ui_layout_node *Node)
{
    ui_scroll_context *Context = Node->Value.ScrollContext;
    Assert(Context);

    f32      ScrolledPixels = ScrollDelta * Context->PixelPerLine;
    vec2_f32 WindowSize     = GetContentBoxSize(&Node->Value);

    f32 ScrollLimit = 0.f;
    if (Context->Axis == ScrollAxis_X)
    {
        ScrollLimit = -(Context->ContentSize.X - WindowSize.X);
    } else
    if (Context->Axis == ScrollAxis_Y)
    {
        ScrollLimit = -(Context->ContentSize.Y - WindowSize.Y);
    }

    if (ScrollLimit > 0.f)
    {
        ScrollLimit = 0.f;
    }

    Context->ScrollOffset += ScrolledPixels;
    Context->ScrollOffset  = ClampTop(ClampBot(ScrollLimit, Context->ScrollOffset), 0);
}

internal vec2_f32
GetScrollNodeTranslation(ui_layout_node *Node)
{
    ui_scroll_context *Context = Node->Value.ScrollContext;
    Assert(Context);

    vec2_f32 Result = Vec2F32(0.f, 0.f);

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

internal void
UpdateScrollNode(ui_layout_node *Node)
{
    ui_scroll_context *Context = Node->Value.ScrollContext;
    Assert(Context);

    vec2_f32 ScrollDelta = Vec2F32(0.f, 0.f);
    if(Context->Axis == ScrollAxis_X)
    {
        ScrollDelta.X = -1.f * Context->ScrollOffset;
    } else
    if(Context->Axis == ScrollAxis_Y)
    {
        ScrollDelta.Y = -1.f * Context->ScrollOffset;
    }

    rect_f32 WindowContent = Node->Value.ContentBox;
    rect_f32 WindowBounds  = TranslatedRectF32(WindowContent, ScrollDelta);

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        if(Child->Value.Display != UIDisplay_None)
        {
            vec2_f32 ContentSize = GetContentBoxSize(&Node->Value);

            if (ContentSize.X > 0.0f && ContentSize.Y > 0.0f) 
            {
                if (RectsIntersect(WindowBounds, Node->Value.ContentBox))
                {
                    ClearFlag(Child->Flags, UILayoutNode_DoNotDraw);
                }
                else
                {
                    SetFlag(Child->Flags, UILayoutNode_DoNotDraw);
                }
            }
        }
    }

}

// -----------------------------------------------------------
// UI Event Internal Implementation

internal void PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree);

typedef enum UIEvent_Type
{
    UIEvent_Hover  = 1 << 0,
    UIEvent_Click  = 1 << 1,
    UIEvent_Resize = 1 << 2,
    UIEvent_Drag   = 1 << 3,
    UIEvent_Scroll = 1 << 4,
} UIEvent_Type;

typedef struct ui_hover_event
{
    ui_layout_node *Node;
} ui_hover_event;

typedef struct ui_click_event
{
    ui_layout_tree *Tree;
    ui_node         Node;
} ui_click_event;

typedef struct ui_resize_event
{
    ui_subtree     *Subtree;
    ui_layout_node *Node;
    vec2_f32        Delta;
} ui_resize_event;

typedef struct ui_drag_event
{
    ui_subtree     *Subtree;
    ui_layout_node *Node;
    vec2_f32        Delta;
} ui_drag_event;

typedef struct ui_scroll_event
{
    ui_layout_node *Node;
    f32             Delta;
} ui_scroll_event;

typedef struct ui_event
{
    UIEvent_Type Type;
    union
    {
        ui_hover_event  Hover;
        ui_click_event  Click;
        ui_resize_event Resize;
        ui_drag_event   Drag;
        ui_scroll_event Scroll;
    };
} ui_event;

typedef struct ui_event_node ui_event_node;
struct ui_event_node
{
    ui_event_node *Next;
    ui_event       Value;
};

typedef struct ui_event_list
{
    ui_event_node *First;
    ui_event_node *Last;
    u32            Count;
} ui_event_list;

internal void 
RecordUIEvent(ui_event Event, ui_subtree *Subtree)
{
    if(!Subtree->Events)
    {
        Subtree->Events = PushStruct(Subtree->FrameData, ui_event_list);
    }

    ui_event_node *Node = PushStruct(Subtree->FrameData, ui_event_node);
    if(Node)
    {
        Node->Value = Event;
        AppendToLinkedList(Subtree->Events, Node, Subtree->Events->Count);
    }
}

internal void
RecordUIHoverEvent(ui_layout_node *Node, ui_subtree *Subtree)
{
    ui_event Event = {.Type = UIEvent_Hover, .Hover.Node = Node};
    RecordUIEvent(Event, Subtree);
}

internal void
RecordUIScrollEvent(ui_layout_node *Node, f32 Delta, ui_subtree *Subtree)
{
    ui_event Event = {.Type = UIEvent_Scroll, .Scroll.Node = Node, .Scroll.Delta = Delta};
    RecordUIEvent(Event, Subtree);
}

internal void
RecordUIResizeEvent(ui_layout_node *Node, vec2_f32 Delta, ui_subtree *Subtree)
{
    ui_event Event = {.Type = UIEvent_Resize, .Resize.Node = Node, .Resize.Delta = Delta, .Resize.Subtree = Subtree};
    RecordUIEvent(Event, Subtree);
}

internal void
RecordUIDragEvent(ui_layout_node *Node, vec2_f32 Delta, ui_subtree *Subtree)
{
    ui_event Event = {.Type = UIEvent_Drag, .Resize.Node = Node, .Resize.Delta = Delta, .Resize.Subtree = Subtree};
    RecordUIEvent(Event, Subtree);
}

internal void
ProcessEventList(ui_event_list *Events)
{
    Assert(Events);

    IterateLinkedList(Events, ui_event_node *, Node)
    {
        ui_event *Event = &Node->Value;

        switch(Event->Type)
        {

        case UIEvent_Hover:
        {
            ui_hover_event Hover = Event->Hover;

            SetFlag(Hover.Node->Flags, UILayoutNode_IsHovered);
        } break;

        case UIEvent_Click:
        {
        } break;

        case UIEvent_Resize:
        {
            // BUG:
            // This is not nice! It has a frame of lag.
            // We should recompute the subtree that is resized.
            // Perhaps even parent in some cases?
            // It also directly writes to the style which is heh.

            ui_resize_event Resize = Event->Resize;

            ui_node_style *Style = GetNodeStyle(Resize.Node->Index, Resize.Subtree);
            vec2_unit      Size  = UIGetSize(Style->Properties[StyleState_Basic]);

            Assert(Size.X.Type == UIUnit_Float32);
            Assert(Size.X.Type == UIUnit_Float32);

            Size.X.Float32 += Resize.Delta.X;
            Size.Y.Float32 += Resize.Delta.Y;

            Style->Properties[StyleState_Basic][StyleProperty_Size].Vec2         = Size;
            Style->Properties[StyleState_Basic][StyleProperty_Size].IsSetRuntime = 1;

            Style->IsLastVersion = 0;

        } break;

        case UIEvent_Drag:
        {
            ui_drag_event Drag = Event->Drag;
            Drag.Node->Value.VisualOffset = Vec2F32Add(Drag.Node->Value.VisualOffset, Drag.Delta);

            PreOrderPlaceSubtree(Drag.Node, Drag.Subtree);
        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            Assert(Scroll.Node);
            Assert(Scroll.Delta);

            // NOTE: Why is this not a single call?

            ScrollNode(Scroll.Delta, Scroll.Node);
            UpdateScrollNode(Scroll.Node);
        } break;

        }
    }
}

// -----------------------------------------------------------
// FlexBox Internal Implementation

internal UIAlignItems_Type
GetChildFlexAlignment(ui_layout_box *Parent, ui_layout_box *Child)
{
    UIAlignItems_Type Result = UIAlignItems_Start;

    if (Child->FlexItem.SelfAlign != UIAlignItems_None)
    {
        Result = Child->FlexItem.SelfAlign;
    }
    else
    {
        Result = Parent->FlexBox.Align;
    }

    return Result;
}

internal f32
FlexJustifyCenter(f32 ChildSize, f32 ContentSize)
{
    f32 Result = (ContentSize / 2) - (ChildSize * 0.5f);
    return Result;
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

internal b32 
FindHitNode(vec2_f32 MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    f32 MouseIsOutsideHitbox = 1.f;
    {

        vec2_f32 OuterSize = GetOuterBoxSize(&Root->Value);
        vec2_f32 OuterPos  = GetOuterBoxPosition(&Root->Value);

        vec2_f32 OuterHalf  = Vec2F32(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
        vec2_f32 Center     = Vec2F32Add(OuterPos, OuterHalf);
        vec2_f32 LocalMouse = Vec2F32Sub(MousePosition, Center);

        rect_sdf_params Params = {0};
        Params.HalfSize      = OuterHalf;
        Params.PointPosition = LocalMouse;

        MouseIsOutsideHitbox = RoundedRectSDF(Params);
    }

    if(MouseIsOutsideHitbox <= 0.f)
    {
        IterateLinkedListBackward(Root, ui_layout_node *, Child)
        {
            b32 Hit = FindHitNode(MousePosition, Child, Subtree);
            if(Hit)
            {
                return Hit;
            }
        }

        f32 ScrollDelta  = OSGetScrollDelta();
        b32 MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

        // Dynamic Events
        {
            if(HasFlag(Root->Flags, UILayoutNode_IsResizable) && MouseClicked && Subtree->CapturedNode == 0)
            {
                vec2_f32 InnerSize = GetInnerBoxSize(&Root->Value);
                vec2_f32 InnerPos  = GetInnerBoxPosition(&Root->Value);

                f32 MouseIsOutsideBorder = 1.f;
                {
                    vec2_f32 InnerHalf   = Vec2F32(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
                    vec2_f32 InnerCenter = Vec2F32Add(InnerPos, InnerHalf);

                    rect_sdf_params Params = {0};
                    Params.Radius        = 0.f;
                    Params.HalfSize      = InnerHalf;
                    Params.PointPosition = Vec2F32Sub(MousePosition, InnerCenter);

                    MouseIsOutsideBorder = RoundedRectSDF(Params);
                }

                if(MouseIsOutsideBorder >= 0.f)
                {
                    f32 Left        = InnerPos.X;
                    f32 Top         = InnerPos.Y;
                    f32 Width       = InnerSize.X;
                    f32 Height      = InnerSize.Y;
                    f32 BorderWidth = Root->Value.BorderWidth;

                    f32 CornerTolerance = 5.f;
                    f32 CornerX         = Left + Width;
                    f32 CornerY         = Top  + Height;

                    rect_f32 RightEdge  = RectF32(Left + Width, Top, BorderWidth, Height);
                    rect_f32 BottomEdge = RectF32(Left, Top + Height, Width, BorderWidth);
                    rect_f32 Corner     = RectF32(CornerX, CornerY, CornerTolerance, CornerTolerance);

                    if(IsPointInRect(Corner, MousePosition))
                    {
                        Subtree->Intent = UIIntent_ResizeXY;
                    } else
                    if(IsPointInRect(RightEdge, MousePosition))
                    {
                        Subtree->Intent = UIIntent_ResizeX;
                    } else
                    if (IsPointInRect(BottomEdge, MousePosition))
                    {
                        Subtree->Intent = UIIntent_ResizeY;
                    }

                    Subtree->CapturedNode = Root;
                }
            }

            if(HasFlag(Root->Flags, UILayoutNode_IsDraggable) && MouseClicked && Subtree->CapturedNode == 0)
            {
                Subtree->CapturedNode = Root;
                Subtree->Intent       = UIIntent_Drag;
            }
        }

        // Static Events
        {
            if (HasFlag(Root->Flags, UILayoutNode_IsClickable) && MouseClicked)
            {
            }

            if (Subtree->CapturedNode == 0)
            {
                RecordUIHoverEvent(Root, Subtree);
            }

            if (HasFlag(Root->Flags, UILayoutNode_IsScrollable) && ScrollDelta != 0.f)
            {
                RecordUIScrollEvent(Root, ScrollDelta, Subtree);
            }
        }
    }

    return 0;
}

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

// Places a layout subtree beginning at root from the subtree.

internal void
PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Subtree);
    Assert(Subtree->FrameData);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->FrameData);
    if (Queue.Data)
    {
        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node = PopNodeQueue(&Queue);
            ui_layout_box  *Box  = &Node->Value;

            vec2_f32 ContentPos  = GetContentBoxPosition(Box);
            vec2_f32 ContentSize = GetContentBoxSize(Box);

            vec2_f32 Cursor    = ContentPos;
            vec2_f32 MaxCursor = Vec2F32Add(Cursor, ContentSize);

            if(HasFlag(Node->Flags, UILayoutNode_IsScrollable))
            {
                ui_scroll_context *Scroll = Box->ScrollContext;
                if(Scroll->Axis == ScrollAxis_X)
                {
                    MaxCursor.X = FLT_MAX;
                }
                else
                {
                    MaxCursor.Y = FLT_MAX;
                }
            }

            if(Box->Display == UIDisplay_Flex)
            {
                if(Box->FlexBox.Direction == UIFlexDirection_Row)
                {
                    Assert(!"Not implemented");
                } else
                if(Box->FlexBox.Direction == UIFlexDirection_Column)
                {
                    IterateLinkedList(Node, ui_layout_node *, Child)
                    {
                        ui_layout_box *CBox      = &Child->Value;
                        vec2_f32       ChildSize = GetOuterBoxSize(CBox);

                        UIJustifyContent_Type Justify = Box->FlexBox.Justify;
                        switch(Justify)
                        {

                        case UIJustifyContent_Start:
                        case UIJustifyContent_End:
                        case UIJustifyContent_SpaceBetween:
                        case UIJustifyContent_SpaceAround:
                        case UIJustifyContent_SpaceEvenly:
                        {
                            Assert(!"Not Implemented");
                        } break;

                        case UIJustifyContent_Center:
                        {
                            CBox->VisualOffset.Y = FlexJustifyCenter(ChildSize.Y, ContentSize.Y) + Cursor.Y;
                        } break;

                        }

                        UIAlignItems_Type Align = GetChildFlexAlignment(Box, CBox);
                        switch(Align)
                        {

                        case UIAlignItems_Start:
                        case UIAlignItems_End:
                        case UIAlignItems_None:
                        {
                            Assert(!"Not Implemented");
                        } break;

                        case UIAlignItems_Center:
                        {
                            CBox->VisualOffset.X = FlexJustifyCenter(ChildSize.X, ContentSize.X) + Cursor.X;
                        } break;

                        case UIAlignItems_Stretch:
                        {
                            CBox->VisualOffset.X = Cursor.X;
                        } break;

                        }

                        // BUG: Need to increment Cursor. By what? Uh. Depends on direction probably?

                        PushNodeQueue(&Queue, Child);
                    }
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_PlaceChildrenX))
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    Child->Value.VisualOffset = Cursor;

                    vec2_f32 ChildSize = GetOuterBoxSize(&Child->Value);
                    Cursor.X += ChildSize.X + Box->Spacing.Horizontal;
                    if(Cursor.X > MaxCursor.X)
                    {
                        break;
                    }

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_PlaceChildrenY))
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    Child->Value.VisualOffset = Cursor;

                    vec2_f32 ChildSize = GetOuterBoxSize(&Child->Value);
                    Cursor.Y += ChildSize.Y + Box->Spacing.Vertical;
                    if(Cursor.Y >= MaxCursor.Y)
                    {
                        break;
                    }

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_DrawText))
            {
                ui_resource_key Key = GetNodeResource(Node->Index, Subtree);
                ui_glyph_run   *Run = GetTextResource(Key, UIState.ResourceTable);

                u32 FilledLine = 0.f;
                f32 LineWidth  = 0.f;
                f32 LineStartX = 0.f;
                f32 LineStartY = 0.f;

                for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
                {
                    ui_glyph *Glyph = &Run->Glyphs[Idx];

                    f32      GlyphWidth    = Glyph->AdvanceX;
                    vec2_f32 GlyphPosition = Vec2F32(LineStartX + LineWidth, LineStartY + (FilledLine * Run->LineHeight));

                    AlignGlyph(GlyphPosition, Glyph);

                    if(LineWidth + GlyphWidth > ContentSize.X)
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
PreOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Subtree);
    Assert(Subtree->FrameData);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->FrameData);

    if(Queue.Data)
    {
        UpdateNodeIfNeeded(Root->Index, Subtree);
        Assert(Root->Value.Width.Type  == UIUnit_Float32);
        Assert(Root->Value.Height.Type == UIUnit_Float32);

        ComputeNodeBoxes(Root, Root->Value.Width.Float32, Root->Value.Height.Float32);

        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node = PopNodeQueue(&Queue);
            Assert(Node);

            ui_layout_box *Box     = &Node->Value;
            vec2_f32       AvSpace = GetContentBoxSize(Box);

            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                UpdateNodeIfNeeded(Child->Index, Subtree);

                Box = &Child->Value;

                if(Box->Display != UIDisplay_None)
                {
                    resolved_unit Width  = TryConvertUnitToFloat(Box->Width , AvSpace.X);
                    resolved_unit Height = TryConvertUnitToFloat(Box->Height, AvSpace.Y);

                    // NOTE: Should it be &&?

                    if(Width.Resolved || Height.Resolved)
                    {
                        ComputeNodeBoxes(Child, Width.Value, Height.Value);
                    }
                }

                PushNodeQueue(&Queue, Child);
            }
        }
    }
}

internal void
PostOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasureSubtree(Child, Subtree);
    }

    ui_layout_box *Box = &Root->Value;

    if(HasFlag(Root->Flags, UILayoutNode_DrawText))
    {
        ui_resource_key Key = GetNodeResource(Root->Index, Subtree);
        ui_glyph_run   *Run = GetTextResource(Key, UIState.ResourceTable);

        // TODO: Handle other units
        f32 InnerAvWidth = 0.f;
        if (Box->Width.Type == UIUnit_Auto)
        {
            ui_layout_node *Parent = Root->Parent;
            if(Parent)
            {
                // TODO: Unsure how we want to handle height?
                InnerAvWidth = GetContentBoxSize(&Parent->Value).X;
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
            f32 Width  = (LineWrapCount == 0) ? LineWidth : InnerAvWidth;
            f32 Height = (LineWrapCount  + 1) * Run->LineHeight;

            ComputeNodeBoxes(Root, Width, Height);
        }
    }

    if(Box->Display == UIDisplay_Flex)
    {
        UIFlexDirection_Type MainAxis  = Box->FlexBox.Direction;
        UIFlexDirection_Type CrossAxis = MainAxis == UIFlexDirection_Row ? UIFlexDirection_Column : UIFlexDirection_Row;

        vec2_f32 ContentSize = GetContentBoxSize(&Root->Value);
        f32      MainSize    = MainAxis == UIFlexDirection_Row ? ContentSize.X : ContentSize.Y;
        f32      CrossSize   = MainAxis == UIFlexDirection_Row ? ContentSize.Y : ContentSize.X;

        f32 *MainAxisSize = PushArray(Subtree->FrameData, f32, Root->ChildCount);
        Assert(MainAxisSize);

        f32 SpaceNeeded = 0.f;
        u32 ItemCount   = 0;
        u32 ChildIdx    = 0;
        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            // NOTE: This usually uses something called "flex-basis"
            // But I do not really see the point? Unless we remove what we call
            // size and work directly from display modes.

            if(Child->Value.Display != UIDisplay_None)
            {
                vec2_f32 ChildSize = GetOuterBoxSize(&Child->Value);

                if(MainAxis == UIFlexDirection_Row)
                {
                    MainAxisSize[ChildIdx] = ChildSize.X;
                    SpaceNeeded           += ChildSize.X;
                } else
                if(MainAxis == UIFlexDirection_Column)
                {
                    MainAxisSize[ChildIdx] = ChildSize.Y;
                    SpaceNeeded           += ChildSize.Y;
                }

                ++ItemCount;
                ++ChildIdx;
            }
        }

        if(ItemCount)
        {
            f32 Spacing   = MainAxis == UIFlexDirection_Row ? Box->Spacing.Horizontal : Box->Spacing.Vertical;
            f32 FreeSpace = MainSize - SpaceNeeded - (ItemCount * Spacing);

            if(FreeSpace > 0)
            {
                // TODO: Handle Grow
            }else 
            if(FreeSpace < 0)
            {
                // TODO: Handle Shrink
            }

            // WARN: We assume no-wrap here.

            ChildIdx = 0;
            IterateLinkedList(Root, ui_layout_node *, Child)
            {
                ui_layout_box *CBox = &Child->Value;

                if(CBox->Display != UIDisplay_None)
                {
                    f32 FinalWidth  = 0.f;
                    f32 FinalHeight = 0.f;

                    UIAlignItems_Type Align = GetChildFlexAlignment(Box, CBox);
                    if(Align == UIAlignItems_Stretch)
                    {
                        if(CrossAxis == UIFlexDirection_Row)
                        {
                            FinalWidth  = CrossSize;
                            FinalHeight = MainAxisSize[ChildIdx];
                        } else
                        if(CrossAxis == UIFlexDirection_Column)
                        {
                            FinalWidth  = MainAxisSize[ChildIdx];
                            FinalHeight = CrossSize;
                        }
                    }
                    else
                    {
                        if(CrossAxis == UIFlexDirection_Row)
                        {
                            FinalWidth  = GetOuterBoxSize(CBox).X;
                            FinalHeight = MainAxisSize[ChildIdx];
                        } else
                        if(CrossAxis == UIFlexDirection_Column)
                        {
                            FinalWidth  = MainAxisSize[ChildIdx];
                            FinalHeight = GetOuterBoxSize(CBox).Y;
                        }
                    }

                    ComputeNodeBoxes(Child, FinalWidth, FinalHeight);

                    ++ChildIdx;
                }
            }
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

        u32 VisibleCount = 0;

        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            if(Child->Value.Display != UIDisplay_None)
            {
                vec2_f32 ChildSize = GetOuterBoxSize(&Child->Value);
                TotalWidth  += ChildSize.X;
                TotalHeight += ChildSize.Y;

                if (ChildSize.X > MaxWidth)
                {
                    MaxWidth = ChildSize.X;
                }

                if (ChildSize.Y > MaxHeight)
                {
                    MaxHeight = ChildSize.Y;
                }

                ++VisibleCount;
            }
        }

        ui_node_style *Style   = GetNodeStyle(Root->Index, Subtree);
        ui_spacing     Spacing = UIGetSpacing(Style->Properties[StyleState_Basic]);

        if (Scroll->Axis == ScrollAxis_X)
        {
            if(VisibleCount)
            {
                TotalWidth += (VisibleCount - 1) * Spacing.Horizontal;
            }

            Scroll->ContentSize = Vec2F32(TotalWidth, MaxHeight);
        } else
        if (Scroll->Axis == ScrollAxis_Y)
        {
            if(VisibleCount)
            {
                TotalHeight += (VisibleCount - 1) * Spacing.Vertical;
            }

            Scroll->ContentSize = Vec2F32(MaxWidth, TotalHeight);
        }
    }
}

internal style_property *
ComputeFinalStyle(ui_layout_node *Node, ui_subtree *Subtree)
{
    style_property *Result = 0;

    ui_node_style  *NodeStyle = GetNodeStyle(Node->Index, Subtree);
    StyleState_Type State     = StyleState_Basic;

    if(HasFlag(Node->Flags, UILayoutNode_IsHovered))
    {
        State = StyleState_Hover;

        // NOTE: This could be bad if other parts of the code depend on this flag.
        ClearFlag(Node->Flags, UILayoutNode_IsHovered);
    }

    if(State == StyleState_Basic)
    {
        Result = NodeStyle->Properties[State]; // Get base properties.
    }
    else
    {
        Result = PushArray(Subtree->FrameData, style_property, StyleProperty_Count);

        if(Result)
        {

            MemoryCopy(Result, NodeStyle->Properties[StyleState_Basic], sizeof(NodeStyle->Properties[StyleState_Basic]));

            ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
            {
                if(NodeStyle->Properties[State][Prop].IsSet)
                {
                    Result[Prop] = NodeStyle->Properties[State][Prop];
                }
            }
        }
    }

    Assert(Result);
    return Result;
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
                        ui_resource_key Key = GetNodeResource(Frame.Node->Index, Subtree);
                        ui_glyph_run   *Run = GetTextResource(Key, UIState.ResourceTable);

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

            style_property *Style = ComputeFinalStyle(Frame.Node, Subtree);

            vec2_f32 NodeOrigin = Vec2F32Add(Frame.Node->Value.VisualOffset, Frame.AccScroll);
            rect_f32 FinalRect  = TranslateRectF32(Frame.Node->Value.OuterBox, NodeOrigin);

            // Draws
            {
                ui_color BackgroundColor = UIGetColor(Style);
                if(IsVisibleColor(BackgroundColor))
                {
                    ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                    Rect->RectBounds  = FinalRect; 
                    Rect->Color       = BackgroundColor;
                    Rect->CornerRadii = UIGetCornerRadius(Style);
                    Rect->Softness    = UIGetSoftness(Style);
                }

                ui_color BorderColor = UIGetBorderColor(Style);
                f32      BorderWidth = UIGetBorderWidth(Style);
                if(IsVisibleColor(BorderColor) && BorderWidth > 0.f)
                {
                    ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                    Rect->RectBounds  = FinalRect;
                    Rect->Color       = BorderColor;
                    Rect->CornerRadii = UIGetCornerRadius(Style);
                    Rect->BorderWidth = BorderWidth;
                    Rect->Softness    = UIGetSoftness(Style);
                }

                if(HasFlag(Frame.Node->Flags, UILayoutNode_DrawText))
                {
                    ui_resource_key Key = GetNodeResource(Frame.Node->Index, Subtree);
                    ui_glyph_run   *Run = GetTextResource(Key, UIState.ResourceTable);

                    ui_color TextColor = UIGetTextColor(Style);
                    for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
                    {
                        ui_rect *Rect = PushDataInBatchList(Arena, BatchList);
                        Rect->SampleTexture = 1.f;
                        Rect->TextureSource = Run->Glyphs[Idx].Source;
                        Rect->RectBounds    = TranslatedRectF32(Run->Glyphs[Idx].Position, NodeOrigin);
                        Rect->Color         = TextColor;
                    }
                }
            }

            // Stack Frame
            {
                if (HasFlag(Frame.Node->Flags, UILayoutNode_IsScrollable))
                {
                    vec2_f32 NodeScroll = GetScrollNodeTranslation(Frame.Node);

                    ChildAccScroll.X += NodeScroll.X;
                    ChildAccScroll.Y += NodeScroll.Y;
                }

                if(HasFlag(Frame.Node->Flags, UILayoutNode_HasClip))
                {
                    ChildClip = TranslatedRectF32(Frame.Node->Value.ContentBox, NodeOrigin);

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
                if (!(HasFlag(Child->Flags, UILayoutNode_DoNotDraw)) && Child->Value.Display != UIDisplay_None)
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

    b32 MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    if(MouseReleased)
    {
        Subtree->CapturedNode = 0;
        Subtree->Intent       = UIIntent_None;
    }

    vec2_f32 MousePosition = OSGetMousePosition();

    FindHitNode(MousePosition, Root, Subtree);

    // NOTE: 
    // Could technically move this inside FindHitNode?

    if(Subtree->CapturedNode)
    {
        vec2_f32 MouseDelta = OSGetMouseDelta();
        b32      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

        if(MouseMoved)
        {
            ui_layout_node *Node = Subtree->CapturedNode;

            if(Subtree->Intent >= UIIntent_ResizeX && Subtree->Intent <= UIIntent_ResizeXY)
            {
                vec2_f32 Delta = Vec2F32(0.f, 0.f);

                if(Subtree->Intent == UIIntent_ResizeX)
                {
                    Delta.X = MouseDelta.X;
                } else
                if(Subtree->Intent == UIIntent_ResizeY)
                {
                    Delta.Y = MouseDelta.Y;
                } else
                if(Subtree->Intent == UIIntent_ResizeXY)
                {
                    Delta = MouseDelta;
                }

                RecordUIResizeEvent(Node, Delta, Subtree);
            } else
            if(Subtree->Intent == UIIntent_Drag)
            {
                RecordUIDragEvent(Node, MouseDelta, Subtree);
            }
        }
    }

    if(Subtree->Events)
    {
        // NOTE: Unsure when it is the right time to process events..

        ProcessEventList(Subtree->Events);
    }
}

internal void
ComputeLayout(ui_subtree *Subtree, memory_arena *Arena)
{
    Assert(Subtree);
    Assert(Arena);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);
    {
        PreOrderMeasureSubtree   (Root, Subtree);
        PostOrderMeasureSubtree  (Root, Subtree);
        PreOrderPlaceSubtree     (Root, Subtree);
    }
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
