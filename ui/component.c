// WARN: This is really garbage? Yeah this is complete shit and it kinda makes no sense...

internal ui_node_chain * 
UIComponentAll(u32 StyleIndex, bit_field Flags, byte_string Text)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    Assert(Subtree);

    ui_node Node = AllocateUINode(Flags, Subtree);
    Assert(Node.CanUse);

    ui_node_style *Style = GetNodeStyle(Node.IndexInTree, Subtree);
    Style->CachedStyleIndex = StyleIndex;

    // NOTE: This needs to go!
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
UITextInput(u8 *TextBuffer, u64 TextBufferSize, u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_HasTextInput);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);

    // NOTE:
    // These resource allocations are killing me.

    ui_resource_key   Key   = MakeResourceKey(UIResource_TextInput, Chain->Node.IndexInTree, Chain->Subtree);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    if(!State.Resource)
    {
        u64   Size     = sizeof(ui_text_input);
        void *Memory   = OSReserveMemory(Size);
        b32   Commited = OSCommitMemory(Memory, Size);
        Assert(Memory && Commited);

        ui_text_input *TextInput = (ui_text_input *)Memory;
        TextInput->UserData = ByteString(TextBuffer, strlen((char *)TextBuffer));
        TextInput->Size     = TextBufferSize;

        UpdateResourceTable(State.Id, Key, TextInput, UIResource_TextInput, UIState.ResourceTable);
        SetLayoutNodeFlags(Chain->Node.IndexInTree, UILayoutNode_HasTextInput, Chain->Subtree);
    }
    else
    {
        // NOTE:
        // This case should not really be possible?
        // Not really sure.
    }

    return Chain;
}
