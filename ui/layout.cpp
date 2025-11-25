// [Helpers]

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
    u32 ItemCount;
    f32 TotalSize;
    f32 TotalGrow;
    f32 TotalShrink;
    b32 HasAutoSizedChild;
} ui_flex_box;

struct ui_flex_item
{
    f32               Main;
    f32               Cross;
    f32               Grow;
    f32               Shrink;
    UIAlignItems_Type Alignment;

    f32        BorderWidth; // NOTE: Maybe on the box?
    ui_padding Padding;     // NOTE: Maybe on the box?
};

typedef struct ui_layout_box
{
    // Output
    vec2_f32 FixedOuterPosition;
    vec2_f32 FixedOuterSize;
    vec2_f32 FixedInnerPosition;
    vec2_f32 FixedInnerSize;
    vec2_f32 FixedContentPosition;
    vec2_f32 FixedContentSize;

    // Transformations (Applied every time we place)
    vec2_f32 ScrollOffset;
    vec2_f32 DragOffset;

    union
    {
        ui_flex_item FlexItem;
    };

    union
    {
        ui_flex_box FlexBox;
    };
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

internal rect_f32
GetOuterBoxRect(ui_layout_box *Box)
{
    vec2_f32 ScreenPosition = Box->FixedOuterPosition + Box->DragOffset + Box->ScrollOffset;
    rect_f32 Result = rect_f32::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedOuterSize.X, Box->FixedOuterSize.Y);
    return Result;
}

internal rect_f32
GetInnerBoxRect(ui_layout_box *Box)
{
    vec2_f32 ScreenPosition = Box->FixedInnerPosition + Box->DragOffset + Box->ScrollOffset;
    rect_f32 Result = rect_f32::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedInnerSize.X, Box->FixedInnerSize.Y);
    return Result;
}

internal rect_f32
GetContentBoxRect(ui_layout_box *Box)
{
    vec2_f32 ScreenPosition = Box->FixedContentPosition + Box->DragOffset + Box->ScrollOffset;
    rect_f32 Result = rect_f32::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedContentSize.X, Box->FixedContentSize.Y);
    return Result;
}

internal void
ComputeNodeBoxes(ui_layout_box *Box, f32 BorderWidth, ui_padding Padding, f32 Width, f32 Height)
{
    Assert(Box);

    Box->FixedOuterSize   = vec2_f32(Width, Height);
    Box->FixedInnerSize   = Box->FixedOuterSize - vec2_f32(BorderWidth * 2, BorderWidth * 2);
    Box->FixedContentSize = Box->FixedInnerSize - vec2_f32(Padding.Left + Padding.Right, Padding.Top + Padding.Bot);
}

internal void
PlaceLayoutBoxes(ui_layout_box *Box, vec2_f32 Origin, f32 BorderWidth, ui_padding Padding)
{
    Assert(Box);

    Box->FixedOuterPosition   = Origin;
    Box->FixedInnerPosition   = Box->FixedOuterPosition + (vec2_f32(BorderWidth , BorderWidth));
    Box->FixedContentPosition = Box->FixedInnerPosition + (vec2_f32(Padding.Left, Padding.Top));
}

internal b32 
IsValidLayoutNode(ui_layout_node *Node)
{
    b32 Result = Node && Node->Index != InvalidLayoutNodeIndex;
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
            Tree->ParentStack = BeginParentStack(Tree->NodeCapacity, Subtree->Transient);
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
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// @Internal: Tree Queries

internal u32
UITreeFindChild(u32 NodeIndex, u32 ChildIndex, ui_subtree *Subtree)
{
    u32 Result = InvalidLayoutNodeIndex;

    ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    if(IsValidLayoutNode(LayoutNode))
    {
        ui_layout_node *Child = LayoutNode->First;
        while(ChildIndex--)
        {
            Child = Child->Next;
        }

        if(Child)
        {
            Result = Child->Index;
        }
    }

    return Result;
}

internal void
UITreeAppendChild(u32 ParentIndex, u32 ChildIndex, ui_subtree *Subtree)
{
    Assert(ParentIndex != ChildIndex);

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Subtree->LayoutTree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Subtree->LayoutTree);

    Assert(Parent);
    Assert(Child);

    AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
    Child->Parent = Parent;
}

// BUG:
// Does not check if there is enough space to allocate the children.
// An assertion should be enough? Unsure, same for append layout child.
// Perhaps both?

internal void
UITreeReserve(u32 NodeIndex, u32 Amount, ui_subtree *Subtree)
{
    Assert(NodeIndex != InvalidLayoutNodeIndex);
    Assert(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    b32 NeedPush = (PeekParentStack(&Tree->ParentStack)->Index != NodeIndex);

    if(NeedPush)
    {
        ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Tree);
        Assert(LayoutNode);

        PushParentStack(LayoutNode, &Tree->ParentStack);
    }

    // WARN:
    // Possibly bugged.

    for(u32 Idx = 0; Idx < Amount; ++Idx)
    {
        CreateAndInsertLayoutNode(0, Subtree);
    }

    if(NeedPush)
    {
        PopParentStack(&Tree->ParentStack);
    }
}

// NOTE:
// We surely do not want to expose this...

internal void
SetLayoutNodeFlags(u32 NodeIndex, bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);

    SetFlag(Node->Flags, Flags);
}

internal void
ClearLayoutNodeFlags(u32 NodeIndex, bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);

    ClearFlag(Node->Flags, Flags);
}

internal u32
AllocateLayoutNode(bit_field Flags, ui_subtree *Subtree)
{
    Assert(Subtree);

    u32 Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = CreateAndInsertLayoutNode(Flags, Subtree);
    if (Node)
    {
        Result = Node->Index;
    }

    return Result;
}

