internal memory_arena *
AllocateArena(memory_arena_params Params)
{
    u64 ReserveSize = AlignPow2(Params.ReserveSize, OSGetSystemInfo()->PageSize);
    u64 CommitSize  = AlignPow2(Params.CommitSize , OSGetSystemInfo()->PageSize);

    if (CommitSize > ReserveSize)
    {
        CommitSize = ReserveSize;
    }

    void *HeapBase     = OSReserveMemory(ReserveSize);
    b32   CommitResult = OSCommitMemory(HeapBase, CommitSize);
    if(!HeapBase || !CommitResult)
    {
    }

    memory_arena *Arena = (memory_arena*)HeapBase;
    Arena->Current           = Arena;
    Arena->CommitSize        = Params.CommitSize;
    Arena->ReserveSize       = Params.ReserveSize;
    Arena->Committed         = CommitSize;
    Arena->Reserved          = ReserveSize;
    Arena->BasePosition      = 0;
    Arena->Position          = sizeof(memory_arena);
    Arena->AllocatedFromFile = Params.AllocatedFromFile;
    Arena->AllocatedFromLine = Params.AllocatedFromLine;

    return Arena;
}

internal void
ReleaseArena(memory_arena *Arena)
{
    memory_arena *Prev = 0;
    for (memory_arena *Node = Arena->Current; Node != 0; Node = Prev)
    {
        Prev = Node->Prev;
        OSRelease(Node);
    }
}

internal void *
PushArena(memory_arena *Arena, u64 Size, u64 Alignment)
{
    memory_arena *Active       = Arena->Current;
    u64           PrePosition  = AlignPow2(Active->Position, Alignment);
    u64           PostPosition = PrePosition + Size;

    if(Active->Reserved < PostPosition)
    {
        memory_arena *New = 0;

        u64 ReserveSize = Active->ReserveSize;
        u64 CommitSize  = Active->CommitSize;
        if(Size + sizeof(memory_arena) > ReserveSize)
        {
            ReserveSize = AlignPow2(Size + sizeof(memory_arena), Alignment);
            CommitSize  = AlignPow2(Size + sizeof(memory_arena), Alignment);
        }

        memory_arena_params Params = {0};
        Params.CommitSize        = CommitSize;
        Params.ReserveSize       = ReserveSize;
        Params.AllocatedFromFile = Active->AllocatedFromFile;
        Params.AllocatedFromLine = Active->AllocatedFromLine;

        New = AllocateArena(Params);
        New->BasePosition = Active->BasePosition + Active->ReserveSize;
        New->Prev         = Active;

        Active       = New;
        PrePosition  = AlignPow2(Active->Position, Alignment);
        PostPosition = PrePosition + Size;
    }

    if(Active->Committed < PostPosition)
    {
        u64 CommitPostAligned = PostPosition;
        AlignMultiple(CommitPostAligned, Active->CommitSize);

        u64 CommitPostClamped = ClampTop(CommitPostAligned, Active->Reserved);
        u64 CommitSize        = CommitPostClamped - Active->Committed;
        u8 *CommitPointer     = (u8*)Active + Active->Committed;

        b32 CommitResult = OSCommitMemory(CommitPointer, CommitSize);
        if(!CommitResult)
        {
            OSAbort(1);
        }

        Active->Committed = CommitPostClamped;
    }

    void *Result = 0;
    if(Active->Committed >= PostPosition)
    {
        Result          = (u8*)Active + PrePosition;
        Active->Position = PostPosition;
    }

    return Result;
}

internal void
ClearArena(memory_arena *Arena)
{
    PopArenaTo(Arena, 0);
}

external void
PopArenaTo(memory_arena *Arena, u64 Position)
{
    memory_arena *Active    = Arena->Current;
    u64           PoppedPos = ClampBot(sizeof(memory_arena), Position);

    for(memory_arena *Prev = 0; Active->BasePosition >= PoppedPos; Active = Prev)
    {
        Prev = Active->Prev;
        OSRelease(Active);
    }

    Arena->Current           = Active;
    Arena->Current->Position = PoppedPos - Arena->Current->BasePosition;
}

external void
PopArena(memory_arena *Arena, u64 Amount)
{
    u64 OldPosition = GetArenaPosition(Arena);
    u64 NewPosition = OldPosition;

    if (Amount < OldPosition)
    {
        NewPosition = OldPosition - Amount;
    }

    PopArenaTo(Arena, NewPosition);
}

internal u64
GetArenaPosition(memory_arena *Arena)
{
    memory_arena *Active = Arena->Current;

    u64 Result = Active->BasePosition + Active->Position;
    return Result;
}

external memory_region 
EnterMemoryRegion(memory_arena *Arena)
{
    memory_region Result;
    Result.Arena    = Arena;
    Result.Position = GetArenaPosition(Arena);

    return Result;
}

external void
LeaveMemoryRegion(memory_region Region)
{
    PopArenaTo(Region.Arena, Region.Position);
}
