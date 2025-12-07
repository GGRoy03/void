static ui_node
UIWindow(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    ui_node Node = {};

    uint32_t Flags     = UILayoutNode_HasClip | UILayoutNode_IsDraggable | UILayoutNode_IsResizable;
    uint32_t NodeIndex = AllocateLayoutNode(Flags, Pipeline.Tree);

    if(NodeIndex != InvalidLayoutNodeIndex)
    {
        bool Pushed = PushLayoutParent(NodeIndex, Pipeline.Tree, Pipeline.FrameArena);
        if(Pushed)
        {
            Node = {.Index = NodeIndex};

            Node.SetStyle(StyleIndex, Pipeline);
        }
    }

    return Node;
}

static void
UIEndWindow(ui_node Node, ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    PopLayoutParent(Node.Index, Pipeline.Tree);
}