// NOTE:
// Still hate this.

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

typedef struct ui_scroll_region
{
    vec2_f32    ContentSize;
    f32         ScrollOffset;
    f32         PixelPerLine;
    UIAxis_Type Axis;
} ui_scroll_region;

internal u64
GetScrollRegionFootprint(void)
{
    u64 Result = sizeof(ui_scroll_region);
    return Result;
}

internal ui_scroll_region *
PlaceScrollRegionInMemory(scroll_region_params Params, void *Memory)
{
    ui_scroll_region *Result = 0;

    if(Memory)
    {
        Result = (ui_scroll_region *)Memory;
        Result->ContentSize  = vec2_f32(0.f, 0.f);
        Result->ScrollOffset = 0.f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis         = Params.Axis;
    }

    return Result;
}

internal vec2_f32
GetScrollNodeTranslation(ui_scroll_region *Region)
{
    vec2_f32 Result = vec2_f32(0.f, 0.f);

    if (Region->Axis == UIAxis_X)
    {
        Result = vec2_f32(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        Result = vec2_f32(0.f, Region->ScrollOffset);
    }

    return Result;
}

internal void
UpdateScrollNode(f32 ScrolledLines, ui_layout_node *Node, ui_subtree *Subtree, ui_scroll_region *Region)
{
    f32      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_f32 WindowSize     = Node->Value.FixedOuterSize;

    f32 ScrollLimit = 0.f;
    if (Region->Axis == UIAxis_X)
    {
        ScrollLimit = -(Region->ContentSize.X - WindowSize.X);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        ScrollLimit = -(Region->ContentSize.Y - WindowSize.Y);
    }

    Region->ScrollOffset += ScrolledPixels;
    Region->ScrollOffset  = ClampTop(ClampBot(ScrollLimit, Region->ScrollOffset), 0);

    vec2_f32 ScrollDelta = vec2_f32(0.f, 0.f);
    if(Region->Axis == UIAxis_X)
    {
        ScrollDelta.X = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == UIAxis_Y)
    {
        ScrollDelta.Y = -1.f * Region->ScrollOffset;
    }

    rect_f32 WindowContent = GetOuterBoxRect(&Node->Value);
    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        style_property *Props   = GetPaintProperties(Child->Index, 0, Subtree);
        UIDisplay_Type  Display = UIGetDisplay(Props);

        if(Display != UIDisplay_None)
        {
            Child->Value.ScrollOffset = vec2_f32(-ScrollDelta.X, -ScrollDelta.Y);

            vec2_f32 FixedContentSize = Child->Value.FixedOuterSize;
            rect_f32 ChildContent     = GetOuterBoxRect(&Child->Value);

            if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
            {
                if (WindowContent.IsIntersecting(ChildContent))
                {
                    ClearFlag(Child->Flags, UILayoutNode_DoNotPaint);
                }
                else
                {
                    SetFlag(Child->Flags, UILayoutNode_DoNotPaint);
                }
            }
        }
    }
}

// -----------------------------------------------------------
// UI Event Internal Implementation

typedef enum UIEvent_Type
{
    UIEvent_Hover     = 1 << 0,
    UIEvent_Click     = 1 << 1,
    UIEvent_Resize    = 1 << 2,
    UIEvent_Drag      = 1 << 3,
    UIEvent_Scroll    = 1 << 4,
    UIEvent_TextInput = 1 << 5,
    UIEvent_Key       = 1 << 6,
} UIEvent_Type;

typedef struct ui_hover_event
{
    ui_layout_node *Node;
} ui_hover_event;

typedef struct ui_click_event
{
    ui_layout_node *Node;
} ui_click_event;

typedef struct ui_resize_event
{
    ui_layout_node *Node;
    vec2_f32        Delta;
} ui_resize_event;

typedef struct ui_drag_event
{
    ui_layout_node *Node;
    vec2_f32        Delta;
} ui_drag_event;

typedef struct ui_scroll_event
{
    ui_layout_node *Node;
    f32             Delta;
} ui_scroll_event;

typedef struct ui_text_input_event
{
    byte_string     Text;
    ui_layout_node *Node;
} ui_text_input_event;

typedef struct ui_key_event
{
    u32             Keycode;
    ui_layout_node *Node;
} ui_key_event;

typedef struct ui_event
{
    UIEvent_Type Type;
    union
    {
        ui_hover_event      Hover;
        ui_click_event      Click;
        ui_resize_event     Resize;
        ui_drag_event       Drag;
        ui_scroll_event     Scroll;
        ui_text_input_event TextInput;
        ui_key_event        Key;
    };
} ui_event;

typedef struct ui_event_node ui_event_node;
struct ui_event_node
{
    ui_event_node *Next;
    ui_event       Value;
};

internal void 
RecordUIEvent(ui_event Event, ui_event_list *EventList, memory_arena *Arena)
{
    Assert(EventList);

    ui_event_node *Node = PushStruct(Arena, ui_event_node);
    if(Node)
    {
        Node->Value = Event;
        AppendToLinkedList(EventList, Node, EventList->Count);
    }
}

