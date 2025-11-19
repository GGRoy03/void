#pragma once

// [STD-LIB]

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

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

// [Detect Compiler]

#if defined(_MSC_VER)
#define MSVC_COMPILER
#elif defined(__clang__)
#define CLANG_COMPILER
#elif defined(__GNUC__)
#define GCC_COMPILER
#endif

// [Useful Macros]

#define internal      static
#define global        static
#define local_persist static
#define read_only     const

#define Assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)
#define Useless(x) (void)(x)
#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))
#define IsPowerOfTwo(Value) (((Value) & ((Value) - 1)) == 0)
#define Concat(a, b) Concat2(a, b)
#define Concat2(a, b) a##b

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
#define DeferLoop(Begin, End) for(i32 Concat(_defer_, __LINE__) = ((Begin), 0); !Concat(_defer_, __LINE__); Concat(_defer_, __LINE__)++, (End))

// [Compiler Warnings]

#if defined(MSVC_COMPILER)
#define DisableWarning(Code) __pragma (warning(disable: Code))
#endif

// [Bits Handling]

#define SetFlag(v, f)    ((v) |=  (f))
#define ClearFlag(v, f)  ((v) &= ~(f))
#define ToggleFlag(v, f) ((v) ^=  (f))
#define HasFlag(v, f)    ((v) &   (f))

#ifdef MSVC_COMPILER
#define FindFirstBit(Mask) _tzcnt_u32(Mask)
#elif defined(CLANG_COMPILER)
#define FindFirstBit(Mask) __builtin_ctz(Mask)
#elif defined(GCC_COMPILER)
#define FindFirstBit(Mask) __builtin_ctz(Mask)
#else
#error  FindFirstBit not defined for this compiler.
#endif

#define NoFlag 0

global read_only u32 Bitmask1  = 0x00000001;
global read_only u32 Bitmask2  = 0x00000003;
global read_only u32 Bitmask3  = 0x00000007;
global read_only u32 Bitmask4  = 0x0000000f;
global read_only u32 Bitmask5  = 0x0000001f;
global read_only u32 Bitmask6  = 0x0000003f;
global read_only u32 Bitmask7  = 0x0000007f;
global read_only u32 Bitmask8  = 0x000000ff;
global read_only u32 Bitmask9  = 0x000001ff;
global read_only u32 Bitmask10 = 0x000003ff;
global read_only u32 Bitmask11 = 0x000007ff;
global read_only u32 Bitmask12 = 0x00000fff;
global read_only u32 Bitmask13 = 0x00001fff;
global read_only u32 Bitmask14 = 0x00003fff;
global read_only u32 Bitmask15 = 0x00007fff;
global read_only u32 Bitmask16 = 0x0000ffff;
global read_only u32 Bitmask17 = 0x0001ffff;
global read_only u32 Bitmask18 = 0x0003ffff;
global read_only u32 Bitmask19 = 0x0007ffff;
global read_only u32 Bitmask20 = 0x000fffff;
global read_only u32 Bitmask21 = 0x001fffff;
global read_only u32 Bitmask22 = 0x003fffff;
global read_only u32 Bitmask23 = 0x007fffff;
global read_only u32 Bitmask24 = 0x00ffffff;
global read_only u32 Bitmask25 = 0x01ffffff;
global read_only u32 Bitmask26 = 0x03ffffff;
global read_only u32 Bitmask27 = 0x07ffffff;
global read_only u32 Bitmask28 = 0x0fffffff;
global read_only u32 Bitmask29 = 0x1fffffff;
global read_only u32 Bitmask30 = 0x3fffffff;
global read_only u32 Bitmask31 = 0x7fffffff;
global read_only u32 Bitmask32 = 0xffffffff;

global read_only u64 Bitmask33 = 0x00000001ffffffffull;
global read_only u64 Bitmask34 = 0x00000003ffffffffull;
global read_only u64 Bitmask35 = 0x00000007ffffffffull;
global read_only u64 Bitmask36 = 0x0000000fffffffffull;
global read_only u64 Bitmask37 = 0x0000001fffffffffull;
global read_only u64 Bitmask38 = 0x0000003fffffffffull;
global read_only u64 Bitmask39 = 0x0000007fffffffffull;
global read_only u64 Bitmask40 = 0x000000ffffffffffull;
global read_only u64 Bitmask41 = 0x000001ffffffffffull;
global read_only u64 Bitmask42 = 0x000003ffffffffffull;
global read_only u64 Bitmask43 = 0x000007ffffffffffull;
global read_only u64 Bitmask44 = 0x00000fffffffffffull;
global read_only u64 Bitmask45 = 0x00001fffffffffffull;
global read_only u64 Bitmask46 = 0x00003fffffffffffull;
global read_only u64 Bitmask47 = 0x00007fffffffffffull;
global read_only u64 Bitmask48 = 0x0000ffffffffffffull;
global read_only u64 Bitmask49 = 0x0001ffffffffffffull;
global read_only u64 Bitmask50 = 0x0003ffffffffffffull;
global read_only u64 Bitmask51 = 0x0007ffffffffffffull;
global read_only u64 Bitmask52 = 0x000fffffffffffffull;
global read_only u64 Bitmask53 = 0x001fffffffffffffull;
global read_only u64 Bitmask54 = 0x003fffffffffffffull;
global read_only u64 Bitmask55 = 0x007fffffffffffffull;
global read_only u64 Bitmask56 = 0x00ffffffffffffffull;
global read_only u64 Bitmask57 = 0x01ffffffffffffffull;
global read_only u64 Bitmask58 = 0x03ffffffffffffffull;
global read_only u64 Bitmask59 = 0x07ffffffffffffffull;
global read_only u64 Bitmask60 = 0x0fffffffffffffffull;
global read_only u64 Bitmask61 = 0x1fffffffffffffffull;
global read_only u64 Bitmask62 = 0x3fffffffffffffffull;
global read_only u64 Bitmask63 = 0x7fffffffffffffffull;
global read_only u64 Bitmask64 = 0xffffffffffffffffull;

