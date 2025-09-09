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

        if(!Context->UIPassArena || Context->UIPassArena->Current->Prev)
        {
            if (Context->UIPassArena)
            {
                ClearArena(Context->UIPassArena);
            }

            memory_arena_params Params = {0};
            Params.ReserveSize       = EstimateUIPassFootprint(Context->UIStats);
            Params.CommitSize        = ArenaDefaultCommitSize;
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;

            Context->UIPassArena = AllocateArena(Params);
        }

        // Reset stats
        Context->UIStats.BatchCount       = 0;
        Context->UIStats.GroupCount       = 0;
        Context->UIStats.PassCount        = 0;
        Context->UIStats.RenderedDataSize = 0;
    }
}

internal b32 
IsValidRenderHandle(render_handle Handle)
{
    b32 Result = (Handle.u64[0] != 0);
    return Result;
}