internal void
RecordUIHoverEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Hover;
    Event.Hover.Node = Node;

    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIClickEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Click;
    Event.Click.Node = Node;
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIScrollEvent(ui_layout_node *Node, f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Scroll;
    Event.Scroll.Node  = Node;
    Event.Scroll.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIResizeEvent(ui_layout_node *Node, vec2_f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Resize;
    Event.Resize.Node  = Node;
    Event.Resize.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIDragEvent(ui_layout_node *Node, vec2_f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Drag;
    Event.Drag.Node  = Node;
    Event.Drag.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIKeyEvent(u32 Keycode, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type        = UIEvent_Key;
    Event.Key.Node    = Node;
    Event.Key.Keycode = Keycode;
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUITextInputEvent(byte_string Text, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type           = UIEvent_TextInput;
    Event.TextInput.Node = Node;
    Event.TextInput.Text = Text;
    RecordUIEvent(Event, EventList, Arena);
}

internal b32
IsMouseInsideOuterBox(vec2_f32 MousePosition, u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    Assert(Node);

    vec2_f32 OuterSize  = Node->Value.FixedOuterSize;
    vec2_f32 OuterPos   = Node->Value.FixedOuterPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_f32 OuterHalf  = vec2_f32(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_f32 Center     = OuterPos + OuterHalf;
    vec2_f32 LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    f32 Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

internal b32
IsMouseInsideBorder(vec2_f32 MousePosition, ui_layout_node *Node)
{
    vec2_f32 InnerSize   = Node->Value.FixedInnerSize;
    vec2_f32 InnerPos    = Node->Value.FixedInnerPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_f32 InnerHalf   = vec2_f32(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_f32 InnerCenter = InnerPos + InnerHalf;

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = MousePosition - InnerCenter;

    f32 Distance = RoundedRectSDF(Params);
    return Distance >= 0.f;
}

internal void
ClearUIEvents(ui_event_list *EventList)
{
    Assert(EventList);

    EventList->First = 0;
    EventList->Last  = 0;
    EventList->Count = 0;
}

internal UIIntent_Type
DetermineResizeIntent(vec2_f32 MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
{
    vec2_f32 InnerSize = Node->Value.FixedInnerSize;
    vec2_f32 InnerPos  = Node->Value.FixedInnerPosition;

    f32 Left        = InnerPos.X;
    f32 Top         = InnerPos.Y;
    f32 Width       = InnerSize.X;
    f32 Height      = InnerSize.Y;
    f32 BorderWidth = UIGetBorderWidth(GetPaintProperties(Node->Index, 0, Subtree));

    f32 CornerTolerance = 5.f;
    f32 CornerX         = Left + Width;
    f32 CornerY         = Top  + Height;

    rect_f32 RightEdge  = rect_f32::FromXYWH(Left + Width, Top, BorderWidth, Height);
    rect_f32 BottomEdge = rect_f32::FromXYWH(Left, Top + Height, Width, BorderWidth);
    rect_f32 Corner     = rect_f32::FromXYWH(CornerX, CornerY, CornerTolerance, CornerTolerance);

    UIIntent_Type Result = UIIntent_None;

    if(Corner.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeXY;
    } else
    if(RightEdge.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeX;
    } else
    if(BottomEdge.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeY;
    }

    return Result;
}

// NOTE:
// The hovered node is set on the global state by: UISetNodeHover.
// We call: UIHasNodeHover to check if that global state was set.
// It's a bit opaque but it fits well with other parts of the code.

// WARN:
// Errr actually, it's not that good. Maybe re-visit.

internal void
FindHoveredNode(vec2_f32 MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
{
    if(IsMouseInsideOuterBox(MousePosition, Node->Index, Subtree))
    {
        IterateLinkedListBackward(Node, ui_layout_node *, Child)
        {
            FindHoveredNode(MousePosition, Child, Subtree);

            if(UIHasNodeHover())
            {
                return;
            }
        }

        UISetNodeHover(Node->Index, Subtree);
    }

    return;
}

internal void
AttemptNodeFocus(vec2_f32 MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    FindHoveredNode(MousePosition, Root, Subtree);
    if(UIHasNodeHover())
    {
        ui_hovered_node Hovered = UIGetNodeHover();
        Assert(Hovered.Subtree == Subtree);

        ui_layout_node *Node   = GetLayoutNode(Hovered.Index, Hovered.Subtree->LayoutTree);
        memory_arena   *Arena  = Subtree->Transient;
        ui_event_list  *Events = &Subtree->Events;

        b32 MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

        if (HasFlag(Node->Flags, UILayoutNode_IsResizable) && MouseClicked)
        {
            if (IsMouseInsideBorder(MousePosition, Node))
            {
                UIIntent_Type Intent = DetermineResizeIntent(MousePosition, Node, Subtree);
                if (Intent != UIIntent_None)
                {
                    UISetNodeFocus(Node->Index, Subtree, 0, Intent);
                    return;
                }
            }
        }

        if (HasFlag(Node->Flags, UILayoutNode_IsDraggable) && MouseClicked)
        {
            UISetNodeFocus(Node->Index, Subtree, 0, UIIntent_Drag);
            return;
        }

        if (MouseClicked)
        {
            if (HasFlag(Node->Flags, UILayoutNode_HasTextInput))
            {
                UISetNodeFocus(Node->Index, Subtree, 1, UIIntent_EditTextInput);
            }

            RecordUIClickEvent(Node, Events, Arena);
            return;
        }

        // NOTE:
        // Should this take focus?

        f32 ScrollDelta = OSGetScrollDelta();
        if (HasFlag(Node->Flags, UILayoutNode_HasScrollRegion) && ScrollDelta != 0.f)
        {
            RecordUIScrollEvent(Node, ScrollDelta, Events, Arena);
        }

        RecordUIHoverEvent(Node, Events, Arena);
    }
}

internal void
GenerateFocusedNodeEvents(ui_focused_node Focused, ui_event_list *Events, memory_arena *Arena)
{
    vec2_f32 MouseDelta = OSGetMouseDelta();
    b32      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

    ui_layout_node *Node = GetLayoutNode(Focused.Index, Focused.Subtree->LayoutTree);
    Assert(Node);

    if(MouseMoved && Focused.Intent >= UIIntent_ResizeX && Focused.Intent <= UIIntent_ResizeXY)
    {
        vec2_f32 Delta = vec2_f32(0.f, 0.f);

        if(Focused.Intent == UIIntent_ResizeX)
        {
            Delta.X = MouseDelta.X;
        } else
        if(Focused.Intent == UIIntent_ResizeY)
        {
            Delta.Y = MouseDelta.Y;
        } else
        if(Focused.Intent == UIIntent_ResizeXY)
        {
            Delta = MouseDelta;
        }

        RecordUIResizeEvent(Node, Delta, Events, Arena);
    } else
    if(MouseMoved && Focused.Intent == UIIntent_Drag)
    {
        RecordUIDragEvent(Node, MouseDelta, Events, Arena);
    } else
    if(Focused.Intent == UIIntent_EditTextInput)
    {
        // NOTE:
        // Might be a more general event.

        os_button_playback *Playback = OSGetButtonPlayback();
        Assert(Playback);

        for(u32 Idx = 0; Idx < Playback->Count; ++Idx)
        {
            os_button_action Action = Playback->Actions[Idx];
            if(Action.IsPressed)
            {
                u32 Key = Action.Keycode;
                RecordUIKeyEvent(Key, Node, Events, Arena);
            }
        }

        os_utf8_playback *Text = OSGetTextPlayback();
        Assert(Text);

        if(Text->Count)
        {
            byte_string TextAsString = ByteString(Text->UTF8, Text->Count);
            RecordUITextInputEvent(TextAsString, Node, Events, Arena);
        }
    }
}

internal void
GenerateUIEvents(vec2_f32 MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    ui_event_list *Events = &Subtree->Events;
    memory_arena  *Arena  = Subtree->Transient;
    Assert(Arena);

    if(!UIHasNodeFocus())
    {
        AttemptNodeFocus(MousePosition, Root, Subtree);
    }

    // BUG:
    // The weird part is that this is done on a per-tree basis.
    // This makes pretty much no sense. Well... Idk?

    if(UIHasNodeFocus())
    {
        ui_focused_node Focused = UIGetNodeFocus();

        SetNodeStyleState(StyleState_Focused, Focused.Index, Focused.Subtree);
        GenerateFocusedNodeEvents(Focused, Events, Arena);
    }
}

internal void
ProcessUIEvents(ui_event_list *Events, ui_subtree *Subtree)
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
            Assert(Hover.Node);

            SetNodeStyleState(StyleState_Hovered, Hover.Node->Index, Subtree);
        } break;

        case UIEvent_Click:
        {
        } break;

        case UIEvent_Resize:
        {
            ui_resize_event Resize = Event->Resize;

            ui_node_style *Style        = GetNodeStyle(Resize.Node->Index, Subtree);
            vec2_unit      CurrentSize  = UIGetSize(Style->Properties[StyleState_Default]);

            Assert(CurrentSize.X.Type == UIUnit_Float32);
            Assert(CurrentSize.X.Type == UIUnit_Float32);

            vec2_unit Resized = CurrentSize;
            Resized.X.Float32 += Resize.Delta.X;
            Resized.Y.Float32 += Resize.Delta.Y;

            UISetSize(Resize.Node->Index, Resized, Subtree);
        } break;

        // NOTE:
        // Perhaps these are animations? I do not have an animation system right now.

        case UIEvent_Drag:
        {
            ui_drag_event Drag = Event->Drag;

            ui_layout_node *Node = Drag.Node;
            Assert(Node);

            Node->Value.DragOffset += Drag.Delta;
        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            Assert(Scroll.Node);
            Assert(Scroll.Delta);

            ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Scroll.Node->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
            Assert(Region);

            UpdateScrollNode(Scroll.Delta, Scroll.Node, Subtree, Region);
        } break;

        case UIEvent_Key:
        {
            ui_key_event Key = Event->Key;
            Assert(Key.Node);

            ui_layout_node    *Node  = Key.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            if(HasFlag(Node->Flags, UILayoutNode_HasTextInput))
            {
                ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);
                Assert(Input);

                if(Input->OnKey)
                {
                    Input->OnKey((OSInputKey_Type)Key.Keycode, Input->OnKeyUserData);
                }
            }
        } break;

        case UIEvent_TextInput:
        {
            ui_text_input_event TextInput = Event->TextInput;
            Assert(TextInput.Node);

            ui_layout_node    *Node  = TextInput.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            ui_text       *Text  = (ui_text *)      QueryNodeResource(Node->Index, Subtree, UIResource_Text     , Table);
            ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);

            Assert(Text);
            Assert(Input);

            if(IsValidByteString(TextInput.Text))
            {
                ui_node_style *Style = GetNodeStyle(Node->Index, Subtree);
                ui_font       *Font  = UIGetFont(Style->Properties[StyleState_Default]);

                u32 CountBeforeAppend = Text->ShapedCount;
                UITextAppend_(TextInput.Text, Font, Text);
                u32 CountAfterAppend = Text->ShapedCount;

                UITextInputMoveCaret_(Text, Input, CountAfterAppend - CountBeforeAppend);

                // NOTE: BAD! IDK WHAT TO DO!
                ByteStringAppendTo(Input->UserBuffer, TextInput.Text, Input->InternalCount);
                Input->InternalCount += TextInput.Text.Size;
            }

            if(Text->ShapedCount > 0)
            {
                SetFlag(Node->Flags, UILayoutNode_HasText);
            }
            else
            {
                ClearFlag(Node->Flags, UILayoutNode_HasText);
            }
        } break;

        }
    }
}

// ----------------------------------------------------------------------------------
// Text Layout Implementation

internal f32
GetUITextOffset(UIAlign_Type Align, f32 FixedContentSize, f32 TextSize)
{
    f32 Result = 0.f;

    if(Align == UIAlign_Center)
    {
        Result = (FixedContentSize - TextSize) * 0.5f;
    } else
    if(Align == UIAlign_End)
    {
        Result = (FixedContentSize - TextSize);
    }

    return Result;
}

internal f32
GetUITextSpace(ui_layout_node *Node, ui_unit Width)
{
    Assert(Node);

    f32 Result = Node->Value.FixedContentSize.X;

    if(Width.Type == UIUnit_Auto)
    {
        ui_layout_node *Parent = Node->Parent;
        Assert(Parent);

        Result = Parent->Value.FixedContentSize.X;
    }

    return Result;
}

internal void
AlignUITextLine(f32 ContentWidth, f32 LineWidth, UIAlign_Type XAlign, ui_shaped_glyph *Glyphs, u32 Count)
{
    f32 XAlignOffset = GetUITextOffset(XAlign, ContentWidth, LineWidth);

    if(XAlignOffset != 0.f)
    {
        for(u32 Idx = 0; Idx < Count; ++Idx)
        {
            Glyphs[Idx].Position.Left += XAlignOffset;
            Glyphs[Idx].Position.Right += XAlignOffset;
        }
    }
}

// -----------------------------------------------------------
// FlexBox Internal Implementation

internal UIAlignItems_Type
GetChildFlexAlignment(UIAlignItems_Type ParentAlign, UIAlignItems_Type ChildAlign)
{
    UIAlignItems_Type Result = ParentAlign;

    if (ChildAlign != UIAlignItems_None)
    {
        Result = ChildAlign;
    }

    return Result;
}

internal f32
FlexJustifyCenter(f32 ChildSize, f32 FixedContentSize)
{
    f32 Result = (FixedContentSize / 2) - (ChildSize * 0.5f);
    return Result;
}

// WARN:
// There is too much dependency on the subtree. Need to remove that.
// It's quite easy.

internal void
GrowFlexChildren(f32 FreeSpace, f32 TotalGrow, ui_layout_box **Boxes, u32 ItemCount)
{
    Assert(Boxes);
    Assert(FreeSpace > 0.f);
    Assert(TotalGrow > 0.f);

    for(u32 Idx = 0; Idx < ItemCount; ++Idx)
    {
        ui_flex_item *Item = &Boxes[Idx]->FlexItem;
        Item->Main += (Item->Grow / TotalGrow) * FreeSpace;
    }
}

internal void
ShrinkFlexChildren(f32 FreeSpace, f32 TotalShrink, ui_layout_box **Boxes, u32 ItemCount)
{
    Assert(Boxes);
    Assert(FreeSpace < 0.f);
    Assert(TotalShrink > 0.f);

    for(u32 Idx = 0; Idx < ItemCount; ++Idx)
    {
        ui_flex_item *Item = &Boxes[Idx]->FlexItem;
        Item->Main = ((Item->Shrink * Item->Main) / TotalShrink) * FreeSpace;
    }
}

// -----------------------------------------------------------
// Layout Pass Internal Helpers/Types

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

// NOTE:
// This code is really garbage.

internal void
PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Subtree);
    Assert(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    ui_resource_table *Table = UIState.ResourceTable;
    Assert(Table);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->Transient);
    if (Queue.Data)
    {
        {
            style_property *Props = GetPaintProperties(Root->Index, 0, Subtree);
            PlaceLayoutBoxes(&Root->Value, vec2_f32(0, 0), UIGetBorderWidth(Props), UIGetPadding(Props));
        }

        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node  = PopNodeQueue(&Queue);
            ui_layout_box  *Box   = &Node->Value;
            style_property *Props = GetPaintProperties(Node->Index, 0, Subtree);

            ui_spacing     Spacing = UIGetSpacing(Props);
            UIDisplay_Type Display = UIGetDisplay(Props);

            rect_f32 Content = GetContentBoxRect(Box);
            vec2_f32 Cursor  = vec2_f32(Content.Left, Content.Top);

            if(Display == UIDisplay_Flex)
            {
                UIFlexDirection_Type  Direction = UIGetFlexDirection(Props);
                UIJustifyContent_Type Justify   = UIGetJustifyContent(Props);

                f32 MainSize  = Direction == UIFlexDirection_Row ? Box->FixedContentSize.X : Box->FixedContentSize.Y;
                f32 CrossSize = Direction == UIFlexDirection_Row ? Box->FixedContentSize.Y : Box->FixedContentSize.X;
                f32 FreeSpace = MainSize - Box->FlexBox.TotalSize;

                f32 MainAxisSpacing  = Direction == UIFlexDirection_Row ? Spacing.Horizontal : Spacing.Vertical;
                f32 CrossAxisSpacing = Direction == UIFlexDirection_Row ? Spacing.Vertical   : Spacing.Horizontal;

                f32 MainAxisCursor      = Direction == UIFlexDirection_Row ? Cursor.X : Cursor.Y;
                f32 CrossAxisCursorBase = Direction == UIFlexDirection_Row ? Cursor.Y : Cursor.X;

                switch(Justify)
                {
                    case UIJustifyContent_Start: break;

                    case UIJustifyContent_End:
                    case UIJustifyContent_SpaceBetween:
                    case UIJustifyContent_SpaceAround:
                    case UIJustifyContent_SpaceEvenly:
                    {
                        Assert(!"Not Implemented");
                    } break;

                    case UIJustifyContent_Center:
                    {
                        MainAxisCursor += (FreeSpace * 0.5f);
                    } break;
                }

                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    ui_layout_box *CBox = &Child->Value;
                    ui_flex_item  *Item = &CBox->FlexItem;

                    vec2_f32 ChildSize          = CBox->FixedOuterSize;
                    f32      MainAxisChildSize  = Direction == UIFlexDirection_Row ? ChildSize.X : ChildSize.Y;
                    f32      CrossAxisChildSize = Direction == UIFlexDirection_Row ? ChildSize.Y : ChildSize.X;

                    f32 CrossAxisCursor = CrossAxisCursorBase;

                    switch(Item->Alignment)
                    {

                    case UIAlignItems_Start:    break;
                    case UIAlignItems_Stretch : break;

                    case UIAlignItems_End:
                    {
                        Assert(!"Not Implemented");
                    } break;

                    case UIAlignItems_Center:
                    {
                        CrossAxisCursor += (CrossSize - CrossAxisChildSize) * 0.5f;
                    } break;

                    case UIAlignItems_None:
                    {
                        Assert(!"Impossible.");
                    } break;

                    }

                    if(Direction == UIFlexDirection_Row)
                    {
                        PlaceLayoutBoxes(CBox, vec2_f32(MainAxisCursor, CrossAxisCursor), Item->BorderWidth, Item->Padding);
                    } else
                    if(Direction == UIFlexDirection_Column)
                    {
                        PlaceLayoutBoxes(CBox, vec2_f32(CrossAxisCursor, MainAxisCursor), Item->BorderWidth, Item->Padding);
                    }

                    MainAxisCursor += MainAxisChildSize + MainAxisSpacing;

                    PushNodeQueue(&Queue, Child);
                }

                Useless(CrossAxisSpacing);
            }

            if(Display == UIDisplay_Normal)
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    ui_layout_box  *CBox  = &Child->Value;
                    style_property *Props = GetPaintProperties(Child->Index, 0, Subtree);

                    f32        BorderWidth = UIGetBorderWidth(Props);
                    ui_padding Padding     = UIGetPadding(Props);

                    PlaceLayoutBoxes(CBox, vec2_f32(Cursor.X, Cursor.Y), BorderWidth, Padding);

                    Cursor.Y += CBox->FixedOuterSize.Y;

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_HasText))
            {
                ui_text *Text = (ui_text *)QueryNodeResource(Node->Index, Subtree, UIResource_Text, Table);

                UIAlign_Type XAlign = UIGetXTextAlign(Props);
                UIAlign_Type YAlign = UIGetYTextAlign(Props);
                ui_unit      Width  = UIGetSize(Props).X;

                f32 WrapWidth   = GetUITextSpace(Node, Width);
                f32 LineWidth   = 0.f;
                f32 LineStartX  = Cursor.X;
                f32 LineStartY  = Cursor.Y + GetUITextOffset(YAlign, Box->FixedContentSize.Y, Text->TotalHeight);

                u32 LineStartIdx = 0;

                for(u32 Idx = 0; Idx < Text->ShapedCount; ++Idx)
                {
                    ui_shaped_glyph *Shaped = &Text->Shaped[Idx];

                    if(LineWidth + Shaped->AdvanceX > WrapWidth && LineWidth > 0.f)
                    {
                        u32              LineCount = Idx - LineStartIdx;
                        ui_shaped_glyph *LineStart = Text->Shaped + LineStartIdx;
                        AlignUITextLine(WrapWidth, LineWidth, XAlign, LineStart, LineCount);

                        LineStartY  += Text->LineHeight;
                        LineWidth    = 0.f;
                        LineStartIdx = Idx;
                    }

                    vec2_f32 Position = vec2_f32(LineStartX + LineWidth, LineStartY);
                    Shaped->Position.Left   = Position.X + Shaped->Offset.X;
                    Shaped->Position.Top    = Position.Y + Shaped->Offset.Y;
                    Shaped->Position.Right  = Shaped->Position.Left + Shaped->Size.X;
                    Shaped->Position.Bottom = Shaped->Position.Top + Shaped->Size.Y;

                    LineWidth += Shaped->AdvanceX;
                }

                u32              LineCount = Text->ShapedCount - LineStartIdx;
                ui_shaped_glyph *LineStart = Text->Shaped + LineStartIdx;
                AlignUITextLine(WrapWidth, LineWidth, XAlign, LineStart, LineCount);
            }

        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: PreOrder Measure Passes Implementation

