// -----------------------------------------------------------------------------------
// Painting internal Implementation

typedef struct paint_stack_frame
{
    ui_layout_node *Node;
    rect_float        Clip;
} paint_stack_frame;

typedef struct paint_stack
{
    uint64_t NextWrite;
    uint64_t Size;

    paint_stack_frame *Frames;
} paint_stack;

static paint_stack
BeginPaintStack(uint64_t Size, memory_arena *Arena)
{
    paint_stack Result = {};
    Result.Frames = PushArray(Arena, paint_stack_frame, Size);
    Result.Size   = Size;

    return Result;
}

static bool
IsValidPaintStack(paint_stack *Stack)
{
    bool Result = (Stack) && (Stack->Frames) && (Stack->Size);
    return Result;
}

static bool
IsPaintStackEmpty(paint_stack *Stack)
{
    bool Result = Stack->NextWrite == 0;
    return Result;
}

static void
PushPaintStack(paint_stack_frame Value, paint_stack *Stack)
{
    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Frames[Stack->NextWrite++] = Value;
    }
}

static paint_stack_frame
PopPaintStack(paint_stack *Stack)
{
    VOID_ASSERT(Stack->NextWrite != 0);

    paint_stack_frame Result = Stack->Frames[--Stack->NextWrite];
    return Result;
}

static render_batch_list *
GetPaintBatchList(ui_layout_node *LayoutNode, ui_subtree *Subtree, rect_float Clip)
{
    VOID_ASSERT(LayoutNode);
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->Transient);

    render_pass           *Pass     = GetRenderPass(Subtree->Transient, RenderPass_UI);
    render_pass_params_ui *UIParams = &Pass->Params.UI.Params;
    rect_group_node       *Node     = UIParams->Last;

    rect_group_params Params = {};
    {
        Params.Transform = Mat3x3Identity();
        Params.Clip      = Clip;

        if((LayoutNode->Flags & UILayoutNode_HasText))
        {
            ui_text *Text = (ui_text *)QueryNodeResource(LayoutNode->Index, Subtree, UIResource_Text, UIState.ResourceTable);
            VOID_ASSERT(Text);

            Params.Texture     = Text->Atlas;
            Params.TextureSize = Text->AtlasSize;
        }

        if((LayoutNode->Flags & UILayoutNode_HasImage))
        {
            ui_image *Image = (ui_image *)QueryNodeResource(LayoutNode->Index, Subtree, UIResource_Image, UIState.ResourceTable);
            VOID_ASSERT(Image);

            ui_image_group *Group = (ui_image_group *)QueryGlobalResource(Image->GroupName, UIResource_ImageGroup, UIState.ResourceTable);
            VOID_ASSERT(Group);

            Params.Texture     = Group->RenderTexture.View;
            Params.TextureSize = Group->Size;
        }
    }

    bool CanMergeNodes = (Node && CanMergeRectGroupParams(&Node->Params, &Params));
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

    VOID_ASSERT(Node);

    Node->Params = Params;

    render_batch_list *Result = &Node->BatchList;
    return Result;
}

