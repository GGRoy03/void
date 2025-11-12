// -----------------------------------------------------------------------------------
// Painting Internal Implementation

typedef struct paint_stack_frame
{
    ui_layout_node *Node;
    vec2_f32        AccScroll;
    rect_f32        Clip;
} paint_stack_frame;

typedef struct paint_stack
{
    u64 NextWrite;
    u64 Size;

    paint_stack_frame *Frames;
} paint_stack;

internal paint_stack
BeginPaintStack(u64 Size, memory_arena *Arena)
{
    paint_stack Result = {0};
    Result.Frames = PushArray(Arena, paint_stack_frame, Size);
    Result.Size   = Size;

    return Result;
}

internal b32
IsValidPaintStack(paint_stack *Stack)
{
    b32 Result = (Stack) && (Stack->Frames) && (Stack->Size);
    return Result;
}

internal b32
IsPaintStackEmpty(paint_stack *Stack)
{
    b32 Result = Stack->NextWrite == 0;
    return Result;
}

internal void
PushPaintStack(paint_stack_frame Value, paint_stack *Stack)
{
    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Frames[Stack->NextWrite++] = Value;
    }
}

internal paint_stack_frame
PopPaintStack(paint_stack *Stack)
{
    Assert(Stack->NextWrite != 0);

    paint_stack_frame Result = Stack->Frames[--Stack->NextWrite];
    return Result;
}

internal render_batch_list *
GetPaintBatchList(ui_layout_node *LayoutNode, ui_subtree *Subtree, rect_f32 Clip)
{
    Assert(LayoutNode);
    Assert(Subtree);
    Assert(Subtree->Transient);

    render_pass           *Pass     = GetRenderPass(Subtree->Transient, RenderPass_UI);
    render_pass_params_ui *UIParams = &Pass->Params.UI.Params;
    rect_group_node       *Node     = UIParams->Last;

    rect_group_params Params = {0};
    {
        Params.Transform = Mat3x3Identity();
        Params.Clip      = Clip;

        if(HasFlag(LayoutNode->Flags, UILayoutNode_HasText))
        {
            ui_resource_table *Table = UIState.ResourceTable;

            ui_text *Text = QueryTextResource(LayoutNode->Index, Subtree, Table);
            Assert(Text);

            Params.Texture     = Text->Atlas;
            Params.TextureSize = Text->AtlasSize;
        }
    }

    b32 CanMergeNodes = (Node && CanMergeRectGroupParams(&Node->Params, &Params));
    if(CanMergeNodes)
    {
        Params.Texture     = IsValidRenderHandle(Node->Params.Texture) ? Node->Params.Texture     : Params.Texture;
        Params.TextureSize = IsValidRenderHandle(Node->Params.Texture) ? Node->Params.TextureSize : Params.TextureSize;
    }

    if(!Node || !CanMergeNodes)
    {
        Node = PushStruct(Subtree->Transient, rect_group_node);
        Node->BatchList.BytesPerInstance = sizeof(ui_rect);

        AppendToLinkedList(UIParams, Node, UIParams->Count);
    }

    Assert(Node);

    Node->Params = Params;

    render_batch_list *Result = &Node->BatchList;
    return Result;
}

internal void
PaintUIRect_(rect_f32 Rect, ui_color Color, ui_corner_radius CornerRadii, f32 BorderWidth, f32 Softness, rect_f32 Source, b32 Sample, render_batch_list *BatchList, memory_arena *Arena)
{
    ui_rect *UIRect = PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->TextureSource = Source;
    UIRect->Color         = Color;
    UIRect->CornerRadii   = CornerRadii;
    UIRect->BorderWidth   = BorderWidth;
    UIRect->Softness      = Softness;
    UIRect->SampleTexture = Sample;
}

