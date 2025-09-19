// [Components]

//LayoutNode = GetNextLayoutNode(&Pipeline->LayoutTree);
//LayoutNode->Base.Id = Root->Base.Id;
//
//
//u32 ParentId = ((ui_layout_node *)(Root->Base.Parent))->Base.Id;
//if (ParentId < Pipeline->StyleTree.NodeCount)
//{
//    ui_layout_node *Parent = Pipeline->LayoutTree.LayoutNodes + ParentId;
//    if (Parent)
//    {
//        ui_layout_node **First = (ui_layout_node **)&Parent->Base.First;
//        if (!First)
//        {
//            *First = LayoutNode;
//        }
//
//        ui_layout_node **Last = (ui_layout_node **)&Parent->Base.Last;
//        if (Last)
//        {
//            *Last = LayoutNode;
//        }
//
//        LayoutNode->Base.Prev = Parent->Base.Last;
//        Parent->Base.Last = LayoutNode;
//    }
//}

internal void
UIWindow(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, &Pipeline->StyleRegistery);

    // Style Node
    {
        ui_node *Node = UIGetNextNode(&Pipeline->StyleTree, UINode_Window);
        if (Node)
        {
            Node->Style = Style;

            // TODO: Link the base somewhow.
        }
    }
}

internal void
UIButton(ui_style_name StyleName, ui_pipeline *Pipeline)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, &Pipeline->StyleRegistery);

    // Style Node
    {
        ui_node *Node = UIGetNextNode(&Pipeline->LayoutTree, UINode_Button);
        if (Node)
        {
            Node->Style = Style;

            // TODO: Link the base somewhow.
        }
    }
}

// NOTE: Only supports direct glyph access for now.
// NOTE: Only supports extended ASCII for now.
// NOTE: Doesn't quite work yet, since we do not create any node.

