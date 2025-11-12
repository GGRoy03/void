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
    UIFlexDirection_Type  Direction;
    UIJustifyContent_Type Justify;
    UIAlignItems_Type     Align;

    u32 ItemCount;
    f32 TotalSize;
    f32 TotalGrow;
    f32 TotalShrink;
} ui_flex_box;

typedef struct ui_flex_item
{
    UIAlignItems_Type SelfAlign;
    f32               Size;
    f32               Grow;
    f32               Shrink;
} ui_flex_item;

typedef struct ui_layout_box
{
    // Output
    rect_f32 OuterBox;   // Box not accounting for border-width and padding (Is Drawn)
    rect_f32 InnerBox;   // Box not accounting for padding
    rect_f32 ContentBox; // Where children are placed.

    // NOTE:
    // Not sure I understand. Why don't I query this always from the style?
    // Instead of keeping it on the node?? I am confused. I guess it's just a
    // trade-off. Unsure which is better.

    // Layout Info
    f32        BorderWidth; // NOTE: Idk about this.
    ui_unit    Width;
    ui_unit    Height;
    ui_spacing Spacing;
    ui_padding Padding;

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
            Result->Nodes[Idx].Index = InvalidUINodeIndex;
        }
    }

    return Result;
}

internal ui_node
FindLayoutChild(u32 NodeIndex, u32 ChildIndex, ui_subtree *Subtree)
{
    Assert(IsValidSubtree(Subtree));

    ui_node Result = {0};

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
            Result.CanUse      = 1;
            Result.SubtreeId   = Subtree->Id;
            Result.IndexInTree = Child->Index;
        }
    }

    return Result;
}

internal void
AppendLayoutChild(u32 ParentIndex, u32 ChildIndex, ui_subtree *Subtree)
{
    Assert(IsValidSubtree(Subtree));
    Assert(ParentIndex != ChildIndex);

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Subtree->LayoutTree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Subtree->LayoutTree);

    Assert(Parent);
    Assert(Child);

    AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
    Child->Parent = Parent;
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
    Assert(Style);

    if(Style->CachedStyleIndex != InvalidCachedStyleIndex && !Style->IsLastVersion)
    {
        ui_pipeline *Pipeline = GetCurrentPipeline();
        Assert(Pipeline);

        style_property *Cached[StyleState_Count] = {0};
        Cached[StyleState_Default] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Default, Pipeline->Registry);
        Cached[StyleState_Hovered] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Hovered, Pipeline->Registry);
        Cached[StyleState_Focused] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Focused, Pipeline->Registry);

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
            ui_layout_box  *Box     = &LayoutNode->Value;
            style_property *Default = Style->Properties[StyleState_Default];
            Assert(Default);

            // NOTE:
            // I still don't know about this part. We could very well query the needed style
            // from the node's style. Unsure why we keep them here? Trying it out
            // with some attribute (TextAlign-X, TextAlign-Y)

            // Defaults
            Box->BorderWidth = UIGetBorderWidth(Default);
            Box->Width       = UIGetSize(Default).X;
            Box->Height      = UIGetSize(Default).Y;
            Box->Padding     = UIGetPadding(Default);
            Box->Spacing     = UIGetSpacing(Default);
            Box->Display     = UIGetDisplay(Default);

            // Flex
            Box->FlexBox.Direction  = UIGetFlexDirection(Default);
            Box->FlexBox.Justify    = UIGetJustifyContent(Default);
            Box->FlexBox.Align      = UIGetAlignItems(Default);
            Box->FlexItem.SelfAlign = UIGetSelfAlign(Default);
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

