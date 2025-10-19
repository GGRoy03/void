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

internal f32
GetVerticalSpacing(ui_layout_node *Node)
{
    f32 Result = (Node->ChildCount - 1) * Node->Value.Spacing.Vertical;
    return Result;
}

internal f32
GetHorizontalSpacing(ui_layout_node *Node)
{
    f32 Result = (Node->ChildCount - 1) * Node->Value.Spacing.Horizontal;
    return Result;
}

internal vec2_f32
GetInnerBoxSize(ui_layout_box *Box)
{
    vec2_f32 Result = Vec2F32(Box->FinalWidth - (Box->Padding.Left + Box->Padding.Right), Box->FinalHeight - (Box->Padding.Top + Box->Padding.Bot));
    return Result;
}

// [Layout Tree/Nodes]

internal b32 
IsValidLayoutNode(ui_layout_node *Node)
{
    b32 Result = Node && Node->Index != LayoutTree_InvalidNodeIndex;
    return Result;
}

internal ui_layout_node *
GetLastAddedLayoutNode(ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if(Tree->NodeCount > 0)
    {
        Result = Tree->Nodes + (Tree->NodeCount - 1);
    }

    return Result;
}

internal ui_layout_tree_params
SetDefaultTreeParams(ui_layout_tree_params Params)
{
    ui_layout_tree_params Result = {0};

    if(Params.NodeCount == 0)
    {
        Result.NodeCount = LayoutTree_DefaultNodeCount;
    }

    if(Params.NodeDepth == 0)
    {
        Result.NodeDepth = LayoutTree_DefaultNodeDepth;
    }

    return Result;
}

internal u64
GetLayoutTreeFootprint(ui_layout_tree_params Params)
{
    Params = SetDefaultTreeParams(Params);

    u64 ArraySize = Params.NodeCount * sizeof(ui_layout_node);
    u64 Result    = sizeof(ui_layout_tree) + ArraySize;

    return Result;
}

internal ui_layout_tree *
PlaceLayoutTreeInMemory(ui_layout_tree_params Params, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        Params = SetDefaultTreeParams(Params);

        Result = (ui_layout_tree *)Memory;
        Result->Nodes        = (ui_layout_node *)(Result + 1);
        Result->NodeDepth    = Params.NodeDepth;
        Result->NodeCapacity = Params.NodeCount;

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = LayoutTree_InvalidNodeIndex;
        }
    }

    return Result;
}

