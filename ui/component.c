internal ui_node_chain * 
UIComponentAll(u32 StyleIndex, bit_field Flags, byte_string Text)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    Assert(Subtree);

    bit_field FinalFlags = Flags;

    style_property *BaseProperties = GetBaseStyle(StyleIndex, Pipeline->Registry);
    {
        Assert(BaseProperties);

        ui_color BackgroundColor = UIGetColor(BaseProperties);
        f32      BorderWidth     = UIGetBorderWidth(BaseProperties);

        if(IsVisibleColor(BackgroundColor))
        {
            Assert(!HasFlag(Flags, UILayoutNode_DrawBackground));
            SetFlag(FinalFlags, UILayoutNode_DrawBackground);
        }

        if(BorderWidth > 0.f)
        {
            Assert(!HasFlag(Flags, UILayoutNode_DrawBorders));
            SetFlag(FinalFlags, UILayoutNode_DrawBorders);
        }
    }

    ui_node Node = AllocateUINode(BaseProperties, FinalFlags, Subtree);
    Assert(Node.CanUse);

    // NOTE: This is messy, but where should I do it? Uhmmm..
    // I really hate this. How can I bind it? I can't be lazy
    // it has to come from here->node style.
    ui_node_style *Style = GetNodeStyle(Node.IndexInTree, Subtree);
    Style->CachedStyleIndex = StyleIndex;

    // WARN: Still a work in progress. Bit of a mess..

    if(HasFlag(Flags, UILayoutNode_DrawText))
    {
        ui_resource_key   Key   = MakeUITextResourceKey(Text);
        ui_resource_state State = FindUIResourceByKey(Key, UIState.ResourceTable);
        if(State.Type == UIResource_None)
        {
            ui_font *Font = UIGetFont(BaseProperties);
             UpdateUITextResource(State.Id, Text, Font, UIState.ResourceTable);
        }
        else
        {
            Assert(!"Not Implemented");
        }
    }

    if(HasFlag(Flags, UILayoutNode_IsScrollable))
    {
        BindScrollContext(Node, ScrollAxis_Y, Subtree->LayoutTree, Pipeline->StaticArena);
    }

    ui_node_chain *Chain = UIChain(Node);
    return Chain;
}

internal ui_node_chain *
UIComponent(u32 StyleIndex, bit_field Flags)
{
    ui_node_chain *Chain = UIComponentAll(StyleIndex, Flags, ByteString(0, 0));
    Assert(Chain);

    return Chain;
}

// ------------------------------------------------------------------------

internal ui_node_chain *
UIWindow(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);
    return Chain;
}

internal ui_node_chain *
UIScrollView(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsScrollable);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);
    return Chain;
}

internal ui_node_chain *
UILabel(u32 StyleIndex, byte_string Text)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_DrawText);
    }

    ui_node_chain *Chain = UIComponentAll(StyleIndex, Flags, Text);
    return Chain;
}
