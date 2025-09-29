// [Helpers]

internal ui_color
UIColor(f32 R, f32 G, f32 B, f32 A)
{
    ui_color Result = { R, G, B, A };
    return Result;
}

internal ui_spacing
UISpacing(f32 Horizontal, f32 Vertical)
{
    ui_spacing Result = { Horizontal, Vertical };
    return Result;
}

internal ui_padding
UIPadding(f32 Left, f32 Top, f32 Right, f32 Bot)
{
    ui_padding Result = { Left, Top, Right, Bot };
    return Result;
}

internal ui_corner_radius
UICornerRadius(f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight)
{
    ui_corner_radius Result = { TopLeft, TopRight, BotLeft, BotRight };
    return Result;
}

internal vec2_unit
Vec2Unit(ui_unit U0, ui_unit U1)
{
    vec2_unit Result = { U0, U1 };
    return Result;
}

internal b32
IsNormalizedColor(ui_color Color)
{
    b32 Result = (Color.R >= 0.f && Color.R <= 1.f) &&
                 (Color.G >= 0.f && Color.G <= 1.f) &&
                 (Color.B >= 0.f && Color.B <= 1.f) &&
                 (Color.A >= 0.f && Color.A <= 1.f);

    return Result;
}

// [Components]

// NOTE: Temporary global ID tracker. Obviously, this is not good.
// TODO: Remove this system?
global i32 NextId;

internal void
UIWindow(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UITree_GetFreeNode(Pipeline->StyleTree, UINode_Window);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UITree_LinkNodes(Node, UITree_GetParent(Pipeline->StyleTree));
        UITree_PushParent(Node, Pipeline->StyleTree);

        if(Node->Style.BorderWidth > 0)
        {
            UITree_BindFlag(Node, 1, UILayoutNode_DrawBorders, Pipeline);
        }

        UITree_BindFlag(Node, 1, UILayoutNode_DrawBackground         , Pipeline);
        UITree_BindFlag(Node, 1, UILayoutNode_PlaceChildrenVertically, Pipeline);
        UITree_BindFlag(Node, 1, UILayoutNode_IsDraggable            , Pipeline);
        UITree_BindFlag(Node, 1, UILayoutNode_IsResizable            , Pipeline);
    }
}

internal void
UIButton(ui_style_name StyleName, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UITree_GetFreeNode(Pipeline->StyleTree, UINode_Button);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UITree_LinkNodes(Node, UITree_GetParent(Pipeline->StyleTree));

        // Draw Flags
        {
            if (Node->Style.BorderWidth > 0)
            {
                UITree_BindFlag(Node, 1, UILayoutNode_DrawBorders, Pipeline);
            }
            UITree_BindFlag(Node, 1, UILayoutNode_DrawBackground, Pipeline);
        }

        // Callbacks (What about hovers?)
        if (Callback)
        {
            UITree_BindClickCallback(Node, Callback, Pipeline);
        }
    }
}

// NOTE: Only supports direct glyph access for now.
// NOTE: Only supports extended ASCII for now.

