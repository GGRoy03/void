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

internal void
UIWindow(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = UIGetStyle(StyleName, Pipeline->StyleRegistery);
    ui_layout_node  *Node  = GetFreeLayoutNode(Pipeline->LayoutTree, UILayoutNode_Window);

    if (Node)
    {
        AttachLayoutNode    (Node, GetLayoutNodeParent(Pipeline->LayoutTree));
        SetLayoutNodeStyle  (Style, Node, SetLayoutNodeStyle_NoFlag);
        PushLayoutNodeParent(Node, Pipeline->LayoutTree);

        if(Style->Value.BorderWidth > 0)
        {
            SetFlag(Node->Value.Flags, UILayoutNode_DrawBorders);
        }

        SetFlag(Node->Value.Flags, UILayoutNode_DrawBackground         );
        SetFlag(Node->Value.Flags, UILayoutNode_PlaceChildrenVertically);
        SetFlag(Node->Value.Flags, UILayoutNode_IsDraggable            );
        SetFlag(Node->Value.Flags, UILayoutNode_IsResizable            );
    }
}

internal void
UIButton(ui_style_name StyleName, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = UIGetStyle(StyleName, Pipeline->StyleRegistery);
    ui_layout_node  *Node  = GetFreeLayoutNode(Pipeline->LayoutTree, UILayoutNode_Button);

    if (Node)
    {
        AttachLayoutNode  (Node, GetLayoutNodeParent(Pipeline->LayoutTree));
        SetLayoutNodeStyle(Style, Node, SetLayoutNodeStyle_NoFlag);

        // Draw Flags
        {
            if (Style->Value.BorderWidth > 0)
            {
                SetFlag(Node->Value.Flags, UILayoutNode_DrawBorders);
            }

            SetFlag(Node->Value.Flags, UILayoutNode_DrawBackground);
        }

        // Callbacks (What about hovers?)
        if (Callback)
        {
            UITree_BindClickCallback(Node, Callback);
        }
    }
}

// NOTE: Only supports direct glyph access for now.
// NOTE: Only supports extended ASCII for now.

internal void
UILabel(ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = UIGetStyle(StyleName, Pipeline->StyleRegistery);
    ui_layout_node  *Node  = GetFreeLayoutNode(Pipeline->LayoutTree, UILayoutNode_Label);

    if (Node)
    {
        AttachLayoutNode  (Node, GetLayoutNodeParent(Pipeline->LayoutTree));
        SetLayoutNodeStyle(Style, Node, SetLayoutNodeStyle_NoFlag);

        // Draw Flags
        {
            if (Style->Value.BorderWidth > 0)
            {
                SetFlag(Node->Value.Flags, UILayoutNode_DrawBorders);
            }

            if (Text.Size > 0)
            {
                SetFlag(Node->Value.Flags, UILayoutNode_DrawText);
            }
        }

        ui_unit TextWidth  = {UIUnit_Float32, 0};
        ui_unit TextHeight = {UIUnit_Float32, 0};

        ui_font      *Font           = Style->Value.Font.Ref;
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

        Node->Value.Width  = TextWidth;
        Node->Value.Height = TextHeight;
        UITree_BindText(Pipeline, Characters, CharacterCount, Font, Node);
    }
}

