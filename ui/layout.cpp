// [Helpers]

typedef struct resolved_unit
{
    float Value;
    bool  Resolved;
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

enum class Constraint
{
    None      = 0,
    Exact     = 1,
    AtMost    = 2,
    Unbounded = 3,
};

struct ui_layout_node
{
    // Hierarchy
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    // Output
    float ResultX;
    float ResultY;
    float ResultWidth;
    float ResultHeight;

    // Transient State
    vec2_float ScrollOffset;
    vec2_float DragOffset;
    ui_size    ConstrainedSize;
    Constraint Constraint;
    float      FreeSpaceM;

    // Static Properties
    ui_sizing       Sizing;
    ui_size         MinSize;
    ui_size         MaxSize;
    ui_padding      Padding;
    float           Spacing;
    LayoutDirection Direction;
    Alignment       AlignmentM;
    Alignment       AlignmentC;
    float           Grow;
    float           Shrink;

    // Extra Info
    uint32_t SubIndex;
    uint32_t Index;
    uint32_t ChildCount;
    uint32_t Flags;
};

typedef struct ui_layout_tree
{
    // Nodes
    uint64_t        NodeCapacity;
    uint64_t        NodeCount;
    ui_layout_node *Nodes;

    // Paint
    ui_paint_properties *PaintArray;

    // Depth
    ui_parent_stack ParentStack;
} ui_layout_tree;

static bool 
IsValidLayoutNode(ui_layout_node *Node)
{
    bool Result = Node && Node->Index != InvalidLayoutNodeIndex;
    return Result;
}

static bool
IsValidLayoutTree(ui_layout_tree *Tree)
{
    bool Result = (Tree) && (Tree->Nodes) && (Tree->NodeCount <= Tree->NodeCapacity);
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
GetLayoutRoot(ui_layout_tree *Tree)
{
    ui_layout_node *Result = Tree->Nodes;
    return Result;
}

static ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);

    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

static bool
IsValidParentStack(ui_parent_stack *Stack)
{
    bool Result = (Stack) && (Stack->Size);
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
CreateAndInsertLayoutNode(uint32_t Flags, ui_layout_tree *Tree, memory_arena *Arena)
{
    VOID_ASSERT(Tree); // Internal Corruption

    ui_layout_node *Result = GetFreeLayoutNode(Tree);
    if(Result)
    {
        Result->Flags = Flags;

        if (!IsValidParentStack(&Tree->ParentStack))
        {
            Tree->ParentStack = BeginParentStack(Tree->NodeCapacity, Arena);
        }

        Result->Last   = 0;
        Result->Next   = 0;
        Result->First  = 0;
        Result->Parent = PeekParentStack(&Tree->ParentStack);

        if (Result->Parent)
        {
            AppendToDoublyLinkedList(Result->Parent, Result, Result->Parent->ChildCount);
        }

        if (Result->Flags & UILayoutNode_IsParent)
        {
            PushParentStack(Result, &Tree->ParentStack);
        }
    }

    return Result;
}

// ==================================================================================
// @Public : Tree/Node Public API.

static rect_float
GetNodeOuterRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->ResultX, Node->ResultY) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->ResultWidth, Node->ResultHeight);
    return Result;
}

static rect_float
GetNodeInnerRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->ResultX, Node->ResultY) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->ResultWidth, Node->ResultHeight);
    return Result;
}

static rect_float
GetNodeContentRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->ResultX, Node->ResultY) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->ResultWidth, Node->ResultHeight);
    return Result;
}

static void
SetNodeProperties(uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree)
{
    // BUG: Not a safe read!

    ui_paint_properties *Paint  = Tree->PaintArray + NodeIndex;
    if(Paint)
    {
        Paint->Color        = Cached.Default.Color;
        Paint->BorderColor  = Cached.Default.BorderColor;
        Paint->TextColor    = Cached.Default.TextColor;
        Paint->CaretColor   = Cached.Default.CaretColor;
        Paint->Softness     = Cached.Default.Softness;
        Paint->BorderWidth  = Cached.Default.BorderWidth;
        Paint->CaretWidth   = Cached.Default.CaretWidth;
        Paint->CornerRadius = Cached.Default.CornerRadius;
        Paint->Font         = Cached.Default.Font;
        Paint->CachedIndex  = StyleIndex;
    }

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->Sizing     = Cached.Default.Sizing;
        Node->MinSize    = Cached.Default.MinSize;
        Node->MaxSize    = Cached.Default.MaxSize;
        Node->Padding    = Cached.Default.Padding;
        Node->Spacing    = Cached.Default.Spacing;
        Node->Direction  = Cached.Default.LayoutDirection;
        Node->AlignmentM = Cached.Default.AlignmentM;
        Node->AlignmentC = Cached.Default.AlignmentC;
        Node->Grow       = Cached.Default.Grow;
        Node->Shrink     = Cached.Default.Shrink;
    }
}