global read_only u32 Bit1  = (1<<0);
global read_only u32 Bit2  = (1<<1);
global read_only u32 Bit3  = (1<<2);
global read_only u32 Bit4  = (1<<3);
global read_only u32 Bit5  = (1<<4);
global read_only u32 Bit6  = (1<<5);
global read_only u32 Bit7  = (1<<6);
global read_only u32 Bit8  = (1<<7);
global read_only u32 Bit9  = (1<<8);
global read_only u32 Bit10 = (1<<9);
global read_only u32 Bit11 = (1<<10);
global read_only u32 Bit12 = (1<<11);
global read_only u32 Bit13 = (1<<12);
global read_only u32 Bit14 = (1<<13);
global read_only u32 Bit15 = (1<<14);
global read_only u32 Bit16 = (1<<15);
global read_only u32 Bit17 = (1<<16);
global read_only u32 Bit18 = (1<<17);
global read_only u32 Bit19 = (1<<18);
global read_only u32 Bit20 = (1<<19);
global read_only u32 Bit21 = (1<<20);
global read_only u32 Bit22 = (1<<21);
global read_only u32 Bit23 = (1<<22);
global read_only u32 Bit24 = (1<<23);
global read_only u32 Bit25 = (1<<24);
global read_only u32 Bit26 = (1<<25);
global read_only u32 Bit27 = (1<<26);
global read_only u32 Bit28 = (1<<27);
global read_only u32 Bit29 = (1<<28);
global read_only u32 Bit30 = (1<<29);
global read_only u32 Bit31 = (1<<30);
global read_only u32 Bit32 = (1<<31);

global read_only u64 Bit33 = (1ull<<32);
global read_only u64 Bit34 = (1ull<<33);
global read_only u64 Bit35 = (1ull<<34);
global read_only u64 Bit36 = (1ull<<35);
global read_only u64 Bit37 = (1ull<<36);
global read_only u64 Bit38 = (1ull<<37);
global read_only u64 Bit39 = (1ull<<38);
global read_only u64 Bit40 = (1ull<<39);
global read_only u64 Bit41 = (1ull<<40);
global read_only u64 Bit42 = (1ull<<41);
global read_only u64 Bit43 = (1ull<<42);
global read_only u64 Bit44 = (1ull<<43);
global read_only u64 Bit45 = (1ull<<44);
global read_only u64 Bit46 = (1ull<<45);
global read_only u64 Bit47 = (1ull<<46);
global read_only u64 Bit48 = (1ull<<47);
global read_only u64 Bit49 = (1ull<<48);
global read_only u64 Bit50 = (1ull<<49);
global read_only u64 Bit51 = (1ull<<50);
global read_only u64 Bit52 = (1ull<<51);
global read_only u64 Bit53 = (1ull<<52);
global read_only u64 Bit54 = (1ull<<53);
global read_only u64 Bit55 = (1ull<<54);
global read_only u64 Bit56 = (1ull<<55);
global read_only u64 Bit57 = (1ull<<56);
global read_only u64 Bit58 = (1ull<<57);
global read_only u64 Bit59 = (1ull<<58);
global read_only u64 Bit60 = (1ull<<59);
global read_only u64 Bit61 = (1ull<<60);
global read_only u64 Bit62 = (1ull<<61);
global read_only u64 Bit63 = (1ull<<62);
global read_only u64 Bit64 = (1ull<<63);

// [Linked List]

#define AppendToLinkedList(List, Node, Counter)       if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; List->Last = Node; ++Counter
#define AppendToDoublyLinkedList(List, Node, Counter) if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; Node->Prev = List->Last; List->Last = Node; ++Counter;
#define IterateLinkedList(List, Type, N)             for(Type N = List->First; N != 0; N = N->Next)
#define IterateLinkedListBackward(List, Type, N)     for(Type N = List->Last ; N != 0; N = N->Prev)
