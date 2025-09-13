// [Internal API]

internal size_t 
EstimateUIPassFootprint(render_pass_ui_stats Stats)
{
    size_t Result = UIPassDefaultPaddingSize;

    if(Stats.GroupCount == 0 || Stats.PassCount == 0 || Stats.RenderedDataSize == 0)
    {
        Result += UIPassDefaultRenderedDataSize;
        Result += UIPassDefaultBatchCount * sizeof(render_batch_node);
        Result += UIPassDefaultGroupCount * sizeof(render_rect_group_node);
        Result += UIPassDefaultPassCount  * sizeof(render_pass_node);
    }
    else
    {
        Result += Stats.RenderedDataSize;
        Result += (Stats.BatchCount - 0) * sizeof(render_batch_node);
        Result += (Stats.GroupCount - 0) * sizeof(render_rect_group_node);
        Result += (Stats.PassCount  - 1) * sizeof(render_pass_node);
    }

    return Result;
}

// [Core API]

internal void
BeginRendereringContext(render_context *Context)
{
    if(Context)
    {
        // BUG: This looks really off. I never release any arenas.

        // UI Pass
        {
            if (!Context->UIArena || Context->UIArena->Prev)
            {
                // TODO: We must release the old arena if it exists and it chained.

                memory_arena_params Params = { 0 };
                Params.ReserveSize       = EstimateUIPassFootprint(Context->UIStats);
                Params.CommitSize        = ArenaDefaultCommitSize;
                Params.AllocatedFromFile = __FILE__;
                Params.AllocatedFromLine = __LINE__;

                Context->UIArena = AllocateArena(Params);
            }
            else
            {
                ClearArena(Context->UIArena);
            }

            // Reset stats
            Context->UIStats.BatchCount       = 0;
            Context->UIStats.GroupCount       = 0;
            Context->UIStats.PassCount        = 0;
            Context->UIStats.RenderedDataSize = 0;

            // Reset Pointers
            Context->UIParams.First = 0;
            Context->UIParams.Last  = 0;
        }

        // Game Pass
        {

        }
    }
}

internal b32 
IsValidRenderHandle(render_handle Handle)
{
    b32 Result = (Handle.u64[0] != 0);
    return Result;
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
        Node->Value.ByteCapacity = Kilobyte(5); // BUG: This works, but would be better if we could infer it?
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

internal render_batch_list *
GetUIBatchList(render_context *Context)
{
    render_batch_list      *Result    = 0;
    render_pass_params_ui  *Params    = &Context->UIParams;
    render_rect_group_node *GroupNode = Params->Last;

    if (!GroupNode)
    {
        GroupNode = PushArena(Context->UIArena, sizeof(render_rect_group_node), AlignOf(render_rect_group_node));
        GroupNode->Next                       = 0;
        GroupNode->BatchList.BatchCount       = 0;
        GroupNode->BatchList.ByteCount        = 0;
        GroupNode->BatchList.BytesPerInstance = RenderPassDataSizeTable[RenderPass_UI];
        GroupNode->BatchList.First            = 0;
        GroupNode->BatchList.Last             = 0;

        Params->First = GroupNode;
    }

    // NOTE: What is this mess?
    if (Params->Last && Params->Last != GroupNode)
    {
        Params->Last->Next = GroupNode;
    }
    Params->Last = GroupNode;

    Result = &GroupNode->BatchList;
    return Result;
}