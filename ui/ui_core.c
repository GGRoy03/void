// [API]

// [Components]

internal void
UIWindow(ui_style_name StyleName, ui_layout_tree *LayoutTree, ui_style_registery *StyleRegistery)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, StyleRegistery);

    ui_layout_box LayoutBox = { 0 };
    LayoutBox.Width   = Style.Size.X;
    LayoutBox.Height  = Style.Size.Y;
    LayoutBox.Padding = Style.Padding;
    LayoutBox.Spacing = Style.Spacing;
    LayoutBox.Color   = Style.Color;
    LayoutBox.Flags   = UILayoutBox_DrawBackground | UILayoutBox_FlowColumn;

    UICreateLayoutNode(Vec2F32(400.f, 400.f), LayoutBox, 0, LayoutTree);
}

internal void
UIButton(ui_style_name StyleName, ui_layout_tree *LayoutTree, ui_style_registery *StyleRegistery)
{
    ui_style Style = UIGetStyleFromCachedName(StyleName, StyleRegistery);

    ui_layout_box LayoutBox = {0};
    LayoutBox.Width   = Style.Size.X;
    LayoutBox.Height  = Style.Size.Y;
    LayoutBox.Padding = Style.Padding;
    LayoutBox.Spacing = Style.Spacing;
    LayoutBox.Color   = Style.Color;
    LayoutBox.Flags   = UILayoutBox_DrawBackground | UILayoutBox_FlowRow;

    UICreateLayoutNode(Vec2F32(0, 0), LayoutBox, 1, LayoutTree);
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

// [Layout]

internal ui_layout_node *
UIGetParentForLayoutNode(ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if(Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal void
UIPopParentLayoutNode(ui_layout_tree *Tree)
{
    if (Tree->ParentTop > 0)
    {
        --Tree->ParentTop;
    }
}

internal ui_layout_tree
UIAllocateLayoutTree(ui_layout_tree_params Params)
{
    memory_arena_params ArenaParams = { 0 };
    ArenaParams.AllocatedFromFile = __FILE__;
    ArenaParams.AllocatedFromLine = __LINE__;
    ArenaParams.CommitSize        = ArenaDefaultCommitSize;
    ArenaParams.ReserveSize       = ArenaDefaultReserveSize;

    ui_layout_tree Result = { 0 };
    Result.MaximumDepth = Params.Depth;
    Result.NodeCapacity = Params.NodeCount;
    Result.Arena        = AllocateArena(ArenaParams);
    Result.NodeCount    = 0;
    Result.Nodes        = PushArena(Result.Arena, Result.NodeCapacity * sizeof(ui_layout_node)  , AlignOf(ui_layout_node));
    Result.ParentStack  = PushArena(Result.Arena, Result.MaximumDepth * sizeof(ui_layout_node *), AlignOf(ui_layout_node *));
    Result.ParentTop    = 0;

    return Result;
}

internal void
UICreateLayoutNode(vec2_f32 Position, ui_layout_box LayoutBox, b32 IsAlwaysLeaf, ui_layout_tree *Tree)
{
    if(Tree->NodeCount < Tree->NodeCapacity)
    {
        ui_layout_node *Node = Tree->Nodes + Tree->NodeCount++;
        Node->First     = 0;
        Node->Last      = 0;
        Node->Next      = 0;
        Node->Parent    = UIGetParentForLayoutNode(Tree);
        Node->LayoutBox = LayoutBox;
        Node->ClientX   = Position.X;
        Node->ClientY   = Position.Y;

        ui_layout_node *Parent = Node->Parent;
        if(Parent)
        {
            Node->Prev = Parent->Last;

            if (!Parent->First)
            {
                Parent->First = Node;
            }

            if (Parent->Last)
            {
                Parent->Last->Next = Node;
            }

            Parent->Last = Node;
        }

        if (!IsAlwaysLeaf)
        {
            Tree->ParentStack[Tree->ParentTop++] = Node;
        }
    }
}

internal void
UIComputeLayout(ui_layout_tree *Tree, render_context *RenderContext)
{
    // NOTE: This function has no dirty logic for now.
    // It's also only top-bottom for now. 
    // It is a very simple implementation.

    ui_layout_node **Queue     = PushArena(Tree->Arena, Tree->NodeCount * sizeof(ui_layout_node *), AlignOf(ui_layout_node *));
    u32              QueueHead = 0;
    u32              QueueTail = 0;

    Queue[QueueTail++] = &Tree->Nodes[0];

    while(QueueHead != QueueTail)
    {
        ui_layout_node *Current = Queue[QueueHead];
        ui_layout_box  *Box     = &Current->LayoutBox;

        f32 AvailableWidth  = Box->Width  - (Box->Padding.X + Box->Padding.Z);
        f32 AvailableHeight = Box->Height - (Box->Padding.Y + Box->Padding.W);

        f32 ClientX = Current->ClientX + Box->Padding.X;
        f32 ClientY = Current->ClientY + Box->Padding.Y;

        // Positioning
        {
            if (Box->Flags & UILayoutBox_FlowRow)
            {
                for (ui_layout_node *ChildNode = Current->First; (ChildNode != 0 && AvailableWidth); ChildNode = ChildNode->Next)
                {
                    ui_layout_box *ChildBox = &ChildNode->LayoutBox;
                
                    ChildNode->ClientX = ClientX;
                    ChildNode->ClientY = ClientY;
                
                    ChildBox->Width  = ChildBox->Width  <= AvailableWidth  ? ChildBox->Width  : AvailableWidth;
                    ChildBox->Height = ChildBox->Height <= AvailableHeight ? ChildBox->Height : AvailableHeight;
                
                    f32 OccupiedWidth = ChildBox->Width + Box->Spacing.X;
                    ClientX        += OccupiedWidth;
                    AvailableWidth -= OccupiedWidth;
                
                    Queue[QueueTail] = ChildNode;
                    QueueTail        = (QueueTail + 1) % Tree->NodeCount;
                }
            }
            else if (Box->Flags & UILayoutBox_FlowColumn)
            {
                for (ui_layout_node *ChildNode = Current->First; (ChildNode != 0 && AvailableHeight); ChildNode = ChildNode->Next)
                {
                    ChildNode->ClientX = ClientX;
                    ChildNode->ClientY = ClientY;
                
                    ui_layout_box *ChildBox = &ChildNode->LayoutBox;
                    ChildBox->Width  = ChildBox->Width  <= AvailableWidth  ? ChildBox->Width  : AvailableWidth;
                    ChildBox->Height = ChildBox->Height <= AvailableHeight ? ChildBox->Height : AvailableHeight;
                
                    f32 OccupiedHeight = ChildBox->Height + Box->Spacing.Y;
                    ClientY         += OccupiedHeight;
                    AvailableHeight -= OccupiedHeight;
                
                    Queue[QueueTail] = ChildNode;
                    QueueTail        = (QueueTail + 1) % Tree->NodeCount;
                }
            }
            else
            {

            }
        }

        // Drawing
        {
            if (Box->Flags & UILayoutBox_DrawBackground)
            {
                render_batch_list *BatchList = GetUIBatchList(RenderContext);
                render_rect       *Rect      = PushDataInBatchList(RenderContext->UIArena, BatchList);
                
                Rect->RectBounds = Vec4F32(Current->ClientX, Current->ClientY, Current->ClientX + Box->Width, Current->ClientY + Box->Height);
                Rect->Color      = Box->Color;
            }
        }

        QueueHead = (QueueHead + 1) % Tree->NodeCount;
    }

    PopArena(Tree->Arena, Tree->NodeCount * sizeof(ui_layout_node *));
}