internal void
PreOrderMeasureNormal(ui_layout_node *Node, ui_subtree *Subtree, style_property *Props, vec2_f32 ContentSize, node_queue *Queue)
{
    Assert(Node);
    Assert(Props);
    Assert(Queue);
    Assert(UIGetDisplay(Props) == UIDisplay_Normal);

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        style_property *CProps = GetPaintProperties(Child->Index, 0, Subtree);

        vec2_unit  Size        = UIGetSize(CProps);
        f32        BorderWidth = UIGetBorderWidth(CProps);
        ui_padding Padding     = UIGetPadding(CProps);

        resolved_unit Width  = TryConvertUnitToFloat(Size.X, ContentSize.X);
        resolved_unit Height = TryConvertUnitToFloat(Size.Y, ContentSize.Y);

        if(Width.Resolved || Height.Resolved)
        {
            ComputeNodeBoxes(&Child->Value, BorderWidth, Padding, Width.Value, Height.Value);
        }

        PushNodeQueue(Queue, Child);
    }
}

internal void
PreOrderMeasureFlex(ui_layout_node *Node, ui_subtree *Subtree, style_property *Props, vec2_f32 ContentSize, node_queue *Queue)
{
    Assert(Node);
    Assert(Props);
    Assert(Queue);
    Assert(UIGetDisplay(Props) == UIDisplay_Flex);

    ui_flex_box *FlexBox = &Node->Value.FlexBox;
    {
        FlexBox->TotalGrow         = 0.f;
        FlexBox->TotalShrink       = 0.f;
        FlexBox->TotalSize         = 0.f;
        FlexBox->ItemCount         = 0;
        FlexBox->HasAutoSizedChild = 0;
    }

    ui_spacing           Spacing       = UIGetSpacing(Props);
    UIFlexDirection_Type FlexDirection = UIGetFlexDirection(Props);
    UIAlignItems_Type    Alignment     = UIGetAlignItems(Props);

    bool            IsRow = (FlexDirection == UIFlexDirection_Row);
    ui_layout_box **Boxes = PushArray(Subtree->Transient, ui_layout_box*, Node->ChildCount);

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        style_property *CProps = GetPaintProperties(Child->Index, 0, Subtree);

        if(UIGetDisplay(CProps) == UIDisplay_None)
        {
            continue;
        }

        Boxes[FlexBox->ItemCount] = &Child->Value;

        vec2_unit     Size   = UIGetSize(CProps);
        resolved_unit Width  = TryConvertUnitToFloat(Size.X, ContentSize.X);
        resolved_unit Height = TryConvertUnitToFloat(Size.Y, ContentSize.Y);

        ui_flex_item *Item = &Boxes[FlexBox->ItemCount]->FlexItem;
        Item->BorderWidth = UIGetBorderWidth(CProps);
        Item->Padding     = UIGetPadding(CProps);
        Item->Alignment   = GetChildFlexAlignment(Alignment, UIGetAlignItems(CProps));
        Item->Shrink      = UIGetFlexShrink(CProps);
        Item->Grow        = UIGetFlexGrow(CProps);
        Item->Main        = IsRow ? Width.Value  : Height.Value;
        Item->Cross       = IsRow ? Height.Value : Width.Value;

        if((IsRow && !Width.Resolved) || (!IsRow && !Height.Resolved))
        {
            FlexBox->HasAutoSizedChild = 1;
        }

        FlexBox->TotalSize   += Item->Main;
        FlexBox->TotalGrow   += Item->Grow;
        FlexBox->TotalShrink += Item->Shrink * Item->Main;
        FlexBox->ItemCount   += 1;

        PushNodeQueue(Queue, Child);
    }

    if(FlexBox->ItemCount > 0)
    {
        FlexBox->TotalSize += (FlexBox->ItemCount - 1) * (IsRow ? Spacing.Horizontal : Spacing.Vertical);
    }
    else
    {
        return;
    }

    f32 FreeSpace = (IsRow ? ContentSize.X : ContentSize.Y) - FlexBox->TotalSize;
    if(FreeSpace > 0.f && FlexBox->TotalGrow > 0.f)
    {
        GrowFlexChildren(FreeSpace, FlexBox->TotalGrow, Boxes, FlexBox->ItemCount);
    } else
    if(FreeSpace < 0.f && FlexBox->TotalShrink > 0.f)
    {
        ShrinkFlexChildren(FreeSpace, FlexBox->TotalShrink, Boxes, FlexBox->ItemCount);
    }

    for(u32 Idx = 0; Idx < FlexBox->ItemCount; ++Idx)
    {
        ui_layout_box *Box  = Boxes[Idx];
        ui_flex_item  *Item = &Box->FlexItem;

        if(Item->Alignment == UIAlignItems_Stretch)
        {
            Item->Cross = IsRow ? ContentSize.Y : ContentSize.X;
        }

        f32 FinalWidth  = IsRow ? Item->Main  : Item->Cross;
        f32 FinalHeight = IsRow ? Item->Cross : Item->Main;
        ComputeNodeBoxes(Box, Item->BorderWidth, Item->Padding, FinalWidth, FinalHeight);
    }
}