internal vec2_f32
GetScrollNodeTranslation(ui_scroll_region *Region)
{
    vec2_f32 Result = Vec2F32(0.f, 0.f);

    if (Region->Axis == UIAxis_X)
    {
        Result = Vec2F32(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        Result = Vec2F32(0.f, Region->ScrollOffset);
    }

    return Result;
}

internal void
UpdateScrollNode(f32 ScrolledLines, ui_layout_node *Node, ui_scroll_region *Region)
{
    f32      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_f32 WindowSize     = GetContentBoxSize(&Node->Value);

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

    vec2_f32 ScrollDelta = Vec2F32(0.f, 0.f);
    if(Region->Axis == UIAxis_X)
    {
        ScrollDelta.X = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == UIAxis_Y)
    {
        ScrollDelta.Y = -1.f * Region->ScrollOffset;
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

internal void PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree);

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
    ui_event Event = {.Type = UIEvent_Hover, .Hover.Node = Node};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIClickEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Click, .Click.Node = Node};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIScrollEvent(ui_layout_node *Node, f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Scroll, .Scroll.Node = Node, .Scroll.Delta = Delta};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIResizeEvent(ui_layout_node *Node, vec2_f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Resize, .Resize.Node = Node, .Resize.Delta = Delta};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIDragEvent(ui_layout_node *Node, vec2_f32 Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Drag, .Drag.Node = Node, .Drag.Delta = Delta};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUIKeyEvent(u32 Keycode, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_Key, .Key.Node = Node, .Key.Keycode = Keycode};
    RecordUIEvent(Event, EventList, Arena);
}

internal void
RecordUITextInputEvent(byte_string Text, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {.Type = UIEvent_TextInput, .TextInput.Node = Node, .TextInput.Text = Text};
    RecordUIEvent(Event, EventList, Arena);
}