internal void
UILabel(byte_string Text, ui_pipeline *Pipeline, ui_font *Font)
{   UNUSED(Pipeline);

    f32 TextWidth  = 0;
    f32 TextHeight = 0;

    for (u32 Idx = 0; Idx < Text.Size; Idx++)
    {
        u8 Character = Text.String[Idx];

        glyph_state State = FindGlyphEntryByDirectAccess((u32)Character, Font->GlyphTable);
        if (!State.RasterizeInfo.IsRasterized)
        {
            os_glyph_layout GlyphLayout = OSGetGlyphLayout(Character, &Font->OSFontObjects, Font->TextureSize, Font->Size);

            stbrp_rect STBRect = {0};
            STBRect.w = (u16)GlyphLayout.Size.X; Assert(STBRect.w == GlyphLayout.Size.X);
            STBRect.h = (u16)GlyphLayout.Size.Y; Assert(STBRect.h == GlyphLayout.Size.Y);
            stbrp_pack_rects(&Font->AtlasContext, &STBRect, 1);

            if (STBRect.was_packed)
            {
                rect_f32 Rect;
                Rect.Min.X = (f32)STBRect.x;
                Rect.Min.Y = (f32)STBRect.y;
                Rect.Max.X = (f32)STBRect.x + STBRect.w;
                Rect.Max.Y = (f32)STBRect.y + STBRect.h;
                os_glyph_rasterize_info RasterInfo = OSRasterizeGlyph(Character, Rect, Font->TextureSize, &Font->OSFontObjects, &Font->GPUFontObjects);

                UpdateDirectGlyphTableEntry((u32)Character, GlyphLayout, RasterInfo, Font->GlyphTable);
            }
            else
            {
                OSLogMessage(byte_string_literal("Failed to pack rect."), OSMessage_Error);
            }

            TextWidth += GlyphLayout.Size.X;
            TextHeight = GlyphLayout.Size.Y > TextHeight ? GlyphLayout.Size.Y : TextHeight;
        }
        else
        {
            TextWidth += State.Layout.Size.X;
            TextHeight = State.Layout.Size.Y > TextHeight ? State.Layout.Size.Y : TextHeight;
        }
    }

    // TODO: Create the style node... Then how do we draw text?
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
UIGetCachedNameFromStyleName(byte_string Name, ui_style_registery *Registery)
{
    ui_style_name Result = {0};

    if (Name.Size)
    {
        ui_cached_style *CachedStyle = 0;
        ui_cached_style *Sentinel    = UIGetStyleSentinel(Name, Registery);

        for (CachedStyle = Sentinel->Next; CachedStyle != 0; CachedStyle = CachedStyle->Next)
        {
            byte_string CachedString = Registery->CachedName[CachedStyle->Index].Value;
            if (ByteStringMatches(Name, CachedString))
            {
                Result = Registery->CachedName[CachedStyle->Index];
                break;
            }
        }
    }

    return Result;
}

internal ui_style
UIGetStyleFromCachedName(ui_style_name Name, ui_style_registery *Registery)
{
    ui_style Result = { 0 };

    if (Registery)
    {
        ui_cached_style *Sentinel = UIGetStyleSentinel(Name.Value, Registery);
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

    return Result;
}

// [Tree]

internal void *
GetParentNodeFromTree(ui_tree *Tree)
{   Assert(Tree);

    void *Result = 0;

    if(Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal void
PushParentNodeInTree(void *Node, ui_tree *Tree)
{   Assert(Node && Tree);

    if(Tree->ParentTop < Tree->MaximumDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Node;
    }
}

internal void
PopParentNodeFromTree(ui_tree *Tree)
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

internal ui_tree
AllocateUITree(ui_tree_params Params)
{   Assert(Params.Depth > 0 && Params.NodeCount > 0 && Params.Type != UITree_None);

    ui_tree Result = {0};

    memory_arena *Arena = 0;
    {
        size_t Footprint =  0;
        Footprint += Params.Depth * sizeof(void *);

        switch(Params.Type)
        {

        case UITree_Style:
        {
            Footprint += Params.NodeCount * sizeof(ui_style);         // Node Array
        } break;

        case UITree_Layout:
        {
            Footprint += Params.NodeCount * sizeof(ui_layout_box);    // Node Array
            Footprint += Params.NodeCount * sizeof(ui_node *);        // Layout Queue
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
        Result.Nodes        = PushArray(Arena, ui_node, Params.NodeCount);
        Result.ParentStack  = PushArray(Arena, ui_node *, Params.Depth    );
        Result.MaximumDepth = Params.Depth;
        Result.NodeCapacity = Params.NodeCount;
        Result.Type         = Params.Type;
    }

    return Result;
}

// [Pipeline Helpers]

internal b32
IsParallelUINode(ui_node *Node1, ui_node *Node2)
{
    b32 Result = (Node1->Id == Node2->Id);
    return Result;
}

internal b32
IsUINodeALeaf(UINode_Type Type)
{
    b32 Result = (Type == UINode_Button);
    return Result;
}

// [Pipeline]

internal ui_pipeline
UICreatePipeline(ui_pipeline_params Params)
{
    ui_pipeline Result = {0};

    // Trees
    {
        {
            ui_tree_params TreeParams = {0};
            TreeParams.Depth     = Params.TreeDepth;
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.Type      = UITree_Style;

            Result.StyleTree = AllocateUITree(TreeParams);
        }

        {
            ui_tree_params TreeParams = { 0 };
            TreeParams.Depth     = Params.TreeDepth;
            TreeParams.NodeCount = Params.TreeNodeCount;
            TreeParams.Type      = UITree_Layout;

            Result.LayoutTree = AllocateUITree(TreeParams);
        }
    }

    // Registery
    {
        LoadThemeFiles(Params.ThemeFiles, Params.ThemeCount, &Result.StyleRegistery);
    }

    return Result;
}

internal void
UIPipelineExecute(ui_pipeline *Pipeline)
{   Assert(Pipeline);

    // Unpacking
    ui_node     *Root       = &Pipeline->StyleTree.Nodes[0];
    render_pass *RenderPass = GetRenderPass(0, 0, RenderPass_UI);

    UIPipelineSynchronize(Pipeline, Root);

    UIPipelineTopDownLayout(Pipeline);

    UIPipelineCollectDrawList(Pipeline, RenderPass, Root);
}

internal void
UIPipelineSynchronize(ui_pipeline *Pipeline, ui_node *Root)
{
    // Unpacking

    ui_node *LNode       = &Pipeline->LayoutTree.Nodes[Root->Id];
    ui_node *SNodeParent = (ui_node *)Root->Parent;

    if (!IsParallelUINode(Root, LNode))
    {
        LNode = UIGetNextNode(&Pipeline->LayoutTree, Root->Type);
        LNode->Id = Root->Id;
  
        u32 ParentId = SNodeParent->Id;
        if (ParentId < Pipeline->StyleTree.NodeCount)
        {
            ui_node *Parent = Pipeline->LayoutTree.Nodes + ParentId;
            if (Parent)
            {
                ui_node **First = (ui_node **)&Parent->First;
                if (!First)
                {
                    *First = LNode;
                }

                ui_node **Last = (ui_node **)&Parent->Last;
                if (Last)
                {
                    *Last = LNode;
                }

                LNode->Prev  = *Last;
                Parent->Last = LNode;
            }
        }
    }

    // NOTE: This looks a bit stupid at first glance..
    // But it allows to completely separate the layout from the styling tree?
    // In some of the passes, at least. Uhmmm, it does make it so that
    // the style tree is the unique source of truth again... Seems
    // really cheap as well? Just need to figure out the flags then...
    {
        LNode->Layout.Width   = Root->Style.Size.X;
        LNode->Layout.Height  = Root->Style.Size.Y;
        LNode->Layout.Padding = Root->Style.Padding;
        LNode->Layout.Spacing = Root->Style.Spacing;
        LNode->Layout.Flags   = UILayoutBox_DrawBackground; // TODO: Where are these coming from?
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

            Queue = BeginNodeQueue(Params, StyleTree->Arena);
        }

        if (Queue.Data)
        {
            PushNodeQueue(&Queue, &LayoutTree->Nodes[0]);

            while (!IsNodeQueueEmpty(&Queue))
            {
                ui_node        *Current = PopNodeQueue(&Queue);
                ui_layout_box  *Box     = &Current->Layout;

                f32 AvailableWidth  = Box->Width  - (Box->Padding.X + Box->Padding.Z);
                f32 AvailableHeight = Box->Height - (Box->Padding.Y + Box->Padding.W);
                
                f32 ClientX = Box->ClientX + Box->Padding.X;
                f32 ClientY = Box->ClientY + Box->Padding.Y;
                
                // TODO: Simplify this.

                // Positioning
                if (Box->Flags & UILayoutBox_FlowRow)
                {
                    for (ui_node *ChildNode = Current->First; (ChildNode != 0 && AvailableWidth); ChildNode = ChildNode->Next)
                    {
                        ChildNode->Layout.ClientX = ClientX;
                        ChildNode->Layout.ClientY = ClientY;
                
                        ui_layout_box *ChildBox = &ChildNode->Layout;
                        ChildBox->Width  = ChildBox->Width  <= AvailableWidth  ? ChildBox->Width  : AvailableWidth;
                        ChildBox->Height = ChildBox->Height <= AvailableHeight ? ChildBox->Height : AvailableHeight;
                
                        f32 OccupiedWidth = ChildBox->Width + Box->Spacing.X;
                        ClientX        += OccupiedWidth;
                        AvailableWidth -= OccupiedWidth;

                        PushNodeQueue(&Queue, ChildNode);
                    }
                }
                else if (Box->Flags & UILayoutBox_FlowColumn)
                {
                    for (ui_node *ChildNode = Current->First; (ChildNode != 0 && AvailableHeight); ChildNode = ChildNode->Next)
                    {
                        ChildNode->Layout.ClientX = ClientX;
                        ChildNode->Layout.ClientY = ClientY;
                
                        ui_layout_box *ChildBox = &ChildNode->Layout;
                        ChildBox->Width  = ChildBox->Width  <= AvailableWidth  ? ChildBox->Width  : AvailableWidth;
                        ChildBox->Height = ChildBox->Height <= AvailableHeight ? ChildBox->Height : AvailableHeight;
                
                        f32 OccupiedHeight = ChildBox->Height + Box->Spacing.Y;
                        ClientY         += OccupiedHeight;
                        AvailableHeight -= OccupiedHeight;
                
                        PushNodeQueue(&Queue, ChildNode);
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

internal void
UIPipelineCollectDrawList(ui_pipeline *Pipeline, render_pass *Pass, ui_node *Root)
{
    ui_style      *Style = &Root->Style;
    ui_layout_box *Box   = &Pipeline->LayoutTree.Nodes[Root->Id].Layout;

    render_batch_list *List = 0;
    rect_group_node   *Node = 0;
    {
        Node = Pass->Params.UI.Params.Last;

        // TODO: Merge Check
        b32 CanMerge = 1;
        {
            // TOOD: Clip Check
            if(Box->Flags & UILayoutBox_HasClip)
            {

            }

        }

        // Alloc Check
        {
            if (!Node || !CanMerge)
            {
                Node = PushArray(0, rect_group_node, 1);
                Node->BatchList.BytesPerInstance = sizeof(render_rect);
            }
        }

        List = &Node->BatchList;
    }

    if (Box->Flags & UILayoutBox_DrawBackground)
    {
        render_rect *Rect = PushDataInBatchList(0, List);

        // Rect
        {
            Rect->RectBounds  = Vec4F32(Box->ClientX, Box->ClientY, Box->ClientX + Box->Width, Box->ClientY + Box->Height);
            Rect->Color       = Style->Color;
            Rect->BorderWidth = 0;
            Rect->CornerRadii = Style->CornerRadius;
            Rect->SampleAtlas = 0;
            Rect->Softness    = Style->Softness;
        }
    }

    if (Box->Flags & UILayoutBox_DrawBorders)
    {
        render_rect *Rect = PushDataInBatchList(0, List);

        // Rect
        {
            Rect->RectBounds  = Vec4F32(Box->ClientX, Box->ClientY, Box->ClientX + Box->Width, Box->ClientY + Box->Height);
            Rect->Color       = Style->BorderColor;
            Rect->BorderWidth = (f32)Style->BorderWidth;
            Rect->CornerRadii = Style->CornerRadius;
            Rect->SampleAtlas = 0;
            Rect->Softness    = Style->Softness;
        }
    }

    for (ui_node *Child = Root->First; Child != 0; Child = Child->Next)
    {
        UIPipelineCollectDrawList(Pipeline, Pass, Child);
    }
}