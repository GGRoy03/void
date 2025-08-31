static void
PopParent()
{
    Cim_Assert(CimCurrent);

    cim_layout     *Layout = UIP_LAYOUT;
    ui_layout_tree *Tree   = &Layout->Tree; // This would access the current tree.

    if (Tree->AtParent > 0)
    {
        Tree->AtParent--;
    }
    else
    {
        CimLog_Error("Unable to pop parent since we are already at depth 0");
    }
}

static cim_rect
MakeRectFromNode(ui_layout_node *Node)
{
    cim_rect Rect;
    Rect.MinX = Node->X;
    Rect.MinY = Node->Y;
    Rect.MaxX = Node->X + Node->Width;
    Rect.MaxY = Node->Y + Node->Height;

    return Rect;
}

static ui_layout_node_state
GetUILayoutNodeState(cim_u32 Index)
{
    Cim_Assert(CimCurrent);

    ui_layout_node_state Result = {};
    cim_layout          *Layout = UIP_LAYOUT;
    ui_layout_tree      *Tree   = &Layout->Tree; // This would access the current tree.

    if (Index < Tree->NodeCount)
    {
        ui_layout_node *Node = Tree->Nodes + Index;

        Result.X      = Node->X;
        Result.Y      = Node->Y;
        Result.Height = Node->Height;
        Result.Width  = Node->Width;
    }
    else
    { 
        CimLog_Error("Internal Error: Invalid index:%u used to access a layout node.", Index);
    }

    return Result;
}

static ui_layout_node *
PushLayoutNode(bool IsContainer, ui_component_state *State)
{
    Cim_Assert(CimCurrent);

    cim_layout     *Layout = UIP_LAYOUT;
    ui_layout_tree *Tree   = &Layout->Tree; // This would access the current tree.

    if (Tree->NodeCount == Tree->NodeSize)
    {
        size_t NewSize = Tree->NodeSize ? (size_t)Tree->NodeSize * 2u : 8u;

        if (NewSize > (SIZE_MAX / sizeof(*Tree->Nodes)))
        {
            CimLog_Fatal("Requested allocation size too large.");
            return NULL;
        }

        ui_layout_node *Temp = (ui_layout_node *)realloc(Tree->Nodes, NewSize * sizeof(*Temp));
        if (!Temp)
        {
            CimLog_Fatal("Failed to heap-allocate.");
            return NULL;
        }

        Tree->Nodes    = Temp;
        Tree->NodeSize = (cim_u32)NewSize;
    }

    cim_u32         NodeId = Tree->NodeCount;
    ui_layout_node *Node   = Tree->Nodes + NodeId;
    cim_u32         Parent = Tree->ParentStack[Tree->AtParent];

    Node->Id          = NodeId;
    Node->ChildCount  = 0;
    Node->FirstChild  = CimLayout_InvalidNode;
    Node->LastChild   = CimLayout_InvalidNode;
    Node->NextSibling = CimLayout_InvalidNode;
    Node->Parent      = CimLayout_InvalidNode;
    Node->State       = State;

    if (Parent != CimLayout_InvalidNode)
    {
        ui_layout_node *ParentNode = Tree->Nodes + Parent;

        if (ParentNode->FirstChild == CimLayout_InvalidNode)
        {
            ParentNode->FirstChild = NodeId;
            ParentNode->LastChild  = NodeId;
        }
        else
        {
            ui_layout_node *LastChild = Tree->Nodes + ParentNode->LastChild;
            LastChild->NextSibling = NodeId;
            ParentNode->LastChild = NodeId;
        }

        Node->Parent = Parent;
    }

    if (IsContainer)
    {
        if (Tree->AtParent + 1 < CimLayout_MaxNestDepth)
        {
            Tree->ParentStack[++Tree->AtParent] = NodeId;
        }
        else
        {
            CimLog_Error("Maximum nest depth reached: %u", CimLayout_MaxNestDepth);
        }
    }

    ++Tree->NodeCount;

    return Node;
}