// BUG:
// Unsafe.

static ui_paint_properties *
GetPaintProperties(uint32_t NodeIndex, ui_layout_tree *Tree)
{
    ui_paint_properties *Result = Tree->PaintArray + NodeIndex;
    return Result;
}

static uint64_t
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = NodeCount * sizeof(ui_layout_node);
    uint64_t PaintSize = NodeCount * sizeof(ui_paint_properties);
    uint64_t Result    = sizeof(ui_layout_tree) + ArraySize + PaintSize;

    return Result;
}

static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        ui_layout_node      *Nodes      = static_cast<ui_layout_node      *>(Memory);
        ui_paint_properties *PaintArray = reinterpret_cast<ui_paint_properties *>(Nodes + NodeCount);

        Result = reinterpret_cast<ui_layout_tree *>(PaintArray + NodeCount);
        Result->Nodes        = Nodes;
        Result->PaintArray   = PaintArray;
        Result->NodeCount    = 0;
        Result->NodeCapacity = NodeCount;

        for (uint32_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

// -----------------------------------------------------------------------------------
// @Internal: Tree Queries

static uint32_t
UITreeFindChild(uint32_t NodeIndex, uint32_t FindIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Tree);
    if(IsValidLayoutNode(LayoutNode))
    {
        ui_layout_node *Child = LayoutNode->First;
        while(Child && FindIndex--)
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
UITreeAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);                      // Internal Corruption
    VOID_ASSERT(ParentIndex != ChildIndex); // Trying to append the node to itself

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Tree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Tree);

    if(IsValidLayoutNode(Parent) && IsValidLayoutNode(Child) && ParentIndex != ChildIndex)
    {
        AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
        Child->Parent = Parent;
    }
    else
    {
        LogError("Invalid Indices Provided | Parent = %u, Child = %u", ParentIndex, ChildIndex);
    }
}

static void
UITreeReserve(uint32_t NodeIndex, uint32_t Amount, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);   // Internal Corruption
    VOID_ASSERT(Amount); // Calling this with 0 is useless

    ui_layout_node *Parent = GetLayoutNode(NodeIndex, Tree);
    if(IsValidLayoutNode(Parent))
    {
        for(uint32_t Idx = 0; Idx < Amount; ++Idx)
        {
            ui_layout_node *Reserved = GetFreeLayoutNode(Tree);
            if(IsValidLayoutNode(Reserved))
            {
                AppendToDoublyLinkedList(Parent, Reserved, Parent->ChildCount);
            }
        }
    }

}

// NOTE:
// We surely do not want to expose this...

static void
SetLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->Flags |= Flags;
    }
}

static void
ClearLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->Flags &= ~Flags;
    }
}

static uint32_t
AllocateLayoutNode(uint32_t Flags, ui_layout_tree *Tree, memory_arena *Arena)
{

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = CreateAndInsertLayoutNode(Flags, Tree, Arena);
    if (Node)
    {
        Result = Node->Index;
    }

    return Result;
}

