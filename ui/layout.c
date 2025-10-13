// [Helpers]

internal void
AlignGlyph(vec2_f32 Position, f32 LineHeight, ui_glyph *Glyph)
{
    Glyph->Position.Min.X = roundf(Position.X + Glyph->Offset.X);
    Glyph->Position.Min.Y = roundf(Position.Y + Glyph->Offset.Y);
    Glyph->Position.Max.X = roundf(Glyph->Position.Min.X + Glyph->AdvanceX);
    Glyph->Position.Max.Y = roundf(Glyph->Position.Min.Y + LineHeight); // WARN: Wrong?
}

// [Layout Tree/Nodes]

internal b32 
IsValidLayoutNode(ui_layout_node *Node)
{
    b32 Result = Node && Node->Index != LayoutTree_InvalidNodeIndex;
    return Result;
}

internal u64
GetLayoutTreeFootprint(ui_layout_tree_params Params)
{
    u64 StackSize = Params.Depth     * sizeof(ui_layout_node *);
    u64 ArraySize = Params.NodeCount * sizeof(ui_layout_node *);
    u64 Result    = sizeof(ui_layout_tree) + StackSize + ArraySize;

    return Result;
}

internal ui_layout_tree *
PlaceLayoutTreeInMemory(ui_layout_tree_params Params, void *Memory)
{
    Assert(Params.NodeCount > 0 && Params.Depth > 0);

    ui_layout_tree *Result = 0;

    if (Memory)
    {
        Result = (ui_layout_tree *)Memory;
        Result->Nodes        = (ui_layout_node *) (Result + 1);
        Result->ParentStack  = (ui_layout_node **)(Result->Nodes + Params.NodeCount);
        Result->MaximumDepth = Params.Depth;
        Result->NodeCapacity = Params.NodeCount;

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = LayoutTree_InvalidNodeIndex;
        }
    }

    return Result;
}


internal void
PopLayoutNodeParent(ui_layout_tree*Tree)
{
    if (Tree->ParentTop > 0)
    {
        --Tree->ParentTop;
    }
}

internal void
PushLayoutNodeParent(ui_layout_node *Node, ui_layout_tree*Tree)
{
    if (Tree->ParentTop < Tree->MaximumDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Node;
    }
}

