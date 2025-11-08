internal ui_node
UIWindow(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    ui_node Node = UINode(Flags);
    if(Node.CanUse)
    {
        UINodeSetStyle(Node, StyleIndex);
    }

    return Node;
}

internal ui_node
UIScrollableContent(UIAxis_Type Axis, u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node Node = UINode(Flags);
    if(Node.CanUse)
    {
        UINodeSetStyle(Node, StyleIndex);
        UINodeSetScroll(Node, Axis);
    }

    return Node;
}

internal ui_node
UITextInput(u8 *Buffer, u64 BufferSize, u32 StyleIndex)
{
    ui_node Node = UINode(NoFlag);
    if(Node.CanUse)
    {
        UINodeSetStyle(Node, StyleIndex);
        UINodeSetTextInput(Node, Buffer, BufferSize);
    }

    return Node;
}