internal void
UILabel(ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UITree_GetFreeNode(Pipeline->StyleTree, UINode_Label);
    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UITree_LinkNodes(Node, UITree_GetParent(Pipeline->StyleTree));

        // Draw Binds
        {
            if (Node->Style.BorderWidth > 0)
            {
                UITree_BindFlag(Node, 1, UILayoutNode_DrawBorders, Pipeline);
            }

            if (Text.Size > 0)
            {
                UITree_BindFlag(Node, 1, UILayoutNode_DrawText, Pipeline);
            }
        }

        ui_unit TextWidth  = {UIUnit_Float32, 0};
        ui_unit TextHeight = {UIUnit_Float32, 0};

        ui_font      *Font           = Node->Style.Font.Ref;
        ui_character *Characters     = PushArray(Pipeline->StaticArena, ui_character, Text.Size);
        u32           CharacterCount = (u32)Text.Size; Assert(CharacterCount == Text.Size);
        if (Characters)
        {
            for (u32 Idx = 0; Idx < Text.Size; Idx++)
            {
                u8 Character = Text.String[Idx];

                glyph_state          State      = FindGlyphEntryByDirectAccess((u32)Character, Font->GlyphTable);
                os_glyph_layout      Layout     = State.Layout;
                os_glyph_raster_info RasterInfo = State.RasterInfo;

                if (!RasterInfo.IsRasterized)
                {
                    Layout = OSGetGlyphLayout(Character, &Font->OSFontObjects, Font->TextureSize, Font->Size);

                    stbrp_rect STBRect = { 0 };
                    STBRect.w = (u16)Layout.Size.X; Assert(STBRect.w == Layout.Size.X);
                    STBRect.h = (u16)Layout.Size.Y; Assert(STBRect.h == Layout.Size.Y);
                    stbrp_pack_rects(&Font->AtlasContext, &STBRect, 1);

                    if (STBRect.was_packed)
                    {
                        rect_f32 Rect;
                        Rect.Min.X = (f32)STBRect.x;
                        Rect.Min.Y = (f32)STBRect.y;
                        Rect.Max.X = (f32)STBRect.x + STBRect.w;
                        Rect.Max.Y = (f32)STBRect.y + STBRect.h;
                        RasterInfo = OSRasterizeGlyph(Character, Rect, &Font->OSFontObjects, &Font->GPUFontObjects, Pipeline->RendererHandle);

                        UpdateDirectGlyphTableEntry((u32)Character, Layout, RasterInfo, Font->GlyphTable);
                    }
                    else
                    {
                        OSLogMessage(byte_string_literal("Failed to pack rect."), OSMessage_Error);
                    }
                }

                TextWidth.Float32 += Layout.AdvanceX;
                TextHeight.Float32 = Layout.Size.Y > TextHeight.Float32 ? Layout.Size.Y : TextHeight.Float32;

                Characters[Idx].Layout       = Layout;
                Characters[Idx].SampleSource = RasterInfo.SampleSource;
            }
        }

        Node->Style.Size = Vec2Unit(TextWidth, TextHeight);
        UITree_BindText(Pipeline, Characters, CharacterCount, Font, Node);
    }
}

internal void
UIHeader(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UITree_GetFreeNode(Pipeline->StyleTree, UINode_Header);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UITree_LinkNodes(Node, UITree_GetParent(Pipeline->StyleTree));
        UITree_PushParent(Node, Pipeline->StyleTree);

        if (Node->Style.BorderWidth > 0)
        {
            UITree_BindFlag(Node, 1, UILayoutNode_DrawBorders, Pipeline);
        }
    }
}

// [Style]

internal ui_cached_style *
UIGetStyleSentinel(byte_string Name, ui_style_registery *Registery)
{
    ui_cached_style *Result = 0;

    if (Name.Size)
    {
        Result = Registery->Sentinels + (Name.Size - 1);
    }

    return Result;
}

internal ui_style_name
UIGetCachedName(byte_string Name, ui_style_registery *Registery)
{
    ui_style_name Result = {0};

    if (Name.Size)
    {
        ui_cached_style *CachedStyle = 0;
        ui_cached_style *Sentinel    = UIGetStyleSentinel(Name, Registery);

        for (CachedStyle = Sentinel->Next; CachedStyle != 0; CachedStyle = CachedStyle->Next)
        {
            byte_string CachedString = Registery->CachedNames[CachedStyle->Index].Value;
            if (ByteStringMatches(Name, CachedString, StringMatch_CaseSensitive))
            {
                Result = Registery->CachedNames[CachedStyle->Index];
                break;
            }
        }
    }

    return Result;
}

internal ui_style
UIGetStyle(ui_style_name Name, ui_style_registery *Registery)
{
    ui_style Result = { 0 };

    if (Registery)
    {
        ui_cached_style *Sentinel = UIGetStyleSentinel(Name.Value, Registery);
        if (Sentinel)
        {
            for (ui_cached_style *CachedStyle = Sentinel->Next; CachedStyle != 0; CachedStyle = CachedStyle->Next)
            {
                byte_string CachedString = Registery->CachedNames[CachedStyle->Index].Value;
                if (Name.Value.String == CachedString.String)
                {
                    Result = CachedStyle->Style;
                    break;
                }
            }
        }
        else
        {
            OSLogMessage(byte_string_literal("Style does not exist."), OSMessage_Warn);
        }
    }

    return Result;
}

