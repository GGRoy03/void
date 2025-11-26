// [Handles]

static render_handle
RenderHandle(uint64_t Handle)
{
    render_handle Result = { Handle };
    return Result;
}

static bool
IsValidRenderHandle(render_handle Handle)
{
    bool Result = (Handle.uint64_t[0] != 0);
    return Result;
}

static bool 
RenderHandleMatches(render_handle H1, render_handle H2)
{
    bool Result = (H1.uint64_t[0] == H2.uint64_t[0]);
    return Result;
}

// [Batches]

static void *
PushDataInBatchList(memory_arena *Arena, render_batch_list *BatchList)
{
    void *Result = 0;

    render_batch_node *Node = BatchList->Last;
    if (!Node || Node->Value.ByteCount + BatchList->BytesPerInstance > Node->Value.ByteCapacity)
    {
        Node = PushArray(Arena, render_batch_node, 1);
        Node->Value.ByteCount    = 0;
        Node->Value.ByteCapacity = VOID_KILOBYTE(5); // WARN: This works, but would be better if we could infer it?
        Node->Value.Memory       = PushArrayNoZero(Arena, uint8_t, Node->Value.ByteCapacity);

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

static render_pass *
GetRenderPass(memory_arena *Arena,  RenderPass_Type Type)
{
    render_pass_list *List   = &RenderState.PassList;
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

static bool
CanMergeRectGroupParams(rect_group_params *Old, rect_group_params *New)
{
    // If the old value had some texture and it is not the same, then we can't merge.
    if (IsValidRenderHandle(Old->Texture) && !RenderHandleMatches(Old->Texture, New->Texture))
    {
        return 0;
    }

    // If the old value has a different transform, then we can't merge.
    if (!Mat3x3AreEqual(&Old->Transform, &New->Transform))
    {
        return 0;
    }

    // If the clip is different, then we can't merge.
    if(MemoryCompare(&Old->Clip, &New->Clip, sizeof(rect_float)) != 0)
    {
        return 0;
    }

    return 1;
}