internal void
PreOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    // TODO: Call IsValidX(Y)
    Assert(Root);
    Assert(Subtree);

    node_queue Queue = BeginNodeQueue({.QueueSize = Subtree->LayoutTree->NodeCount}, Subtree->Transient);
    if(Queue.Data)
    {
        style_property *RootProps = GetPaintProperties(Root->Index, 0, Subtree);

        f32        BorderWidth = UIGetBorderWidth(RootProps);
        ui_padding Padding     = UIGetPadding(RootProps);
        vec2_unit  Size        = UIGetSize(RootProps);

        ComputeNodeBoxes(&Root->Value, BorderWidth, Padding, Size.X.Float32, Size.Y.Float32);

        PushNodeQueue(&Queue, Root);

        while(!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node  = PopNodeQueue(&Queue);
            style_property *Props = GetPaintProperties(Node->Index, 0, Subtree);

            UIDisplay_Type Display     = UIGetDisplay(Props);
            vec2_f32       ContentSize = Node->Value.FixedContentSize;

            switch(Display)
            {

            case UIDisplay_Normal:
            {
                PreOrderMeasureNormal(Node, Subtree, Props, ContentSize, &Queue);
            } break;

            case UIDisplay_Flex:
            {
                PreOrderMeasureFlex(Node, Subtree, Props, ContentSize, &Queue);
            } break;

            case UIDisplay_None:
            {
                Assert(!"Why are you pushed?");
            } break;

            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: PostOrder Measure Pass Implementation

internal void
PostOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasureSubtree(Child, Subtree);
    }

    ui_layout_box  *Box   = &Root->Value;
    style_property *Props = GetPaintProperties(Root->Index, 0, Subtree);

    vec2_unit      Size        = UIGetSize(Props);
    f32            BorderWidth = UIGetBorderWidth(Props);
    ui_padding     Padding     = UIGetPadding(Props);
    UIDisplay_Type Display     = UIGetDisplay(Props);

    if(HasFlag(Root->Flags, UILayoutNode_HasImage))
    {
        ui_image *Image = (ui_image *)QueryNodeResource(Root->Index, Subtree, UIResource_Image, UIState.ResourceTable);
        Assert(Image);

        if(Size.X.Type == UIUnit_Auto && Size.Y.Type == UIUnit_Auto)
        {
            f32 Width  = Image->Source.Right  - Image->Source.Left;
            f32 Height = Image->Source.Bottom - Image->Source.Top;

            ComputeNodeBoxes(&Root->Value, BorderWidth, Padding, Width, Height);
        }
        else
        {
            Assert(!"Not Implemented");
        }
    }

    if (HasFlag(Root->Flags, UILayoutNode_HasText))
    {
        ui_text *Text = (ui_text *)QueryNodeResource(Root->Index, Subtree, UIResource_Text, UIState.ResourceTable);

        f32 InnerAvWidth  = GetUITextSpace(Root, Size.X);
        f32 LineWidth     = 0.f;
        u32 LineWrapCount = 0;

        if (Text->ShapedCount)
        {
            for (u32 Idx = 0; Idx < Text->ShapedCount; ++Idx)
            {
                ui_shaped_glyph *Shaped = &Text->Shaped[Idx];

                if (LineWidth + Shaped->AdvanceX > InnerAvWidth && LineWidth > 0.f)
                {
                    LineWrapCount += 1;
                    LineWidth      = 0.f;
                }

                LineWidth += Shaped->AdvanceX;
            }
        }

        Text->TotalHeight = ((LineWrapCount + 1) * Text->LineHeight);

        if (Size.X.Type == UIUnit_Auto)
        {
            f32 Width  = (LineWrapCount == 0) ? LineWidth : InnerAvWidth;
            f32 Height = (LineWrapCount + 1) * Text->LineHeight;

            ComputeNodeBoxes(&Root->Value, BorderWidth, Padding, Width, Height);
        }
    }

    if(Display == UIDisplay_Flex && Box->FlexBox.HasAutoSizedChild)
    {
        UIFlexDirection_Type MainAxis = UIGetFlexDirection(Props);

        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            // NOTE:
            // Isn't this already checked? What?

            b32 MainAxisIsAuto =
                (MainAxis == UIFlexDirection_Row    && Size.X.Type == UIUnit_Auto) ||
                (MainAxis == UIFlexDirection_Column && Size.Y.Type == UIUnit_Auto);

            // NOTE:
            // I assume this may trigger possible wrapping? What about nested autos?
            // What if the main axis is auto when trying to display a flex element?
            // We might need a full recompute then? Unless we disallow?

            if(MainAxisIsAuto)
            {
                vec2_f32 Measured     = Child->Value.FixedOuterSize;
                f32      MeasuredMain = (MainAxis == UIFlexDirection_Row) ? Measured.X : Measured.Y;

                Box->FlexBox.TotalSize += MeasuredMain;
            }
        }
    }

    if(HasFlag(Root->Flags, UILayoutNode_HasScrollRegion))
    {
        ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Root->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
        Assert(Region);

        f32 TotalWidth  = 0.f;
        f32 TotalHeight = 0.f;
        f32 MaxWidth    = 0.f;
        f32 MaxHeight   = 0.f;

        u32 VisibleCount = 0;

        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            style_property *Props = GetPaintProperties(Child->Index, 0, Subtree);

            UIDisplay_Type Display = UIGetDisplay(Props);

            if(Display != UIDisplay_None)
            {
                vec2_f32 ChildSize = Child->Value.FixedOuterSize;
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

        ui_spacing Spacing = UIGetSpacing(Props);

        if (Region->Axis == UIAxis_X)
        {
            if(VisibleCount)
            {
                TotalWidth += (VisibleCount - 1) * Spacing.Horizontal;
            }

            Region->ContentSize = vec2_f32(TotalWidth, MaxHeight);
        } else
        if (Region->Axis == UIAxis_Y)
        {
            if(VisibleCount)
            {
                TotalHeight += (VisibleCount - 1) * Spacing.Vertical;
            }

            Region->ContentSize = vec2_f32(MaxWidth, TotalHeight);
        }
    }
}

// -----------------------------------------------------------------------------------
// Layout Pass Public API implementation

// NOTE:
// Core?

internal void
UpdateSubtreeState(ui_subtree *Subtree)
{
    Assert(Subtree);
    Assert(Subtree->LayoutTree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    vec2_f32 MousePosition = OSGetMousePosition();

    ClearUIEvents(&Subtree->Events);
    GenerateUIEvents(MousePosition, Root, Subtree);
    ProcessUIEvents(&Subtree->Events, Subtree);
}

internal void
ComputeSubtreeLayout(ui_subtree *Subtree)
{
    TimeFunction;

    Assert(Subtree);

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
PaintSubtree(ui_subtree *Subtree)
{
    TimeFunction;

    Assert(Subtree);
    Assert(Subtree->LayoutTree);
    Assert(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    if(Root)
    {
        PaintLayoutTreeFromRoot(Root, Subtree);
    }
}
