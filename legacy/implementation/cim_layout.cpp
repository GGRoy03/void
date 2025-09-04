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

// [Trees]

static void
ClearUITree(ui_tree *Tree)
{
    if (Tree)
    {
        Tree->NodeCount      = 0;
        Tree->ParentTop      = 0;
        Tree->ParentStack[0] = UILayout_InvalidNode;
    }
}

static void
PushParentNode(ui_tree *Tree, cim_u32 NodeIndex)
{
    if (Tree->ParentTop + 1 < UITree_NestingDepth)
    {
        Tree->ParentStack[++Tree->ParentTop] = NodeIndex;
    }
}

static void
PopParentNode(ui_tree *Tree)
{
    if (Tree->ParentTop > 0)
    {
        Tree->ParentTop--;
    }
}

static cim_u32
GetParentIndex(ui_tree *Tree)
{
    cim_u32 Result = UITree_InvalidNode;

    if (Tree->ParentTop >= 0)
    {
        Result = Tree->ParentStack[Tree->ParentTop];
    }

    return Result;
}

static ui_layout_node *
GetLayoutNode(ui_tree *Tree, cim_u32 Index)
{
    ui_layout_node *Result = NULL;

    if(Index < Cim_ArrayCount(Tree->Nodes.Layout))
    {
        Result = Tree->Nodes.Layout + Index;

        if(Index == Tree->NodeCount)
        {
            Tree->NodeCount++;
        }
    }

    return Result;
}

static void
LinkChildToParent(ui_tree *LayoutTree, ui_tree *DrawTree, cim_u32 NodeIndex, cim_u32 ParentIndex)
{
    ui_layout_node *ParentNode = GetLayoutNode(LayoutTree, ParentIndex);
    ui_draw_node   *DrawNode   = GetDrawNode(DrawTree, ParentIndex);

    if (ParentNode && DrawNode)
    {
        cim_u32         LastIndex = ParentNode->FirstChild;
        ui_layout_node *LastNode  = GetLayoutNode(LayoutTree, LastIndex);
        while (LastNode && LastNode->Next != UITree_InvalidNode)
        {
            LastIndex = LastNode->Next;
            LastNode  = GetLayoutNode(LayoutTree, LastIndex);
        }

        if (!LastNode)
        {
            ParentNode->FirstChild = NodeIndex;
            DrawNode->FirstChild   = NodeIndex;
        }
        else
        {
            LastNode->Next = NodeIndex;

            ui_draw_node *LastDrawNode = GetDrawNode(DrawTree, LastIndex);
            if (LastDrawNode)
            {
                LastDrawNode->Next = NodeIndex;
            }
        }
    }
}

static void
PushLayoutAndDrawNode(UIContainer_Type ContainerType, UIDrawNode_Type DrawNodeType, ui_component_state *State,
                      ui_layout_node **OutLayoutNode, ui_draw_node **OutDrawNode)
{
    ui_tree *LayoutTree = &UI_LayoutTree;
    ui_tree *DrawTree   = &UI_DrawTree;

    cim_u32 NodeIndex   = LayoutTree->NodeCount;
    cim_u32 ParentIndex = GetParentIndex(LayoutTree);

    ui_layout_node *LayoutNode = GetLayoutNode(LayoutTree, NodeIndex);
    ui_draw_node   *DrawNode   = GetDrawNode(DrawTree, NodeIndex);
    if (LayoutNode && DrawNode)
    {
        LayoutNode->Id            = NodeIndex;
        LayoutNode->FirstChild    = UILayout_InvalidNode;
        LayoutNode->Next          = UILayout_InvalidNode;
        LayoutNode->Parent        = ParentIndex;
        LayoutNode->ChildCount    = 0;
        LayoutNode->ContainerType = ContainerType;
        LayoutNode->State         = State;

        DrawNode->Id         = NodeIndex;
        DrawNode->FirstChild = UILayout_InvalidNode;
        DrawNode->Next       = UILayout_InvalidNode;
        DrawNode->Type       = DrawNodeType;

        LinkChildToParent(LayoutTree, DrawTree, NodeIndex, ParentIndex);

        if (ContainerType != UIContainer_None)
        {
            PushParentNode(LayoutTree, NodeIndex);
            PushParentNode(DrawTree  , NodeIndex);
        }
    }

    *OutLayoutNode = LayoutNode;
    *OutDrawNode   = DrawNode;
}