static ui_tree_sizing_result
SizeLayoutTree(ui_layout_tree *Tree, ui_tree_stats *Stats)
{
    ui_tree_sizing_result Result         = {};
    char                  IsVisited[512] = {}; Cim_Assert(Tree->NodeCount < 512);

    Result.Top = 1;

    while (Result.Top > 0)
    {
        cim_u32 NodeIndex = Result.Stack[Result.Top - 1];

        if (!IsVisited[NodeIndex])
        {
            IsVisited[NodeIndex] = 1;

            cim_u32 ChildIndex = Tree->Nodes[NodeIndex].FirstChild;
            while (ChildIndex != CimLayout_InvalidNode)
            {
                Result.Stack[Result.Top++] = ChildIndex;
                ChildIndex = Tree->Nodes[ChildIndex].NextSibling;
            }

            Stats->VisitedNodes += 1;
        }
        else
        {
            ui_layout_node *Node = Tree->Nodes + NodeIndex;

            switch (Node->ContainerType)
            {

            case UIContainer_None:
            {
                // If we directly set the with, then we don't have to do anything?
            } break;

            // NOTE: This code is only correct in the case where we want the container to fit the 
            // content. Otherwise it is way different. And simpler perhaps? Except that we might need
            // to clip elements? It seems quite trivial to change the behavior so that's good.
            case UIContainer_Row:
            {
                cim_f32 MaximumHeight = 0;
                cim_f32 TotalWidth = 0;

                cim_u32 ChildIndex = Tree->Nodes[NodeIndex].FirstChild;
                while (ChildIndex != CimLayout_InvalidNode)
                {
                    ui_layout_node *ChildNode = Tree->Nodes + ChildIndex;

                    if (ChildNode->Height > MaximumHeight)
                    {
                        MaximumHeight = ChildNode->Height;
                    }

                    TotalWidth       += ChildNode->Width;
                    Node->ChildCount += 1;

                    ChildIndex = ChildNode->NextSibling;
                }

                cim_f32 TotalSpacing = Node->ChildCount > 1 ? Node->Spacing.x * (Node->ChildCount - 1) : 0.0f;

                Node->Width  = TotalWidth + TotalSpacing + (Node->Padding.x + Node->Padding.z);
                Node->Height = MaximumHeight + (Node->Padding.y + Node->Padding.w);

            } break;

            // NOTE: This code is only correct in the case where we want the container to fit the 
            // content. Otherwise it is way different. And simpler perhaps? Except that we might need
            // to clip elements? It seems quite trivial to change the behavior so that's good.
            case UIContainer_Column:
            {
                cim_f32 MaximumWidth = 0;
                cim_f32 TotalHeight = 0;

                cim_u32 ChildIndex = Tree->Nodes[NodeIndex].FirstChild;
                while (ChildIndex != CimLayout_InvalidNode)
                {
                    ui_layout_node *ChildNode = Tree->Nodes + ChildIndex;

                    if (ChildNode->Width > MaximumWidth)
                    {
                        MaximumWidth = ChildNode->Width;
                    }

                    TotalHeight      += ChildNode->Height;
                    Node->ChildCount += 1;

                    ChildIndex = ChildNode->NextSibling;
                }

                cim_f32 TotalSpacing = (Node->ChildCount > 1) ? Node->Spacing.y * (Node->ChildCount - 1) : 0.0f;

                Node->Width  = MaximumWidth + (Node->Padding.x + Node->Padding.z);
                Node->Height = TotalHeight + (Node->Padding.y + Node->Padding.w) + TotalSpacing;
            } break;

            }

            --Result.Top;
        }
    }

    return Result;
}

static void
PlaceLayoutTree(ui_layout_tree *Tree)
{
    cim_u32 Stack[1024] = {};
    cim_u32 Top = 1;

    cim_f32 ClientX, ClientY;

    while (Top > 0)
    {
        Cim_Assert(Top < 1024);

        cim_u32         NodeIndex = Stack[--Top];
        ui_layout_node *Node      = Tree->Nodes + NodeIndex;

        ClientX = Node->X + Node->Padding.x;
        ClientY = Node->Y + Node->Padding.y;

        cim_u32 StackWrite = Top + Node->ChildCount - 1;
        cim_u32 ChildIndex = Node->FirstChild;
        while (ChildIndex != CimLayout_InvalidNode)
        {
            ui_layout_node *ChildNode = Tree->Nodes + ChildIndex;
            ChildNode->X = ClientX;
            ChildNode->Y = ClientY;

            switch (Node->ContainerType)
            {
            case UIContainer_None:
            {
                // NOTE: Pretty sure this should return an error.
            } break;

            case UIContainer_Row:
            {
                ClientX += ChildNode->Width + Node->Spacing.x;
            } break;

            case UIContainer_Column:
            {
                ClientY += ChildNode->Height + Node->Spacing.y;
            } break;

            }

            Stack[StackWrite--] = ChildIndex;
            ChildIndex          = ChildNode->NextSibling;
        }

        Top += Node->ChildCount;
    }
}

static void
HitTestLayoutTree(ui_layout_tree *Tree, ui_tree_sizing_result *SizingResult)
{
    bool MouseClicked = IsMouseClicked(CimMouse_Left, UIP_INPUT);

    if (MouseClicked)
    {
        ui_layout_node *Root = Tree->Nodes;
        cim_rect        WindowHitBox = MakeRectFromNode(Root);

        if (IsInsideRect(WindowHitBox))
        {
            for (cim_u32 StackIdx = Tree->NodeCount - 1; StackIdx > 0; StackIdx--)
            {
                ui_layout_node *Node = Tree->Nodes + SizingResult->Stack[StackIdx];
                cim_rect        HitBox = MakeRectFromNode(Node);

                if (IsInsideRect(HitBox))
                {
                    // NOTE: I need to rethink everything. Because the current way
                    // is quite limiting.

                    if (Node->State)
                    {
                        Node->State->Clicked = MouseClicked;
                        Node->State->Hovered = true;
                    }

                    return;
                }
            }

            if (Root->State)
            {
                Root->State->Hovered = true;
                Root->State->Clicked = MouseClicked;
            }
        }
    }
}

static void
UIEndLayout()
{
    Cim_Assert(CimCurrent);
    ui_layout_tree *Tree = UIP_LAYOUT.Tree;

    ui_tree_stats         Stats        = {};
    ui_tree_sizing_result SizingResult = {};

    SizingResult = SizeLayoutTree(Tree, &Stats); Cim_Assert(SizingResult.Top == 0);

    PlaceLayoutTree(Tree);
    HitTestLayoutTree(Tree, &SizingResult);
}