internal ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree)
{
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
InitializeLayoutNode(ui_cached_style *Style, bit_field ConstantFlags, byte_string Id, ui_pipeline *Pipeline)
{
    ui_layout_tree *Tree = Pipeline->LayoutTree;
    ui_layout_node *Node = GetFreeLayoutNode(Tree);
    if(Node)
    {
        Node->Value.Width   = UIGetSize(Style).X;
        Node->Value.Height  = UIGetSize(Style).Y;
        Node->Value.Padding = UIGetPadding(Style);
        Node->Value.Spacing = UIGetSpacing(Style);

        Node->Flags = ConstantFlags;

        // Tree Hierarchy
        {
            Node->Last   = 0;
            Node->Next   = 0;
            Node->First  = 0;
            Node->Parent = PeekLayoutNodeStack(&Tree->Parents);

            if(Node->Parent)
            {
                AppendToDoublyLinkedList(Node->Parent, Node, Node->Parent->ChildCount);
            }

            if(HasFlag(Node->Flags, UILayoutNode_IsParent))
            {
                PushLayoutNodeStack(&Tree->Parents, Node);
            }
        }

        // Draw Flags
        if(Style)
        {
            ui_color BackgroundColor = UIGetColor(Style);
            f32      BorderWidth     = UIGetBorderWidth(Style);

            if(IsVisibleColor(BackgroundColor))
            {
                Assert(!HasFlag(Node->Flags, UILayoutNode_DrawBackground));
                SetFlag(Node->Flags, UILayoutNode_DrawBackground);
            }

            if(BorderWidth > 0.f)
            {
                Assert(!HasFlag(Node->Flags, UILayoutNode_DrawBorders));
                SetFlag(Node->Flags, UILayoutNode_DrawBorders);
            }
        }

        // Id
        if(IsValidByteString(Id))
        {
            SetNodeId(Id, Node, Pipeline->IdTable);
        }

        // Style Info
        {
            ui_node_style NodeStyle = {0};
            NodeStyle.CachedStyleIndex = Style->CachedIndex;

            Pipeline->NodesStyle[Node->Index] = NodeStyle;
        }
    }

    return Node;
}

// [Pass]

internal ui_hit_test
HitTestLayout(vec2_f32 MousePosition, ui_layout_node *Root, ui_pipeline *Pipeline)
{
    ui_hit_test    Result = {0};
    ui_layout_box *Box    = &Root->Value;

    f32      Radius        = 0.f;
    vec2_f32 FullHalfSize  = Vec2F32(Box->FinalWidth * 0.5f, Box->FinalHeight * 0.5f);
    vec2_f32 Origin        = Vec2F32Add(Vec2F32(Box->FinalX, Box->FinalY), FullHalfSize);
    vec2_f32 LocalPosition = Vec2F32Sub(MousePosition, Origin);

    f32 TargetSDF = RoundedRectSDF(LocalPosition, FullHalfSize, Radius);
    if(TargetSDF <= 0.f)
    {
        // Recurse into all of the children. Respects draw order by iterating backwards

        for(ui_layout_node *Child = Root->Last; Child != 0; Child = Child->Prev)
        {
            Result = HitTestLayout(MousePosition, Child, Pipeline);
            if(Result.Success)
            {
                return Result;
            }
        }

        Result.Node    = Root;
        Result.Intent  = UIIntent_Hover;
        Result.Success = 1;

        if(HasFlag(Root->Flags, UILayoutNode_IsResizable))
        {
            ui_node_style   *NodeStyle   = GetNodeStyle(Root->Index, Pipeline);
            ui_cached_style *CachedStyle = GetCachedStyle(Pipeline->Registry, NodeStyle->CachedStyleIndex);

            f32      BorderWidth        = UIGetBorderWidth(CachedStyle);
            vec2_f32 BorderWidthVector  = Vec2F32(BorderWidth, BorderWidth);
            vec2_f32 HalfSizeWithBorder = Vec2F32Sub(FullHalfSize, BorderWidthVector);

            f32 BorderSDF = RoundedRectSDF(LocalPosition, HalfSizeWithBorder, Radius);
            if(BorderSDF >= 0.f)
            {
                f32 CornerTolerance = 5.f;                            // The higher this is, the greedier corner detection is.
                f32 ResizeBorderX   = Box->FinalX + Box->FinalWidth;  // Right  Border
                f32 ResizeBorderY   = Box->FinalY + Box->FinalHeight; // Bottom Border

                if(MousePosition.X >= (ResizeBorderX - BorderWidth))
                {
                    if(MousePosition.Y >= (Origin.Y + FullHalfSize.Y - CornerTolerance))
                    {
                        Result.Intent = UIIntent_ResizeXY;
                    }
                    else
                    {
                        Result.Intent = UIIntent_ResizeX;
                    }
                }
                else if(MousePosition.Y >= (ResizeBorderY - BorderWidth))
                {
                    if(MousePosition.X >= (Origin.X + FullHalfSize.X - CornerTolerance))
                    {
                        Result.Intent = UIIntent_ResizeXY;
                    }
                    else
                    {
                        Result.Intent = UIIntent_ResizeY;
                    }
                }
            }
        }

        if (Result.Intent == UIIntent_Hover && HasFlag(Root->Flags, UILayoutNode_IsDraggable))
        {
            Result.Intent = UIIntent_Drag;
        }
    }

    return Result;
}

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
PreOrderPlace(ui_layout_node *Root, ui_pipeline *Pipeline)
{
    if (Pipeline->LayoutTree && Pipeline->LayoutTree->NodeCount)
    {
        memory_region Region = EnterMemoryRegion(Pipeline->FrameArena);

        node_queue Queue = { 0 };
        {
            typed_queue_params Params = { 0 };
            Params.QueueSize = Pipeline->LayoutTree->NodeCount;

            Queue = BeginNodeQueue(Params, Region.Arena);
        }

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
                if(HasFlag(Node->Flags, UILayoutNode_IsScrollRegion))
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

                if(HasFlag(Node->Flags, UILayoutNode_TextIsBound))
                {
                    ui_glyph_run *Run = Box->DisplayText;

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

            LeaveMemoryRegion(Region);
        }
    }
}

internal void
PreOrderMeasure(ui_layout_node *Root, ui_pipeline *Pipeline)
{
    if (!Pipeline->LayoutTree || !Pipeline->LayoutTree->NodeCount) 
    {
        return;
    }

    memory_region Region = EnterMemoryRegion(Pipeline->FrameArena);

    node_queue Queue = { 0 };
    {
        typed_queue_params Params = { 0 };
        Params.QueueSize = Pipeline->LayoutTree->NodeCount;

        Queue = BeginNodeQueue(Params, Region.Arena);
    }

    if (!Queue.Data)
    {
        LeaveMemoryRegion(Region);
        return;
    }

    PushNodeQueue(&Queue, Root);

    while (!IsNodeQueueEmpty(&Queue))
    {
        ui_layout_node *Node = PopNodeQueue(&Queue);
        ui_layout_box  *Box  = &Node->Value;

        f32 AvWidth  = Box->FinalWidth  - (Box->Padding.Left + Box->Padding.Right);
        f32 AvHeight = Box->FinalHeight - (Box->Padding.Top  + Box->Padding.Bot  );

        if(HasFlag(Node->Flags, UILayoutNode_IsScrollRegion))
        {
            Assert(Box->ScrollContext);
            ui_scroll_context *Scroll = Box->ScrollContext;
            if(Scroll->Axis == ScrollAxis_X)
            {
                AvWidth = FLT_MAX;
            }
            else if(Scroll->Axis == ScrollAxis_Y)
            {
                AvHeight = FLT_MAX;
            }

            Scroll->ContentWindowSize = GetInnerBoxSize(Box);
        }

        IterateLinkedList(Node, ui_layout_node *, Child)
        {
            Box = &Child->Value;
            Box->FinalWidth  = ConvertUnitToFloat(Box->Width , AvWidth);
            Box->FinalHeight = ConvertUnitToFloat(Box->Height, AvHeight);

            PushNodeQueue(&Queue, Child);
        }
    }

    LeaveMemoryRegion(Region);
}

internal void
PostOrderMeasure(ui_layout_node *Root)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasure(Child);
    }

    ui_layout_box *Box = &Root->Value;

    if(HasFlag(Root->Flags, UILayoutNode_TextIsBound))
    {
        ui_glyph_run *Run = Box->DisplayText;

        // TODO: Handle other units..

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

    if(HasFlag(Root->Flags, UILayoutNode_IsScrollRegion))
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
            TotalWidth += GetHorizontalSpacing(Root);
            Scroll->ContentSize = Vec2F32(TotalWidth, MaxHeight);
        } else
        if (Scroll->Axis == ScrollAxis_Y)
        {
            TotalHeight += GetVerticalSpacing(Root);
            Scroll->ContentSize = Vec2F32(MaxWidth, TotalHeight);
        }
    }

}

