#pragma once

typedef struct typed_queue_params
{
    uint64_t QueueSize;
} typed_queue_params;

#define DEFINE_TYPED_QUEUE(Prefix, Name, Type)                       \
typedef struct Name##_queue                                          \
{                                                                    \
    Type *Data;                                                      \
    uint64_t   Head;                                                      \
    uint64_t   Tail;                                                      \
    uint64_t   Count;                                                     \
    uint64_t   Capacity;                                                  \
} Name##_queue;                                                      \
                                                                     \
static Name##_queue                                                \
Begin##Prefix##Queue(typed_queue_params Params, memory_arena *Arena) \
{   VOID_ASSERT(Params.QueueSize > 0 && Arena);                           \
                                                                     \
    Name##_queue Result = {0};                                       \
    Result.Data     = PushArray(Arena, Type, Params.QueueSize);      \
    Result.Head     = 0;                                             \
    Result.Tail     = 0;                                             \
    Result.Count    = 0;                                             \
    Result.Capacity = Params.QueueSize;                              \
                                                                     \
    return Result;                                                   \
}                                                                    \
                                                                     \
static void                                                        \
Push##Prefix##Queue(Name##_queue *Q, Type Value)                     \
{                                                                    \
    if(Q && Q->Count != Q->Capacity)                                 \
    {                                                                \
        Q->Data[Q->Tail++] = Value;                                  \
                                                                     \
        if(Q->Tail == Q->Capacity)                                   \
        {                                                            \
            Q->Tail = 0;                                             \
        }                                                            \
                                                                     \
        ++Q->Count;                                                  \
    }                                                                \
}                                                                    \
                                                                     \
static Type                                                        \
Pop##Prefix##Queue(Name##_queue *Q)                                  \
{                                                                    \
    Type Result = 0;                                                 \
                                                                     \
    if(Q && Q->Count)                                                \
    {                                                                \
        Result = Q->Data[Q->Head++];                                 \
                                                                     \
        if(Q->Head == Q->Capacity)                                   \
        {                                                            \
            Q->Head = 0;                                             \
        }                                                            \
                                                                     \
        --Q->Count;                                                  \
    }                                                                \
                                                                     \
    return Result;                                                   \
}                                                                    \
                                                                     \
static bool                                                         \
Is##Prefix##QueueEmpty(Name##_queue *Q)                              \
{   VOID_ASSERT(Q);                                                       \
                                                                     \
    bool Result = Q->Count == 0;                                      \
    return Result;                                                   \
}