static void
PaintUIRect(rect_float Rect, ui_color Color, ui_corner_radius CornerRadii, float BorderWidth, float Softness, render_batch_list *BatchList, memory_arena *Arena)
{
    ui_rect *UIRect = (ui_rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->Color         = Color;
    UIRect->CornerRadii   = CornerRadii;
    UIRect->BorderWidth   = BorderWidth;
    UIRect->Softness      = Softness;
    UIRect->TextureSource = {};
    UIRect->SampleTexture = 0;
}

static void
PaintUIImage(rect_float Rect, rect_float Source, render_batch_list *BatchList, memory_arena *Arena)
{
    ui_rect *UIRect = (ui_rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->Color         = {.R  = 1, .G  = 1, .B  = 1, .A  = 1};
    UIRect->CornerRadii   = {.TL = 0, .TR = 0, .BR = 0, .BL = 0};
    UIRect->BorderWidth   = 0;
    UIRect->Softness      = 0;
    UIRect->TextureSource = Source;
    UIRect->SampleTexture = 1;
}

static void
PaintUIGlyph(rect_float Rect, ui_color Color, rect_float Source, render_batch_list *BatchList, memory_arena *Arena)
{
    ui_rect *UIRect = (ui_rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->Color         = Color;
    UIRect->CornerRadii   = {.TL = 0, .TR = 0, .BR = 0, .BL = 0};
    UIRect->BorderWidth   = 0;
    UIRect->Softness      = 0;
    UIRect->TextureSource = Source;
    UIRect->SampleTexture = 1;
}

// -----------------------------------------------------------------------------------
// Painting Debug Implementation

#ifdef DEBUG

static void
PaintDebugInformation(ui_layout_node *Node, ui_corner_radius CornerRadii, float Softness, render_batch_list *BatchList, memory_arena *Arena)
{

    if((Node->Flags & UILayoutNode_DebugOuterBox))
    {
        PaintUIRect(GetNodeOuterRect(Node), {.R = 255.f, .G = 0.f, .B = 0.f, .A = 255.f}, CornerRadii, 1.f, Softness, BatchList, Arena);
    }

    if((Node->Flags & UILayoutNode_DebugInnerBox))
    {
        PaintUIRect(GetNodeInnerRect(Node), {.R = 0.f, .G = 255.f, .B = 0.f, .A = 255.f}, CornerRadii, 1.f, Softness, BatchList, Arena);
    }

    if((Node->Flags & UILayoutNode_DebugContentBox))
    {
        PaintUIRect(GetNodeContentRect(Node), {.R = 0.f, .G = 0.f, .B = 255.f, .A = 255.f}, CornerRadii, 1.f, Softness, BatchList, Arena);
    }
}

#else
#define PaintDebugInformation(...)
#endif

// -----------------------------------------------------------------------------------
// Painting Public API Implementation

static void
PaintLayoutTreeFromRoot(ui_layout_node *Root, ui_subtree *Subtree)
{
    memory_arena *Arena = Subtree->Transient;
    paint_stack   Stack = BeginPaintStack(Subtree->NodeCount, Arena);

    if(IsValidPaintStack(&Stack))
    {
        PushPaintStack((paint_stack_frame){.Node = Root, .Clip = rect_float(0, 0, 0, 0)}, &Stack);

        while(!IsPaintStackEmpty(&Stack))
        {
            paint_stack_frame Frame = PopPaintStack(&Stack);

            rect_float     ClipRect  = Frame.Clip;
            ui_layout_node *Node      = Frame.Node;

            rect_float FinalRect = GetNodeOuterRect(Node);

            // Painting
            {
                render_batch_list   *BatchList = GetPaintBatchList(Node, Subtree, ClipRect);
                ui_paint_properties *Paint     = GetPaintProperties(Node->Index, Subtree);

                ui_corner_radius CornerRadii = Paint->CornerRadius;
                float            Softness    = Paint->Softness;
                ui_color         Color       = Paint->Color;
                ui_color         BorderColor = Paint->BorderColor;
                ui_color         TextColor   = Paint->TextColor;
                float            BorderWidth = Paint->BorderWidth;

                if(IsVisibleColor(Color))
                {
                    PaintUIRect(FinalRect, Color, CornerRadii, 0, Softness, BatchList, Arena);
                }

                if(IsVisibleColor(BorderColor) && BorderWidth > 0.f)
                {
                    PaintUIRect(FinalRect, BorderColor, CornerRadii, BorderWidth, Softness, BatchList, Arena);
                }

                if((Node->Flags & UILayoutNode_HasText))
                {
                    ui_text *Text = (ui_text *)QueryNodeResource(Node->Index, Subtree, UIResource_Text, UIState.ResourceTable);
                    VOID_ASSERT(Text);

                    for(uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
                    {
                        PaintUIGlyph(Text->Shaped[Idx].Position, TextColor, Text->Shaped[Idx].Source, BatchList, Arena);
                    }
                }

                if((Node->Flags & UILayoutNode_HasImage))
                {
                    ui_image *Image = (ui_image *)QueryNodeResource(Node->Index, Subtree, UIResource_Image, UIState.ResourceTable);
                    VOID_ASSERT(Image);

                    PaintUIImage(FinalRect, Image->Source, BatchList, Arena);
                }

                if((Node->Flags & UILayoutNode_HasTextInput))
                {
                    ui_text       *Text  = (ui_text *)      QueryNodeResource(Node->Index, Subtree, UIResource_Text     , UIState.ResourceTable);
                    ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, UIState.ResourceTable);

                    VOID_ASSERT(Text);
                    VOID_ASSERT(Input);
                    VOID_ASSERT(Input->CaretAnchor <= Text->ShapedCount);

                    vec2_float CaretStart = vec2_float(Node->ResultX, Node->ResultY);

                    if(Input->CaretAnchor > 0)
                    {
                        ui_shaped_glyph *AfterGlyph = &Text->Shaped[Input->CaretAnchor - 1];
                        CaretStart.X = AfterGlyph->Position.Right;
                        CaretStart.Y = AfterGlyph->Position.Top;
                    }

                    ui_color CaretColor = Paint->CaretColor;
                    float    CaretWidth = Paint->CaretWidth;

                    rect_float Caret = {};
                    Caret.Left   = CaretStart.X;
                    Caret.Right  = CaretStart.X + CaretWidth;
                    Caret.Top    = CaretStart.Y;
                    Caret.Bottom = CaretStart.Y + Text->LineHeight;

                    Input->CaretTimer += 0.05f;
                    CaretColor.A       = (sinf(Input->CaretTimer * 3.14159f) + 1.f) * 0.5f;

                    PaintUIRect(Caret, CaretColor, {.TL = 0, .TR = 0, .BR = 0, .BL = 0}, 0, 0, BatchList, Arena);
                }

                PaintDebugInformation(Node, CornerRadii, Softness, BatchList, Arena);
            }

            // Stack Frame
            {
                if ((Frame.Node->Flags & UILayoutNode_HasScrollRegion))
                {
                    ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Frame.Node->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
                    VOID_ASSERT(Region);
                }

                if((Frame.Node->Flags & UILayoutNode_HasClip))
                {
                    rect_float EmptyClip     = {};
                    bool      ParentHasClip = (MemoryCompare(&ClipRect, &EmptyClip, sizeof(rect_float)) != 0);

                    ClipRect = GetNodeContentRect(Node);

                    if(ParentHasClip)
                    {
                        ClipRect = Frame.Clip.Intersect(ClipRect);
                    }
                }
            }

            IterateLinkedListBackward(Frame.Node, ui_layout_node *, Child)
            {
                if (!(Child->Flags & UILayoutNode_DoNotPaint))
                {
                    PushPaintStack({.Node = Child, .Clip = ClipRect}, &Stack);
                }
            }
        }

    }
}

