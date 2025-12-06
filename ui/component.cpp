static ui_node
UIWindow(uint32_t StyleIndex, const ui_pipeline &Pipeline)
{
    ui_node Node = {};

    uint32_t Flags     = UILayoutNode_HasClip | UILayoutNode_IsDraggable | UILayoutNode_IsResizable;
    uint32_t NodeIndex = AllocateLayoutNode(Flags, Pipeline.Tree);

    if(NodeIndex != InvalidLayoutNodeIndex)
    {
        bool Pushed = PushLayoutParent(NodeIndex, Pipeline.Tree, Pipeline.Arena);
        if(Pushed)
        {
            Node = {.Index = NodeIndex};
        }
    }

    return Node;
}

static void
UIEndWindow(ui_node Node, const ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    PopLayoutParent(Node.Index, Pipeline.Tree);
}
