// [Tree Management]

internal ui_layout_tree *
AllocateLayoutTree(ui_layout_tree_params Params)
{
    Assert(Params.NodeCount > 0 && Params.Depth > 0);

    ui_layout_tree *Result = 0;

    memory_arena *Arena = 0;
    {
        size_t StackSize = Params.Depth     * sizeof(Result->ParentStack[0]);
        size_t ArraySize = Params.NodeCount * sizeof(Result->Nodes[0]);
        size_t QueueSize = Params.NodeCount * sizeof(Result->Nodes[0]);
        size_t Footprint = sizeof(ui_layout_tree) + StackSize + ArraySize + QueueSize;

        memory_arena_params ArenaParams = { 0 };
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Footprint;
        ArenaParams.CommitSize        = Footprint;

        Arena = AllocateArena(ArenaParams);
    }

    if (Arena)
    {
        Result = PushArena(Arena, sizeof(ui_layout_tree), AlignOf(ui_layout_tree));
        Result->Arena        = Arena;
        Result->Nodes        = PushArray(Arena, ui_layout_node, Params.NodeCount);
        Result->ParentStack  = PushArray(Arena, ui_layout_node *, Params.Depth);
        Result->MaximumDepth = Params.Depth;
        Result->NodeCapacity = Params.NodeCount;

        for (u32 Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

// [Parent Stack]

internal void
PopLayoutNodeParent(ui_layout_tree*Tree)
{
    if (Tree->ParentTop > 0)
    {
        --Tree->ParentTop;
    }
}

internal void
PushLayoutNodeParent(ui_layout_node *Node, ui_layout_tree*Tree)
{
    if (Tree->ParentTop < Tree->MaximumDepth)
    {
        Tree->ParentStack[Tree->ParentTop++] = Node;
    }
}

// [Getters]

internal ui_layout_node *
GetLayoutNodeParent(ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if (Tree->ParentTop)
    {
        Result = Tree->ParentStack[Tree->ParentTop - 1];
    }

    return Result;
}

internal ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree, UILayoutNode_Type Type)
{
    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Type  = Type;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

internal void
SetLayoutNodeStyle(ui_cached_style *CachedStyle, ui_layout_node *Node, bit_field Flags)
{
    if (IsValidLayoutNode(Node) && CachedStyle)
    {
        Node->CachedStyle = CachedStyle;

        Node->Value.Width   = CachedStyle->Value.Size.X;
        Node->Value.Height  = CachedStyle->Value.Size.Y;
        Node->Value.Padding = CachedStyle->Value.Padding;
        Node->Value.Spacing = CachedStyle->Value.Spacing;

        if (!HasFlag(Flags, SetLayoutNodeStyle_OmitReference))
        {
            AppendToLinkedList(CachedStyle, Node, CachedStyle->RefCount);
        }
    }
}

// [Bindings]

// NOTE: This seems weird.

internal void
UITree_BindText(ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_layout_node *Node)
{
    Assert(Node->Index < Pipeline->LayoutTree->NodeCapacity);

    if (IsValidLayoutNode(Node) && Font)
    {
        Node->Value.Text = PushArray(Pipeline->StaticArena, ui_text, 1);
        Node->Value.Text->AtlasTexture     = RenderHandle((u64)Font->GPUFontObjects.GlyphCacheView);
        Node->Value.Text->AtlasTextureSize = Font->TextureSize;
        Node->Value.Text->Size             = Count;
        Node->Value.Text->Characters       = Characters;
        Node->Value.Text->LineHeight       = Font->LineHeight;
    }
    else
    {
        OSLogMessage(byte_string_literal("Could not set text binding."), OSMessage_Error);
    }
}

// NOTE: This seems weird.

internal void
UITree_BindClickCallback(ui_layout_node *Node, ui_click_callback *Callback)
{
    if (IsValidLayoutNode(Node))
    {
        Node->Value.ClickCallback = Callback;
    }
}

// [Helpers]

internal b32 
IsValidLayoutNode(ui_layout_node *Node)
{
    b32 Result = Node && Node->Index != InvalidLayoutNodeIndex;
    return Result;
}

internal b32
IsLayoutNodeALeaf(UILayoutNode_Type Type)
{
    b32 Result = (Type == UILayoutNode_Button || Type == UILayoutNode_Label);
    return Result;
}

internal void 
AttachLayoutNode(ui_layout_node *Node, ui_layout_node *Parent)
{
    Node->Last   = 0;
    Node->Next   = 0;
    Node->First  = 0;
    Node->Parent = Parent;

    if (Parent)
    {
        AppendToDoublyLinkedList(Parent, Node, Parent->ChildCount);
    }
}