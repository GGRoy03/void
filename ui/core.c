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
    vec2_unit Result = { .X = U0, .Y = U1 };
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

// [Frame Context]

internal void
UIBeginFrame()
{
    ui_state *State = &UIState;

    b32 MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    if(MouseReleased)
    {
        State->CapturedNode = 0;
        State->Intent       = UIIntent_None;
    }

    ui_pipeline_list *PipelineList  = &State->Pipelines;
    b32               IsActiveFrame = OSIsActiveFrame();
    IterateLinkedList(PipelineList->First, ui_pipeline *, Pipeline)
    {
        Pipeline->IsStale = IsActiveFrame;
    }
}

// TODO: Lerp the drag-delta or something if vsync is activated. Also, figure
// out if this introduces too much delay on the cursor and whatnot.

internal void
UIEndFrame()
{
    ui_state *State = &UIState;

    // Set the correct cursor

    vec2_f32 MouseDelta = OSGetMouseDelta();
    b32      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

    switch (State->Intent)
    {

    default:
    {
        OSSetCursor(OSCursor_Default);
    } break;

    case UIIntent_Drag:
    {
        if (State->CapturedNode && MouseMoved)
        {
            DragUISubtree(MouseDelta, State->CapturedNode, State->TargetPipeline);
            OSSetCursor(OSCursor_GrabHand);
        }
        else
        {
            OSSetCursor(OSCursor_Default);
        }
    } break;

    case UIIntent_ResizeX:
    {
        if (State->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(Vec2F32(MouseDelta.X, 0.f), State->CapturedNode, State->TargetPipeline);
        }

        OSSetCursor(OSCursor_ResizeHorizontal);
    } break;

    case UIIntent_ResizeY:
    {
        if (State->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(Vec2F32(0.f, MouseDelta.Y), State->CapturedNode, State->TargetPipeline);
        }

        OSSetCursor(OSCursor_ResizeVertical);
    } break;

    case UIIntent_ResizeXY:
    {
        if (State->CapturedNode && MouseMoved)
        {
            ResizeUISubtree(MouseDelta, State->CapturedNode, State->TargetPipeline);
        }

        OSSetCursor(OSCursor_ResizeDiagonalLeftToRight);
    } break;

    }

    SubmitRenderCommands();
}

// [Pipeline]

internal ui_pipeline *
UICreatePipeline(ui_pipeline_params Params)
{
    ui_state    *State  = &UIState;
    ui_pipeline *Result = PushArray(State->StaticData, ui_pipeline, 1);

    if(Result)
    {
        // Frame Data
        {
            memory_arena_params ArenaParams = { 0 };
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result->FrameArena = AllocateArena(ArenaParams);
        }

        // Static Data
        {
            memory_arena_params ArenaParams = { 0 };
            ArenaParams.AllocatedFromFile = __FILE__;
            ArenaParams.AllocatedFromLine = __LINE__;
            ArenaParams.ReserveSize       = Megabyte(1);
            ArenaParams.CommitSize        = Kilobyte(1);

            Result->StaticArena = AllocateArena(ArenaParams);
        }

        // Layout Tree
        {
            ui_layout_tree_params TreeParams = {0};
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.NodeDepth = Params.TreeNodeDepth;

            u64   Footprint = GetLayoutTreeFootprint(TreeParams);
            void *Memory    = PushArray(Result->StaticArena, u8, Footprint);

            Result->LayoutTree = PlaceLayoutTreeInMemory(TreeParams, Memory);
        }

        // Node Id Table
        {
            ui_node_id_table_params Params = { 0 };
            Params.GroupSize = NodeIdTable_GroupSize;
            Params.GroupCount = 32;

            u64   Footprint = GetNodeIdTableFootprint(Params);
            void *Memory    = PushArray(Result->StaticArena, u8, Footprint);

            Result->IdTable = PlaceNodeIdTableInMemory(Params, Memory);
        }

        // Registry
        {
            Result->Registry = CreateStyleRegistry(Params.ThemeFile, Result->StaticArena);
        }

        // Style Info
        {
            Result->NodesStyle = PushArray(Result->StaticArena, ui_node_style, Result->LayoutTree->NodeCapacity);
        }

        AppendToLinkedList((&State->Pipelines), Result, State->Pipelines.Count);
    }

    return Result;
}

internal void
UIPipelineBegin(ui_pipeline *Pipeline)
{
    Assert(Pipeline);

    ClearArena(Pipeline->FrameArena);
}