internal b32
IsMouseInsideOuterBox(vec2_f32 MousePosition, ui_layout_node *Node)
{
    vec2_f32 OuterSize  = GetOuterBoxSize(&Node->Value);
    vec2_f32 OuterPos   = GetOuterBoxPosition(&Node->Value);
    vec2_f32 OuterHalf  = Vec2F32(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_f32 Center     = Vec2F32Add(OuterPos, OuterHalf);
    vec2_f32 LocalMouse = Vec2F32Sub(MousePosition, Center);

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    f32 Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

internal b32
IsMouseInsideBorder(vec2_f32 MousePosition, ui_layout_node *Node)
{
    vec2_f32 InnerSize   = GetInnerBoxSize(&Node->Value);
    vec2_f32 InnerPos    = GetInnerBoxPosition(&Node->Value);
    vec2_f32 InnerHalf   = Vec2F32(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_f32 InnerCenter = Vec2F32Add(InnerPos, InnerHalf);

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = Vec2F32Sub(MousePosition, InnerCenter);

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

    b32      MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    b32      MouseClicked  = OSIsMouseClicked(OSMouseButton_Left);
    vec2_f32 MousePosition = OSGetMousePosition();

    if(EventList->Focused)
    {
        ui_layout_node *Focused = EventList->Focused;

        if(MouseReleased && !HasFlag(Focused->Flags, UILayoutNode_HasTextInput))
        {
            EventList->Focused = 0;
            EventList->Intent  = UIIntent_None;
        } else
        if(MouseClicked && !IsMouseInsideOuterBox(MousePosition, EventList->Focused))
        {
            EventList->Focused = 0;
            EventList->Intent  = UIIntent_None;
        }
    }
}

internal UIIntent_Type
DetermineResizeIntent(vec2_f32 MousePosition, ui_layout_node *Node)
{
    vec2_f32 InnerSize = GetInnerBoxSize(&Node->Value);
    vec2_f32 InnerPos  = GetInnerBoxPosition(&Node->Value);

    f32 Left        = InnerPos.X;
    f32 Top         = InnerPos.Y;
    f32 Width       = InnerSize.X;
    f32 Height      = InnerSize.Y;
    f32 BorderWidth = Node->Value.BorderWidth;

    f32 CornerTolerance = 5.f;
    f32 CornerX         = Left + Width;
    f32 CornerY         = Top  + Height;

    rect_f32 RightEdge  = RectF32(Left + Width, Top, BorderWidth, Height);
    rect_f32 BottomEdge = RectF32(Left, Top + Height, Width, BorderWidth);
    rect_f32 Corner     = RectF32(CornerX, CornerY, CornerTolerance, CornerTolerance);

    UIIntent_Type Result = UIIntent_None;

    if(IsPointInRect(Corner, MousePosition))
    {
        Result = UIIntent_ResizeXY;
    } else
    if(IsPointInRect(RightEdge, MousePosition))
    {
        Result = UIIntent_ResizeX;
    } else
    if(IsPointInRect(BottomEdge, MousePosition))
    {
        Result = UIIntent_ResizeY;
    }

    return Result;
}

internal b32
AttemptNodeFocus(vec2_f32 MousePosition, ui_layout_node *Root, memory_arena *Arena, ui_event_list *Result)
{
    if(!IsMouseInsideOuterBox(MousePosition, Root))
    {
        return 0;
    }

    IterateLinkedListBackward(Root, ui_layout_node *, Child)
    {
        b32 ChildCaptured = AttemptNodeFocus(MousePosition, Child, Arena, Result);
        if(ChildCaptured)
        {
            return 1;
        }
    }

    b32 MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

    if(HasFlag(Root->Flags, UILayoutNode_IsResizable) && MouseClicked)
    {
        if(IsMouseInsideBorder(MousePosition, Root))
        {
            UIIntent_Type Intent = DetermineResizeIntent(MousePosition, Root);
            if(Intent != UIIntent_None)
            {
                Result->Focused = Root;
                Result->Intent  = Intent;
                return 1;
            }
        }
    }

    if(HasFlag(Root->Flags, UILayoutNode_IsDraggable) && MouseClicked)
    {
        Result->Focused = Root;
        Result->Intent  = UIIntent_Drag;
        return 1;
    }

    if(MouseClicked)
    {
        if(HasFlag(Root->Flags, UILayoutNode_HasTextInput))
        {
            Result->Focused = Root;
            Result->Intent  = UIIntent_EditTextInput;
        }

        RecordUIClickEvent(Root, Result, Arena);
        return 1;
    }

    // NOTE:
    // Should this take focus?

    f32 ScrollDelta = OSGetScrollDelta();
    if(HasFlag(Root->Flags, UILayoutNode_HasScrollRegion) && ScrollDelta != 0.f)
    {
        RecordUIScrollEvent(Root, ScrollDelta, Result, Arena);
    }

    RecordUIHoverEvent(Root, Result, Arena);

    return 0;
}

internal void
GenerateFocusedNodeEvents(ui_event_list *Events, memory_arena *Arena)
{
    Assert(Events->Focused);

    vec2_f32 MouseDelta = OSGetMouseDelta();
    b32      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

    if(MouseMoved && Events->Intent >= UIIntent_ResizeX && Events->Intent <= UIIntent_ResizeXY)
    {
        vec2_f32 Delta = Vec2F32(0.f, 0.f);

        if(Events->Intent == UIIntent_ResizeX)
        {
            Delta.X = MouseDelta.X;
        } else
        if(Events->Intent == UIIntent_ResizeY)
        {
            Delta.Y = MouseDelta.Y;
        } else
        if(Events->Intent == UIIntent_ResizeXY)
        {
            Delta = MouseDelta;
        }

        RecordUIResizeEvent(Events->Focused, Delta, Events, Arena);
    } else
    if(MouseMoved && Events->Intent == UIIntent_Drag)
    {
        RecordUIDragEvent(Events->Focused, MouseDelta, Events, Arena);
    } else
    if(Events->Intent == UIIntent_EditTextInput)
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
                RecordUIKeyEvent(Key, Events->Focused, Events, Arena);
            }
        }

        os_utf8_playback *Text = OSGetTextPlayback();
        Assert(Text);

        if(Text->Count)
        {
            byte_string TextAsString = ByteString(Text->UTF8, Text->Count);
            RecordUITextInputEvent(TextAsString, Events->Focused, Events, Arena);
        }
    }
}

