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

enum class Constraint
{
    None      = 0,
    Exact     = 1,
    AtMost    = 2,
    Unbounded = 3,
};

struct ui_size_bounds
{
    float Min;
    float Max;
};

enum class LayoutNodeFlag
{
    None                 = 0,
    UseHoveredStyle = 1 << 0,
    UseFocusedStyle = 1 << 1,
};

inline LayoutNodeFlag operator|(LayoutNodeFlag A, LayoutNodeFlag B)   {return static_cast<LayoutNodeFlag>(static_cast<int>(A) | static_cast<int>(B));}
inline LayoutNodeFlag operator&(LayoutNodeFlag A, LayoutNodeFlag B)   {return static_cast<LayoutNodeFlag>(static_cast<int>(A) & static_cast<int>(B));}
inline LayoutNodeFlag operator|=(LayoutNodeFlag& A, LayoutNodeFlag B) {return A = A | B;}
inline LayoutNodeFlag operator&=(LayoutNodeFlag& A, LayoutNodeFlag B) {return A = A & B;}
inline LayoutNodeFlag operator~(LayoutNodeFlag A)                     {return static_cast<LayoutNodeFlag>(~static_cast<int>(A));}

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
    float      MajorSize;
    float      MinorSize;
    Constraint Constraint;

    // Static Properties
    ui_sizing_axis  MinorSizing;
    ui_sizing_axis  MajorSizing;
    Alignment       MinorAlign;
    Alignment       MajorAlign;
    ui_size_bounds  MinorBounds;
    ui_size_bounds  MajorBounds;
    ui_padding      Padding;
    float           Spacing;
    LayoutDirection Direction;
    float           Grow;
    float           Shrink;

    // Extra Info
    uint32_t       Index;
    uint32_t       StyleIndex;
    uint32_t       ChildCount;
    LayoutNodeFlag Flags;
    uint32_t       LegacyFlags;
};

struct ui_parent_node
{
    ui_parent_node *Prev;
    uint32_t        Index;
};

struct ui_parent_list
{
    ui_parent_node *First;
    ui_parent_node *Last;
    uint32_t        Count;
};

typedef struct ui_layout_tree
{
    uint64_t          NodeCapacity;
    uint64_t          NodeCount;
    ui_layout_node   *Nodes;

    ui_paint_command *PaintBuffer;
    ui_parent_list    ParentList;
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
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        bool           IsXMajor = (Cached.Default.Direction == LayoutDirection::Horizontal);
        ui_size_bounds BoundsX  = {Cached.Default.MinSize.Width , Cached.Default.MaxSize.Width};
        ui_size_bounds BoundsY  = {Cached.Default.MinSize.Height, Cached.Default.MaxSize.Height};

        Node->MinorSizing = IsXMajor ? Cached.Default.SizingY : Cached.Default.SizingX;
        Node->MajorSizing = IsXMajor ? Cached.Default.SizingX : Cached.Default.SizingY;
        Node->MinorAlign  = IsXMajor ? Cached.Default.AlignY  : Cached.Default.AlignX;
        Node->MajorAlign  = IsXMajor ? Cached.Default.AlignX  : Cached.Default.AlignY;
        Node->MinorBounds = IsXMajor ? BoundsY                : BoundsX;
        Node->MajorBounds = IsXMajor ? BoundsX                : BoundsY;
        Node->Padding     = Cached.Default.Padding;
        Node->Spacing     = Cached.Default.Spacing;
        Node->Direction   = Cached.Default.Direction;
        Node->Grow        = Cached.Default.Grow;
        Node->Shrink      = Cached.Default.Shrink;

        Node->StyleIndex = StyleIndex;
    }
}

static uint64_t
GetLayoutTreeAlignment(void)
{
    uint64_t Result = AlignOf(ui_layout_node);
    return Result;
}

static uint64_t
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = NodeCount * sizeof(ui_layout_node);
    uint64_t PaintSize = NodeCount * sizeof(ui_paint_command);
    uint64_t Result    = sizeof(ui_layout_tree) + ArraySize + PaintSize;

    return Result;
}