internal ui_layout_node *
GetLayoutNodeParent(ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if (Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree, UILayoutNode_Type Type)
{
    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Type  = Type;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

internal ui_layout_node *
InitializeLayoutNode(ui_cached_style *Style, UILayoutNode_Type Type, ui_layout_node *Parent, bit_field ConstantFlags, byte_string Id, ui_pipeline *Pipeline)
{
    ui_layout_tree *Tree = Pipeline->LayoutTree;
    ui_layout_node *Node = GetFreeLayoutNode(Tree, Type);
    if(Node)
    {
        Node->Flags       = ConstantFlags;
        Node->CachedStyle = Style;

        Node->Value.Width   = UIGetSize(Style).X;
        Node->Value.Height  = UIGetSize(Style).Y;
        Node->Value.Padding = UIGetPadding(Style);
        Node->Value.Spacing = UIGetSpacing(Style);

        // Tree Hierarchy
        {
            Node->Last   = 0;
            Node->Next   = 0;
            Node->First  = 0;

            if(Parent)
            {
                Node->Parent = Parent;
            }
            else
            {
                Node->Parent = GetLayoutNodeParent(Tree);
            }

            if(Node->Parent)
            {
                AppendToDoublyLinkedList(Node->Parent, Node, Node->Parent->ChildCount);
            }

            if(HasFlag(Node->Flags, UILayoutNode_IsParent))
            {
                PushLayoutNodeParent(Node, Tree);
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
    }

    return Node;
}

// [Pass]

internal ui_hit_test
HitTestLayout(vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline)
{
    ui_hit_test    Result = {0};
    ui_layout_box *Box    = &LayoutRoot->Value;

    f32      Radius        = 0.f;
    vec2_f32 FullHalfSize  = Vec2F32(Box->FinalWidth * 0.5f, Box->FinalHeight * 0.5f);
    vec2_f32 Origin        = Vec2F32Add(Vec2F32(Box->FinalX, Box->FinalY), FullHalfSize);
    vec2_f32 LocalPosition = Vec2F32Sub(MousePosition, Origin);

    f32 TargetSDF = RoundedRectSDF(LocalPosition, FullHalfSize, Radius);
    if(TargetSDF <= 0.f)
    {
        // Recurse into all of the children. Respects draw order.
        for(ui_layout_node *Child = LayoutRoot->Last; Child != 0; Child = Child->Prev)
        {
            Result = HitTestLayout(MousePosition, Child, Pipeline);
            if(Result.Success)
            {
                return Result;
            }
        }

        Result.Node    = LayoutRoot;
        Result.Intent  = UIIntent_Hover;
        Result.Success = 1;

        if(HasFlag(LayoutRoot->Flags, UILayoutNode_IsResizable))
        {
            f32      BorderWidth        = UIGetBorderWidth(LayoutRoot->CachedStyle);
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

        if (Result.Intent == UIIntent_Hover && HasFlag(LayoutRoot->Flags, UILayoutNode_IsDraggable))
        {
            Result.Intent = UIIntent_Drag;
        }
    }

    return Result;
}

internal void
DragUISubtree(vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline)
{
    Assert(Delta.X != 0.f || Delta.Y != 0.f);

    // NOTE: Should we allow this mutation? It does work...

    LayoutRoot->Value.FinalX += Delta.X;
    LayoutRoot->Value.FinalY += Delta.Y;

    for (ui_layout_node *Child = LayoutRoot->First; Child != 0; Child = Child->Next)
    {
        DragUISubtree(Delta, Child, Pipeline);
    }
}

internal void
ResizeUISubtree(vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline)
{
    Assert(LayoutNode->Value.Width.Type  != UIUnit_Percent);
    Assert(LayoutNode->Value.Height.Type != UIUnit_Percent);

    // NOTE: Should we allow this mutation? It does work...

    LayoutNode->Value.Width.Float32  += Delta.X;
    LayoutNode->Value.Height.Float32 += Delta.Y;
}

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

internal void
PreOrderPlace(ui_layout_node *LayoutRoot, ui_pipeline *Pipeline)
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
            PushNodeQueue(&Queue, LayoutRoot);

            while (!IsNodeQueueEmpty(&Queue))
            {
                ui_layout_node *Current = PopNodeQueue(&Queue);
                ui_layout_box  *Box     = &Current->Value;

                // NOTE: Interesting that we apply padding here...
                f32 CursorX = Box->FinalX + Box->Padding.Left;
                f32 CursorY = Box->FinalY + Box->Padding.Top;

               if(HasFlag(Current->Flags, UILayoutNode_PlaceChildrenX))
                {
                    IterateLinkedList(Current->First, ui_layout_node *, Child)
                    {
                        Child->Value.FinalX = CursorX;
                        Child->Value.FinalY = CursorY;

                        CursorX += Child->Value.FinalWidth + Box->Spacing.Horizontal;

                        PushNodeQueue(&Queue, Child);
                    }
                }

                if(HasFlag(Current->Flags, UILayoutNode_PlaceChildrenY))
                {
                    IterateLinkedList(Current->First, ui_layout_node *, Child)
                    {
                        Child->Value.FinalX = CursorX;
                        Child->Value.FinalY = CursorY;

                        CursorY += Child->Value.FinalHeight + Box->Spacing.Vertical;

                        PushNodeQueue(&Queue, Child);
                    }
                }

                if(HasFlag(Current->Flags, UILayoutNode_TextIsBound))
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

                        AlignGlyph(GlyphPosition, Run->LineHeight, Glyph);

                        if(LineWidth + GlyphWidth > Box->FinalWidth)
                        {
                            if(LineWidth > 0.f)
                            {
                                ++FilledLine;
                            }
                            LineWidth = GlyphWidth;

                            GlyphPosition = Vec2F32(LineStartX, LineStartY + (FilledLine * Run->LineHeight));
                            AlignGlyph(GlyphPosition, Run->LineHeight, Glyph);

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
PreOrderMeasure(ui_layout_node *LayoutRoot, ui_pipeline *Pipeline)
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

    PushNodeQueue(&Queue, LayoutRoot);

    while (!IsNodeQueueEmpty(&Queue))
    {
        ui_layout_node *Current = PopNodeQueue(&Queue);
        ui_layout_box  *Box     = &Current->Value;

        f32 AvWidth  = Box->FinalWidth  - (Box->Padding.Left + Box->Padding.Right);
        f32 AvHeight = Box->FinalHeight - (Box->Padding.Top  + Box->Padding.Bot);

        IterateLinkedList(Current->First, ui_layout_node *, Child)
        {
            ui_layout_box *CBox = &Child->Value;

            if (CBox->Width.Type == UIUnit_Percent)
            {
                CBox->FinalWidth = (CBox->Width.Percent / 100.f) * AvWidth;
            }
            else if (Box->Width.Type == UIUnit_Float32)
            {
                CBox->FinalWidth = CBox->Width.Float32 <= AvWidth ? CBox->Width.Float32 : AvWidth;
            }

            if (CBox->Height.Type == UIUnit_Percent)
            {
                CBox->FinalHeight = (CBox->Height.Percent / 100.f) * AvHeight;
            }
            else if (CBox->Height.Type == UIUnit_Float32)
            {
                CBox->FinalHeight = CBox->Height.Float32 <= AvHeight ? CBox->Height.Float32 : AvHeight;
            }

            PushNodeQueue(&Queue, Child);
        }
    }

    LeaveMemoryRegion(Region);
}

internal void
PostOrderMeasure(ui_layout_node *LayoutRoot)
{
    IterateLinkedList(LayoutRoot->First, ui_layout_node *, Child)
    {
        PostOrderMeasure(Child);
    }

    ui_layout_box *Box = &LayoutRoot->Value;

    f32 InnerAvWidth = Box->FinalWidth - (Box->Padding.Left + Box->Padding.Right);

    if(HasFlag(LayoutRoot->Flags, UILayoutNode_TextIsBound))
    {
        ui_glyph_run *Run = Box->DisplayText;

        f32 LineWidth = 0.f;
        u32 LineCount = 0;

        if(Run->GlyphCount)
        {
            LineCount = 1;

            for(u32 Idx = 0; Idx < Run->GlyphCount; ++Idx)
            {
                ui_glyph *Glyph      = &Run->Glyphs[Idx];
                f32       GlyphWidth = Glyph->AdvanceX;

                if(LineWidth + GlyphWidth > InnerAvWidth)
                {
                    if(LineWidth > 0.f)
                    {
                        ++LineCount;
                    }

                    LineWidth = GlyphWidth;
                }
                else
                {
                    LineWidth += GlyphWidth;
                }
            }

            Box->FinalHeight = LineCount * Run->LineHeight;
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

        b32 Valid = (Run[0].Glyphs && Run[0].GlyphCount);

        if(Valid)
        {
            Node->Value.DisplayText = Run;
            SetFlag(Node->Flags, UILayoutNode_TextIsBound);
        }
    }
}

// [Node ID Hashmap]

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

        ConsoleWriteMessage(warn_message("ID could not be set because it already exists for this pipeline"), &Console);
    }
}

// NOTE: Maybe we don't want to return the raw node...
// Because we should disallow mutation by the user.

internal ui_layout_node *
UIFindNodeById(byte_string Id, ui_pipeline *Pipeline)
{
    ui_layout_node   *Result = 0;
    ui_node_id_table *Table  = Pipeline->IdTable;

    if(IsValidNodeIdTable(Table))
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
            ConsoleWriteMessage(warn_message("Could not find queried node."), &Console);
        }
    }
    else
    {
        ConsoleWriteMessage(warn_message("Internal Error. Id table is not allocated correctly for this pipeline."), &Console);
    }

    return Result;
}