internal void
GenerateUIEvents(vec2_f32 MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    ui_event_list *Events = &Subtree->Events;
    memory_arena  *Arena  = Subtree->Transient;
    Assert(Arena);

    if(!Events->Focused)
    {
        AttemptNodeFocus(MousePosition, Root, Arena, Events);
    }

    if(Events->Focused)
    {
        SetNodeStyleState(StyleState_Focused, Events->Focused->Index, Subtree);

        GenerateFocusedNodeEvents(Events, Arena);
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

        case UIEvent_Drag:
        {
            ui_drag_event Drag = Event->Drag;
            Drag.Node->Value.VisualOffset = Vec2F32Add(Drag.Node->Value.VisualOffset, Drag.Delta);

        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            Assert(Scroll.Node);
            Assert(Scroll.Delta);

            ui_scroll_region *Region = QueryScrollRegion(Scroll.Node->Index, Subtree, UIState.ResourceTable);
            Assert(Region);

            UpdateScrollNode(Scroll.Delta, Scroll.Node, Region);
        } break;

        case UIEvent_Key:
        {
            ui_key_event Key = Event->Key;
            Assert(Key.Node);

            ui_layout_node    *Node  = Key.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            if(HasFlag(Node->Flags, UILayoutNode_HasTextInput))
            {
                ui_text_input *Input = QueryTextInputResource(Node->Index, Subtree, Table);
                Assert(Input);

                if(Input->OnKey)
                {
                    Input->OnKey(Key.Keycode, Input->OnKeyUserData);
                }
            }
        } break;

        case UIEvent_TextInput:
        {
            ui_text_input_event TextInput = Event->TextInput;
            Assert(TextInput.Node);

            ui_layout_node    *Node  = TextInput.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            ui_text       *Text  = QueryTextResource(Node->Index, Subtree, Table);
            ui_text_input *Input = QueryTextInputResource(Node->Index, Subtree, Table);

            Assert(Text);
            Assert(Input);

            if(IsValidByteString(TextInput.Text))
            {
                ui_node_style *Style = GetNodeStyle(Node->Index, Subtree);
                ui_font       *Font  = UIGetFont(Style->Properties[StyleState_Default]);
                UITextAppend_(TextInput.Text, Font, Text);

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
GetUITextOffset(UIAlign_Type Align, f32 ContentSize, f32 TextSize)
{
    f32 Result = 0.f;

    if(Align == UIAlign_Center)
    {
        Result = (ContentSize - TextSize) * 0.5f;
    } else
    if(Align == UIAlign_End)
    {
        Result = (ContentSize - TextSize);
    }

    return Result;
}

internal f32
GetUITextSpace(ui_layout_node *Node)
{
    Assert(Node);

    ui_layout_box *Box = &Node->Value;

    f32 Result = GetContentBoxSize(Box).X;

    if(Box->Width.Type == UIUnit_Auto)
    {
        ui_layout_node *Parent = Node->Parent;
        Result = GetContentBoxSize(&Parent->Value).X;
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
            Glyphs[Idx].Position.Min.X += XAlignOffset;
            Glyphs[Idx].Position.Max.X += XAlignOffset;
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

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

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
        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            ui_layout_node *Node  = PopNodeQueue(&Queue);
            ui_layout_box  *Box   = &Node->Value;
            ui_node_style  *Style = GetNodeStyle(Node->Index, Subtree);

            vec2_f32 ContentPos  = GetContentBoxPosition(Box);
            vec2_f32 ContentSize = GetContentBoxSize(Box);

            vec2_f32 Cursor    = ContentPos;
            vec2_f32 MaxCursor = Vec2F32Add(Cursor, ContentSize);

            if(HasFlag(Node->Flags, UILayoutNode_HasScrollRegion))
            {
                ui_scroll_region *Region = QueryScrollRegion(Node->Index, Subtree, Table);
                Assert(Region);

                if(Region->Axis == UIAxis_X)
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
                UIFlexDirection_Type Direction = Box->FlexBox.Direction;

                f32 MainSize  = Direction == UIFlexDirection_Row ? ContentSize.X : ContentSize.Y;
                f32 CrossSize = Direction == UIFlexDirection_Row ? ContentSize.Y : ContentSize.X;
                f32 FreeSpace = MainSize - Box->FlexBox.TotalSize;

                f32 MainAxisSpacing  = Direction == UIFlexDirection_Row ? Box->Spacing.Horizontal : Box->Spacing.Vertical;
                f32 CrossAxisSpacing = Direction == UIFlexDirection_Row ? Box->Spacing.Vertical   : Box->Spacing.Horizontal;

                f32 MainAxisCursor      = Direction == UIFlexDirection_Row ? Cursor.X : Cursor.Y;
                f32 CrossAxisCursorBase = Direction == UIFlexDirection_Row ? Cursor.Y : Cursor.X;

                switch(Box->FlexBox.Justify)
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

                    vec2_f32 ChildSize          = GetOuterBoxSize(CBox);
                    f32      MainAxisChildSize  = Direction == UIFlexDirection_Row ? ChildSize.X : ChildSize.Y;
                    f32      CrossAxisChildSize = Direction == UIFlexDirection_Row ? ChildSize.Y : ChildSize.X;

                    f32 CrossAxisCursor = CrossAxisCursorBase;

                    UIAlignItems_Type Align = GetChildFlexAlignment(Box, CBox);
                    switch(Align)
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
                        CBox->VisualOffset = Vec2F32(MainAxisCursor, CrossAxisCursor);
                    } else
                    if(Direction == UIFlexDirection_Column)
                    {
                        CBox->VisualOffset = Vec2F32(CrossAxisCursor, MainAxisCursor);
                    }

                    MainAxisCursor += MainAxisChildSize + MainAxisSpacing;

                    PushNodeQueue(&Queue, Child);
                }

                Useless(CrossAxisSpacing);
            }

            if(Box->Display == UIDisplay_Normal)
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    ui_layout_box *CBox  = &Child->Value;
                    vec2_f32       CSize = GetOuterBoxSize(CBox);

                    CBox->VisualOffset = Vec2F32(Cursor.X, Cursor.Y);

                    Cursor.Y += CSize.Y;

                    PushNodeQueue(&Queue, Child);
                }
            }

            if(HasFlag(Node->Flags, UILayoutNode_HasText))
            {
                ui_text *Text = QueryTextResource(Node->Index, Subtree, Table);

                UIAlign_Type XAlign = UIGetXTextAlign(Style->Properties[StyleState_Default]);
                UIAlign_Type YAlign = UIGetYTextAlign(Style->Properties[StyleState_Default]);

                f32 WrapWidth   = GetUITextSpace(Node);
                f32 LineWidth   = 0.f;
                f32 LineStartX  = Cursor.X;
                f32 LineStartY  = Cursor.Y + GetUITextOffset(YAlign, ContentSize.Y, Text->TotalHeight);

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

                    vec2_f32 Position = Vec2F32(LineStartX + LineWidth, LineStartY);
                    Shaped->Position.Min.X = Position.X + Shaped->Offset.X;
                    Shaped->Position.Min.Y = Position.Y + Shaped->Offset.Y;
                    Shaped->Position.Max.X = Shaped->Position.Min.X + Shaped->Size.X;
                    Shaped->Position.Max.Y = Shaped->Position.Min.Y + Shaped->Size.Y;

                    LineWidth += Shaped->AdvanceX;
                }

                u32              LineCount = Text->ShapedCount - LineStartIdx;
                ui_shaped_glyph *LineStart = Text->Shaped + LineStartIdx;
                AlignUITextLine(WrapWidth, LineWidth, XAlign, LineStart, LineCount);
            }

        }
    }
}