static void
SizeLayoutTree(ui_tree *Tree, cim_u32 *PostOrderArray)
{
    cim_u32 PostOrderIndex = 0;
    cim_u32 Stack[256]     = {};
    cim_u32 Top            = 1;
    char    IsVisited[512] = {}; Cim_Assert(Tree->NodeCount < 512);

    while (Top > 0)
    {
        cim_u32         NodeIndex  = Stack[Top - 1];
        ui_layout_node *LayoutNode = GetLayoutNode(Tree, NodeIndex);

        if (!IsVisited[NodeIndex])
        {
            IsVisited[NodeIndex] = 1;

            cim_u32 ChildIndex = Tree->Nodes.Layout[NodeIndex].FirstChild;
            while (ChildIndex != UILayout_InvalidNode)
            {
                Stack[Top++] = ChildIndex;
                ChildIndex   = Tree->Nodes.Layout[ChildIndex].Next;
            }
        }
        else
        {
            ui_layout_node *Node = GetLayoutNode(Tree, NodeIndex);

            switch (Node->ContainerType)
            {

            case UIContainer_None:
            {
                // If we directly set the width, then we don't have to do anything?
            } break;

            // NOTE: This code is only correct in the case where we want the container to fit the 
            // content. Otherwise it is way different. And simpler perhaps? Except that we might need
            // to clip elements? It seems quite trivial to change the behavior so that's good.
            case UIContainer_Row:
            {
                cim_f32 MaximumHeight = 0;
                cim_f32 TotalWidth    = 0;

                cim_u32 ChildIndex = LayoutNode->FirstChild;
                while (ChildIndex != UILayout_InvalidNode)
                {
                    ui_layout_node *ChildNode = GetLayoutNode(Tree, ChildIndex);

                    if (ChildNode->Height > MaximumHeight)
                    {
                        MaximumHeight = ChildNode->Height;
                    }

                    TotalWidth       += ChildNode->Width;
                    Node->ChildCount += 1;

                    ChildIndex = ChildNode->Next;
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

                cim_u32 ChildIndex = LayoutNode->FirstChild;
                while (ChildIndex != UILayout_InvalidNode)
                {
                    ui_layout_node *ChildNode = GetLayoutNode(Tree, ChildIndex);

                    if (ChildNode->Width > MaximumWidth)
                    {
                        MaximumWidth = ChildNode->Width;
                    }

                    TotalHeight      += ChildNode->Height;
                    Node->ChildCount += 1;

                    ChildIndex = ChildNode->Next;
                }

                cim_f32 TotalSpacing = (Node->ChildCount > 1) ? Node->Spacing.y * (Node->ChildCount - 1) : 0.0f;

                Node->Width  = MaximumWidth + (Node->Padding.x + Node->Padding.z);
                Node->Height = TotalHeight  + (Node->Padding.y + Node->Padding.w) + TotalSpacing;
            } break;

            }

            PostOrderArray[PostOrderIndex++] = NodeIndex;
            Cim_Assert(PostOrderIndex != 256);

            --Top;
        }
    }
}

static void
PlaceLayoutTree(ui_tree *Tree)
{
    cim_u32 Stack[1024] = {};
    cim_u32 Top         = 1;

    cim_f32 ClientX, ClientY;

    while (Top > 0)
    {
        Cim_Assert(Top < 1024);

        cim_u32         NodeIndex = Stack[--Top];
        ui_layout_node *Node      = GetLayoutNode(Tree, NodeIndex);

        // NOTE: I cannot tell if this is good or bad.
        {
            ui_draw_node *DrawNode = GetDrawNode(&UI_DrawTree, NodeIndex); // NOTE: Trees are parallel.
            DrawNode->Position = { Node->X, Node->Y };
            DrawNode->Size     = { Node->Width, Node->Height };
        }

        ClientX = Node->X + Node->Padding.x;
        ClientY = Node->Y + Node->Padding.y;

        cim_u32 StackWrite = Top + Node->ChildCount - 1;
        cim_u32 ChildIndex = Node->FirstChild;
        while (ChildIndex != UILayout_InvalidNode)
        {
            ui_layout_node *ChildNode = GetLayoutNode(Tree, ChildIndex);
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
            ChildIndex          = ChildNode->Next;
        }

        Top += Node->ChildCount;
    }
}

static void
HitTestLayoutTree(ui_tree *Tree, cim_u32 *PostOrderArray)
{
    bool MouseClicked = IsMouseClicked(CimMouse_Left, UIP_INPUT);

    if (MouseClicked)
    {
        ui_layout_node *Root         = GetLayoutNode(Tree, 0);
        cim_rect        WindowHitBox = MakeRectFromNode(Root);

        if (IsInsideRect(WindowHitBox))
        {
            for (cim_u32 Idx = 0; Idx < Tree->NodeCount - 1; Idx++)
            {
                cim_u32         NodeIndex = PostOrderArray[Idx];
                ui_layout_node *Node      = GetLayoutNode(Tree, NodeIndex);
                cim_rect        HitBox    = MakeRectFromNode(Node);

                if (IsInsideRect(HitBox))
                {
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

// NOTE: This is quite shit. A lot of this code is terrible.
// I guess it's better than it was?
static void
UIEndLayout()
{
    Cim_Assert(CimCurrent);
    ui_tree *Tree = &UI_LayoutTree;

    cim_u32 PostOrderArray[256] = {};
    SizeLayoutTree(Tree, PostOrderArray);

    PlaceLayoutTree(Tree);

    HitTestLayoutTree(Tree, PostOrderArray);
}

// NOTE: Everything feels like a mess.
// (User -> Style -> Layout -> User -> Draw)