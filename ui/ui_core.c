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
    ui_node *Node  = UIGetNextNode(&Pipeline->StyleTree, UINode_Window);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UILinkNodes(Node, UIGetParentNode(&Pipeline->StyleTree));
        UIPushParentNode(Node, &Pipeline->StyleTree);

        if(Node->Style.BorderWidth > 0)
        {
            UISetFlagBinding(Node, 1, UILayoutNode_DrawBorders, Pipeline);
        }

        UISetFlagBinding(Node, 1, UILayoutNode_DrawBackground         , Pipeline);
        UISetFlagBinding(Node, 1, UILayoutNode_PlaceChildrenVertically, Pipeline);
        UISetFlagBinding(Node, 1, UILayoutNode_IsDraggable            , Pipeline);
    }
}

internal void
UIButton(ui_style_name StyleName, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UIGetNextNode(&Pipeline->StyleTree, UINode_Button);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;
        UILinkNodes(Node, UIGetParentNode(&Pipeline->StyleTree));

        // Draw Flags
        {
            if (Node->Style.BorderWidth > 0)
            {
                UISetFlagBinding(Node, 1, UILayoutNode_DrawBorders, Pipeline);
            }
            UISetFlagBinding(Node, 1, UILayoutNode_DrawBackground, Pipeline);
        }

        // Callbacks (What about hovers?)
        if (Callback)
        {
            UISetClickCallbackBinding(Node, Callback, Pipeline);
        }
    }
}

// NOTE: Only supports direct glyph access for now.
// NOTE: Only supports extended ASCII for now.