internal void
UISetColor(ui_node *Node, ui_color Color)
{
    b32 ValidColor = IsNormalizedColor(Color);
    if (ValidColor)
    {
        Node->Style.Version += 1;
        Node->Style.Color    = Color;
        SetFlag(Node->Style.Flags, UIStyleNode_StyleSetAtRuntime);
    }
    else
    {
        OSLogMessage(byte_string_literal("Color given to 'UISetColor' is not normalized."), OSMessage_Warn);
    }
}

// [Pipeline]

internal ui_pipeline
UICreatePipeline(ui_pipeline_params Params)
{
    ui_pipeline Result = {0};

    // Arena (TODO: Params Alloc)
    {
        {
            memory_arena_params ArenaParams = {0};
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result.FrameArena = AllocateArena(ArenaParams);
        }

        {
            memory_arena_params ArenaParams = { 0 };
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result.StaticArena = AllocateArena(ArenaParams);
        }
    }

    // Trees
    {
        {
            ui_tree_params TreeParams = {0};
            TreeParams.Depth     = Params.TreeDepth;
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.Type      = UITree_Style;

            Result.StyleTree = UITree_Allocate(TreeParams);
        }

        {
            ui_tree_params TreeParams = { 0 };
            TreeParams.Depth     = Params.TreeDepth;
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.Type      = UITree_Layout;

            Result.LayoutTree = UITree_Allocate(TreeParams);
        }
    }

    // Others
    {
        Result.StyleRegistery = LoadStyleFromFiles(Params.ThemeFiles, Params.ThemeCount, Result.StaticArena);
        Result.RendererHandle = Params.RendererHandle;
    }

    return Result;
}

internal void
UIPipelineBegin(ui_pipeline *Pipeline)
{   Assert(Pipeline);

    ClearArena(Pipeline->FrameArena);
}