internal void
UIPipelineExecute(ui_pipeline *Pipeline)
{
    Assert(Pipeline);

    // NOTE: The stale optimization is not yet possible because the renderer doesn't
    // really allow a "command list" approach. Unsure, how I need to structure it such
    // that we can omit the pipeline rendering in the case that it isn't stale.

    // Unpacking
    ui_state       *State      = &UIState;
    ui_layout_node *LayoutRoot = &Pipeline->LayoutTree->Nodes[0];
    render_pass    *RenderPass = GetRenderPass(Pipeline->FrameArena, RenderPass_UI);

    // Layout
    {
        // NOTE: We set the root values to it's absolute width/height.
        // Which is probably a mistake in the long run, but works fine for now.

        Assert(LayoutRoot->Value.Width.Type == UIUnit_Float32);
        Assert(LayoutRoot->Value.Height.Type == UIUnit_Float32);

        LayoutRoot->Value.FinalWidth  = LayoutRoot->Value.Width.Float32;
        LayoutRoot->Value.FinalHeight = LayoutRoot->Value.Height.Float32;

        PreOrderMeasure(LayoutRoot, Pipeline);

        PostOrderMeasure(LayoutRoot);

        PreOrderPlace(LayoutRoot, Pipeline);
    }

    if (!State->CapturedNode)
    {
        b32      MouseIsClicked = OSIsMouseClicked(OSMouseButton_Left);
        vec2_f32 MousePosition  = OSGetMousePosition();

        ui_hit_test Hit = HitTestLayout(MousePosition, LayoutRoot, Pipeline);
        if (Hit.Success)
        {
            Assert(Hit.Node);
            Assert(Hit.Intent != UIIntent_None);

            if (MouseIsClicked)
            {
                if (HasFlag(Hit.Node->Flags, UILayoutNode_None))
                {
                    // TODO: Callback or something?
                }

                State->CapturedNode   = Hit.Node;
                State->Intent         = Hit.Intent;
                State->TargetPipeline = Pipeline;

                SetFlag(Hit.Node->Flags, UILayoutNode_IsClicked);
            }
        }

        State->Intent = Hit.Intent;
    }

    BuildDrawList(Pipeline, RenderPass, LayoutRoot);
}

internal void
BuildDrawList(ui_pipeline *Pipeline, render_pass *Pass, ui_layout_node *LayoutRoot)
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
            if(HasFlag(LayoutRoot->Flags, UILayoutNode_TextIsBound))
            {
                NewParams.AtlasTextureView = LayoutRoot->Value.DisplayText->Atlas;
                NewParams.AtlasTextureSize = LayoutRoot->Value.DisplayText->AtlasSize;
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

    ui_cached_style FinalStyle = {0};
    {
        ui_node_style *NodeStyle = GetNodeStyle(LayoutRoot->Index, Pipeline);
        if(NodeStyle)
        {
            ui_cached_style *BaseStyle = GetCachedStyle(Pipeline->Registry, NodeStyle->CachedStyleIndex);
            if(BaseStyle)
            {
                FinalStyle = BaseStyle[0];

                IterateLinkedList(NodeStyle->Overrides.First, ui_style_override *, Override)
                {
                    FinalStyle.Properties[StyleEffect_Base][Override->Property.Type] = Override->Property;
                }

                if(HasFlag(LayoutRoot->Flags, UILayoutNode_IsClicked))
                {
                    ClearFlag(LayoutRoot->Flags, UILayoutNode_IsClicked);

                    if(HasFlag(BaseStyle->Flags, CachedStyle_HasClickStyle))
                    {
                        FinalStyle = SuperposeStyle(BaseStyle, StyleEffect_Click);
                        goto Draw;
                    }
                }

                if(HasFlag(LayoutRoot->Flags, UILayoutNode_IsHovered))
                {
                    ClearFlag(LayoutRoot->Flags, UILayoutNode_IsHovered);

                    if(HasFlag(BaseStyle->Flags, CachedStyle_HasHoverStyle))
                    {
                        FinalStyle = SuperposeStyle(BaseStyle, StyleEffect_Hover);
                        goto Draw;
                    }
                }
            }
            else
            {
            }
        }
        else
        {
        }
    }

Draw:

    if (HasFlag(LayoutRoot->Flags, UILayoutNode_DrawBackground))
    {
        ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
        Rect->RectBounds  = RectF32(Box->FinalX, Box->FinalY, Box->FinalWidth, Box->FinalHeight);
        Rect->Color       = UIGetColor(&FinalStyle);
        Rect->CornerRadii = UIGetCornerRadius(&FinalStyle);
        Rect->Softness    = UIGetSoftness(&FinalStyle);
    }

    if (HasFlag(LayoutRoot->Flags, UILayoutNode_DrawBorders))
    {
        ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
        Rect->RectBounds  = RectF32(Box->FinalX, Box->FinalY, Box->FinalWidth, Box->FinalHeight);
        Rect->Color       = UIGetBorderColor(&FinalStyle);
        Rect->CornerRadii = UIGetCornerRadius(&FinalStyle);
        Rect->BorderWidth = UIGetBorderWidth(&FinalStyle);
        Rect->Softness    = UIGetSoftness(&FinalStyle);
    }

    if (HasFlag(LayoutRoot->Flags, UILayoutNode_DrawText))
    {
        ui_color TextColor = UIGetTextColor(&FinalStyle);

        for (u32 Idx = 0; Idx < Box->DisplayText->GlyphCount; ++Idx)
        {
            ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
            Rect->SampleAtlas       = 1.f;
            Rect->AtlasSampleSource = Box->DisplayText->Glyphs[Idx].Source;
            Rect->RectBounds        = Box->DisplayText->Glyphs[Idx].Position;
            Rect->Color             = TextColor;
            Rect->Softness          = 1.f;
        }
    }

    IterateLinkedList(LayoutRoot->First, ui_layout_node *, Child)
    {
        BuildDrawList(Pipeline, Pass, Child);
    }
}