// BUG: This needs a nice rework. Some sort of destructor?
static void
UIEnd()
{
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
UpdateScrollNode(float ScrolledLines, ui_layout_node *Node, ui_layout_tree *Tree, ui_scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_float WindowSize     = vec2_float(Node->ResultWidth, Node->ResultHeight);

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

    rect_float WindowContent = GetNodeOuterRect(Node);
    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        // BUG: Not a safe read! Or is it?

        ui_paint_properties *Paint = Tree->PaintArray + Node->Index;
        if(Paint)
        {
            Child->ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

            vec2_float FixedContentSize = vec2_float(Child->ResultWidth, Child->ResultHeight);
            rect_float ChildContent     = GetNodeOuterRect(Child);

            if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
            {
                if (WindowContent.IsIntersecting(ChildContent))
                {
                    Child->Flags &= ~UILayoutNode_DoNotPaint;
                }
                else
                {
                    Child->Flags |= UILayoutNode_DoNotPaint;
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
IsMouseInsideOuterBox(vec2_float MousePosition, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    float Distance = 1.f;

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        vec2_float OuterSize  = vec2_float(Node->ResultWidth, Node->ResultHeight);
        vec2_float OuterPos   = vec2_float(Node->ResultX    , Node->ResultY     ) + Node->ScrollOffset;
        vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
        vec2_float Center     = OuterPos + OuterHalf;
        vec2_float LocalMouse = MousePosition - Center;

        rect_sdf_params Params = {0};
        Params.HalfSize      = OuterHalf;
        Params.PointPosition = LocalMouse;

        Distance = RoundedRectSDF(Params);
    }

    return Distance <= 0.f;
}

// BUG:
// No access to border width.

static bool
IsMouseInsideBorder(vec2_float MousePosition, ui_layout_node *Node)
{
    vec2_float InnerSize   = vec2_float(Node->ResultWidth, Node->ResultHeight) - vec2_float(0.f, 0.f);
    vec2_float InnerPos    = vec2_float(Node->ResultX    , Node->ResultY     ) - vec2_float(0.f, 0.f) + Node->ScrollOffset;
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

static FocusIntent
DetermineResizeIntent(vec2_float MousePosition, ui_layout_node *Node, ui_layout_tree *Tree)
{
    vec2_float InnerSize   = vec2_float(Node->ResultWidth, Node->ResultHeight) - vec2_float(0.f, 0.f);
    vec2_float InnerPos    = vec2_float(Node->ResultX    , Node->ResultY     ) - vec2_float(0.f, 0.f) + Node->ScrollOffset;

    float Left        = InnerPos.X;
    float Top         = InnerPos.Y;
    float Width       = InnerSize.X;
    float Height      = InnerSize.Y;
    float BorderWidth = GetPaintProperties(Node->Index, Tree)->BorderWidth; // WARN: Not safe.

    float CornerTolerance = 5.f;
    float CornerX         = Left + Width;
    float CornerY         = Top  + Height;

    rect_float RightEdge  = rect_float::FromXYWH(Left + Width, Top, BorderWidth, Height);
    rect_float BottomEdge = rect_float::FromXYWH(Left, Top + Height, Width, BorderWidth);
    rect_float Corner     = rect_float::FromXYWH(CornerX, CornerY, CornerTolerance, CornerTolerance);

    FocusIntent Result = FocusIntent::None;

    if(Corner.IsPointInside(MousePosition))
    {
        Result = FocusIntent::ResizeXY;
    } else
    if(RightEdge.IsPointInside(MousePosition))
    {
        Result = FocusIntent::ResizeX;
    } else
    if(BottomEdge.IsPointInside(MousePosition))
    {
        Result = FocusIntent::ResizeY;
    }

    return Result;
}

static void
FindHoveredNode(vec2_float MousePosition, ui_layout_node *Node, ui_layout_tree *Tree)
{
    // TODO: Re-Implement
}

static void
AttemptNodeFocus(vec2_float MousePosition, ui_layout_node *Root, ui_layout_tree *Tree)
{
    // TODO: Re-Implement
}

static void
GenerateFocusedNodeEvents(ui_event_list *Events, memory_arena *Arena)
{
    // TODO: Re-Implement
}

// WARN:
// This code is quite garbage.

static void
GenerateUIEvents(vec2_float MousePosition, ui_layout_node *Root, ui_layout_tree *Tree)
{
    // TODO: Re-Implement
}

static void
ProcessUIEvents(ui_event_list *Events, ui_layout_tree *Tree)
{
    VOID_ASSERT(Events);

    void_context &Context = GetVoidContext();

    IterateLinkedList(Events, ui_event_node *, Node)
    {
        ui_event *Event = &Node->Value;

        switch(Event->Type)
        {

        case UIEvent_Hover:
        {
            ui_hover_event Hover = Event->Hover;
            VOID_ASSERT(Hover.Node);

            // TODO: Modify the paint properties.
            // SetNodeStyleState(StyleState_Hovered, Hover.Node->Index, Tree);
        } break;

        case UIEvent_Click:
        {
        } break;

        case UIEvent_Resize:
        {
            ui_resize_event Resize = Event->Resize;

            // BUG: Not a safe read?

            ui_paint_properties *Paint = Tree->PaintArray + Resize.Node->Index;
            if(Paint)
            {
                // TODO: Reimplement this.
            }
        } break;

        // NOTE:
        // Perhaps these are animations? I do not have an animation system right now.

        case UIEvent_Drag:
        {
            ui_drag_event Drag = Event->Drag;

            ui_layout_node *Node = Drag.Node;
            VOID_ASSERT(Node);

            Node->ResultX += Drag.Delta.X;
            Node->ResultY += Drag.Delta.Y;
        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            VOID_ASSERT(Scroll.Node);
            VOID_ASSERT(Scroll.Delta);

            ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Scroll.Node->Index, Tree, UIResource_ScrollRegion, Context.ResourceTable);
            VOID_ASSERT(Region);

            UpdateScrollNode(Scroll.Delta, Scroll.Node, Tree, Region);
        } break;

        case UIEvent_Key:
        {
            ui_key_event Key = Event->Key;
            VOID_ASSERT(Key.Node);

            ui_layout_node    *Node  = Key.Node;
            ui_resource_table *Table = Context.ResourceTable;

            if((Node->Flags & UILayoutNode_HasTextInput))
            {
                ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Tree, UIResource_TextInput, Table);
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
            ui_resource_table *Table = Context.ResourceTable;

            ui_text       *Text  = (ui_text *)      QueryNodeResource(Node->Index, Tree, UIResource_Text     , Table);
            ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Tree, UIResource_TextInput, Table);

            VOID_ASSERT(Text);
            VOID_ASSERT(Input);

            if(IsValidByteString(TextInput.Text))
            {
                ui_paint_properties *Paint = GetPaintProperties(Node->Index, Tree);
                if(Paint)
                {
                    uint32_t CountBeforeAppend = Text->ShapedCount;
                    UITextAppend_(TextInput.Text, Paint->Font, Text);
                    uint32_t CountAfterAppend = Text->ShapedCount;

                    UITextInputMoveCaret_(Text, Input, CountAfterAppend - CountBeforeAppend);

                    // NOTE: BAD! IDK WHAT TO DO!
                    ByteStringAppendTo(Input->UserBuffer, TextInput.Text, Input->internalCount);
                    Input->internalCount += TextInput.Text.Size;
                }
            }

            if(Text->ShapedCount > 0)
            {
                Node->Flags |= UILayoutNode_HasText;
            }
            else
            {
                Node->Flags &= ~UILayoutNode_HasText;
            }
        } break;

        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: Text Layout Implementation

// -----------------------------------------------------------

static float
GetCursorOffsetFromAlignment(float FreeSpace, Alignment Alignment)
{
    float Offset = 0.f;

    if(Alignment == Alignment::Start)
    {
        Offset = 0.f;
    } else
    if(Alignment == Alignment::Center)
    {
        Offset = 0.5f * FreeSpace;
    } else
    if(Alignment == Alignment::End)
    {
        VOID_ASSERT(!"Idk");
        Offset = 0.f;
    }

    return Offset;
}

static void
PlaceLayoutTree(ui_layout_tree *Tree, memory_arena *Arena)
{
    VOID_ASSERT(Arena);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    ui_layout_node **LayoutBuffer = PushArray(Arena, ui_layout_node *, Tree->NodeCount);

    // NOTE: Temporary hack just to see.
    ui_layout_node *Root = GetLayoutRoot(Tree);
    Root->ResultWidth  = Root->ConstrainedSize.Width;
    Root->ResultHeight = Root->ConstrainedSize.Height;

    uint64_t VisitedNodes = 0;
    LayoutBuffer[VisitedNodes++] = Root;

    for (uint64_t Idx = 0; Idx < VisitedNodes; ++Idx)
    {
        ui_layout_node *Parent = LayoutBuffer[Idx];

        LayoutDirection Direction    = Parent->Direction;
        bool            IsHorizontal = Direction == LayoutDirection::Horizontal;

        float      Spacing = Parent->Spacing;
        ui_padding Padding = Parent->Padding;
        float      StartX  = Parent->ResultX + Padding.Left;
        float      StartY  = Parent->ResultY + Padding.Top;

        float ParentC = IsHorizontal ? Parent->ResultWidth : Parent->ResultHeight;
        float CursorM = GetCursorOffsetFromAlignment(Parent->FreeSpaceM, Parent->AlignmentM);

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            // NOTE: Temporary hack just to see.
            Child->ResultWidth  = Child->ConstrainedSize.Width;
            Child->ResultHeight = Child->ConstrainedSize.Height;

            float ChildSizeC = IsHorizontal ? Child->ResultHeight : Child->ResultWidth;
            float CursorC    = GetCursorOffsetFromAlignment((ParentC - ChildSizeC), Parent->AlignmentC);

            if (Child != Parent->First)
            {
                CursorM += Spacing;
            }

            if (IsHorizontal)
            {
                Child->ResultX = StartX + CursorM;
                Child->ResultY = StartY + CursorC;

                CursorM += Child->ResultWidth;
            }
            else
            {
                Child->ResultX = StartX + CursorC;
                Child->ResultY = StartY + CursorM;

                CursorM += Child->ResultHeight;
            }

            if (Child->ChildCount > 0)
            {
                LayoutBuffer[VisitedNodes++] = Child;
            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @internal: Layout Pass

static float
GetConstrainedSize(float ParentSize, float MinSize, float MaxSize, ui_sizing_axis Axis)
{
    float Result = Axis.Fixed;

    if(Axis.Type == Sizing::Percent)
    {
        Result = Axis.Percent * ParentSize;
    }

    Result = min(max(Result, MinSize), MaxSize);

    return Result;
}

static void
PreOrderMeasureTree(ui_layout_tree *Tree, memory_arena *Arena)
{
    VOID_ASSERT(Arena);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    void_context &Context = GetVoidContext();

    ui_layout_node **LayoutBuffer = PushArray(Arena, ui_layout_node *, Tree->NodeCount);

    ui_layout_node *Root = GetLayoutRoot(Tree);

    // BUG:
    // I'm not sure how we treat fixed node sizes. Currently roots are shrinked to min-size since Root->ResultWidth will be 0 at first iteration
    // and then Root->ResultWidth depends on the constrained size, thus unless manually resized it has no effect. So I guess we need to figure out
    // what exactly we consider to be "fixed" size.

    if(Root->Sizing.Horizontal.Type != Sizing::Percent)
    {
        Root->ConstrainedSize.Width = min(max(Root->ResultWidth, Root->MinSize.Width), Root->MaxSize.Width);
    }

    if(Root->Sizing.Vertical.Type != Sizing::Percent)
    {
        Root->ConstrainedSize.Height = min(max(Root->ResultHeight, Root->MinSize.Height), Root->MaxSize.Height);
    }

    uint64_t VisitedNodes = 0;
    LayoutBuffer[VisitedNodes++] = Root;

    for(uint64_t Idx = 0; Idx < VisitedNodes; Idx++)
    {
        ui_layout_node *Parent = LayoutBuffer[Idx];

        LayoutDirection Direction    = Parent->Direction;
        bool            IsHorizontal = Direction == LayoutDirection::Horizontal;

        float      Spacing  = Parent->Spacing;
        ui_padding Padding  = Parent->Padding;
        float      PaddingM = IsHorizontal ? Padding.Left + Padding.Right : Padding.Top + Padding.Bot;
        float      PaddingC = IsHorizontal ? Padding.Top  + Padding.Bot   : Padding.Left + Padding.Right;


        ui_size ParentSize  = Parent->ConstrainedSize;
        float   ParentSizeM = (IsHorizontal ? ParentSize.Width  : ParentSize.Height) - PaddingM;
        float   ParentSizeC = (IsHorizontal ? ParentSize.Height : ParentSize.Width ) - PaddingC;

        float InnerContentSizeM = 0.f;
        float InnerContentSizeC = 0.f;
        float TotalGrowWeight   = 0.f;
        float TotalShrinkWeight = 0.f;

        // BUG:
        // How do we correctly track the remaining free space? I think it's in the post-order pass.
        // Yeah, because at this point we don't really know how much space we have.

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            float ChildSizeM = 0.f;
            float ChildSizeC = 0.f;
            float MinWidth   = Child->MinSize.Width;
            float MaxWidth   = Child->MaxSize.Width;
            float MinHeight  = Child->MinSize.Height;
            float MaxHeight  = Child->MaxSize.Height;

            // NOTE:
            // Unsure if this is enough or should the parser just generate a maximum
            // with a FLOAT32_MAX for example? Not sure yet, but this should do the
            // trick for the time being.

            if(MaxWidth  <= 0.f) MaxWidth  = FLT_MAX;
            if(MaxHeight <= 0.f) MaxHeight = FLT_MAX;

            if(IsHorizontal)
            {
                Child->ConstrainedSize.Width  = GetConstrainedSize(ParentSizeM, MinWidth , MaxWidth , Child->Sizing.Horizontal);
                Child->ConstrainedSize.Height = GetConstrainedSize(ParentSizeC, MinHeight, MaxHeight, Child->Sizing.Vertical);

                ChildSizeM = Child->ConstrainedSize.Width;
                ChildSizeC = Child->ConstrainedSize.Height;
            }
            else
            {
                Child->ConstrainedSize.Width  = GetConstrainedSize(ParentSizeC, MinWidth , MaxWidth , Child->Sizing.Horizontal);
                Child->ConstrainedSize.Height = GetConstrainedSize(ParentSizeM, MinHeight, MaxHeight, Child->Sizing.Vertical);

                ChildSizeM = Child->ConstrainedSize.Height;
                ChildSizeC = Child->ConstrainedSize.Width;
            }

            if(Child->ChildCount > 0)
            {
                LayoutBuffer[VisitedNodes++] = Child;
            }

            InnerContentSizeM += ChildSizeM;
            TotalGrowWeight   += Child->Grow   * ChildSizeM;
            TotalShrinkWeight += Child->Shrink * ChildSizeM;

            if(Child != Parent->First)
            {
                InnerContentSizeM += Spacing;
            }

            InnerContentSizeC = max(InnerContentSizeC, ChildSizeC);
        }

        UIAxis_Type UnboundedAxis = UIAxis_None;

        if(Parent->Flags & UILayoutNode_HasScrollRegion)
        {
            VOID_ASSERT(!"Implement");
            auto *Region = static_cast<ui_scroll_region *>(QueryNodeResource(Parent->Index, 0, UIResource_ScrollRegion, Context.ResourceTable));
            if(Region)
            {
                UnboundedAxis = Region->Axis;
            }
        }

        float Epsilon = 1e-6f;

        // NOTE: The grow/shrink part is about measuring content on the main axis.
        // the only part we seem to be missing is: Sizing along the cross axis.

        if(UnboundedAxis != (IsHorizontal ? UIAxis_X : UIAxis_Y))
        {
            float FreeSpaceM = ParentSizeM - InnerContentSizeM;

            if(FreeSpaceM < 0.f && TotalShrinkWeight > 0.f)
            {
                float ToShrink = -1.f * FreeSpaceM;

                ui_layout_node **Shrinkable      = PushArray(Arena, ui_layout_node *, Parent->ChildCount);
                uint32_t         ShrinkableCount = 0;

                IterateLinkedList(Parent, ui_layout_node *, Child)
                {
                    float SizeM = IsHorizontal ? Child->ConstrainedSize.Width : Child->ConstrainedSize.Height;
                    float MinM  = IsHorizontal ? Child->MinSize.Width         : Child->MinSize.Height;

                    if(Child->Shrink > 0.f && SizeM > MinM + Epsilon)
                    {
                        Shrinkable[ShrinkableCount++] = Child;
                    }
                }

                while(ToShrink > Epsilon && ShrinkableCount > 0 && TotalShrinkWeight > 0.f)
                {
                    float IterationShrink = ToShrink;

                    for(uint32_t Idx = 0; Idx < ShrinkableCount; Idx++)
                    {
                        ui_layout_node *Child = Shrinkable[Idx];

                        float *SizePointer = IsHorizontal ? &Child->ConstrainedSize.Width : &Child->ConstrainedSize.Height;

                        float SizeM = IsHorizontal ? Child->ConstrainedSize.Width : Child->ConstrainedSize.Height;
                        float MinM  = IsHorizontal ? Child->MinSize.Width         : Child->MinSize.Height;

                        float ShrinkWeight = Child->Shrink * SizeM;
                        float ShrinkAmount = (ShrinkWeight / TotalShrinkWeight) * IterationShrink;
                        float ShrinkLimit  = SizeM - MinM;

                        if(ShrinkAmount >= ShrinkLimit)
                        {
                            *SizePointer      = MinM;
                            Child->Constraint = Constraint::Exact;

                            ToShrink          -= ShrinkLimit;
                            IterationShrink   -= ShrinkLimit;
                            TotalShrinkWeight -= ShrinkWeight;

                            Shrinkable[Idx--] = Shrinkable[--ShrinkableCount];
                        }
                        else
                        {
                            ToShrink     -= ShrinkAmount;
                            *SizePointer -= ShrinkAmount;

                            Child->Constraint = Constraint::AtMost;
                        }
                    }
                }
            } else
            if(FreeSpaceM > 0.f && TotalGrowWeight > 0.f)
            {
                float ToGrow = FreeSpaceM;

                ui_layout_node **Growable      = PushArray(Arena, ui_layout_node *, Parent->ChildCount);
                uint32_t         GrowableCount = 0;

                IterateLinkedList(Parent, ui_layout_node *, Child)
                {
                    float SizeM = IsHorizontal ? Child->ConstrainedSize.Width : Child->ConstrainedSize.Height;
                    float MaxM  = IsHorizontal ? Child->MaxSize.Width         : Child->MaxSize.Height;

                    if(Child->Grow > 0.f && SizeM < MaxM - Epsilon)
                    {
                        Growable[GrowableCount++] = Child;
                    }
                }

                while(ToGrow > Epsilon && GrowableCount > 0)
                {
                    float IterationGrow = ToGrow;

                    for(uint32_t Idx = 0; Idx < GrowableCount; Idx++)
                    {
                        ui_layout_node *Child = Growable[Idx];

                        float *SizePointer = IsHorizontal ? &Child->ConstrainedSize.Width : &Child->ConstrainedSize.Height;

                        float SizeM = IsHorizontal ? Child->ConstrainedSize.Width : Child->ConstrainedSize.Height;
                        float MaxM  = IsHorizontal ? Child->MaxSize.Width         : Child->MaxSize.Height;

                        float GrowWeight = Child->Grow * SizeM;
                        float GrowAmount = (GrowWeight / TotalGrowWeight) * IterationGrow;
                        float GrowLimit  = MaxM - SizeM;

                        if(GrowAmount >= GrowLimit)
                        {
                            *SizePointer      = MaxM;
                            Child->Constraint = Constraint::Exact;

                            ToGrow          -= GrowLimit;
                            IterationGrow   -= GrowLimit;
                            TotalGrowWeight -= GrowWeight;

                            Growable[Idx--] = Growable[--GrowableCount];
                        }
                        else
                        {
                            *SizePointer += GrowAmount;
                            ToGrow       -= GrowAmount;

                            Child->Constraint = Constraint::AtMost;
                        }
                    }
                }
            }
        }
        else
        {
            IterateLinkedList(Parent, ui_layout_node *, Child)
            {
                Child->Constraint = Constraint::Unbounded;
            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: PostOrder Measure Pass Implementation

static void
PostOrderMeasureTree(ui_layout_node *Root, ui_layout_tree *Tree)
{
}

// -----------------------------------------------------------------------------------
// Layout Pass Public API implementation

// NOTE:
// Core?

static void
UpdateTreeState(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);

    // TODO:
    // Re-Implement

}

static void
ComputeTreeLayout(ui_layout_tree *Tree)
{
    // TODO: Re-Implement
}

// NOTE:
// Why is this even a thing?

static void 
PaintTree(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);

    PaintLayoutTreeFromRoot(Tree);
}