internal void
UIHeader(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = UIGetStyle(StyleName, Pipeline->StyleRegistery);
    ui_layout_node  *Node  = GetFreeLayoutNode(Pipeline->LayoutTree, UILayoutNode_Header);

    if (Node)
    {
        AttachLayoutNode    (Node, GetLayoutNodeParent(Pipeline->LayoutTree));
        SetLayoutNodeStyle  (Style, Node, SetLayoutNodeStyle_NoFlag);
        PushLayoutNodeParent(Node, Pipeline->LayoutTree);

        if (Style->Value.BorderWidth > 0)
        {
            SetFlag(Node->Value.Flags, UILayoutNode_DrawBorders);
        }
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

    // Layout Tree
    {
        ui_layout_tree_params TreeParams = {0};
        TreeParams.Depth     = Params.TreeDepth;
        TreeParams.NodeCount = Params.TreeNodeCount;

        Result.LayoutTree = AllocateLayoutTree(TreeParams);
    }

    // Others
    {
        Result.StyleRegistery = CreateStyleRegistry(Params.ThemeFiles, Params.ThemeCount, Result.StaticArena);
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
    ui_layout_node *LayoutRoot = &Pipeline->LayoutTree->Nodes[0];
    render_pass    *RenderPass = GetRenderPass(Pipeline->FrameArena, PassList, RenderPass_UI);

    // Layout
    {
        TopDownLayout(LayoutRoot, Pipeline);
    }

    vec2_f32 MouseDelta     = OSGetMouseDelta();
    b32      MouseMoved     = (MouseDelta.X != 0 || MouseDelta.Y != 0);
    b32      MouseIsClicked = OSIsMouseClicked(OSMouseButton_Left);
    b32      MouseReleased  = OSIsMouseReleased(OSMouseButton_Left);

    UIIntent_Type Intent = Pipeline->Intent;

    if (!Pipeline->CapturedNode)
    {
        ui_hit_test_result Hit = HitTestLayout(OSGetMousePosition(), LayoutRoot, Pipeline);
        if(Hit.Success)
        {
            Assert(Hit.Node);
            Assert(Hit.Intent != UIIntent_None);
        
            ui_layout_box *Box = &Hit.Node->Value;
        
            if (MouseIsClicked)
            {
                if (Box->ClickCallback)
                {
                    Box->ClickCallback(Hit.Node, Pipeline);
                }
        
                SetFlag(Box->Flags, UILayoutNode_IsClicked);
            }
        
            switch(Hit.Intent)
            {
        
            default: break;
        
            case UIIntent_Drag:
            {
                if(MouseIsClicked)
                {
                    Pipeline->CapturedNode = Hit.Node;
                    Pipeline->Intent       = Hit.Intent;
                }
                SetFlag(Box->Flags, UILayoutNode_IsHovered);
            } break;
        
            case UIIntent_ResizeX:
            case UIIntent_ResizeY:
            case UIIntent_ResizeXY:
            {
                if (MouseIsClicked)
                {
                    Pipeline->CapturedNode = Hit.Node;
                    Pipeline->Intent       = Hit.Intent;
                }
            } break;
        
            }
        }

        Intent = Hit.Intent;
    }

    if (MouseReleased)
    {
        Pipeline->CapturedNode = 0;
        Pipeline->Intent       = UIIntent_None;
    }
    
    switch (Intent)
    {

    case UIIntent_None:
    {
        OSSetCursor(OSCursor_Default);
    } break;

    case UIIntent_Drag:
    {
        if (Pipeline->CapturedNode && MouseMoved)
        {
            DragUISubtree(MouseDelta, Pipeline->CapturedNode, Pipeline);
            OSSetCursor(OSCursor_GrabHand);
        }
        else
        {
            OSSetCursor(OSCursor_Default);
        }
    } break;

    case UIIntent_ResizeX:
    {
        if (Pipeline->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(Vec2F32(MouseDelta.X, 0.f), Pipeline->CapturedNode, Pipeline);
        }

        OSSetCursor(OSCursor_ResizeHorizontal);
    } break;

    case UIIntent_ResizeY:
    {
        if (Pipeline->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(Vec2F32(0.f, MouseDelta.Y), Pipeline->CapturedNode, Pipeline);
        }

        OSSetCursor(OSCursor_ResizeVertical);
    } break;

    case UIIntent_ResizeXY:
    {
        if (Pipeline->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(MouseDelta, Pipeline->CapturedNode, Pipeline);
        }

        OSSetCursor(OSCursor_ResizeDiagonalLeftToRight);
    } break;

    }
    

    UIPipelineBuildDrawList(Pipeline, RenderPass, LayoutRoot);
}

internal void
UIPipelineBuildDrawList(ui_pipeline *Pipeline, render_pass *Pass, ui_layout_node *LayoutRoot)
{
    ui_layout_box *Box = &LayoutRoot->Value;

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

    ui_style *BaseStyle = &LayoutRoot->CachedStyle->Value;
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

        if (!Overriden && HasFlag(Box->Flags, UILayoutNode_IsHovered))
        {
            if (Style->HoverOverride)
            {
                Style     = Style->HoverOverride;
                Overriden = 1;
            }

            ClearFlag(Box->Flags, UILayoutNode_IsHovered);
        }

        if (Overriden && Style->BaseVersion != BaseStyle->BaseVersion)
        {
            if (!HasFlag(Style->Flags, UIStyleNode_HasColor))        Style->Color        = BaseStyle->Color;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderColor))  Style->BorderColor  = BaseStyle->BorderColor;
            if (!HasFlag(Style->Flags, UIStyleNode_HasSoftness))     Style->Softness     = BaseStyle->Softness;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderColor))  Style->BorderColor  = BaseStyle->BorderColor;
            if (!HasFlag(Style->Flags, UIStyleNode_HasBorderWidth))  Style->BorderWidth  = BaseStyle->BorderWidth;
            if (!HasFlag(Style->Flags, UIStyleNode_HasCornerRadius)) Style->CornerRadius = BaseStyle->CornerRadius;

            Style->BaseVersion = BaseStyle->BaseVersion;
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

    for (ui_layout_node *Child = LayoutRoot->First; Child != 0; Child = Child->Next)
    {
        UIPipelineBuildDrawList(Pipeline, Pass, Child);
    }
}