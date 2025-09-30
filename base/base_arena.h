#pragma once

typedef struct memory_arena_params
{
    u64   CommitSize;
    u64   ReserveSize;
    char *AllocatedFromFile;
    u32   AllocatedFromLine;
} memory_arena_params;

typedef struct memory_arena memory_arena;
struct memory_arena
{
    memory_arena *Prev;
    memory_arena *Current;

    u64 CommitSize;
    u64 ReserveSize;
    u64 Committed;
    u64 Reserved;

    u64 BasePosition;
    u64 Position;

    char *AllocatedFromFile;
    u32   AllocatedFromLine;
};

typedef struct memory_region
{
    memory_arena *Arena;
    u64           Position;
} memory_region;

global u64 ArenaDefaultReserveSize = Megabyte(64);
global u64 ArenaDefaultCommitSize  = Kilobyte(64);

internal memory_arena *AllocateArena  (memory_arena_params Params);
internal void          ReleaseArena   (memory_arena *Arena);

internal void *PushArena   (memory_arena *Arena, u64 Size, u64 Align);
internal void  ClearArena  (memory_arena *Arena);
external void  PopArenaTo  (memory_arena *Arena, u64 Position);
external void  PopArena    (memory_arena *Arena, u64 Amount);

internal u64 GetArenaPosition(memory_arena *Arena);

internal memory_region EnterMemoryRegion  (memory_arena *Arena);
internal void          LeaveMemoryRegion  (memory_region Region);

#define PushArrayNoZeroAligned(a, T, c, align) (T *)PushArena((a), sizeof(T)*(c), (align))
#define PushArrayAligned(a, T, c, align) (T *)MemoryZero(PushArrayNoZeroAligned(a, T, c, align), sizeof(T)*(c))
#define PushArrayNoZero(a, T, c) PushArrayNoZeroAligned(a, T, c, Max(8, AlignOf(T)))
#define PushArray(a, T, c) PushArrayAligned(a, T, c, Max(8, AlignOf(T)))