internal void
UILabel(ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UIGetNextNode(&Pipeline->StyleTree, UINode_Label);
    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;
        UILinkNodes(Node, UIGetParentNode(&Pipeline->StyleTree));

        // Draw Binds
        {
            if (Node->Style.BorderWidth > 0)
            {
                UISetFlagBinding(Node, 1, UILayoutNode_DrawBorders, Pipeline);
            }

            if (Text.Size > 0)
            {
                UISetFlagBinding(Node, 1, UILayoutNode_DrawText, Pipeline);
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
        UISetTextBinding(Pipeline, Characters, CharacterCount, Font, Node);
    }
}

internal void
UIHeader(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyle(StyleName, &Pipeline->StyleRegistery);
    ui_node *Node  = UIGetNextNode(&Pipeline->StyleTree, UINode_Header);

    if (Node)
    {
        Node->Style = Style;
        Node->Id    = NextId++;

        UILinkNodes(Node, UIGetParentNode(&Pipeline->StyleTree));
        UIPushParentNode(Node, &Pipeline->StyleTree);

        if (Node->Style.BorderWidth > 0)
        {
            UISetFlagBinding(Node, 1, UILayoutNode_DrawBorders, Pipeline);
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
            byte_string CachedString = Registery->CachedName[CachedStyle->Index].Value;
            if (ByteStringMatches(Name, CachedString, StringMatch_CaseSensitive))
            {
                Result = Registery->CachedName[CachedStyle->Index];
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
                byte_string CachedString = Registery->CachedName[CachedStyle->Index].Value;
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

// [Tree]

internal ui_node *
UIGetParentNode(ui_tree *Tree)
{   Assert(Tree);

    ui_node *Result = 0;

    if(Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal void
UIPushParentNode(void *Node, ui_tree *Tree)
{   Assert(Node && Tree);

    if(Tree->ParentTop < Tree->MaximumDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Node;
    }
}

internal void
UIPopParentNode(ui_tree *Tree)
{   Assert(Tree);

    if (Tree->ParentTop > 0)
    {
        --Tree->ParentTop;
    }
}

internal ui_node *
UIGetNextNode(ui_tree *Tree, UINode_Type Type)
{
    ui_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Type = Type;

        ++Tree->NodeCount;
    }

    return Result;
}

internal ui_node *
UIGetLayoutNodeFromStyleNode(ui_node *Node, ui_pipeline *Pipeline)
{
    ui_node *Result = 0;

    if (Node->Id < Pipeline->LayoutTree.NodeCapacity)
    {
        Result = Pipeline->LayoutTree.Nodes + Node->Id;
    }

    return Result;
}

internal ui_node *
UIGetStyleNodeFromLayoutNode(ui_node *Node, ui_pipeline *Pipeline)
{
    ui_node *Result = 0;
    if (Node->Id < Pipeline->StyleTree.NodeCapacity)
    {
        Result = Pipeline->StyleTree.Nodes + Node->Id;
    }

    return Result;
}

internal ui_tree
UIAllocateTree(ui_tree_params Params)
{   Assert(Params.Depth > 0 && Params.NodeCount > 0 && Params.Type != UITree_None);

    ui_tree Result = {0};

    memory_arena *Arena = 0;
    {
        size_t Footprint =  0;
        Footprint += Params.Depth * sizeof(ui_node *);           // Parent Stack

        switch(Params.Type)
        {

        case UITree_Style:
        {
            Footprint += Params.NodeCount * sizeof(ui_node);     // Node Array
        } break;

        case UITree_Layout:
        {
            Footprint += Params.NodeCount * sizeof(ui_node );    // Node Array
            Footprint += Params.NodeCount * sizeof(ui_node*);    // Layout Queue
        } break;

        default:
        {
            OSLogMessage(byte_string_literal("Invalid Tree Type."), OSMessage_Error);
        } break;

        }

        memory_arena_params ArenaParams = {0};
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Footprint;
        ArenaParams.CommitSize        = Footprint;

        Arena = AllocateArena(ArenaParams);
    }

    if(Arena)
    {
        Result.Arena        = Arena;
        Result.Nodes        = PushArray(Arena, ui_node  , Params.NodeCount);
        Result.ParentStack  = PushArray(Arena, ui_node *, Params.Depth    );
        Result.MaximumDepth = Params.Depth;
        Result.NodeCapacity = Params.NodeCount;
        Result.Type         = Params.Type;
    }

    return Result;
}

// [Bindings]

internal void
UISetClickCallbackBinding(ui_node *Node, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_node *LNode = UIGetLayoutNodeFromStyleNode(Node, Pipeline);
    if (LNode)
    {
        LNode->Layout.ClickCallback = Callback;
    }
}

internal void
UISetTextBinding(ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_node *Node)
{   Assert(Node->Id < Pipeline->LayoutTree.NodeCapacity);

    ui_node *LNode = UIGetLayoutNodeFromStyleNode(Node, Pipeline);
    if (LNode && Font)
    {
        LNode->Layout.Text = PushArray(Pipeline->StaticArena, ui_text, 1);
        LNode->Layout.Text->AtlasTexture     = RenderHandle((u64)Font->GPUFontObjects.GlyphCacheView);
        LNode->Layout.Text->AtlasTextureSize = Font->TextureSize;
        LNode->Layout.Text->Size             = Count;
        LNode->Layout.Text->Characters       = Characters;
        LNode->Layout.Text->LineHeight       = Font->LineHeight;
    }
    else
    {
        OSLogMessage(byte_string_literal("Could not set tex binding. Font is invalid."), OSMessage_Error);
    }
}

internal void
UISetFlagBinding(ui_node *Node, b32 Set, UILayoutNode_Flag Flag, ui_pipeline *Pipeline)
{   Assert((Flag >= UILayoutNode_NoFlag && Flag <= UILayoutNode_IsDraggable));

    ui_node *LNode = UIGetLayoutNodeFromStyleNode(Node, Pipeline);
    if(LNode)
    {
        if(Set)
        {
            SetFlag(LNode->Layout.Flags, Flag);
        }
        else
        {
            ClearFlag(LNode->Layout.Flags, Flag);
        }
    }
}

// [Pipeline Helpers]

internal b32
UIIsParallelNode(ui_node *Node1, ui_node *Node2)
{
    b32 Result = (Node1->Id == Node2->Id);
    return Result;
}

internal b32
UIIsNodeALeaf(UINode_Type Type)
{
    b32 Result = (Type == UINode_Button);
    return Result;
}

internal void
UILinkNodes(ui_node *Node, ui_node *Parent)
{
    Node->Last   = 0;
    Node->Next   = 0;
    Node->First  = 0;
    Node->Parent = Parent;

    if (Parent)
    {
        ui_node **First = (ui_node **)&Parent->First;
        if (!*First)
        {
            *First = Node;
        }

        ui_node **Last = (ui_node **)&Parent->Last;
        if (*Last)
        {
            (*Last)->Next = Node;
        }

        Node->Prev   = Parent->Last;
        Parent->Last = Node;
    }
}

// [Pipeline]

internal ui_pipeline
UICreatePipeline(ui_pipeline_params Params, ui_state *UIState)
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

            Result.StyleTree = UIAllocateTree(TreeParams);
        }

        {
            ui_tree_params TreeParams = { 0 };
            TreeParams.Depth     = Params.TreeDepth;
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.Type      = UITree_Layout;

            Result.LayoutTree = UIAllocateTree(TreeParams);
        }

        for (u32 Idx = 0; Idx < Result.StyleTree.NodeCapacity; Idx++)
        {
            Result.StyleTree.Nodes[Idx].Id = InvalidNodeId;
        }

        for (u32 Idx = 0; Idx < Result.LayoutTree.NodeCapacity; Idx++)
        {
            Result.LayoutTree.Nodes[Idx].Id = InvalidNodeId;
        }
    }

    // Others
    {
        LoadThemeFiles(Params.ThemeFiles, Params.ThemeCount, &Result.StyleRegistery, Params.RendererHandle, UIState);
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
    ui_node     *SRoot      = &Pipeline->StyleTree.Nodes[0];
    ui_node     *LRoot      = &Pipeline->LayoutTree.Nodes[0];
    render_pass *RenderPass = GetRenderPass(Pipeline->FrameArena, PassList, RenderPass_UI);

    UIPipelineSynchronize(Pipeline, SRoot);

    UIPipelineTopDownLayout(Pipeline);

    // Hit-Testing
    {
        vec2_f32 MousePosition = OSGetMousePosition();
        vec2_f32 MouseDelta    = OSGetMouseDelta();
        b32      MouseClicked  = OSIsMouseClicked(OSMouseButton_Left);
        b32      MouseReleased = OSIsMouseReleased(OSMouseButton_Left);

        ui_node *Node = UIPipelineHitTest(Pipeline, MousePosition, LRoot);
        if (Node)
        {
            if (MouseClicked && Node->Layout.ClickCallback)
            {
                Node->Layout.ClickCallback(Node, Pipeline);
            }

            if (MouseClicked)
            {
                SetFlag(Node->Layout.Flags, UILayoutNode_IsClicked);
            }

            SetFlag(Node->Layout.Flags, UILayoutNode_IsHovered);
        }

        // Dragging Behavior (TODO: Remove the capture from the pipeline?)
        {
            if (MouseReleased)
            {
                Pipeline->CurrentDragCaptureNode = 0;
            }

            if (!Pipeline->CurrentDragCaptureNode && MouseClicked)
            {
                if (Node && HasFlag(Node->Layout.Flags, UILayoutNode_IsDraggable))
                {
                    Pipeline->CurrentDragCaptureNode = Node;
                }
            }

            if (Pipeline->CurrentDragCaptureNode)
            {
                UIPipelineDragNodes(MouseDelta, Pipeline, Pipeline->CurrentDragCaptureNode);
            }
        }

    }

    UIPipelineBuildDrawList(Pipeline, RenderPass, SRoot, LRoot);
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
UIPipelineSynchronize(ui_pipeline *Pipeline, ui_node *Root)
{
    ui_node *LNode       = &Pipeline->LayoutTree.Nodes[Root->Id];
    ui_node *SNodeParent = (ui_node *)Root->Parent;

    if (!UIIsParallelNode(Root, LNode))
    {
        LNode = UIGetNextNode(&Pipeline->LayoutTree, Root->Type);
        LNode->Id = Root->Id;

        ui_node *LNodeParent = 0;
        if (SNodeParent && SNodeParent->Id < Pipeline->StyleTree.NodeCount)
        {
            LNodeParent = Pipeline->LayoutTree.Nodes + SNodeParent->Id;
        }
        UILinkNodes(LNode, LNodeParent);
    }

    {
        LNode->Layout.Width    = Root->Style.Size.X;
        LNode->Layout.Height   = Root->Style.Size.Y;
        LNode->Layout.Padding  = Root->Style.Padding;
        LNode->Layout.Spacing  = Root->Style.Spacing;
    }

    for (ui_node *Child = Root->First; Child != 0; Child = Child->Next)
    {
        UIPipelineSynchronize(Pipeline, Child);
    }
}

internal void
UIPipelineTopDownLayout(ui_pipeline *Pipeline)
{   Assert(Pipeline);

    ui_tree *StyleTree  = &Pipeline->StyleTree;
    ui_tree *LayoutTree = &Pipeline->LayoutTree;

    if (StyleTree && LayoutTree)
    {
        node_queue Queue = { 0 };
        {
            typed_queue_params Params = { 0 };
            Params.QueueSize = LayoutTree->NodeCount;

            Queue = BeginNodeQueue(Params, LayoutTree->Arena);
        }

        if (Queue.Data)
        {
            ui_node       *RootLayout = &LayoutTree->Nodes[0];
            ui_layout_box *RootBox    = &RootLayout->Layout;

            if (RootBox->Width.Type != UIUnit_Float32)
            {
                return; // TODO: Error.
            }

            RootBox->FinalWidth  = RootBox->Width.Float32;
            RootBox->FinalHeight = RootBox->Height.Float32;

            PushNodeQueue(&Queue, RootLayout);

            while (!IsNodeQueueEmpty(&Queue))
            {
                ui_node        *Current = PopNodeQueue(&Queue);
                ui_layout_box  *Box     = &Current->Layout;
                
                f32 AvWidth    = Box->FinalWidth  - (Box->Padding.Left + Box->Padding.Right);
                f32 AvHeight   = Box->FinalHeight - (Box->Padding.Top  + Box->Padding.Bot);
                f32 UsedWidth  = 0;
                f32 UsedHeight = 0;
                f32 BasePosX   = Box->FinalX + Box->Padding.Left;
                f32 BasePosY   = Box->FinalY + Box->Padding.Top;

                {
                    for (ui_node *ChildNode = Current->First; (ChildNode != 0 && UsedWidth <= AvWidth); ChildNode = ChildNode->Next)
                    {
                        ChildNode->Layout.FinalX = BasePosX + UsedWidth;
                        ChildNode->Layout.FinalY = BasePosY + UsedHeight;

                        ui_layout_box *ChildBox = &ChildNode->Layout;

                        f32 Width  = 0;
                        f32 Height = 0;
                        {
                            if (ChildBox->Width.Type == UIUnit_Percent)
                            {
                                Width = (ChildBox->Width.Percent / 100.f) * AvWidth;
                            }
                            else if (ChildBox->Width.Type == UIUnit_Float32)
                            {
                                Width = ChildBox->Width.Float32 <= AvWidth ? ChildBox->Width.Float32 : 0.f;
                            }
                            else
                            {
                                return;
                            }

                            if (ChildBox->Height.Type == UIUnit_Percent)
                            {
                                Height = (ChildBox->Height.Percent / 100.f) * AvHeight;
                            }
                            else if (ChildBox->Height.Type == UIUnit_Float32)
                            {
                                Height = ChildBox->Height.Float32 <= AvHeight ? ChildBox->Height.Float32 : 0.f;
                            }
                            else
                            {
                                return;
                            }

                            ChildBox->FinalWidth  = Width;
                            ChildBox->FinalHeight = Height;
                        }

                        if (Box->Flags & UILayoutNode_PlaceChildrenVertically)
                        {
                            UsedHeight += Height + Box->Spacing.Vertical;
                        }
                        else
                        {
                            UsedWidth += Width + Box->Spacing.Horizontal;
                        }

                        PushNodeQueue(&Queue, ChildNode);
                    }
    }

                // Text Position
                // TODO: Text wrapping and stuff.
                // TODO: Text Alignment.
                {
                    if (Box->Flags & UILayoutNode_DrawText)
                    {
                        ui_text *Text    = Box->Text;
                        vec2_f32 TextPos = Vec2F32(BasePosX, BasePosY);

                        for (u32 Idx = 0; Idx < Text->Size; Idx++)
                        {
                            ui_character *Character = &Text->Characters[Idx];
                            Character->Position.Min.X = roundf(TextPos.X + Character->Layout.Offset.X);
                            Character->Position.Min.Y = roundf(TextPos.Y + Character->Layout.Offset.Y);
                            Character->Position.Max.X = roundf(Character->Position.Min.X + Character->Layout.AdvanceX);
                            Character->Position.Max.Y = roundf(Character->Position.Min.Y + Text->LineHeight);

                            TextPos.X += (Character->Position.Max.X) - (Character->Position.Min.X);
                        }
                    }
                }

            }

            // NOTE: Not really clear that we are clearing the queue...
            PopArena(LayoutTree->Arena, LayoutTree->NodeCount * sizeof(ui_node *));
        }
        else
        {
            OSLogMessage(byte_string_literal("Failed to allocate queue for (Top Down Layout) ."), OSMessage_Error);
        }
    }
    else
    {
        OSLogMessage(byte_string_literal("Unable to compute (Top Down Layout), because one of the trees is invalid."), OSMessage_Error);
    }
}

internal ui_node *
UIPipelineHitTest(ui_pipeline *Pipeline, vec2_f32 MousePosition, ui_node *LRoot)
{
    ui_layout_box *Box = &LRoot->Layout;

    rect_f32 BoundingBox = RectF32(Box->FinalX, Box->FinalY, Box->Width.Float32, Box->Height.Float32);
    if (IsPointInRect(BoundingBox, MousePosition))
    {
        for (ui_node *Child = LRoot->Last; Child != 0; Child = Child->Prev)
        {
            ui_node *Hit = UIPipelineHitTest(Pipeline, MousePosition, Child);

            if (Hit)
            {
                return Hit;
            }
        }

        return LRoot;
    }

    return 0;
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
        ui_node *LChild = &Pipeline->LayoutTree.Nodes[SChild->Id];
        UIPipelineBuildDrawList(Pipeline, Pass, SChild, LChild);
    }
}