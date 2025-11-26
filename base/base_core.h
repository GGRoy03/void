#pragma once

// ====================================================================================
// @internal: Standard Includes & Context Cracking & Utility Macros

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

#if defined(_MSV_VER)
    #define VOID_MSVC 1
#elif defined(__clang__)
    #define VOID_CLANG 1
#elif defined(__GNUC__)
    #define VOID_GCC 1
#else
    #error "VOID: Unknown Compiler"
#endif

#if VOID_MSVC || VOID_CLANG
    #define AlignOf(T) __alignof(T)
#elif VOID_GCC
    #define AlignOf(T) __alignof__(T)
#endif

#if VOID_MSVC
    #define FindFirstBit(Mask) _tzcnt_u32(Mask)
#elif VOID_CLANG || VOID_GCC
    #define FindFirstBit(Mask) __builtin_ctz(Mask)
#endif

#define VOID_NAMECONCAT2(a, b)   a##b
#define VOID_NAMECONCAT(a, b)    VOID_NAMECONCAT2(a, b)

#define VOID_ASSERT(Cond)        do { if (!(Cond)) __debugbreak(); } while (0)
#define VOID_UNUSED(X)           (void)(X)

#define VOID_ARRAYCOUNT(A)       (sizeof(A) / sizeof(A[0]))
#define VOID_ISPOWEROFTWO(Value) (((Value) & ((Value) - 1)) == 0)

#define VOID_KILOBYTE(n)         (((uint64_t)(n)) << 10)
#define VOID_MEGABYTE(n)         (((uint64_t)(n)) << 20)
#define VOID_GIGABYTE(n)         (((uint64_t)(n)) << 30)

#define VOID_BIT(n)              ((uint64_t)( 1ULL << ((n) - 1)))
#define VOID_BITMASK(n)          ((uint64_t)((1ULL << (n)) - 1ULL))
#define VOID_SETBIT(F, B)        ((F) |=  (B))
#define VOID_CLEARBIT(F, B)      ((F) &= ~(B))
#define VOID_HASBIT(F, B)        ((F) &   (B))

// [Alignment]

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define AlignMultiple(Value, Multiple) (Value += Multiple - 1); (Value -= Value % Multiple);

// [Clampers/Min/Max]

#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)

// [Memory Ops]

#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemorySet(dst, byte, size) memset((dst), (byte), (size))
#define MemoryCompare(a, b, size)  memcmp((a), (b), (size))
#define MemoryZero(s,z)            memset((s),0,(z))

// [Loop Macros]

#define ForEachEnum(Type, Count, It)  for(Type It = (Type)0; It < Count; It = (Type)(It + 1))
#define DeferLoop(Begin, End) for(int VOID_NAMECONCAT(_defer_, __LINE__) = ((Begin), 0); !VOID_NAMECONCAT(_defer_, __LINE__); VOID_NAMECONCAT(_defer_, __LINE__)++, (End))

// [Linked List]

#define AppendToLinkedList(List, Node, Counter)       if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; List->Last = Node; ++Counter
#define AppendToDoublyLinkedList(List, Node, Counter) if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; Node->Prev = List->Last; List->Last = Node; ++Counter;
#define IterateLinkedList(List, Type, N)             for(Type N = List->First; N != 0; N = N->Next)
#define IterateLinkedListBackward(List, Type, N)     for(Type N = List->Last ; N != 0; N = N->Prev)