internal void
UIPipelineExecute(ui_pipeline *Pipeline, render_pass_list *PassList)
{   Assert(Pipeline && PassList);

    // Unpacking
    ui_node     *StyleRoot  = &Pipeline->StyleTree->Nodes[0];
    ui_node     *LayoutRoot = &Pipeline->LayoutTree->Nodes[0];
    render_pass *RenderPass = GetRenderPass(Pipeline->FrameArena, PassList, RenderPass_UI);

    {
        UITree_SynchronizePipeline(StyleRoot, Pipeline);
    }

    {
        UILayout_ComputeParentToChildren(LayoutRoot, Pipeline);
    }

    vec2_f32 MouseDelta     = OSGetMouseDelta();
    b32      MouseIsClicked = OSIsMouseClicked(OSMouseButton_Left);
    b32      MouseReleased  = OSIsMouseReleased(OSMouseButton_Left);

    bit_field HitTestFlags = UIHitTest_NoFlag;
    {
        if(HasFlag(LayoutRoot->Layout.Flags, UILayoutNode_IsResizable))
        {
            SetFlag(HitTestFlags, UIHitTest_CheckForResize);
        }
    }

    ui_hit_test_result Hit = UILayout_HitTest(OSGetMousePosition(), HitTestFlags, LayoutRoot, Pipeline);

    if(Hit.Success)
    {
        Assert(Hit.LayoutNode);
        Assert(Hit.HoverState != UIHover_None);

        ui_layout_box *Box = &Hit.LayoutNode->Layout;

        SetFlag(Box->Flags, UILayoutNode_IsHovered);

        if(MouseIsClicked)
        {
            if(Box->ClickCallback)
            {
                Box->ClickCallback(Hit.LayoutNode, Pipeline);
            }

            SetFlag(Box->Flags, UILayoutNode_IsClicked);
        }

        switch(Hit.HoverState)
        {

        default: break;

        case UIHover_Target:
        {
            if(MouseIsClicked)
            {
                Pipeline->DragCaptureNode = Hit.LayoutNode;
            }
        } break;

        case UIHover_ResizeX:
        {
            if (MouseIsClicked)
            {
                Pipeline->ResizeCaptureNode = Hit.LayoutNode;
                Pipeline->ResizeType        = UIResize_X;
            }
        } break;

        case UIHover_ResizeY:
        {
            if (MouseIsClicked)
            {
                Pipeline->ResizeCaptureNode = Hit.LayoutNode;
                Pipeline->ResizeType        = UIResize_Y;
            }
        } break;

        case UIHover_ResizeCorner:
        {
            if (MouseIsClicked)
            {
                Pipeline->ResizeCaptureNode = Hit.LayoutNode;
                Pipeline->ResizeType        = UIResize_XY;
            }
        } break;

        }
    }

    if (MouseReleased)
    {
        Pipeline->DragCaptureNode   = 0;
        Pipeline->ResizeCaptureNode = 0;
        Pipeline->ResizeType        = UIResize_None;
    }

    if (Pipeline->ResizeCaptureNode)
    {
        switch (Pipeline->ResizeType)
        {
        default: break;

        case UIResize_X:
        {
            vec2_f32 Delta = Vec2F32(MouseDelta.X, 0.f);
            UILayout_ResizeSubtree(Delta, Pipeline->ResizeCaptureNode, Pipeline);
        } break;

        case UIResize_Y:
        {
            vec2_f32 Delta = Vec2F32(0.f, MouseDelta.Y);
            UILayout_ResizeSubtree(Delta, Pipeline->ResizeCaptureNode, Pipeline);
        } break;

        case UIResize_XY:
        {
            UILayout_ResizeSubtree(MouseDelta, Pipeline->ResizeCaptureNode, Pipeline);
        } break;

        }

        if (Hit.Success)
        {
            ClearFlag(Hit.LayoutNode->Layout.Flags, UILayoutNode_IsHovered);
        }
    }

    if (Pipeline->DragCaptureNode && (MouseDelta.X != 0.f || MouseDelta.Y != 0.f))
    {
        UILayout_DragSubtree(MouseDelta, Pipeline->DragCaptureNode, Pipeline);
    }

    // Set Cursor
    {
        if (Pipeline->DragCaptureNode)
        {
            OSSetCursor(OSCursor_GrabHand);
        }
        else if (Hit.HoverState == UIHover_ResizeX || Pipeline->ResizeType == UIResize_X)
        {
            OSSetCursor(OSCursor_ResizeHorizontal);
        }
        else if (Hit.HoverState == UIHover_ResizeY || Pipeline->ResizeType == UIResize_Y)
        {
            OSSetCursor(OSCursor_ResizeVertical);
        }
        else if (Hit.HoverState == UIHover_ResizeCorner || Pipeline->ResizeType == UIResize_XY)
        {
            OSSetCursor(OSCursor_ResizeDiagonalLeftToRight);
        }
        else
        {
            OSSetCursor(OSCursor_Default);
        }
    }

    UIPipelineBuildDrawList(Pipeline, RenderPass, StyleRoot, LayoutRoot);
}

internal void
UIPipelineDragNodes(vec2_f32 MouseDelta, ui_pipeline *Pipeline, ui_node *LRoot)
{
    LRoot->Layout.FinalX += MouseDelta.X;
    LRoot->Layout.FinalY += MouseDelta.Y;

    for (ui_node *Child = LRoot->First; Child != 0; Child = Child->Next)
    {
        UIPipelineDragNodes(MouseDelta, Pipeline, Child);
    }
}

internal void
UIPipelineCrystallizeStyles(ui_pipeline *Pipeline, ui_node *Root)
{   UNUSED(Pipeline && Root);

    // TODO: Implement this....
}