static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        ui_layout_node   *Nodes      = static_cast<ui_layout_node*>(Memory);
        ui_paint_command *PaintBuffer = reinterpret_cast<ui_paint_command *>(Nodes + NodeCount);

        Result = reinterpret_cast<ui_layout_tree *>(PaintBuffer + NodeCount);
        Result->Nodes        = Nodes;
        Result->PaintBuffer  = PaintBuffer;
        Result->NodeCount    = 0;
        Result->NodeCapacity = NodeCount;

        for (uint64_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

static uint32_t
AllocateLayoutNode(uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = GetFreeLayoutNode(Tree);
    if(Node)
    {
        Node->LegacyFlags = Flags;
        Node->Last        = 0;
        Node->Next        = 0;
        Node->First       = 0;

        if(Tree->ParentList.Last)
        {
            Node->Parent = GetLayoutNode(Tree->ParentList.Last->Index, Tree);

            AppendToDoublyLinkedList(Node->Parent, Node, Node->Parent->ChildCount);
        }

        Result = Node->Index;
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
// We surely do not want to expose this... I still don't know. That's not really
// how flags work honestly, an external system shouldn't even know about these flags
// imo. Sooo. How do fix this. They are only used when setting resources, perhaps
// we change how queries work? Yeah something seems to be wrong.

static void
SetLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->LegacyFlags |= Flags;
    }
}

static void
ClearLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->LegacyFlags &= ~Flags;
    }
}

static bool
PushLayoutParent(uint32_t Index, ui_layout_tree *Tree, memory_arena *Arena)
{
    ui_parent_node *Node = PushStruct(Arena, ui_parent_node);
    if(Node)
    {
        ui_parent_list &List = Tree->ParentList;

        if(!List.First)
        {
            List.First = Node;
        }

        Node->Index = Index;
        Node->Prev  = List.Last;

        List.Last   = Node;
        List.Count += 1;
    }

    bool Result = Node != 0;
    return Result;
}

static bool
PopLayoutParent(uint32_t Index, ui_layout_tree *Tree)
{
    bool Result = false;

    if(Tree->ParentList.Last && Index == Tree->ParentList.Last->Index)
    {
        ui_parent_list &List = Tree->ParentList;
        ui_parent_node *Node = List.Last;

        if(!Node->Prev)
        {
            List.First = 0;
        }

        List.Last   = Node->Prev;
        List.Count -= 1;
    }

    return Result;
}

// -----------------------------------------------------------
// UI Scrolling internal Implementation

typedef struct ui_scroll_region
{
    vec2_float  ContentSize;
    float       ScrollOffset;
    float       PixelPerLine;
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
        Child->ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

        vec2_float FixedContentSize = vec2_float(Child->ResultWidth, Child->ResultHeight);
        rect_float ChildContent     = GetNodeOuterRect(Child);

        if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
        {
            if (WindowContent.IsIntersecting(ChildContent))
            {
                Child->LegacyFlags &= ~UILayoutNode_DoNotPaint;
            }
            else
            {
                Child->LegacyFlags |= UILayoutNode_DoNotPaint;
            }
        }
    }
}