internal void
PaintUIRect(rect_f32 Rect, ui_color Color, ui_corner_radius CornerRadii, f32 BorderWidth, f32 Softness, render_batch_list *BatchList, memory_arena *Arena)
{
    PaintUIRect_(Rect, Color, CornerRadii, BorderWidth, Softness, RectF32Zero(), 0, BatchList, Arena);
}

internal void
PaintUIGlyph(rect_f32 Rect, ui_color Color, rect_f32 Sample, render_batch_list *BatchList, memory_arena *Arena)
{
    PaintUIRect_(Rect, Color, (ui_corner_radius){0}, 0, 0, Sample, 1, BatchList, Arena);
}

// -----------------------------------------------------------------------------------
// Painting Debug Implementation

#ifdef DEBUG

internal void
PaintDebugInformation(ui_layout_node *Node, vec2_f32 NodeOrigin, ui_corner_radius CornerRadii, f32 Softness, render_batch_list *BatchList, memory_arena *Arena)
{
    if(HasFlag(Node->Flags, UILayoutNode_DebugOuterBox))
    {
        rect_f32 OuterRect = TranslateRectF32(Node->Value.OuterBox, NodeOrigin);
        PaintUIRect(OuterRect, UIColor(0.f, 255.f, 0.f, 255.f), CornerRadii, 1.f, Softness, BatchList, Arena);
    }

    if(HasFlag(Node->Flags, UILayoutNode_DebugInnerBox))
    {
        rect_f32 InnerRect = TranslateRectF32(Node->Value.InnerBox, NodeOrigin);
        PaintUIRect(InnerRect, UIColor(0.f, 255.f, 0.f, 255.f), CornerRadii, 1.f, Softness, BatchList, Arena);
    }

    if(HasFlag(Node->Flags, UILayoutNode_DebugContentBox))
    {
        rect_f32 ContentRect = TranslateRectF32(Node->Value.ContentBox, NodeOrigin);
        PaintUIRect(ContentRect, UIColor(0.f, 255.f, 0.f, 255.f), CornerRadii, 1.f, Softness, BatchList, Arena);
    }
}

#else
#define PaintDebugInformation(...)
#endif

// -----------------------------------------------------------------------------------
// Painting Public API Implementation

