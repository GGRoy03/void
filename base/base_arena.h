#pragma once

typedef struct memory_arena_params
{
    u64 CommitSize;
    u64 ReserveSize;
    char   *AllocatedFromFile;
    u32 AllocatedFromLine;
} memory_arena_params;

typedef struct memory_arena
{
    u64 CommitSize;
    u64 ReserveSize;

    u64 BasePosition;
    u64 Position;

    char   *AllocatedFromFile;
    u32 AllocatedFromLine;
} memory_arena;

global u64 ArenaDefaultReserveSize = Megabyte(64);
global u64 ArenaDefaultCommitSize  = Kilobyte(64);

internal memory_arena *AllocateArena  (memory_arena_params Params);
internal void         *PushArena      (memory_arena *Arena, u64 Size, u64 Align);
internal void          ClearArena     (memory_arena *Arena);
internal void          PopArena       (memory_arena *Arena, u64 Amount);