internal void
UIPipelineBuildDrawList(ui_pipeline *Pipeline, render_pass *Pass, ui_node *SRoot, ui_node *LRoot)
{
    ui_layout_box *Box = &LRoot->Layout;

    render_batch_list     *List      = 0;
    render_pass_params_ui *UIParams  = &Pass->Params.UI.Params;
    rect_group_params      NewParams = {.Transform = Mat3x3Identity()};
    {
        rect_group_node *Node = UIParams->Last;

        b32 CanMerge = 1;
        if(Node)
        {
            if(HasFlag(Box->Flags, UILayoutNode_DrawText))
            {
                Assert(Box->Text);
                NewParams.AtlasTextureSize = Box->Text->AtlasTextureSize;
                NewParams.AtlasTextureView = Box->Text->AtlasTexture;
            }

            CanMerge = CanMergeGroupParams(&Node->Params, &NewParams);
        }

        if (!Node || !CanMerge)
        {
            Node = PushArray(Pipeline->FrameArena, rect_group_node, 1);
            Node->BatchList.BytesPerInstance = sizeof(ui_rect);

            if (!UIParams->First)
            {
                UIParams->First = Node;
            }

            if (UIParams->Last)
            {
                UIParams->Last->Next = Node;
            }

            UIParams->Last = Node;
        }

        Node->Params = NewParams;        // BUG: This is simply wrong.
        List         = &Node->BatchList;
    }

    ui_style *BaseStyle = &SRoot->Style;
    ui_style *Style     = BaseStyle;
    {
        b32 Overriden = 0;

        if (HasFlag(Box->Flags, UILayoutNode_IsClicked))
        {
            if (Style->ClickOverride)
            {
                Style     = Style->ClickOverride;
                Overriden = 1;
            }

            ClearFlag(Box->Flags, UILayoutNode_IsClicked);
        }
        else if (HasFlag(Box->Flags, UILayoutNode_IsHovered))
        {
            if (Style->HoverOverride)
            {
                Style     = Style->HoverOverride;
                Overriden = 1;
            }

            ClearFlag(Box->Flags, UILayoutNode_IsHovered);
        }

        if (Overriden && Style && Style->Version != BaseStyle->Version)
        {
            if (!HasFlag(Style->Flags, UIStyleNode_HasColor))        Style->Color        = BaseStyle->Color;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderColor))  Style->BorderColor  = BaseStyle->BorderColor;
            if (!HasFlag(Style->Flags, UIStyleNode_HasSoftness))     Style->Softness     = BaseStyle->Softness;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderColor))  Style->BorderColor  = BaseStyle->BorderColor;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderWidth))  Style->BorderWidth  = BaseStyle->BorderWidth;
            if (!HasFlag(Style->Flags, UIStyleNode_HasCornerRadius)) Style->CornerRadius = BaseStyle->CornerRadius;

            Style->Version = BaseStyle->Version;
        }
    }

    if (HasFlag(Box->Flags, UILayoutNode_DrawBackground))
    {
        ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);  
        Rect->RectBounds  = RectF32(Box->FinalX, Box->FinalY, Box->FinalWidth, Box->FinalHeight);
        Rect->Color       = Style->Color;
        Rect->CornerRadii = Style->CornerRadius;
        Rect->Softness    = Style->Softness;
    }

    if (HasFlag(Box->Flags, UILayoutNode_DrawBorders))
    {
        ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
        Rect->RectBounds  = RectF32(Box->FinalX, Box->FinalY, Box->FinalWidth, Box->FinalHeight);
        Rect->Color       = Style->BorderColor;
        Rect->CornerRadii = Style->CornerRadius;
        Rect->BorderWidth = Style->BorderWidth;
        Rect->Softness    = Style->Softness;
    }

    if (HasFlag(Box->Flags, UILayoutNode_DrawText))
    {
        for (u32 Idx = 0; Idx < Box->Text->Size; Idx++)
        {
            ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
            Rect->SampleAtlas       = 1.f;
            Rect->AtlasSampleSource = Box->Text->Characters[Idx].SampleSource;
            Rect->RectBounds        = Box->Text->Characters[Idx].Position;
            Rect->Color             = UIColor(1.f, 1.f, 1.f, 1.f);
        }
    }

    for (ui_node *SChild = SRoot->First; SChild != 0; SChild = SChild->Next)
    {
        UIPipelineBuildDrawList(Pipeline, Pass, SChild, UITree_GetLayoutNode(SChild, Pipeline));
    }
}
