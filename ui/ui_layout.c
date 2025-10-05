// [Helpers]

internal void
AlignGlyph(vec2_f32 GlyphPosition, f32 LineHeight, ui_character *Character)
{
    Character->Position.Min.X = roundf(GlyphPosition.X + Character->Layout.Offset.X);
    Character->Position.Min.Y = roundf(GlyphPosition.Y + Character->Layout.Offset.Y);
    Character->Position.Max.X = roundf(Character->Position.Min.X + Character->Layout.AdvanceX);
    Character->Position.Max.Y = roundf(Character->Position.Min.Y + LineHeight); // WARN: Wrong?
}

// [API IMPLEMENTATION]

internal ui_hit_test_result 
HitTestLayout(vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline)
{
    ui_hit_test_result Result = {0};
    ui_layout_box     *Box    = &LayoutRoot->Value;

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

        if(HasFlag(LayoutRoot->Value.Flags, UILayoutNode_IsResizable))
        {
            f32      BorderWidth        = LayoutRoot->CachedStyle->Value.BorderWidth;
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

        if (Result.Intent == UIIntent_Hover && HasFlag(LayoutRoot->Value.Flags, UILayoutNode_IsDraggable))
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

               if(HasFlag(Box->Flags, UILayoutNode_PlaceChildrenX))
                {
                    IterateLinkedList(Current->First, ui_layout_node *, Child)
                    {
                        Child->Value.FinalX = CursorX;
                        Child->Value.FinalY = CursorY;

                        CursorX += Child->Value.FinalWidth + Box->Spacing.Horizontal;

                        PushNodeQueue(&Queue, Child);
                    }
                }

                if(HasFlag(Box->Flags, UILayoutNode_PlaceChildrenY))
                {
                    IterateLinkedList(Current->First, ui_layout_node *, Child)
                    {
                        Child->Value.FinalX = CursorX;
                        Child->Value.FinalY = CursorY;

                        CursorY += Child->Value.FinalHeight + Box->Spacing.Vertical;

                        PushNodeQueue(&Queue, Child);
                    }
                }

                // TODO: I believe if we used the offsets the sampling could
                // be more precise? Where are the offsets used anyway?

                if(HasFlag(Box->Flags, UILayoutNode_DrawText) && Box->Text)
                {
                    ui_text *Text       = Box->Text;
                    u32      FilledLine = 0.f;
                    f32      LineWidth  = 0.f;
                    f32      LineStartX = CursorX;
                    f32      LineStartY = CursorY;

                    for(u32 Idx = 0; Idx < Text->Size; ++Idx)
                    {
                        ui_character *Character = &Text->Characters[Idx];

                        f32      GlyphWidth    = Character->Layout.AdvanceX;
                        vec2_f32 GlyphPosition = Vec2F32(LineStartX + LineWidth, LineStartY + (FilledLine * Text->LineHeight));

                        AlignGlyph(GlyphPosition, Text->LineHeight, Character);

                        if(LineWidth + GlyphWidth > Box->FinalWidth)
                        {
                            if(LineWidth > 0.f)
                            {
                                ++FilledLine;
                            }
                            LineWidth = GlyphWidth;

                            GlyphPosition = Vec2F32(LineStartX, LineStartY + (FilledLine * Text->LineHeight));
                            AlignGlyph(GlyphPosition, Text->LineHeight, Character);

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

    if(HasFlag(Box->Flags, UILayoutNode_DrawText) && Box->Text)
    {
        ui_text *Text = Box->Text;

        f32 LineWidth = 0.f;
        u32 LineCount = 0;

        if(Text->Size)
        {
            LineCount = 1;

            for(u32 Idx = 0; Idx < Text->Size; ++Idx)
            {
                ui_character *Character  = &Text->Characters[Idx];
                f32           GlyphWidth = Character->Layout.AdvanceX;

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

            Box->FinalHeight = LineCount * Text->LineHeight;
        }
    }
}
