static memory_arena *
AllocateArena(memory_arena_params Params)
{
    uint64_t ReserveSize = AlignPow2(Params.ReserveSize, OSGetSystemInfo()->PageSize);
    uint64_t CommitSize  = AlignPow2(Params.CommitSize , OSGetSystemInfo()->PageSize);

    if (CommitSize > ReserveSize)
    {
        CommitSize = ReserveSize;
    }

    void *HeapBase     = OSReserveMemory(ReserveSize);
    bool   CommitResult = OSCommitMemory(HeapBase, CommitSize);
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

static void
ReleaseArena(memory_arena *Arena)
{
    memory_arena *Prev = 0;
    for (memory_arena *Node = Arena->Current; Node != 0; Node = Prev)
    {
        Prev = Node->Prev;
        OSRelease(Node);
    }
}

static void *
PushArena(memory_arena *Arena, uint64_t Size, uint64_t Alignment)
{
    memory_arena *Active       = Arena->Current;
    uint64_t      PrePosition  = AlignPow2(Active->Position, Alignment);
    uint64_t      PostPosition = PrePosition + Size;

    if(Active->Reserved < PostPosition)
    {
        memory_arena *New = 0;

        uint64_t ReserveSize = Active->ReserveSize;
        uint64_t CommitSize  = Active->CommitSize;
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

        Active         = New;
        Arena->Current = New;

        PrePosition  = AlignPow2(Active->Position, Alignment);
        PostPosition = PrePosition + Size;
    }

    if(Active->Committed < PostPosition)
    {
        uint64_t CommitPostAligned = PostPosition;
        AlignMultiple(CommitPostAligned, Active->CommitSize);

        uint64_t CommitPostClamped = ClampTop(CommitPostAligned, Active->Reserved);
        uint64_t CommitSize        = CommitPostClamped - Active->Committed;
        uint8_t *CommitPointer     = (uint8_t*)Active + Active->Committed;

        bool CommitResult = OSCommitMemory(CommitPointer, CommitSize);
        if(!CommitResult)
        {
        }

        Active->Committed = CommitPostClamped;
    }

    void *Result = 0;
    if(Active->Committed >= PostPosition)
    {
        Result          = (uint8_t*)Active + PrePosition;
        Active->Position = PostPosition;
    }

    return Result;
}

static void
ClearArena(memory_arena *Arena)
{
    PopArenaTo(Arena, 0);
    MemorySet((Arena + 1), 0, (Arena->Committed - Arena->Position));
}

static void
PopArenaTo(memory_arena *Arena, uint64_t Position)
{
    memory_arena *Active    = Arena->Current;
    uint64_t      PoppedPos = ClampBot(sizeof(memory_arena), Position);

    for(memory_arena *Prev = 0; Active->BasePosition >= PoppedPos; Active = Prev)
    {
        Prev = Active->Prev;
        OSRelease(Active);
    }

    Arena->Current           = Active;
    Arena->Current->Position = PoppedPos - Arena->Current->BasePosition;
}

static void
PopArena(memory_arena *Arena, uint64_t Amount)
{
    uint64_t OldPosition = GetArenaPosition(Arena);
    uint64_t NewPosition = OldPosition;

    if (Amount < OldPosition)
    {
        NewPosition = OldPosition - Amount;
    }

    PopArenaTo(Arena, NewPosition);
}

static uint64_t
GetArenaPosition(memory_arena *Arena)
{
    memory_arena *Active = Arena->Current;

    uint64_t Result = Active->BasePosition + Active->Position;
    return Result;
}

static memory_region 
EnterMemoryRegion(memory_arena *Arena)
{
    memory_region Result;
    Result.Arena    = Arena;
    Result.Position = GetArenaPosition(Arena);

    return Result;
}

static void
LeaveMemoryRegion(memory_region Region)
{
    PopArenaTo(Region.Arena, Region.Position);
}
