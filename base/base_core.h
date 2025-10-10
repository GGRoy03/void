#pragma once

// [STD-LIB]

#include <stdio.h>
#include <stdarg.h>
#include <math.h>

// [Detect Compiler]

#if defined(_MSC_VER)
#define MSVC_COMPILER
#elif defined(__clang__)
#define CLANG_COMPILER
#elif defined(__GNUC__)
#define GCC_COMPILER
#endif

// [Useful Macros]

#define external
#define internal      static
#define global        static
#define local_persist static
#define read_only     const

#define Assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)
#define Useless(x) (void)(x)
#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))
#define IsPowerOfTwo(Value) (((Value) & ((Value) - 1)) == 0)

// [Units]

#define Kilobyte(n) (((u64)(n)) << 10)
#define Megabyte(n) (((u64)(n)) << 20)
#define Gigabyte(n) (((u64)(n)) << 30)

// [Alignment]

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define AlignMultiple(Value, Multiple) (Value += Multiple - 1); (Value -= Value % Multiple);

#ifdef MSVC_COMPILER
#define AlignOf(T) __alignof(T)
#elif defined(CLANG_COMPILER)
#define AlignOf(T) __alignof(T)
#elif defined(GCC_COMPILER)
#define AlignOf(T) __alignof__(T)
#else
#error AlignOf not defined for this compiler.
#endif

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
#define DeferLoop(Begin, End)  for(i32 _i = ((Begin), 0); !_i; _i++, (End))

// [Compiler Warnings]

#if defined(MSVC_COMPILER)
#define DisableWarning(Code) __pragma (warning(disable: Code))
#endif

// [Flags]

#define SetFlag(v, f)    ((v) |=  (f))
#define ClearFlag(v, f)  ((v) &= ~(f))
#define ToggleFlag(v, f) ((v) ^=  (f))
#define HasFlag(v, f)    ((v) &   (f))

// [Linked List]

#define AppendToLinkedList(List, Node, Counter)       if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; List->Last = Node; ++Counter
#define AppendToDoublyLinkedList(List, Node, Counter) if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; Node->Prev = List->Last; List->Last = Node; ++Counter;
#define IterateLinkedList(First, Type, N)             for(Type N = First; N != 0; N = N->Next)

// [Core Types]

#include <stdint.h>

typedef int      b32;

typedef float    f32;
typedef double   f64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int       i32;
typedef long long i64;

typedef uint32_t  bit_field;

typedef struct os_handle
{
    u64 u64[1];
} os_handle;

typedef struct render_handle
{
    u64 u64[1];
} render_handle;
