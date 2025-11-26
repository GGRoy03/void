// [Helpers]

typedef struct resolved_unit
{
    float Value;
    bool Resolved;
} resolved_unit;

static resolved_unit
TryConvertUnitToFloat(ui_unit Unit, float AvSpace)
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
    uint64_t NextWrite;
    uint64_t Size;

    ui_layout_node **Nodes;
} ui_parent_stack;

typedef struct ui_flex_box
{
    uint32_t ItemCount;
    float TotalSize;
    float TotalGrow;
    float TotalShrink;
    bool HasAutoSizedChild;
} ui_flex_box;

struct ui_flex_item
{
    float               Main;
    float               Cross;
    float               Grow;
    float               Shrink;
    UIAlignItems_Type Alignment;

    float        BorderWidth; // NOTE: Maybe on the box?
    ui_padding Padding;     // NOTE: Maybe on the box?
};

typedef struct ui_layout_box
{
    // Output
    vec2_float FixedOuterPosition;
    vec2_float FixedOuterSize;
    vec2_float FixedInnerPosition;
    vec2_float FixedInnerSize;
    vec2_float FixedContentPosition;
    vec2_float FixedContentSize;

    // Transformations (Applied every time we place)
    vec2_float ScrollOffset;
    vec2_float DragOffset;

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
    uint32_t SubIndex;
    uint32_t Index;
    uint32_t ChildCount;

    // Value
    ui_layout_box Value;
    uint32_t     Flags;
};

typedef struct ui_layout_tree
{
    // Nodes
    uint64_t             NodeCapacity;
    uint64_t             NodeCount;
    ui_layout_node *Nodes;

    // Depth
    ui_parent_stack ParentStack;
} ui_layout_tree;

static rect_float
GetOuterBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedOuterPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedOuterSize.X, Box->FixedOuterSize.Y);
    return Result;
}

static rect_float
GetInnerBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedInnerPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedInnerSize.X, Box->FixedInnerSize.Y);
    return Result;
}

static rect_float
GetContentBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedContentPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedContentSize.X, Box->FixedContentSize.Y);
    return Result;
}

static void
ComputeNodeBoxes(ui_layout_box *Box, float BorderWidth, ui_padding Padding, float Width, float Height)
{
    VOID_ASSERT(Box);

    Box->FixedOuterSize   = vec2_float(Width, Height);
    Box->FixedInnerSize   = Box->FixedOuterSize - vec2_float(BorderWidth * 2, BorderWidth * 2);
    Box->FixedContentSize = Box->FixedInnerSize - vec2_float(Padding.Left + Padding.Right, Padding.Top + Padding.Bot);
}

static void
PlaceLayoutBoxes(ui_layout_box *Box, vec2_float Origin, float BorderWidth, ui_padding Padding)
{
    VOID_ASSERT(Box);

    Box->FixedOuterPosition   = Origin;
    Box->FixedInnerPosition   = Box->FixedOuterPosition + (vec2_float(BorderWidth , BorderWidth));
    Box->FixedContentPosition = Box->FixedInnerPosition + (vec2_float(Padding.Left, Padding.Top));
}

static bool 
IsValidLayoutNode(ui_layout_node *Node)
{
    bool Result = Node && Node->Index != InvalidLayoutNodeIndex;
    return Result;
}

static ui_layout_node *
GetLayoutNode(uint32_t Index, ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if(Index < Tree->NodeCount)
    {
        Result = Tree->Nodes + Index;
    }

    return Result;
}

static ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree && "Calling GetFreeLayoutNode with a NULL tree");

    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

static ui_layout_node *
NodeToLayoutNode(ui_node Node, ui_layout_tree *Tree)
{
    ui_layout_node *Result = GetLayoutNode(Node.IndexInTree, Tree);
    return Result;
}

static ui_parent_stack
BeginParentStack(uint64_t Size, memory_arena *Arena)
{
    ui_parent_stack Result = {0};
    Result.Nodes = PushArray(Arena, ui_layout_node *, Size);
    Result.Size  = Size;

    return Result;
}

static bool
IsValidParentStack(ui_parent_stack *Stack)
{
    bool Result = (Stack) && (Stack->Size);
    return Result;
}

static void
PushParentStack(ui_layout_node *Node, ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Nodes[Stack->NextWrite++] = Node;
    }
}

static ui_layout_node *
PeekParentStack(ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    ui_layout_node *Result = 0;

    if(Stack->NextWrite > 0)
    {
        Result = Stack->Nodes[Stack->NextWrite - 1];
    }

    return Result;
}

static void
PopParentStack(ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    if(Stack->NextWrite > 0)
    {
        --Stack->NextWrite;
    }
}

