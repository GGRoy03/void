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

    // Clear the state if needed
    b32 MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    if(MouseReleased)
    {
        State->CapturedNode = 0;
        State->Intent       = UIIntent_None;
    }
}

// TODO: Lerp the drag-delta or something if vsync is activated. Also, figure
// out if this introduces too much delay on the cursor and whatnot. Yeah I am not sure
// if this should even exist?

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
            Params.GroupSize  = NodeIdTable_128Bits;
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
        f32      ScrollDelta    = OSGetScrollDelta();

        ui_hit_test Hit = HitTestLayout(MousePosition, LayoutRoot, Pipeline);
        if (Hit.Success)
        {
            if (MouseIsClicked)
            {
                if (HasFlag(Hit.Node->Flags, NoFlag))
                {
                    // TODO: Callback or something?
                }

                State->CapturedNode   = Hit.Node;
                State->Intent         = Hit.Intent;
                State->TargetPipeline = Pipeline;

                SetFlag(Hit.Node->Flags, UILayoutNode_IsClicked);
            }

            if(HasFlag(Hit.Node->Flags, UILayoutNode_IsScrollRegion) && ScrollDelta != 0.f)
            {
                Assert(Hit.Node->Value.ScrollContext);
                ApplyScrollToContext(ScrollDelta, Hit.Node->Value.ScrollContext);
            }

            SetFlag(Hit.Node->Flags, UILayoutNode_IsHovered);
        }

        State->Intent = Hit.Intent;
    }

    BuildDrawList(Pipeline, RenderPass, LayoutRoot);
}

typedef struct draw_stack_frame
{
    ui_layout_node *Node;
    vec2_f32        AccScroll;
} draw_stack_frame;

DEFINE_TYPED_STACK(Draw, draw, draw_stack_frame)

