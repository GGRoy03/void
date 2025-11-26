// NOTE:
// Makes me think that we can unify flags initialization with functions.

static ui_node *
UIWindow(uint32_t StyleIndex)
{
    uint32_t Flags = 0;
    {
        VOID_SETBIT(Flags, UILayoutNode_IsDraggable);
        VOID_SETBIT(Flags, UILayoutNode_IsResizable);
        VOID_SETBIT(Flags, UILayoutNode_IsParent);
        VOID_SETBIT(Flags, UILayoutNode_HasClip);
    }

    ui_node *Node = UICreateNode(Flags, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(StyleIndex);
    }

    return Node;
}

static ui_node *
UIScrollableContent(float ScrollSpeed, UIAxis_Type Axis, uint32_t Style)
{
    uint32_t Flags = 0;
    {
        VOID_SETBIT(Flags, UILayoutNode_IsParent);
        VOID_SETBIT(Flags, UILayoutNode_HasClip);
    }

    ui_node *Node = UICreateNode(Flags, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetScroll(ScrollSpeed, Axis);
    }

    return Node;
}

static ui_node *
UILabel(byte_string Text, uint32_t Style)
{
    ui_node *Node = UICreateNode(0, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetText(Text);
    }

    return Node;
}

static ui_node *
UITextInput(uint8_t *Buffer, uint64_t BufferSize, uint32_t Style)
{
    ui_node *Node = UICreateNode(0, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetTextInput(Buffer, BufferSize);
    }

    return Node;
}

static ui_node *
UIImage(byte_string Path, byte_string Group, uint32_t Style)
{
    ui_node *Node = UICreateNode(0, 0);

    if(Node && Node->CanUse)
    {
        Node->SetStyle(Style);
        Node->SetImage(Path, Group);
    }

    return Node;
}