static ui_layout_node *
CreateAndInsertLayoutNode(uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

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

        if (VOID_HASBIT(Result->Flags, UILayoutNode_IsParent))
        {
            PushParentStack(Result, &Tree->ParentStack);
        }
    }

    return Result;
}


// -------------------------------------------------------------
// Tree/Node Public API Implementation

static uint64_t
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = NodeCount * sizeof(ui_layout_node);
    uint64_t Result    = sizeof(ui_layout_tree) + ArraySize;

    return Result;
}

static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        Result = (ui_layout_tree *)Memory;
        Result->NodeCapacity = NodeCount;
        Result->Nodes        = (ui_layout_node *)(Result + 1);

        for (uint32_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// @internal: Tree Queries

static uint32_t
UITreeFindChild(uint32_t NodeIndex, uint32_t ChildIndex, ui_subtree *Subtree)
{
    uint32_t Result = InvalidLayoutNodeIndex;

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

static void
UITreeAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(ParentIndex != ChildIndex);

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Subtree->LayoutTree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Subtree->LayoutTree);

    VOID_ASSERT(Parent);
    VOID_ASSERT(Child);

    AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
    Child->Parent = Parent;
}

// BUG:
// Does not check if there is enough space to allocate the children.
// An assertion should be enough? Unsure, same for append layout child.
// Perhaps both?

static void
UITreeReserve(uint32_t NodeIndex, uint32_t Amount, ui_subtree *Subtree)
{
    VOID_ASSERT(NodeIndex != InvalidLayoutNodeIndex);
    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    bool NeedPush = (PeekParentStack(&Tree->ParentStack)->Index != NodeIndex);

    if(NeedPush)
    {
        ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Tree);
        VOID_ASSERT(LayoutNode);

        PushParentStack(LayoutNode, &Tree->ParentStack);
    }

    // WARN:
    // Possibly bugged.

    for(uint32_t Idx = 0; Idx < Amount; ++Idx)
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

static void
SetLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    VOID_SETBIT(Node->Flags, Flags);
}

static void
ClearLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    VOID_CLEARBIT(Node->Flags, Flags);
}

static uint32_t
AllocateLayoutNode(uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = CreateAndInsertLayoutNode(Flags, Subtree);
    if (Node)
    {
        Result = Node->Index;
    }

    return Result;
}

// NOTE:
// Still hate this.

static void
UIEnd()
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    VOID_ASSERT(Subtree);

    ui_layout_tree *LayoutTree = Subtree->LayoutTree;
    VOID_ASSERT(LayoutTree);

    PopParentStack(&LayoutTree->ParentStack);
}

// -----------------------------------------------------------
// UI Scrolling internal Implementation

typedef struct ui_scroll_region
{
    vec2_float    ContentSize;
    float         ScrollOffset;
    float         PixelPerLine;
    UIAxis_Type Axis;
} ui_scroll_region;

static uint64_t
GetScrollRegionFootprint(void)
{
    uint64_t Result = sizeof(ui_scroll_region);
    return Result;
}

static ui_scroll_region *
PlaceScrollRegionInMemory(scroll_region_params Params, void *Memory)
{
    ui_scroll_region *Result = 0;

    if(Memory)
    {
        Result = (ui_scroll_region *)Memory;
        Result->ContentSize  = vec2_float(0.f, 0.f);
        Result->ScrollOffset = 0.f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis         = Params.Axis;
    }

    return Result;
}

static vec2_float
GetScrollNodeTranslation(ui_scroll_region *Region)
{
    vec2_float Result = vec2_float(0.f, 0.f);

    if (Region->Axis == UIAxis_X)
    {
        Result = vec2_float(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        Result = vec2_float(0.f, Region->ScrollOffset);
    }

    return Result;
}

static void
UpdateScrollNode(float ScrolledLines, ui_layout_node *Node, ui_subtree *Subtree, ui_scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_float WindowSize     = Node->Value.FixedOuterSize;

    float ScrollLimit = 0.f;
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

    vec2_float ScrollDelta = vec2_float(0.f, 0.f);
    if(Region->Axis == UIAxis_X)
    {
        ScrollDelta.X = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == UIAxis_Y)
    {
        ScrollDelta.Y = -1.f * Region->ScrollOffset;
    }

    rect_float WindowContent = GetOuterBoxRect(&Node->Value);
    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        style_property *Props   = GetPaintProperties(Child->Index, 0, Subtree);
        UIDisplay_Type  Display = UIGetDisplay(Props);

        if(Display != UIDisplay_None)
        {
            Child->Value.ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

            vec2_float FixedContentSize = Child->Value.FixedOuterSize;
            rect_float ChildContent     = GetOuterBoxRect(&Child->Value);

            if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
            {
                if (WindowContent.IsIntersecting(ChildContent))
                {
                    VOID_CLEARBIT(Child->Flags, UILayoutNode_DoNotPaint);
                }
                else
                {
                    VOID_SETBIT(Child->Flags, UILayoutNode_DoNotPaint);
                }
            }
        }
    }
}

