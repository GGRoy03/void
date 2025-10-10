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
    b32 Result = Node && Node->Index != InvalidLayoutNodeIndex;
    return Result;
}

internal ui_layout_tree *
AllocateLayoutTree(ui_layout_tree_params Params)
{
    Assert(Params.NodeCount > 0 && Params.Depth > 0);

    ui_layout_tree *Result = 0;

    memory_arena *Arena = 0;
    {
        u64 StackSize = Params.Depth     * sizeof(Result->ParentStack[0]);
        u64 ArraySize = Params.NodeCount * sizeof(Result->Nodes[0]);
        u64 QueueSize = Params.NodeCount * sizeof(Result->Nodes[0]);
        u64 Footprint = sizeof(ui_layout_tree) + StackSize + ArraySize + QueueSize;

        memory_arena_params ArenaParams = { 0 };
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Footprint;
        ArenaParams.CommitSize        = Footprint;

        Arena = AllocateArena(ArenaParams);
    }

    if (Arena)
    {
        Result = PushArena(Arena, sizeof(ui_layout_tree), AlignOf(ui_layout_tree));
        Result->Arena        = Arena;
        Result->Nodes        = PushArray(Arena, ui_layout_node, Params.NodeCount);
        Result->ParentStack  = PushArray(Arena, ui_layout_node *, Params.Depth);
        Result->MaximumDepth = Params.Depth;
        Result->NodeCapacity = Params.NodeCount;

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
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
InitializeLayoutNode(ui_cached_style *Style, UILayoutNode_Type Type, bit_field ConstantFlags, ui_layout_tree *Tree)
{
    ui_layout_node *Node = GetFreeLayoutNode(Tree, Type);
    if(Node)
    {
        Node->Flags = ConstantFlags;

        // Tree Hierarchy
        {
            Node->Last   = 0;
            Node->Next   = 0;
            Node->First  = 0;
            Node->Parent = GetLayoutNodeParent(Tree);

            if(Node->Parent)
            {
                AppendToDoublyLinkedList(Node->Parent, Node, Node->Parent->ChildCount);
            }

            if(HasFlag(Node->Flags, UILayoutNode_IsParent))
            {
                PushLayoutNodeParent(Node, Tree);
            }
        }

        if(Style)
        {
            if(UIGetBorderWidth(Style) > 0.f)
            {
                Assert(!HasFlag(Node->Flags, UILayoutNode_DrawBorders));
                SetFlag(Node->Flags, UILayoutNode_DrawBorders);
            }
        }
    }

    return Node;
}

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
    if (Pipeline->LayoutTree)
    {
        memory_region Region = EnterMemoryRegion(Pipeline->LayoutTree->Arena);

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
    if (!Pipeline->LayoutTree) 
    {
        return;
    }

    memory_region Region = EnterMemoryRegion(Pipeline->LayoutTree->Arena);

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

    ui_glyph_run *Result = PushArena(Arena, sizeof(ui_glyph_run), AlignOf(ui_glyph_run));
    if(Result)
    {
        Result[0] = CreateGlyphRun(Text, Font, Arena);

        Node->Value.DisplayText = Result;
        SetFlag(Node->Flags, UILayoutNode_TextIsBound);
    }
}
