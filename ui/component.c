internal ui_node
UIComponentAll_(u32 StyleIndex, bit_field Flags, byte_string Text, ui_pipeline *Pipeline)
{
    ui_node Node = {0};

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    if(Subtree)
    {
        Node = UIBuildLayoutNode(GetBaseStyle(StyleIndex, Pipeline->Registry), Flags, Subtree, Pipeline->FrameArena);

        if(Node.CanUse)
        {
            if(HasFlag(Flags, UILayoutNode_DrawText))
            {
                ui_font *Font = UIGetFont(GetBaseStyle(StyleIndex, Pipeline->Registry));
                BindText(Node, Text, Font, Subtree->LayoutTree, Pipeline->StaticArena);
            }

            if(HasFlag(Flags, UILayoutNode_IsScrollable))
            {
                BindScrollContext(Node, ScrollAxis_Y, Subtree->LayoutTree, Pipeline->StaticArena);
            }
        }
    }

    BeginNodeChain(Node, Pipeline);

    return Node;
}

internal ui_node
UIComponent_(u32 StyleIndex, bit_field Flags, ui_pipeline *Pipeline)
{
    ui_node Node = UIComponentAll_(StyleIndex, Flags, ByteString(0, 0), Pipeline);
    return Node;
}

// ------------------------------------------------------------------------

internal ui_node
UIWindow_(u32 StyleIndex, ui_pipeline *Pipeline)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    ui_node Node = UIComponent_(StyleIndex, Flags, Pipeline);
    return Node;
}

internal ui_node
UIScrollView_(u32 StyleIndex, ui_pipeline *Pipeline)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsScrollable);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node Node = UIComponent_(StyleIndex, Flags, Pipeline);
    return Node;
}

internal ui_node
UILabel_(u32 StyleIndex, byte_string Text, ui_pipeline *Pipeline)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_DrawText);
    }

    ui_node Node = UIComponentAll_(StyleIndex, Flags, Text, Pipeline);
    return Node;
}