// -----------------------------------------------------------
// UI Event internal Implementation

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
    vec2_float        Delta;
} ui_resize_event;

typedef struct ui_drag_event
{
    ui_layout_node *Node;
    vec2_float        Delta;
} ui_drag_event;

typedef struct ui_scroll_event
{
    ui_layout_node *Node;
    float             Delta;
} ui_scroll_event;

typedef struct ui_text_input_event
{
    byte_string     Text;
    ui_layout_node *Node;
} ui_text_input_event;

typedef struct ui_key_event
{
    uint32_t             Keycode;
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

static void 
RecordUIEvent(ui_event Event, ui_event_list *EventList, memory_arena *Arena)
{
    VOID_ASSERT(EventList);

    ui_event_node *Node = PushStruct(Arena, ui_event_node);
    if(Node)
    {
        Node->Value = Event;
        AppendToLinkedList(EventList, Node, EventList->Count);
    }
}

static void
RecordUIHoverEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Hover;
    Event.Hover.Node = Node;

    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIClickEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Click;
    Event.Click.Node = Node;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIScrollEvent(ui_layout_node *Node, float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Scroll;
    Event.Scroll.Node  = Node;
    Event.Scroll.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIResizeEvent(ui_layout_node *Node, vec2_float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Resize;
    Event.Resize.Node  = Node;
    Event.Resize.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIDragEvent(ui_layout_node *Node, vec2_float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Drag;
    Event.Drag.Node  = Node;
    Event.Drag.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIKeyEvent(uint32_t Keycode, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type        = UIEvent_Key;
    Event.Key.Node    = Node;
    Event.Key.Keycode = Keycode;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUITextInputEvent(byte_string Text, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type           = UIEvent_TextInput;
    Event.TextInput.Node = Node;
    Event.TextInput.Text = Text;
    RecordUIEvent(Event, EventList, Arena);
}

static bool
IsMouseInsideOuterBox(vec2_float MousePosition, uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    vec2_float OuterSize  = Node->Value.FixedOuterSize;
    vec2_float OuterPos   = Node->Value.FixedOuterPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_float Center     = OuterPos + OuterHalf;
    vec2_float LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    float Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

static bool
IsMouseInsideBorder(vec2_float MousePosition, ui_layout_node *Node)
{
    vec2_float InnerSize   = Node->Value.FixedInnerSize;
    vec2_float InnerPos    = Node->Value.FixedInnerPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_float InnerHalf   = vec2_float(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_float InnerCenter = InnerPos + InnerHalf;

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = MousePosition - InnerCenter;

    float Distance = RoundedRectSDF(Params);
    return Distance >= 0.f;
}

static void
ClearUIEvents(ui_event_list *EventList)
{
    VOID_ASSERT(EventList);

    EventList->First = 0;
    EventList->Last  = 0;
    EventList->Count = 0;
}

static UIIntent_Type
DetermineResizeIntent(vec2_float MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
{
    vec2_float InnerSize = Node->Value.FixedInnerSize;
    vec2_float InnerPos  = Node->Value.FixedInnerPosition;

    float Left        = InnerPos.X;
    float Top         = InnerPos.Y;
    float Width       = InnerSize.X;
    float Height      = InnerSize.Y;
    float BorderWidth = UIGetBorderWidth(GetPaintProperties(Node->Index, 0, Subtree));

    float CornerTolerance = 5.f;
    float CornerX         = Left + Width;
    float CornerY         = Top  + Height;

    rect_float RightEdge  = rect_float::FromXYWH(Left + Width, Top, BorderWidth, Height);
    rect_float BottomEdge = rect_float::FromXYWH(Left, Top + Height, Width, BorderWidth);
    rect_float Corner     = rect_float::FromXYWH(CornerX, CornerY, CornerTolerance, CornerTolerance);

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

static void
FindHoveredNode(vec2_float MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
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

static void
AttemptNodeFocus(vec2_float MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    FindHoveredNode(MousePosition, Root, Subtree);
    if(UIHasNodeHover())
    {
        ui_hovered_node Hovered = UIGetNodeHover();
        VOID_ASSERT(Hovered.Subtree == Subtree);

        ui_layout_node *Node   = GetLayoutNode(Hovered.Index, Hovered.Subtree->LayoutTree);
        memory_arena   *Arena  = Subtree->Transient;
        ui_event_list  *Events = &Subtree->Events;

        bool MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

        if (VOID_HASBIT(Node->Flags, UILayoutNode_IsResizable) && MouseClicked)
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

        if (VOID_HASBIT(Node->Flags, UILayoutNode_IsDraggable) && MouseClicked)
        {
            UISetNodeFocus(Node->Index, Subtree, 0, UIIntent_Drag);
            return;
        }

        if (MouseClicked)
        {
            if (VOID_HASBIT(Node->Flags, UILayoutNode_HasTextInput))
            {
                UISetNodeFocus(Node->Index, Subtree, 1, UIIntent_EditTextInput);
            }

            RecordUIClickEvent(Node, Events, Arena);
            return;
        }

        // NOTE:
        // Should this take focus?

        float ScrollDelta = OSGetScrollDelta();
        if (VOID_HASBIT(Node->Flags, UILayoutNode_HasScrollRegion) && ScrollDelta != 0.f)
        {
            RecordUIScrollEvent(Node, ScrollDelta, Events, Arena);
        }

        RecordUIHoverEvent(Node, Events, Arena);
    }
}

static void
GenerateFocusedNodeEvents(ui_focused_node Focused, ui_event_list *Events, memory_arena *Arena)
{
    vec2_float MouseDelta = OSGetMouseDelta();
    bool      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

    ui_layout_node *Node = GetLayoutNode(Focused.Index, Focused.Subtree->LayoutTree);
    VOID_ASSERT(Node);

    if(MouseMoved && Focused.Intent >= UIIntent_ResizeX && Focused.Intent <= UIIntent_ResizeXY)
    {
        vec2_float Delta = vec2_float(0.f, 0.f);

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
        VOID_ASSERT(Playback);

        for(uint32_t Idx = 0; Idx < Playback->Count; ++Idx)
        {
            os_button_action Action = Playback->Actions[Idx];
            if(Action.IsPressed)
            {
                uint32_t Key = Action.Keycode;
                RecordUIKeyEvent(Key, Node, Events, Arena);
            }
        }

        os_utf8_playback *Text = OSGetTextPlayback();
        VOID_ASSERT(Text);

        if(Text->Count)
        {
            byte_string TextAsString = ByteString(Text->UTF8, Text->Count);
            RecordUITextInputEvent(TextAsString, Node, Events, Arena);
        }
    }
}

static void
GenerateUIEvents(vec2_float MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    ui_event_list *Events = &Subtree->Events;
    memory_arena  *Arena  = Subtree->Transient;
    VOID_ASSERT(Arena);

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

static void
ProcessUIEvents(ui_event_list *Events, ui_subtree *Subtree)
{
    VOID_ASSERT(Events);

    IterateLinkedList(Events, ui_event_node *, Node)
    {
        ui_event *Event = &Node->Value;

        switch(Event->Type)
        {

        case UIEvent_Hover:
        {
            ui_hover_event Hover = Event->Hover;
            VOID_ASSERT(Hover.Node);

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

            VOID_ASSERT(CurrentSize.X.Type == UIUnit_Float32);
            VOID_ASSERT(CurrentSize.X.Type == UIUnit_Float32);

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
            VOID_ASSERT(Node);

            Node->Value.DragOffset += Drag.Delta;
        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            VOID_ASSERT(Scroll.Node);
            VOID_ASSERT(Scroll.Delta);

            ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Scroll.Node->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
            VOID_ASSERT(Region);

            UpdateScrollNode(Scroll.Delta, Scroll.Node, Subtree, Region);
        } break;

        case UIEvent_Key:
        {
            ui_key_event Key = Event->Key;
            VOID_ASSERT(Key.Node);

            ui_layout_node    *Node  = Key.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            if(VOID_HASBIT(Node->Flags, UILayoutNode_HasTextInput))
            {
                ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);
                VOID_ASSERT(Input);

                if(Input->OnKey)
                {
                    Input->OnKey((OSInputKey_Type)Key.Keycode, Input->OnKeyUserData);
                }
            }
        } break;

        case UIEvent_TextInput:
        {
            ui_text_input_event TextInput = Event->TextInput;
            VOID_ASSERT(TextInput.Node);

            ui_layout_node    *Node  = TextInput.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            ui_text       *Text  = (ui_text *)      QueryNodeResource(Node->Index, Subtree, UIResource_Text     , Table);
            ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);

            VOID_ASSERT(Text);
            VOID_ASSERT(Input);

            if(IsValidByteString(TextInput.Text))
            {
                ui_node_style *Style = GetNodeStyle(Node->Index, Subtree);
                ui_font       *Font  = UIGetFont(Style->Properties[StyleState_Default]);

                uint32_t CountBeforeAppend = Text->ShapedCount;
                UITextAppend_(TextInput.Text, Font, Text);
                uint32_t CountAfterAppend = Text->ShapedCount;

                UITextInputMoveCaret_(Text, Input, CountAfterAppend - CountBeforeAppend);

                // NOTE: BAD! IDK WHAT TO DO!
                ByteStringAppendTo(Input->UserBuffer, TextInput.Text, Input->internalCount);
                Input->internalCount += TextInput.Text.Size;
            }

            if(Text->ShapedCount > 0)
            {
                VOID_SETBIT(Node->Flags, UILayoutNode_HasText);
            }
            else
            {
                VOID_CLEARBIT(Node->Flags, UILayoutNode_HasText);
            }
        } break;

        }
    }
}

// ----------------------------------------------------------------------------------
// Text Layout Implementation

static float
GetUITextOffset(UIAlign_Type Align, float FixedContentSize, float TextSize)
{
    float Result = 0.f;

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

static float
GetUITextSpace(ui_layout_node *Node, ui_unit Width)
{
    VOID_ASSERT(Node);

    float Result = Node->Value.FixedContentSize.X;

    if(Width.Type == UIUnit_Auto)
    {
        ui_layout_node *Parent = Node->Parent;
        VOID_ASSERT(Parent);

        Result = Parent->Value.FixedContentSize.X;
    }

    return Result;
}

static void
AlignUITextLine(float ContentWidth, float LineWidth, UIAlign_Type XAlign, ui_shaped_glyph *Glyphs, uint32_t Count)
{
    float XAlignOffset = GetUITextOffset(XAlign, ContentWidth, LineWidth);

    if(XAlignOffset != 0.f)
    {
        for(uint32_t Idx = 0; Idx < Count; ++Idx)
        {
            Glyphs[Idx].Position.Left += XAlignOffset;
            Glyphs[Idx].Position.Right += XAlignOffset;
        }
    }
}

// -----------------------------------------------------------
// FlexBox internal Implementation

static UIAlignItems_Type
GetChildFlexAlignment(UIAlignItems_Type ParentAlign, UIAlignItems_Type ChildAlign)
{
    UIAlignItems_Type Result = ParentAlign;

    if (ChildAlign != UIAlignItems_None)
    {
        Result = ChildAlign;
    }

    return Result;
}

static float
FlexJustifyCenter(float ChildSize, float FixedContentSize)
{
    float Result = (FixedContentSize / 2) - (ChildSize * 0.5f);
    return Result;
}

static void
GrowFlexChildren(float FreeSpace, float TotalGrow, ui_layout_box **Boxes, uint32_t ItemCount)
{
    VOID_ASSERT(Boxes);
    VOID_ASSERT(FreeSpace > 0.f);
    VOID_ASSERT(TotalGrow > 0.f);

    for(uint32_t Idx = 0; Idx < ItemCount; ++Idx)
    {
        ui_flex_item *Item = &Boxes[Idx]->FlexItem;
        Item->Main += (Item->Grow / TotalGrow) * FreeSpace;
    }
}

static void
ShrinkFlexChildren(float FreeSpace, float TotalShrink, ui_layout_box **Boxes, uint32_t ItemCount)
{
    VOID_ASSERT(Boxes);
    VOID_ASSERT(FreeSpace < 0.f);
    VOID_ASSERT(TotalShrink > 0.f);

    for(uint32_t Idx = 0; Idx < ItemCount; ++Idx)
    {
        ui_flex_item *Item = &Boxes[Idx]->FlexItem;
        Item->Main = ((Item->Shrink * Item->Main) / TotalShrink) * FreeSpace;
    }
}

// -----------------------------------------------------------
// Layout Pass internal Helpers/Types

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

// NOTE:
// This code is really garbage.

static void
PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    VOID_ASSERT(Root);
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    ui_resource_table *Table = UIState.ResourceTable;
    VOID_ASSERT(Table);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->Transient);
    if (Queue.Data)
    {
        {
            style_property *Props = GetPaintProperties(Root->Index, 0, Subtree);
            PlaceLayoutBoxes(&Root->Value, vec2_float(0, 0), UIGetBorderWidth(Props), UIGetPadding(Props));
        }

        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node  = PopNodeQueue(&Queue);
            ui_layout_box  *Box   = &Node->Value;
            style_property *Props = GetPaintProperties(Node->Index, 0, Subtree);

            ui_spacing     Spacing = UIGetSpacing(Props);
            UIDisplay_Type Display = UIGetDisplay(Props);

            rect_float Content = GetContentBoxRect(Box);
            vec2_float Cursor  = vec2_float(Content.Left, Content.Top);

            if(Display == UIDisplay_Flex)
            {
                UIFlexDirection_Type  Direction = UIGetFlexDirection(Props);
                UIJustifyContent_Type Justify   = UIGetJustifyContent(Props);

                float MainSize  = Direction == UIFlexDirection_Row ? Box->FixedContentSize.X : Box->FixedContentSize.Y;
                float CrossSize = Direction == UIFlexDirection_Row ? Box->FixedContentSize.Y : Box->FixedContentSize.X;
                float FreeSpace = MainSize - Box->FlexBox.TotalSize;

                float MainAxisSpacing  = Direction == UIFlexDirection_Row ? Spacing.Horizontal : Spacing.Vertical;
                float CrossAxisSpacing = Direction == UIFlexDirection_Row ? Spacing.Vertical   : Spacing.Horizontal;

                float MainAxisCursor      = Direction == UIFlexDirection_Row ? Cursor.X : Cursor.Y;
                float CrossAxisCursorBase = Direction == UIFlexDirection_Row ? Cursor.Y : Cursor.X;

                switch(Justify)
                {
                    case UIJustifyContent_Start: break;

                    case UIJustifyContent_End:
                    case UIJustifyContent_SpaceBetween:
                    case UIJustifyContent_SpaceAround:
                    case UIJustifyContent_SpaceEvenly:
                    {
                        VOID_ASSERT(!"Not Implemented");
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

                    vec2_float ChildSize          = CBox->FixedOuterSize;
                    float      MainAxisChildSize  = Direction == UIFlexDirection_Row ? ChildSize.X : ChildSize.Y;
                    float      CrossAxisChildSize = Direction == UIFlexDirection_Row ? ChildSize.Y : ChildSize.X;

                    float CrossAxisCursor = CrossAxisCursorBase;

                    switch(Item->Alignment)
                    {

                    case UIAlignItems_Start:    break;
                    case UIAlignItems_Stretch : break;

                    case UIAlignItems_End:
                    {
                        VOID_ASSERT(!"Not Implemented");
                    } break;

                    case UIAlignItems_Center:
                    {
                        CrossAxisCursor += (CrossSize - CrossAxisChildSize) * 0.5f;
                    } break;

                    case UIAlignItems_None:
                    {
                        VOID_ASSERT(!"Impossible.");
                    } break;

                    }

                    if(Direction == UIFlexDirection_Row)
                    {
                        PlaceLayoutBoxes(CBox, vec2_float(MainAxisCursor, CrossAxisCursor), Item->BorderWidth, Item->Padding);
                    } else
                    if(Direction == UIFlexDirection_Column)
                    {
                        PlaceLayoutBoxes(CBox, vec2_float(CrossAxisCursor, MainAxisCursor), Item->BorderWidth, Item->Padding);
                    }

                    MainAxisCursor += MainAxisChildSize + MainAxisSpacing;

                    PushNodeQueue(&Queue, Child);
                }

                VOID_UNUSED(CrossAxisSpacing);
            }

            if(Display == UIDisplay_Normal)
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    ui_layout_box  *CBox  = &Child->Value;
                    style_property *Props = GetPaintProperties(Child->Index, 0, Subtree);

                    float        BorderWidth = UIGetBorderWidth(Props);
                    ui_padding Padding     = UIGetPadding(Props);

                    PlaceLayoutBoxes(CBox, vec2_float(Cursor.X, Cursor.Y), BorderWidth, Padding);

                    Cursor.Y += CBox->FixedOuterSize.Y;

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(VOID_HASBIT(Node->Flags, UILayoutNode_HasText))
            {
                ui_text *Text = (ui_text *)QueryNodeResource(Node->Index, Subtree, UIResource_Text, Table);

                UIAlign_Type XAlign = UIGetXTextAlign(Props);
                UIAlign_Type YAlign = UIGetYTextAlign(Props);
                ui_unit      Width  = UIGetSize(Props).X;

                float WrapWidth   = GetUITextSpace(Node, Width);
                float LineWidth   = 0.f;
                float LineStartX  = Cursor.X;
                float LineStartY  = Cursor.Y + GetUITextOffset(YAlign, Box->FixedContentSize.Y, Text->TotalHeight);

                uint32_t LineStartIdx = 0;

                for(uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
                {
                    ui_shaped_glyph *Shaped = &Text->Shaped[Idx];

                    if(LineWidth + Shaped->AdvanceX > WrapWidth && LineWidth > 0.f)
                    {
                        uint32_t              LineCount = Idx - LineStartIdx;
                        ui_shaped_glyph *LineStart = Text->Shaped + LineStartIdx;
                        AlignUITextLine(WrapWidth, LineWidth, XAlign, LineStart, LineCount);

                        LineStartY  += Text->LineHeight;
                        LineWidth    = 0.f;
                        LineStartIdx = Idx;
                    }

                    vec2_float Position = vec2_float(LineStartX + LineWidth, LineStartY);
                    Shaped->Position.Left   = Position.X + Shaped->Offset.X;
                    Shaped->Position.Top    = Position.Y + Shaped->Offset.Y;
                    Shaped->Position.Right  = Shaped->Position.Left + Shaped->Size.X;
                    Shaped->Position.Bottom = Shaped->Position.Top + Shaped->Size.Y;

                    LineWidth += Shaped->AdvanceX;
                }

                uint32_t              LineCount = Text->ShapedCount - LineStartIdx;
                ui_shaped_glyph *LineStart = Text->Shaped + LineStartIdx;
                AlignUITextLine(WrapWidth, LineWidth, XAlign, LineStart, LineCount);
            }

        }
    }
}

// ----------------------------------------------------------------------------------
// @internal: PreOrder Measure Passes Implementation

static void
PreOrderMeasureNormal(ui_layout_node *Node, ui_subtree *Subtree, style_property *Props, vec2_float ContentSize, node_queue *Queue)
{
    VOID_ASSERT(Node);
    VOID_ASSERT(Props);
    VOID_ASSERT(Queue);
    VOID_ASSERT(UIGetDisplay(Props) == UIDisplay_Normal);

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        style_property *CProps = GetPaintProperties(Child->Index, 0, Subtree);

        vec2_unit  Size        = UIGetSize(CProps);
        float        BorderWidth = UIGetBorderWidth(CProps);
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

static void
PreOrderMeasureFlex(ui_layout_node *Node, ui_subtree *Subtree, style_property *Props, vec2_float ContentSize, node_queue *Queue)
{
    VOID_ASSERT(Node);
    VOID_ASSERT(Props);
    VOID_ASSERT(Queue);
    VOID_ASSERT(UIGetDisplay(Props) == UIDisplay_Flex);

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

    float FreeSpace = (IsRow ? ContentSize.X : ContentSize.Y) - FlexBox->TotalSize;
    if(FreeSpace > 0.f && FlexBox->TotalGrow > 0.f)
    {
        GrowFlexChildren(FreeSpace, FlexBox->TotalGrow, Boxes, FlexBox->ItemCount);
    } else
    if(FreeSpace < 0.f && FlexBox->TotalShrink > 0.f)
    {
        ShrinkFlexChildren(FreeSpace, FlexBox->TotalShrink, Boxes, FlexBox->ItemCount);
    }

    for(uint32_t Idx = 0; Idx < FlexBox->ItemCount; ++Idx)
    {
        ui_layout_box *Box  = Boxes[Idx];
        ui_flex_item  *Item = &Box->FlexItem;

        if(Item->Alignment == UIAlignItems_Stretch)
        {
            Item->Cross = IsRow ? ContentSize.Y : ContentSize.X;
        }

        float FinalWidth  = IsRow ? Item->Main  : Item->Cross;
        float FinalHeight = IsRow ? Item->Cross : Item->Main;
        ComputeNodeBoxes(Box, Item->BorderWidth, Item->Padding, FinalWidth, FinalHeight);
    }
}

static void
PreOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    // TODO: Call IsValidX(Y)
    VOID_ASSERT(Root);
    VOID_ASSERT(Subtree);

    node_queue Queue = BeginNodeQueue({.QueueSize = Subtree->LayoutTree->NodeCount}, Subtree->Transient);
    if(Queue.Data)
    {
        style_property *RootProps = GetPaintProperties(Root->Index, 0, Subtree);

        float        BorderWidth = UIGetBorderWidth(RootProps);
        ui_padding Padding     = UIGetPadding(RootProps);
        vec2_unit  Size        = UIGetSize(RootProps);

        ComputeNodeBoxes(&Root->Value, BorderWidth, Padding, Size.X.Float32, Size.Y.Float32);

        PushNodeQueue(&Queue, Root);

        while(!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node  = PopNodeQueue(&Queue);
            style_property *Props = GetPaintProperties(Node->Index, 0, Subtree);

            UIDisplay_Type Display     = UIGetDisplay(Props);
            vec2_float       ContentSize = Node->Value.FixedContentSize;

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
                VOID_ASSERT(!"Why are you pushed?");
            } break;

            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @internal: PostOrder Measure Pass Implementation

static void
PostOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasureSubtree(Child, Subtree);
    }

    ui_layout_box  *Box   = &Root->Value;
    style_property *Props = GetPaintProperties(Root->Index, 0, Subtree);

    vec2_unit      Size        = UIGetSize(Props);
    float            BorderWidth = UIGetBorderWidth(Props);
    ui_padding     Padding     = UIGetPadding(Props);
    UIDisplay_Type Display     = UIGetDisplay(Props);

    if(VOID_HASBIT(Root->Flags, UILayoutNode_HasImage))
    {
        ui_image *Image = (ui_image *)QueryNodeResource(Root->Index, Subtree, UIResource_Image, UIState.ResourceTable);
        VOID_ASSERT(Image);

        if(Size.X.Type == UIUnit_Auto && Size.Y.Type == UIUnit_Auto)
        {
            float Width  = Image->Source.Right  - Image->Source.Left;
            float Height = Image->Source.Bottom - Image->Source.Top;

            ComputeNodeBoxes(&Root->Value, BorderWidth, Padding, Width, Height);
        }
        else
        {
            VOID_ASSERT(!"Not Implemented");
        }
    }

    if (VOID_HASBIT(Root->Flags, UILayoutNode_HasText))
    {
        ui_text *Text = (ui_text *)QueryNodeResource(Root->Index, Subtree, UIResource_Text, UIState.ResourceTable);

        float InnerAvWidth  = GetUITextSpace(Root, Size.X);
        float LineWidth     = 0.f;
        uint32_t LineWrapCount = 0;

        if (Text->ShapedCount)
        {
            for (uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
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
            float Width  = (LineWrapCount == 0) ? LineWidth : InnerAvWidth;
            float Height = (LineWrapCount + 1) * Text->LineHeight;

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

            bool MainAxisIsAuto =
                (MainAxis == UIFlexDirection_Row    && Size.X.Type == UIUnit_Auto) ||
                (MainAxis == UIFlexDirection_Column && Size.Y.Type == UIUnit_Auto);

            // NOTE:
            // I assume this may trigger possible wrapping? What about nested autos?
            // What if the main axis is auto when trying to display a flex element?
            // We might need a full recompute then? Unless we disallow?

            if(MainAxisIsAuto)
            {
                vec2_float Measured     = Child->Value.FixedOuterSize;
                float      MeasuredMain = (MainAxis == UIFlexDirection_Row) ? Measured.X : Measured.Y;

                Box->FlexBox.TotalSize += MeasuredMain;
            }
        }
    }

    if(VOID_HASBIT(Root->Flags, UILayoutNode_HasScrollRegion))
    {
        ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Root->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
        VOID_ASSERT(Region);

        float TotalWidth  = 0.f;
        float TotalHeight = 0.f;
        float MaxWidth    = 0.f;
        float MaxHeight   = 0.f;

        uint32_t VisibleCount = 0;

        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            style_property *Props = GetPaintProperties(Child->Index, 0, Subtree);

            UIDisplay_Type Display = UIGetDisplay(Props);

            if(Display != UIDisplay_None)
            {
                vec2_float ChildSize = Child->Value.FixedOuterSize;
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

            Region->ContentSize = vec2_float(TotalWidth, MaxHeight);
        } else
        if (Region->Axis == UIAxis_Y)
        {
            if(VisibleCount)
            {
                TotalHeight += (VisibleCount - 1) * Spacing.Vertical;
            }

            Region->ContentSize = vec2_float(MaxWidth, TotalHeight);
        }
    }
}

// -----------------------------------------------------------------------------------
// Layout Pass Public API implementation

// NOTE:
// Core?

static void
UpdateSubtreeState(ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    vec2_float MousePosition = OSGetMousePosition();

    ClearUIEvents(&Subtree->Events);
    GenerateUIEvents(MousePosition, Root, Subtree);
    ProcessUIEvents(&Subtree->Events, Subtree);
}

static void
ComputeSubtreeLayout(ui_subtree *Subtree)
{
    TimeFunction;

    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);
    {
        PreOrderMeasureSubtree   (Root, Subtree);
        PostOrderMeasureSubtree  (Root, Subtree);
        PreOrderPlaceSubtree     (Root, Subtree);
    }
}

static void 
PaintSubtree(ui_subtree *Subtree)
{
    TimeFunction;

    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);
    VOID_ASSERT(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    if(Root)
    {
        PaintLayoutTreeFromRoot(Root, Subtree);
    }
}