internal void
BuildDrawList(ui_pipeline *Pipeline, render_pass *Pass, ui_layout_node *Root)
{
    typed_stack_params StackParams = {0};
    {
        StackParams.StackSize = Pipeline->LayoutTree->NodeCount;
    }
    draw_stack Stack = BeginDrawStack(StackParams, Pipeline->FrameArena);

    PushDrawStack(&Stack, (draw_stack_frame){.Node = Root, .AccScroll = Vec2F32(0.f, 0.f)});

    while(!IsDrawStackEmpty(&Stack))
    {
        draw_stack_frame Frame           = PopDrawStack(&Stack);
        ui_layout_node  *Node            = Frame.Node;
        vec2_f32         ParentAccScroll = Frame.AccScroll;

        // Batching

        render_batch_list     *List      = 0;
        render_pass_params_ui *UIParams  = &Pass->Params.UI.Params;
        rect_group_params      NewParams = {.Transform = Mat3x3Identity()};
        {
            rect_group_node *GroupNode = UIParams->Last;

            b32 CanMerge = 1;
            if(GroupNode)
            {
                if(HasFlag(Node->Flags, UILayoutNode_TextIsBound))
                {
                    NewParams.AtlasTextureView = Node->Value.DisplayText->Atlas;
                    NewParams.AtlasTextureSize = Node->Value.DisplayText->AtlasSize;
                }

                CanMerge = CanMergeGroupParams(&GroupNode->Params, &NewParams);
            }

            if (!GroupNode || !CanMerge)
            {
                GroupNode = PushArray(Pipeline->FrameArena, rect_group_node, 1);
                GroupNode->BatchList.BytesPerInstance = sizeof(ui_rect);

                AppendToLinkedList(UIParams, GroupNode, UIParams->Count);
            }

            GroupNode->Params = NewParams;        // BUG: This is simply wrong.
            List         = &GroupNode->BatchList;
        }

        // Styling

        style_property FinalStyle[StyleProperty_Count] = { 0 };
        {
            ui_node_style *NodeStyle = GetNodeStyle(Node->Index, Pipeline);
            if (NodeStyle)
            {
                ui_cached_style *CachedStyle = GetCachedStyle(Pipeline->Registry, NodeStyle->CachedStyleIndex);
                if (CachedStyle)
                {
                    style_property *Base = UIGetStyleEffect(CachedStyle, StyleEffect_Base);
                    MemoryCopy(FinalStyle, Base, sizeof(FinalStyle));

                    ui_style_override_list *OverrideList = &NodeStyle->Overrides;
                    IterateLinkedList(OverrideList, ui_style_override_node *, Override)
                    {
                        FinalStyle[Override->Value.Type] = Override->Value;
                    }

                    if (HasFlag(Node->Flags, UILayoutNode_IsClicked))
                    {
                        ClearFlag(Node->Flags, UILayoutNode_IsClicked);

                        if (HasFlag(CachedStyle->Flags, CachedStyle_HasClickStyle))
                        {
                            style_property *Click = UIGetStyleEffect(CachedStyle, StyleEffect_Click);
                            SuperposeStyle(FinalStyle, Click);
                            goto Draw;
                        }
                    }

                    if (HasFlag(Node->Flags, UILayoutNode_IsHovered))
                    {
                        ClearFlag(Node->Flags, UILayoutNode_IsHovered);

                        if (HasFlag(CachedStyle->Flags, CachedStyle_HasHoverStyle))
                        {
                            style_property *Hover = UIGetStyleEffect(CachedStyle, StyleEffect_Hover);
                            SuperposeStyle(FinalStyle, Hover);
                            goto Draw;
                        }
                    }
                }
                else
                {
                    // TODO: Log
                }
            }
            else
            {
                // TODO: Log
            }
        }

        // Drawing
Draw:

        // NOTE: Move this?
        vec2_f32 ChildAccScroll = ParentAccScroll;
        if (HasFlag(Node->Flags, UILayoutNode_IsScrollRegion))
        {
            PruneScrollContextNodes(Node);
            vec2_f32 NodeScroll = GetScrollTranslation(Node);

            ChildAccScroll.X += NodeScroll.X;
            ChildAccScroll.Y += NodeScroll.Y;
        }


        ui_layout_box *Box = &Node->Value;
        rect_f32 RectBounds = RectF32(Box->FinalX, Box->FinalY, Box->FinalWidth, Box->FinalHeight);
        TranslateRect(&RectBounds, ParentAccScroll);

        if (HasFlag(Node->Flags, UILayoutNode_DrawBackground))
        {
            ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
            Rect->RectBounds  = RectBounds;
            Rect->Color       = FinalStyle[StyleProperty_Color].Color;
            Rect->CornerRadii = FinalStyle[StyleProperty_CornerRadius].CornerRadius;
            Rect->Softness    = FinalStyle[StyleProperty_Softness].Float32;
        }

        if (HasFlag(Node->Flags, UILayoutNode_DrawBorders))
        {
            ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
            Rect->RectBounds  = RectBounds;
            Rect->Color       = FinalStyle[StyleProperty_BorderColor].Color;
            Rect->CornerRadii = FinalStyle[StyleProperty_CornerRadius].CornerRadius;
            Rect->BorderWidth = FinalStyle[StyleProperty_BorderWidth].Float32;
            Rect->Softness    = FinalStyle[StyleProperty_Softness].Float32;
        }

        if (HasFlag(Node->Flags, UILayoutNode_DrawText))
        {
            ui_color TextColor = FinalStyle[StyleProperty_TextColor].Color;

            for (u32 Idx = 0; Idx < Box->DisplayText->GlyphCount; ++Idx)
            {
                rect_f32 GlyphPos = Box->DisplayText->Glyphs[Idx].Position;
                TranslateRect(&GlyphPos, ParentAccScroll);

                ui_rect *Rect = PushDataInBatchList(Pipeline->FrameArena, List);
                Rect->SampleAtlas       = 1.f;
                Rect->AtlasSampleSource = Box->DisplayText->Glyphs[Idx].Source;
                Rect->RectBounds        = GlyphPos;
                Rect->Color             = TextColor;
                Rect->Softness          = 1.f;
            }
        }

        IterateLinkedListBackward(Node, ui_layout_node *, Child)
        {
            if (!(HasFlag(Child->Flags, UILayoutNode_IsPruned)))
            {
                PushDrawStack(&Stack, (draw_stack_frame){.Node = Child, .AccScroll = ChildAccScroll});
            }
        }

    }
}
