#pragma once

typedef struct memory_arena_params
{
    uint64_t         CommitSize;
    uint64_t         ReserveSize;
    const char *AllocatedFromFile;
    uint32_t         AllocatedFromLine;
} memory_arena_params;

typedef struct memory_arena memory_arena;
struct memory_arena
{
    memory_arena *Prev;
    memory_arena *Current;

    uint64_t CommitSize;
    uint64_t ReserveSize;
    uint64_t Committed;
    uint64_t Reserved;

    uint64_t BasePosition;
    uint64_t Position;

    const char *AllocatedFromFile;
    uint32_t         AllocatedFromLine;
};

typedef struct memory_region
{
    memory_arena *Arena;
    uint64_t           Position;
} memory_region;

static uint64_t ArenaDefaultReserveSize = VOID_MEGABYTE(64);
static uint64_t ArenaDefaultCommitSize  = VOID_KILOBYTE(64);

static memory_arena *AllocateArena  (memory_arena_params Params);
static void          ReleaseArena   (memory_arena *Arena);

static void *PushArena   (memory_arena *Arena, uint64_t Size, uint64_t Align);
static void  ClearArena  (memory_arena *Arena);
static void  PopArenaTo  (memory_arena *Arena, uint64_t Position);
static void  PopArena    (memory_arena *Arena, uint64_t Amount);

static uint64_t GetArenaPosition(memory_arena *Arena);

memory_region EnterMemoryRegion  (memory_arena *Arena);
void          LeaveMemoryRegion  (memory_region Region);

#define PushArrayNoZeroAligned(a, T, c, align) (T *)PushArena((a), sizeof(T)*(c), (align))
#define PushArrayAligned(a, T, c, align) (T *)MemoryZero(PushArrayNoZeroAligned(a, T, c, align), sizeof(T)*(c))
#define PushArrayNoZero(a, T, c) PushArrayNoZeroAligned(a, T, c, Max(8, AlignOf(T)))
#define PushArray(a, T, c) PushArrayAligned(a, T, c, Max(8, AlignOf(T)))
#define PushStruct(a, T)   PushArrayAligned(a, T, 1, Max(8, AlignOf(T)))
