// [Internal API]

internal size_t 
EstimateUIPassFootprint(render_pass_ui_stats Stats)
{
    size_t Result = UIPassPaddingSize;

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
        // Prev is only ever set if we chained, meaning we didn't reserve enough
        // memory and we had to allocate another arena. In those cases we want to 
        // clear the overflowing arenas and re-allocate inside a bigger one. We
        // can estimate a decent size for the next frame using this frame's stat.

        memory_arena *UIPassArena = Context->PassArena[RenderPass_UI];
        if (UIPassArena)
        {
            ClearArena(UIPassArena);
        }

        if(!UIPassArena || UIPassArena->Current->Prev)
        {
            memory_arena_params Params = {0};
            Params.ReserveSize       = EstimateUIPassFootprint(Context->UIStats);
            Params.CommitSize        = ArenaDefaultCommitSize;
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;

            Context->PassArena[RenderPass_UI] = AllocateArena(Params);
        }

        // Reset stats
        Context->UIStats.BatchCount       = 0;
        Context->UIStats.GroupCount       = 0;
        Context->UIStats.PassCount        = 0;
        Context->UIStats.RenderedDataSize = 0;

        // Reset Pointers
        Context->FirstPassNode[RenderPass_UI] = 0;
        Context->LastPassNode[RenderPass_UI]  = 0;
    }
}

internal b32 
IsValidRenderHandle(render_handle Handle)
{
    b32 Result = (Handle.u64[0] != 0);
    return Result;
}

// BUG: 
// 1) Does not track the stats.

internal render_batch_list *
GetBatchList(render_context *Context, RenderPass_Type Pass)
{
    render_batch_list *Result = 0;

    if (Context)
    {
        // NOTE: I believe this should check the validity of the parameters as well.

        render_pass_node *PassNode  = Context->LastPassNode[Pass];
        memory_arena     *PassArena = Context->PassArena[Pass];
        if (!PassNode)
        {
            render_pass_params_ui *Params = PushArena(PassArena, sizeof(render_pass_params_ui), AlignOf(render_pass_params_ui));
            Params->First = 0;
            Params->Last  = 0;

            PassNode = PushArena(PassArena, sizeof(render_pass_node), AlignOf(render_pass_node));
            PassNode->Next           = 0;
            PassNode->Pass.Type      = RenderPass_UI;
            PassNode->Pass.Params.UI = Params;

            Context->FirstPassNode[Pass] = PassNode;
            Context->LastPassNode[Pass]  = PassNode;
        }

        // BUG: This should not access the UI directly. This whole Batch/List/Params code
        // need some serious work.

        render_rect_group_node *RectNode = PassNode->Pass.Params.UI->Last;
        if (!RectNode)
        {
            RectNode = PushArena(PassArena, sizeof(render_rect_group_node), AlignOf(render_rect_group_node));
            RectNode->BatchList.BatchCount       = 0;
            RectNode->BatchList.ByteCount        = 0;
            RectNode->BatchList.BytesPerInstance = RenderPassDataSizeTable[Pass];
            RectNode->BatchList.First            = 0;
            RectNode->BatchList.Last             = 0;

            PassNode->Pass.Params.UI->First = RectNode;
        }

        Result = &RectNode->BatchList;
    }

    return Result;
}

// BUG: Does not track the stats per pass.

internal render_batch *
BeginRenderBatch(u64 Size, render_batch_list *BatchList, memory_arena *Arena)
{
    render_batch *Result = 0;

    if (BatchList)
    {
        void *Memory = PushArena(Arena, Size, AlignOf(void *));

        render_batch_node *Node = PushArena(Arena, sizeof(render_batch_node), AlignOf(render_batch_node));
        Node->Next               = 0;
        Node->Value.ByteCapacity = Size;
        Node->Value.ByteCount    = 0;
        Node->Value.Memory       = Memory;

        if (BatchList->First)
        {
            BatchList->Last->Next = Node;
            BatchList->Last       = Node;
        }
        else
        {
            BatchList->First = Node;
            BatchList->Last  = Node;
        }

        Result = &Node->Value;

        BatchList->BatchCount += 1;
    }

    return Result;
}

internal render_rect *
AllocateRect(render_batch *Batch)
{
    render_rect *Result = 0;

    if (Batch)
    {
        u64 Size  = sizeof(render_rect);

        // NOTE: Not entirely sure what I want to do here.
        // Because I don't know how the caller will use this code yet.
        if (Batch->ByteCount + Size > Batch->ByteCapacity)
        {
            return NULL;
        }

        Result            = (render_rect *)((u8 *)Batch->Memory + Batch->ByteCount);
        Batch->ByteCount += Size;
    }

    return Result;
}