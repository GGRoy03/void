// [Handles]

internal render_handle
RenderHandle(u64 Handle)
{
    render_handle Result = { Handle };
    return Result;
}

internal b32
IsValidRenderHandle(render_handle Handle)
{
    b32 Result = (Handle.u64[0] != 0);
    return Result;
}

internal b32 
RenderHandleMatches(render_handle H1, render_handle H2)
{
    b32 Result = (H1.u64[0] == H2.u64[0]);
    return Result;
}

// [Internal API]

internal size_t 
EstimateUIPassFootprint(render_pass_ui_stats Stats)
{
    size_t Result = UIPassDefaultPaddingSize;

    if(Stats.GroupCount == 0 || Stats.PassCount == 0 || Stats.RenderedDataSize == 0)
    {
        Result += UIPassDefaultRenderedDataSize;
        Result += UIPassDefaultBatchCount * sizeof(render_batch_node);
        Result += UIPassDefaultGroupCount * sizeof(rect_group_node);
        Result += UIPassDefaultPassCount  * sizeof(render_pass_node);
    }
    else
    {
        Result += Stats.RenderedDataSize;
        Result += (Stats.BatchCount - 0) * sizeof(render_batch_node);
        Result += (Stats.GroupCount - 0) * sizeof(rect_group_node);
        Result += (Stats.PassCount  - 1) * sizeof(render_pass_node);
    }

    return Result;
}

// [Core API]

internal void
BeginRendereringContext(render_pass_list *List)
{   UNUSED(List);
}

// [Batches]

internal void *
PushDataInBatchList(memory_arena *Arena, render_batch_list *BatchList)
{
    void *Result = 0;

    render_batch_node *Node = BatchList->Last;
    if (!Node || Node->Value.ByteCount + BatchList->BytesPerInstance > Node->Value.ByteCapacity)
    {
        Node = PushArray(Arena, render_batch_node, 1);
        Node->Value.ByteCount    = 0;
        Node->Value.ByteCapacity = Kilobyte(5); // WARN: This works, but would be better if we could infer it?
        Node->Value.Memory       = PushArrayNoZero(Arena, u8, Node->Value.ByteCapacity);

        if (!BatchList->Last)
        {
            BatchList->First = Node;
            BatchList->Last  = Node;
        }
        else
        {
            BatchList->Last->Next = Node;
            BatchList->Last       = Node;
        }

        BatchList->BatchCount += 1;
    }

    Result = Node->Value.Memory + Node->Value.ByteCount;

    Node->Value.ByteCount += BatchList->BytesPerInstance;
    BatchList->ByteCount  += BatchList->BytesPerInstance;

    return Result;
}

internal render_pass *
GetRenderPass(memory_arena *Arena, render_pass_list *List, RenderPass_Type Type)
{
    render_pass_node *Result = List->Last;

    if (!Result || Result->Value.Type != Type)
    {
        Result = PushArray(Arena, render_pass_node, 1);
        Result->Value.Type = Type;

        if (!List->First)
        {
            List->First = Result;
        }

        if (List->Last)
        {
            List->Last->Next = Result;
        }

        List->Last = Result;
    }

    return &Result->Value;
}

internal b32
CanMergeGroupParams(rect_group_params *Old, rect_group_params *New)
{
    if (IsValidRenderHandle(Old->AtlasTextureView) && !RenderHandleMatches(Old->AtlasTextureView, New->AtlasTextureView))
    {
        return 0;
    }

    if (!Mat3x3AreEqual(&Old->Transform, &New->Transform))
    {
        return 0;
    }

    return 1;
}