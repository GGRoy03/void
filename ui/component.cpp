// NOTE:
// Makes me think that we can unify flags initialization with functions.

internal ui_node *
UIWindow(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node *Node = UICreateNode(Flags, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(StyleIndex);
    }

    return Node;
}

internal ui_node *
UIScrollableContent(f32 ScrollSpeed, UIAxis_Type Axis, u32 Style)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node *Node = UICreateNode(Flags, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetScroll(ScrollSpeed, Axis);
    }

    return Node;
}

internal ui_node *
UILabel(byte_string Text, u32 Style)
{
    ui_node *Node = UICreateNode(NoFlag, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetText(Text);
    }

    return Node;
}

internal ui_node *
UITextInput(u8 *Buffer, u64 BufferSize, u32 Style)
{
    ui_node *Node = UICreateNode(NoFlag, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetTextInput(Buffer, BufferSize);
    }

    return Node;
}

internal ui_node *
UIImage(byte_string Path, byte_string Group, u32 Style)
{
    ui_node *Node = UICreateNode(NoFlag, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetImage(Path, Group);
    }

    return Node;
}