// [Binds]

internal void
BindText(ui_layout_node *Node, byte_string Text, ui_font *Font, memory_arena *Arena)
{
    Assert(IsValidLayoutNode(Node));
    Assert(IsValidByteString(Text));
    Assert(Font);
    Assert(Arena);

    ui_glyph_run *Run = PushArena(Arena, sizeof(ui_glyph_run), AlignOf(ui_glyph_run));
    if(Run)
    {
        Run[0] = CreateGlyphRun(Text, Font, Arena);

        b32 Valid = (Run[0].Glyphs && Run[0].GlyphCount); // NOTE: Weird check.

        if(Valid)
        {
            Node->Value.DisplayText = Run;
            SetFlag(Node->Flags, UILayoutNode_TextIsBound);
        }
    }
}

internal void
BindScrollContext(ui_layout_node *Node, ScrollAxis_Type Axis, memory_arena *Arena)
{
    Assert(IsValidLayoutNode(Node));

    ui_scroll_context *Context = PushArena(Arena, sizeof(ui_scroll_context), AlignOf(ui_scroll_context));
    if(Context)
    {
        Node->Value.ScrollContext = Context;
        Context->Axis         = Axis;
        Context->PixelPerLine = 16.f;
    }
}

// [Context]

internal void
UIEnterSubtree(ui_layout_node *Parent, ui_pipeline *Pipeline)
{
    ui_layout_tree *Tree = Pipeline->LayoutTree;

    Assert(MemoryCompare(&Tree->Parents, &(layout_node_stack){0}, sizeof(typed_stack)) == 0);

    typed_stack_params Params = {0};
    {
        Params.StackSize = Tree->NodeDepth;
    }

    Tree->Parents = BeginLayoutNodeStack(Params, Pipeline->FrameArena);

    if(Parent)
    {
        PushLayoutNodeStack(&Tree->Parents, Parent);
    }
}

internal void
UILeaveSubtree(ui_pipeline *Pipeline)
{
    ui_layout_tree *Tree = Pipeline->LayoutTree;

    Assert(MemoryCompare(&Tree->Parents, &(layout_node_stack){0}, sizeof(typed_stack)) != 0);

    // NOTE: Does not leak memory, because it is allocated from the pipeline's frame
    // arena. Just clear the state internally.

    Tree->Parents = (layout_node_stack){0};
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
    ui_layout_node *Target;
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
InsertNodeId(ui_node_id_hash Hash, ui_layout_node *Target, ui_node_id_table *Table)
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
            Entry->Hash   = Hash;
            Entry->Target = Target;

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
            Entry->Hash   = Hash;
            Entry->Target = Target;

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
SetNodeId(byte_string Id, ui_layout_node *Node, ui_node_id_table *Table)
{
    if(!IsValidNodeIdTable(Table) && !IsValidLayoutNode(Node))
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

internal ui_layout_node *
UIFindNodeById(byte_string Id, ui_node_id_table *Table)
{
    ui_layout_node *Result = 0;

    if(IsValidByteString(Id) && IsValidNodeIdTable(Table))
    {
        ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
        ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

        if(Entry)
        {
            Result = Entry->Target;
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
            ClearFlag(Child->Flags, UILayoutNode_IsPruned);
        }
        else
        {
            SetFlag(Child->Flags, UILayoutNode_IsPruned);
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
