internal void
UIWindow_(byte_string Id, u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    InitializeLayoutNode(Style, Flags, Id, Pipeline);
}

internal void
UIButton_(byte_string Id, u32 StyleIndex, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // TODO: Figure out what we want to do with callbacks.
    Useless(Callback);

    InitializeLayoutNode(Style, Flags, Id, Pipeline);
}

internal void
UIHeader_(byte_string Id, u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenX);
    }

    InitializeLayoutNode(Style, Flags, Id, Pipeline);
}

internal void
UIScrollView_(byte_string Id, u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsScrollRegion);
    }

    ui_layout_node *Node = InitializeLayoutNode(Style, Flags, Id, Pipeline);
    if(Node)
    {
        BindScrollContext(Node, ScrollAxis_Y, Pipeline->StaticArena);
    }
}

internal void
UILabel_(byte_string Id, u32 StyleIndex, byte_string Text, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    ui_layout_node *Node = InitializeLayoutNode(Style, Flags, Id, Pipeline);
    if(IsValidLayoutNode(Node))
    {
        BindText(Node, Text, UIGetFont(Style), Pipeline->StaticArena);

        if(HasFlag(Node->Flags, UILayoutNode_TextIsBound))
        {
            SetFlag(Node->Flags, UILayoutNode_DrawText);
        }
    }
}
