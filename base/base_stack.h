#pragma once

typedef struct typed_stack_params
{
    u64 StackSize;
} typed_stack_params;

typedef struct typed_stack
{
    void *Data;
    u64   Top;
    u64   Capacity;
} typed_stack;

#define DEFINE_TYPED_STACK(Prefix, Name, Type)                       \
typedef struct Name##_stack                                          \
{                                                                    \
    Type *Data;                                                      \
    u64   Top;                                                       \
    u64   Capacity;                                                  \
} Name##_stack;                                                      \
                                                                     \
internal Name##_stack                                                \
Begin##Prefix##Stack(typed_stack_params Params, memory_arena *Arena) \
{                                                                    \
    Assert(Params.StackSize > 0 && Arena);                           \
                                                                     \
    Name##_stack Result = {0};                                       \
    Result.Data     = PushArray(Arena, Type, Params.StackSize);      \
    Result.Top      = 0;                                             \
    Result.Capacity = Params.StackSize;                              \
                                                                     \
    return Result;                                                   \
}                                                                    \
                                                                     \
internal void                                                        \
Push##Prefix##Stack(Name##_stack *Stack, Type Value)                 \
{                                                                    \
    Assert(Stack);                                                   \
                                                                     \
    if (Stack->Top < Stack->Capacity)                                \
    {                                                                \
        Stack->Data[Stack->Top++] = Value;                           \
    }                                                                \
}                                                                    \
                                                                     \
internal Type                                                        \
Pop##Prefix##Stack(Name##_stack *Stack)                              \
{                                                                    \
    Assert(Stack);                                                   \
                                                                     \
    Type Result = (Type){0};                                         \
                                                                     \
    if(Stack->Top > 0)                                               \
    {                                                                \
        Result = Stack->Data[--Stack->Top];                          \
    }                                                                \
                                                                     \
    return Result;                                                   \
}                                                                    \
                                                                     \
internal Type                                                        \
Peek##Prefix##Stack(Name##_stack *Stack)                             \
{                                                                    \
    Assert(Stack);                                                   \
                                                                     \
    Type Result = (Type){0};                                         \
                                                                     \
    if(Stack->Top > 0)                                               \
    {                                                                \
        Result = Stack->Data[Stack->Top - 1];                        \
    }                                                                \
                                                                     \
    return Result;                                                   \
}                                                                    \
                                                                     \
internal b32                                                         \
Is##Prefix##StackEmpty(Name##_stack *Stack)                          \
{                                                                    \
    Assert(Stack);                                                   \
                                                                     \
    b32 Result = Stack->Top == 0;                                    \
    return Result;                                                   \
}