static bool
IsMouseInsideOuterBox(vec2_float MousePosition, ui_layout_node *Node)
{
    VOID_ASSERT(Node); // Internal Corruption

    vec2_float OuterSize  = vec2_float(Node->ResultWidth, Node->ResultHeight);
    vec2_float OuterPos   = vec2_float(Node->ResultX    , Node->ResultY     ) + Node->ScrollOffset;
    vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_float Center     = OuterPos + OuterHalf;
    vec2_float LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    float Distance = RoundedRectSDF(Params);
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
ConsumePointerEvents(uint32_t NodeIndex, ui_layout_tree *Tree, pointer_event_list *EventList)
{
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);

    if(Node)
    {
        IterateLinkedList(Node, ui_layout_node *, Child)
        {
            ConsumePointerEvents(Child->Index, Tree, EventList);
        }

        IterateLinkedList(EventList, pointer_event_node *, EventNode)
        {
            pointer_event &Event = EventNode->Value;

            switch(Event.Type)
            {

            case PointerEvent::Hover:
            {
                if(IsMouseInsideOuterBox(Event.Position, Node))
                {
                    Node->Flags |= LayoutNodeFlag::UseHoveredStyle;

                    // WARN: Direct mutation over what we are iterating. Perhaps we want to prune after the iteration?
                    // Is it even dangerous here since there is no re-allocations?

                    ConsumePointerEvent(EventNode, EventList);
                }
            } break;

            default:
            {
            } break;

            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: Layout Pass Helpers

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

// ----------------------------------------------------------------------------------
// @Public: Layout Pass

// BUG: Seems like shrink is not correctly implemented. The most simple case simply bleeds out.

static void
PreOrderMeasureTree(ui_layout_tree *Tree, memory_arena *Arena)
{
    VOID_ASSERT(Arena);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    void_context &Context = GetVoidContext();

    ui_layout_node **LayoutBuffer = PushArray(Arena, ui_layout_node *, Tree->NodeCount);
    ui_layout_node  *Root         = GetLayoutRoot(Tree);

    if(Root->MajorSizing.Type != Sizing::Percent)
    {
        Root->MajorSize = min(max(Root->ResultWidth , Root->MajorBounds.Min), Root->MajorBounds.Max);
    }

    if(Root->MinorSizing.Type != Sizing::Percent)
    {
        Root->MinorSize = min(max(Root->ResultHeight, Root->MinorBounds.Min), Root->MajorBounds.Max);
    }

    uint64_t VisitedNodes = 0;
    LayoutBuffer[VisitedNodes++] = Root;

    for(uint64_t Idx = 0; Idx < VisitedNodes; Idx++)
    {
        ui_layout_node *Parent = LayoutBuffer[Idx];

        LayoutDirection Direction    = Parent->Direction;
        bool            IsHorizontal = Direction == LayoutDirection::Horizontal;

        float MajorPadding = IsHorizontal ? Parent->Padding.Left + Parent->Padding.Right : Parent->Padding.Top  + Parent->Padding.Bot;
        float MinorPadding = IsHorizontal ? Parent->Padding.Top  + Parent->Padding.Bot   : Parent->Padding.Left + Parent->Padding.Right;

        float MajorInnerParentSize = Parent->MajorSize - MajorPadding;
        float MinorInnerParentSize = Parent->MinorSize - MinorPadding;

        float InnerContentSizeM = 0.f;
        float InnerContentSizeC = 0.f;
        float TotalGrowWeight   = 0.f;
        float TotalShrinkWeight = 0.f;

        // TODO:
        // Lacking the minor axis sizing? Unsure really.

        // BUG:
        // How do we correctly track the remaining free space? I think it's in the post-order pass.
        // Yeah, because at this point we don't really know how much space we have.

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            Child->MinorSize = GetConstrainedSize(MinorInnerParentSize, Child->MinorBounds.Min, Child->MinorBounds.Max, Child->MinorSizing);
            Child->MajorSize = GetConstrainedSize(MajorInnerParentSize, Child->MajorBounds.Min, Child->MajorBounds.Max, Child->MajorSizing);

            if(Child->ChildCount > 0)
            {
                LayoutBuffer[VisitedNodes++] = Child;
            }

            InnerContentSizeM += Child->MajorSize;
            TotalGrowWeight   += Child->Grow   * Child->MajorSize; // NOTE: Should this be weighted?
            TotalShrinkWeight += Child->Shrink * Child->MajorSize;

            if(Child != Parent->First)
            {
                InnerContentSizeM += Parent->Spacing;
            }

            InnerContentSizeC = max(InnerContentSizeC, Child->MinorSize);
        }

        UIAxis_Type UnboundedAxis = UIAxis_None;

        if(Parent->LegacyFlags & UILayoutNode_HasScrollRegion)
        {
            VOID_ASSERT(!"Implement");
            auto *Region = static_cast<ui_scroll_region *>(QueryNodeResource(Parent->Index, 0, UIResource_ScrollRegion, Context.ResourceTable));
            if(Region)
            {
                UnboundedAxis = Region->Axis;
            }
        }

        float Epsilon = 1e-6f;

        if(UnboundedAxis != (IsHorizontal ? UIAxis_X : UIAxis_Y))
        {
            float FreeSpaceM = MajorInnerParentSize - InnerContentSizeM;

            if(FreeSpaceM < 0.f && TotalShrinkWeight > 0.f)
            {
                float ToShrink = -1.f * FreeSpaceM;

                ui_layout_node **Shrinkable      = PushArray(Arena, ui_layout_node *, Parent->ChildCount);
                uint32_t         ShrinkableCount = 0;

                IterateLinkedList(Parent, ui_layout_node *, Child)
                {
                    if(Child->Shrink > 0.f && Child->MajorSize > Child->MajorBounds.Min + Epsilon)
                    {
                        Shrinkable[ShrinkableCount++] = Child;
                    }
                }

                // BUG:
                // We also need to remove some of shrink weight.
                // To compute the shrink weight we'd need the base size which we overwrite in this loop.

                while(ToShrink > Epsilon && ShrinkableCount > 0 && TotalShrinkWeight > 0.f)
                {
                    float IterationShrink = ToShrink;

                    for(uint32_t Idx = 0; Idx < ShrinkableCount; Idx++)
                    {
                        ui_layout_node *Child = Shrinkable[Idx];

                        float ShrinkWeight = Child->Shrink * Child->MajorSize;
                        float ShrinkAmount = (ShrinkWeight / TotalShrinkWeight) * IterationShrink;
                        float ShrinkLimit  = Child->MajorSize - Child->MajorBounds.Min;

                        if(ShrinkAmount >= ShrinkLimit)
                        {
                            Child->MajorSize  = Child->MajorBounds.Min;
                            Child->Constraint = Constraint::Exact;

                            ToShrink          -= ShrinkLimit;
                            IterationShrink   -= ShrinkLimit;
                            TotalShrinkWeight -= ShrinkWeight;

                            Shrinkable[Idx--] = Shrinkable[--ShrinkableCount];
                        }
                        else
                        {
                            ToShrink          -= ShrinkAmount;
                            Child->MajorSize  -= ShrinkAmount;
                            Child->Constraint  = Constraint::AtMost;
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
                    if(Child->Grow > 0.f && Child->MajorSize < Child->MajorBounds.Max - Epsilon)
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

                        float GrowWeight = Child->Grow * Child->MajorSize;
                        float GrowAmount = (GrowWeight / TotalGrowWeight) * IterationGrow;
                        float GrowLimit  = Child->MajorBounds.Max - Child->MajorSize;

                        if(GrowAmount >= GrowLimit)
                        {
                            Child->MajorSize  = Child->MajorBounds.Max;
                            Child->Constraint = Constraint::Exact;

                            ToGrow          -= GrowLimit;
                            IterationGrow   -= GrowLimit;
                            TotalGrowWeight -= GrowWeight;

                            Growable[Idx--] = Growable[--GrowableCount];
                        }
                        else
                        {
                            Child->MajorSize += GrowAmount;
                            ToGrow           -= GrowAmount;
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

static void
PostOrderMeasureTree(uint32_t NodeIndex , ui_layout_tree *Tree)
{
    ui_layout_node *Root = GetLayoutNode(NodeIndex, Tree);

    if(Root)
    {
        IterateLinkedList(Root, ui_layout_node *, Child)
        {
            PostOrderMeasureTree(Child->Index, Tree);
        }

        bool IsXMajor = (Root->Direction == LayoutDirection::Horizontal);

        Root->ResultWidth  = IsXMajor ? Root->MajorSize : Root->MinorSize;
        Root->ResultHeight = IsXMajor ? Root->MinorSize : Root->MajorSize;
    }
}

static void
PlaceLayoutTree(ui_layout_tree *Tree, memory_arena *Arena)
{
    VOID_ASSERT(Arena);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    ui_layout_node **LayoutBuffer = PushArray(Arena, ui_layout_node *, Tree->NodeCount);

    uint64_t VisitedNodes = 0;
    LayoutBuffer[VisitedNodes++] = GetLayoutRoot(Tree);

    for (uint64_t Idx = 0; Idx < VisitedNodes; ++Idx)
    {
        ui_layout_node *Parent = LayoutBuffer[Idx];

        LayoutDirection Direction    = Parent->Direction;
        bool            IsHorizontal = Direction == LayoutDirection::Horizontal;

        float StartX  = Parent->ResultX + Parent->Padding.Left;
        float StartY  = Parent->ResultY + Parent->Padding.Top;

        // BUG: Need the free space. Probably in post-order pass.
        float MajorCursor          = GetCursorOffsetFromAlignment(0.f, Parent->MajorAlign);
        float MinorInnerParentSize = Parent->MinorSize - (IsHorizontal ? (Parent->Padding.Top + Parent->Padding.Bot) : (Parent->Padding.Left + Parent->Padding.Right));

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            float MinorCursor = GetCursorOffsetFromAlignment((MinorInnerParentSize - Child->MinorSize), Parent->MinorAlign);

            if (IsHorizontal)
            {
                Child->ResultX = StartX + MajorCursor;
                Child->ResultY = StartY + MinorCursor;
            }
            else
            {
                Child->ResultX = StartX + MinorCursor;
                Child->ResultY = StartY + MajorCursor;
            }

            MajorCursor += Child->MajorSize;
            if (Child != Parent->First)
            {
                MajorCursor += Parent->Spacing;
            }

            if (Child->ChildCount > 0)
            {
                LayoutBuffer[VisitedNodes++] = Child;
            }
        }
    }
}

// TODO: For some reasons, I really dislike how this is implemented.
// TODO: These small helper buffers are quite common!

static ui_paint_buffer
GeneratePaintBuffer(ui_layout_tree *Tree, ui_cached_style *Cached, memory_arena *Arena)
{
    ui_layout_node **NodeBuffer   = PushArray(Arena, ui_layout_node *, Tree->NodeCount);
    uint32_t         WriteIndex   = 0;
    uint32_t         ReadIndex    = 0;
    uint32_t         CommandCount = 0;

    NodeBuffer[WriteIndex++] = GetLayoutRoot(Tree);

    while (NodeBuffer && ReadIndex < WriteIndex)
    {
        ui_layout_node *Node = NodeBuffer[ReadIndex++];

        if(Node)
        {
            // WARN: We do not do/use bounds checking anymore for this read.
            //       Should we just pass it in?

            ui_cached_style  &Style   = Cached[Node->StyleIndex];
            ui_paint_command &Command = Tree->PaintBuffer[CommandCount++];

            Command.Rectangle     = GetNodeOuterRect(Node);
            Command.RectangleClip = {};
            Command.TextKey       = {};
            Command.ImageKey      = {};
            Command.CornerRadius  = Style.Default.CornerRadius;
            Command.Softness      = Style.Default.Softness;
            Command.BorderWidth   = Style.Default.BorderWidth;

            if ((Node->Flags & LayoutNodeFlag::UseFocusedStyle) != LayoutNodeFlag::None)
            {
                Command.Color       = Style.Focused.Color;
                Command.BorderColor = Style.Focused.BorderColor;

                Node->Flags &= ~(LayoutNodeFlag::UseFocusedStyle);
            } else
            if ((Node->Flags & LayoutNodeFlag::UseHoveredStyle) != LayoutNodeFlag::None)
            {
                Command.Color       = Style.Hovered.Color;
                Command.BorderColor = Style.Hovered.BorderColor;

                Node->Flags &= ~(LayoutNodeFlag::UseHoveredStyle);
            }
            else
            {
                Command.Color       = Style.Default.Color;
                Command.BorderColor = Style.Default.BorderColor;
            }

            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                NodeBuffer[WriteIndex++] = Child;
            }
        }
    }

    ui_paint_buffer Result = {.Commands = Tree->PaintBuffer, .Size = CommandCount};
    return Result;
}