internal void
PreOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    Assert(Root);
    Assert(Subtree);
    Assert(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    Assert(Tree);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->Transient);

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

            ui_layout_box *Box         = &Node->Value;
            vec2_f32       ContentSize = GetContentBoxSize(Box);

            if(Box->Display == UIDisplay_Normal)
            {
                // TODO:
                // Implement this.
            }

            if(Box->Display == UIDisplay_Flex)
            {
                Box->FlexBox.TotalGrow   = 0.f;
                Box->FlexBox.TotalShrink = 0.f;
                Box->FlexBox.ItemCount   = 0;
                Box->FlexBox.TotalSize   = 0;

                UIFlexDirection_Type MainAxis   = Box->FlexBox.Direction;
                f32                  MainAvSize = MainAxis == UIFlexDirection_Row ? ContentSize.X : ContentSize.Y;

                f32 SpaceNeeded = 0.f;
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    UpdateNodeIfNeeded(Child->Index, Subtree);

                    if(Child->Value.Display != UIDisplay_None)
                    {
                        ui_layout_box  *CBox  = &Child->Value;
                        style_property *Props = GetPaintProperties(Child->Index, 0, Subtree);

                        resolved_unit Width  = TryConvertUnitToFloat(CBox->Width , ContentSize.X);
                        resolved_unit Height = TryConvertUnitToFloat(CBox->Height, ContentSize.Y);

                        b32 MainSizeResolved = (MainAxis == UIFlexDirection_Row) ? Width.Resolved : Height.Resolved;
                        if(MainSizeResolved)
                        {
                            if(MainAxis == UIFlexDirection_Row)
                            {
                                CBox->FlexItem.Size = Width.Value;
                            }
                            else if(MainAxis == UIFlexDirection_Column)
                            {
                                CBox->FlexItem.Size = Height.Value;
                            }

                            ComputeNodeBoxes(Child, Width.Value, Height.Value);
                        }
                        else
                        {
                            if(MainAxis == UIFlexDirection_Row)
                            {
                                ComputeNodeBoxes(Child, 0.f, Height.Value);
                            }
                            else if(MainAxis == UIFlexDirection_Column)
                            {
                                ComputeNodeBoxes(Child, Width.Value, 0.f);
                            }

                            CBox->FlexItem.Size = 10.f;
                        }

                        f32 Grow   = UIGetFlexGrow(Props);
                        f32 Shrink = UIGetFlexShrink(Props);

                        CBox->FlexItem.Grow   = Grow;
                        CBox->FlexItem.Shrink = Shrink;

                        Box->FlexBox.TotalGrow   += Grow;
                        Box->FlexBox.TotalShrink += Shrink * CBox->FlexItem.Size;
                        Box->FlexBox.ItemCount   += 1;

                        SpaceNeeded += CBox->FlexItem.Size;

                        PushNodeQueue(&Queue, Child);
                    }
                }

                if(Box->FlexBox.ItemCount)
                {
                    f32 Spacing       = MainAxis == UIFlexDirection_Row ? Box->Spacing.Horizontal : Box->Spacing.Vertical;
                    f32 SpacingAmount = (Box->FlexBox.ItemCount - 1) * Spacing;
                    f32 FreeSpace     = MainAvSize - (SpaceNeeded + SpacingAmount);

                    Box->FlexBox.TotalSize += SpacingAmount;

                    if(FreeSpace > 0 && Box->FlexBox.TotalGrow > 0.f)
                    {
                        IterateLinkedList(Node, ui_layout_node *, Child)
                        {
                            ui_layout_box *CBox = &Child->Value;
                            if(Child->Value.Display != UIDisplay_None)
                            {
                                f32 Grow      = CBox->FlexItem.Grow;
                                f32 GrowRatio = Grow / Box->FlexBox.TotalGrow;

                                CBox->FlexItem.Size += GrowRatio * FreeSpace;
                            }
                        }
                    }
                    else if(FreeSpace < 0 && Box->FlexBox.TotalShrink > 0.f)
                    {
                        IterateLinkedList(Node, ui_layout_node *, Child)
                        {
                            ui_layout_box *CBox = &Child->Value;
                            if(Child->Value.Display != UIDisplay_None)
                            {
                                f32 Shrink       = CBox->FlexItem.Shrink;
                                f32 ShrinkWeight = Shrink * CBox->FlexItem.Size;
                                f32 ShrinkRatio  = ShrinkWeight / Box->FlexBox.TotalShrink;

                                CBox->FlexItem.Size += ShrinkRatio * FreeSpace;
                            }
                        }
                    }

                    f32 CrossAvSize = MainAxis == UIFlexDirection_Row ? ContentSize.Y : ContentSize.X;
                    IterateLinkedList(Node, ui_layout_node *, Child)
                    {
                        ui_layout_box *CBox = &Child->Value;
                        if(CBox->Display != UIDisplay_None)
                        {
                            f32 FinalWidth  = 0.f;
                            f32 FinalHeight = 0.f;

                            UIAlignItems_Type Align = GetChildFlexAlignment(Box, CBox);
                            if(MainAxis == UIFlexDirection_Row)
                            {
                                FinalWidth  = CBox->FlexItem.Size;
                                FinalHeight = Align == UIAlignItems_Stretch ? CrossAvSize : GetOuterBoxSize(CBox).Y;

                                Box->FlexBox.TotalSize += FinalWidth;
                            }
                            else if(MainAxis == UIFlexDirection_Column)
                            {
                                FinalWidth  = Align == UIAlignItems_Stretch ? CrossAvSize : GetOuterBoxSize(CBox).X;
                                FinalHeight = CBox->FlexItem.Size;

                                Box->FlexBox.TotalSize += FinalHeight;
                            }

                            ComputeNodeBoxes(Child, FinalWidth, FinalHeight);
                        }
                    }
                }
            }
            else
            {
                IterateLinkedList(Node, ui_layout_node *, Child)
                {
                    UpdateNodeIfNeeded(Child->Index, Subtree);

                    Box = &Child->Value;

                    resolved_unit Width  = TryConvertUnitToFloat(Box->Width , ContentSize.X);
                    resolved_unit Height = TryConvertUnitToFloat(Box->Height, ContentSize.Y);

                    if(Box->Display != UIDisplay_None)
                    {
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
}

internal void
PostOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasureSubtree(Child, Subtree);
    }

    ui_layout_box *Box = &Root->Value;

    if (HasFlag(Root->Flags, UILayoutNode_HasText))
    {
        ui_text *Text = QueryTextResource(Root->Index, Subtree, UIState.ResourceTable);

        f32 InnerAvWidth  = GetUITextSpace(Root);
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

        if (Box->Width.Type == UIUnit_Auto)
        {
            f32 Width  = (LineWrapCount == 0) ? LineWidth : InnerAvWidth;
            f32 Height = (LineWrapCount + 1) * Text->LineHeight;

            ComputeNodeBoxes(Root, Width, Height);
        }
    }

    if(Box->Display == UIDisplay_Flex)
    {
        // NOTE:
        // I have to somehow handle auto sized elements. So... Is it like:
        // If parent is flex and contains auto elements, then find it/them.
        // And recompute parent if needed? Because, otherwise it thinks auto sized
        // elements have 0 width. I assume the reasons it works for console
        // outputs right now is that: There is no centering. I don't get how auto
        // interacts with other sizes?
    }

    if(HasFlag(Root->Flags, UILayoutNode_HasScrollRegion))
    {
        ui_scroll_region *Region = QueryScrollRegion(Root->Index, Subtree, UIState.ResourceTable);
        Assert(Region);

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
        ui_spacing     Spacing = UIGetSpacing(Style->Properties[StyleState_Default]);

        if (Region->Axis == UIAxis_X)
        {
            if(VisibleCount)
            {
                TotalWidth += (VisibleCount - 1) * Spacing.Horizontal;
            }

            Region->ContentSize = Vec2F32(TotalWidth, MaxHeight);
        } else
        if (Region->Axis == UIAxis_Y)
        {
            if(VisibleCount)
            {
                TotalHeight += (VisibleCount - 1) * Spacing.Vertical;
            }

            Region->ContentSize = Vec2F32(MaxWidth, TotalHeight);
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
