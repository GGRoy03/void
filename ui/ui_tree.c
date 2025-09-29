// [Tree Management]

internal ui_tree *
UITree_Allocate(ui_tree_params Params)
{
	Assert(Params.Depth > 0 && Params.NodeCount > 0 && Params.Type != UITree_None);

    ui_tree *Result = 0;

    memory_arena *Arena = 0;
    {
        size_t Footprint = sizeof(ui_tree);
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
        Result = PushArena(Arena, sizeof(ui_tree), AlignOf(ui_tree));
        Result->Arena        = Arena;
        Result->Nodes        = PushArray(Arena, ui_node  , Params.NodeCount);
        Result->ParentStack  = PushArray(Arena, ui_node *, Params.Depth    );
        Result->MaximumDepth = Params.Depth;
        Result->NodeCapacity = Params.NodeCount;
        Result->Type         = Params.Type;

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Id = InvalidNodeId;
        }
    }

    return Result;
}

// [Parent Stack]

internal void
UITree_PopParent(ui_tree *Tree)
{
    if (Tree->ParentTop > 0)
    {
        --Tree->ParentTop;
    }
}

internal void
UITree_PushParent(ui_node *Node, ui_tree *Tree)
{
    if (Tree->ParentTop < Tree->MaximumDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Node;
    }
}

// [Getters]

internal ui_node *
UITree_GetParent(ui_tree *Tree)
{
    ui_node *Result = 0;

    if (Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal ui_node *
UITree_GetFreeNode(ui_tree *Tree, UINode_Type Type)
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
UITree_GetLayoutNode(ui_node *Node, ui_pipeline *Pipeline)
{
    ui_node *Result = 0;

    if (Node->Id < Pipeline->LayoutTree->NodeCapacity)
    {
        Result = Pipeline->LayoutTree->Nodes + Node->Id;
    }

    return Result;
}

internal ui_node *
UITree_GetStyleNode(ui_node *Node, ui_pipeline *Pipeline)
{
    ui_node *Result = 0;

    if (Node->Id < Pipeline->StyleTree->NodeCapacity)
    {
        Result = Pipeline->StyleTree->Nodes + Node->Id;
    }

    return Result;
}

// [Bindings]

internal void
UITree_BindText(ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_node *Node)
{   Assert(Node->Id < Pipeline->LayoutTree->NodeCapacity);

    ui_node *LayoutNode = UITree_GetLayoutNode(Node, Pipeline);
    if (LayoutNode && Font)
    {
        LayoutNode->Layout.Text = PushArray(Pipeline->StaticArena, ui_text, 1);
        LayoutNode->Layout.Text->AtlasTexture     = RenderHandle((u64)Font->GPUFontObjects.GlyphCacheView);
        LayoutNode->Layout.Text->AtlasTextureSize = Font->TextureSize;
        LayoutNode->Layout.Text->Size             = Count;
        LayoutNode->Layout.Text->Characters       = Characters;
        LayoutNode->Layout.Text->LineHeight       = Font->LineHeight;
    }
    else
    {
        OSLogMessage(byte_string_literal("Could not set tex binding. Font is invalid."), OSMessage_Error);
    }
}

internal void
UITree_BindFlag(ui_node *Node, b32 Set, UILayoutNode_Flag Flag, ui_pipeline *Pipeline)
{   Assert((Flag >= UILayoutNode_NoFlag && Flag <= UILayoutNode_IsResizable));

    ui_node *LayoutNode = UITree_GetLayoutNode(Node, Pipeline);
    if (LayoutNode)
    {
        if (Set)
        {
            SetFlag(LayoutNode->Layout.Flags, Flag);
        }
        else
        {
            ClearFlag(LayoutNode->Layout.Flags, Flag);
        }
    }
}

internal void
UITree_BindClickCallback(ui_node *Node, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_node *LayoutNode = UITree_GetLayoutNode(Node, Pipeline);
    if (LayoutNode)
    {
        LayoutNode->Layout.ClickCallback = Callback;
    }
}

// [Helpers]

internal b32 
UITree_IsValidNode(ui_node *Node, ui_tree *Tree)
{
    b32 Result = Node && Node->Id < Tree->NodeCount;
    return Result;
}

internal b32
UITree_NodesAreParallel(ui_node *Node1, ui_node *Node2)
{
    b32 Result = (Node1->Id == Node2->Id);
    return Result;
}
internal b32
UITree_IsNodeALeaf(UINode_Type Type)
{
    b32 Result = (Type == UINode_Button || Type == UINode_Label);
    return Result;
}

internal void 
UITree_LinkNodes(ui_node *Node, ui_node *Parent)
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

internal void
UITree_SynchronizePipeline(ui_node *StyleRoot, ui_pipeline *Pipeline)
{
    ui_node *LayoutRoot = UITree_GetLayoutNode(StyleRoot, Pipeline);

    if (!UITree_NodesAreParallel(StyleRoot, LayoutRoot))
    {
        LayoutRoot = UITree_GetFreeNode(Pipeline->LayoutTree, StyleRoot->Type);
        LayoutRoot->Id = StyleRoot->Id;

        ui_node *StyleRootParent = StyleRoot->Parent;
        ui_node *LayoutRootParent = 0;
        if (UITree_IsValidNode(StyleRootParent, Pipeline->StyleTree))
        {
            LayoutRootParent = Pipeline->LayoutTree->Nodes + StyleRootParent->Id;
        }

        UITree_LinkNodes(LayoutRoot, LayoutRootParent);
    }

    {
        LayoutRoot->Layout.Width    = StyleRoot->Style.Size.X;
        LayoutRoot->Layout.Height   = StyleRoot->Style.Size.Y;
        LayoutRoot->Layout.Padding  = StyleRoot->Style.Padding;
        LayoutRoot->Layout.Spacing  = StyleRoot->Style.Spacing;
    }

    for (ui_node *Child = StyleRoot->First; Child != 0; Child = Child->Next)
    {
        UITree_SynchronizePipeline(Child, Pipeline);
    }
}