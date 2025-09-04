#pragma once

typedef struct cim_arena
{
    void *Memory;
    size_t At;
    size_t Capacity;
} cim_arena;

typedef struct buffer
{
    cim_u8 *Data;
    cim_u64 At;
    cim_u64 Size;
} buffer;