internal void
PaintLayoutTreeFromRoot(ui_layout_node *Root, ui_subtree *Subtree)
{
    memory_arena *Arena = Subtree->Transient;
    paint_stack   Stack = BeginPaintStack(Subtree->NodeCount, Arena);

    if(IsValidPaintStack(&Stack))
    {
        PushPaintStack((paint_stack_frame){.Node = Root, .AccScroll = Vec2F32(0.f ,0.f), .Clip = RectF32(0, 0, 0, 0)}, &Stack);

        while(!IsPaintStackEmpty(&Stack))
        {
            paint_stack_frame Frame = PopPaintStack(&Stack);

            rect_f32        ClipRect  = Frame.Clip;
            vec2_f32        AccScroll = Frame.AccScroll;
            ui_layout_node *Node      = Frame.Node;

            vec2_f32 NodeOrigin = Vec2F32Add(Node->Value.VisualOffset, AccScroll);
            rect_f32 FinalRect  = TranslateRectF32(Node->Value.OuterBox, NodeOrigin);

            // Painting
            {
                render_batch_list *BatchList = GetPaintBatchList(Node, Subtree, ClipRect);

                style_property *Style = GetPaintProperties(Node->Index, 1, Subtree);

                ui_corner_radius CornerRadii = UIGetCornerRadius(Style);
                f32              Softness    = UIGetSoftness(Style);

                ui_color BackgroundColor = UIGetColor(Style);
                if(IsVisibleColor(BackgroundColor))
                {
                    PaintUIRect(FinalRect, BackgroundColor, CornerRadii, 0, Softness, BatchList, Arena);
                }

                ui_color BorderColor = UIGetBorderColor(Style);
                f32      BorderWidth = UIGetBorderWidth(Style);
                if(IsVisibleColor(BorderColor) && BorderWidth > 0.f)
                {
                    PaintUIRect(FinalRect, BorderColor, CornerRadii, BorderWidth, Softness, BatchList, Arena);
                }

                if(HasFlag(Node->Flags, UILayoutNode_HasText))
                {
                    ui_text *Text = QueryTextResource(Node->Index, Subtree, UIState.ResourceTable);
                    Assert(Text);

                    ui_color TextColor = UIGetTextColor(Style);
                    for(u32 Idx = 0; Idx < Text->ShapedCount; ++Idx)
                    {
                        PaintUIGlyph(Text->Shaped[Idx].Position, TextColor, Text->Shaped[Idx].Source, BatchList, Arena);
                    }
                }

                // NOTE:
                // Should we check if the input is focused or not? Or let the user figure that out?

                if(HasFlag(Node->Flags, UILayoutNode_HasTextInput))
                {
                    ui_text       *Text  = QueryTextResource(Node->Index, Subtree, UIState.ResourceTable);
                    ui_text_input *Input = QueryTextInputResource(Node->Index, Subtree, UIState.ResourceTable);

                    Assert(Text);
                    Assert(Input);
                    Assert(Input->CaretAnchor <= Text->ShapedCount);

                    vec2_f32 ContentPos = GetContentBoxPosition(&Node->Value);
                    vec2_f32 CaretStart = Vec2F32(ContentPos.X, ContentPos.Y);

                    if(Input->CaretAnchor > 0)
                    {
                        ui_shaped_glyph *AfterGlyph = &Text->Shaped[Input->CaretAnchor - 1];
                        CaretStart.X = AfterGlyph->Position.Max.X;
                        CaretStart.Y = AfterGlyph->Position.Min.Y;
                    }

                    ui_color CaretColor = UIGetCaretColor(Style);
                    f32      CaretWidth = UIGetCaretWidth(Style);

                    rect_f32 Caret = 
                    {
                        .Min.X = CaretStart.X,
                        .Max.X = CaretStart.X + CaretWidth,
                        .Min.Y = CaretStart.Y,
                        .Max.Y = CaretStart.Y + Text->LineHeight,
                    };

                    Input->CaretTimer += 0.05f;
                    CaretColor.A       = (sinf(Input->CaretTimer * 3.14159f) + 1.f) * 0.5f;

                    PaintUIRect(Caret, CaretColor, UICornerRadius(0, 0, 0, 0), 0, 0, BatchList, Arena);
                }

                PaintDebugInformation(Node, NodeOrigin, CornerRadii, Softness, BatchList, Arena);
            }

            // Stack Frame
            {
                if (HasFlag(Frame.Node->Flags, UILayoutNode_HasScrollRegion))
                {
                    ui_scroll_region *Region = QueryScrollRegion(Frame.Node->Index, Subtree, UIState.ResourceTable);
                    Assert(Region);

                    vec2_f32 NodeScroll = GetScrollNodeTranslation(Region);

                    AccScroll.X += NodeScroll.X;
                    AccScroll.Y += NodeScroll.Y;
                }

                if(HasFlag(Frame.Node->Flags, UILayoutNode_HasClip))
                {
                    rect_f32 EmptyClip     = RectF32Zero();
                    b32      ParentHasClip = (MemoryCompare(&ClipRect, &EmptyClip, sizeof(rect_f32)) != 0);

                    ClipRect = TranslatedRectF32(Node->Value.ContentBox, NodeOrigin);

                    if(ParentHasClip)
                    {

                        ClipRect = IntersectRectF32(Frame.Clip, ClipRect);
                    }
                }
            }

            IterateLinkedListBackward(Frame.Node, ui_layout_node *, Child)
            {
                if (!(HasFlag(Child->Flags, UILayoutNode_DoNotPaint)) && Child->Value.Display != UIDisplay_None)
                {
                    PushPaintStack((paint_stack_frame){.Node = Child, .AccScroll = AccScroll, .Clip = ClipRect}, &Stack);
                }
            }
        }

    }
}

