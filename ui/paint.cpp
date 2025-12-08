// ------------------------------------------------------------------------------------
// @Internal: Helpers

static bool
IsVisibleColor(ui_color Color)
{
    bool Result = (Color.A > 0.f);
    return Result;
}

static ui_color
NormalizeColor(ui_color Color)
{
    float Inverse = 1.f / 255.f;

    ui_color Result =
    {
        .R = Color.R * Inverse,
        .G = Color.G * Inverse,
        .B = Color.B * Inverse,
        .A = Color.A * Inverse,
    };

    return Result;
}

// -----------------------------------------------------------------------------------
// Painting internal Implementation

// NOTE:
// Slight fritiction with the resource API. The members of resource state are too verbose 
// and can already be inferred from the context.

static render_batch_list *
GetPaintBatchList(ui_resource_key TextKey, ui_resource_key ImageKey, memory_arena *Arena, rect_float RectangleClip)
{
    VOID_ASSERT(Arena); // Internal Corruption

    void_context &Context = GetVoidContext();

    render_pass           *Pass     = GetRenderPass(Arena, RenderPass_UI);
    render_pass_params_ui *UIParams = &Pass->Params.UI.Params;
    rect_group_node       *Node     = UIParams->Last;

    rect_group_params Params = {};
    {
        Params.Transform = Mat3x3Identity();
        Params.Clip      = RectangleClip;

        if(IsValidResourceKey(TextKey))
        {
            ui_resource_state TextResource = FindResourceByKey(TextKey, Context.ResourceTable);
            if(TextResource.ResourceType == UIResource_Text && TextResource.Resource)
            {
                auto *Text = static_cast<ui_text *>(TextResource.Resource);

                Params.Texture     = Text->Atlas;
                Params.TextureSize = Text->AtlasSize;
            }
        }

        if(IsValidResourceKey(ImageKey))
        {
            ui_resource_state ImageResource = FindResourceByKey(ImageKey, Context.ResourceTable);
            if(ImageResource.ResourceType == UIResource_Image && ImageResource.Resource)
            {
                auto *Image = static_cast<ui_image *>(ImageResource.Resource);
                auto *Group = (ui_image_group *)QueryGlobalResource(Image->GroupName, UIResource_ImageGroup, Context.ResourceTable);
                if(Group)
                {
                    Params.Texture     = Group->RenderTexture.View;
                    Params.TextureSize = Group->Size;
                }
            }
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
        Node = PushStruct(Arena, rect_group_node);
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
// Painting Public API Implementation

static void
ExecutePaintCommands(ui_paint_buffer Buffer, memory_arena *Arena)
{
    VOID_ASSERT(Buffer.Commands);  // Internal Corruption
    VOID_ASSERT(Arena);            // Internal Corruption

    for(uint32_t Idx = 0; Idx < Buffer.Size; ++Idx)
    {
        ui_paint_command &Command = Buffer.Commands[Idx];

        rect_float       Rect     = Command.Rectangle;
        ui_color         Color    = Command.Color;
        ui_corner_radius Radius   = Command.CornerRadius;
        float            Softness = Command.Softness;

        // TODO: Can this return NULL?
        render_batch_list *BatchList = GetPaintBatchList(Command.TextKey, Command.ImageKey, Arena, Command.RectangleClip);

        if(Color.A > 0.f)
        {
            PaintUIRect(Rect, Color, Radius, 0, Softness, BatchList, Arena);
        }

        ui_color BorderColor = Command.BorderColor;
        float    BorderWidth = Command.BorderWidth;

        if(BorderColor.A > 0.f && BorderWidth > 0.f)
        {
            PaintUIRect(Rect, BorderColor, Radius, BorderWidth, Softness, BatchList, Arena);
        }


        // TODO: RE-IMPLEMENT TEXT & TEXT-INPUT & IMAGE DRAWING (TRIVIAL, JUST READ RESOURCE KEY?)
        // TODO: RE-IMPLEMENT CLIPPING AND WHATNOT
        // TODO: RE-IMPLEMENT DEBUG DRAWING
    }
}
