#pragma once

// [Detect Compiler]

#if defined(_MSC_VER)
#define MSVC_COMPILER
#elif defined(__clang__)
#define CLANG_COMPILER
#elif defined(__GNUC__)
#define GCC_COMPILER
#endif;

// [Useful Macros]

#define internal      static
#define global        static
#define local_persist static
#define read_only     const

#define UNUSED(x) (void)(x)

// [Units]

#define Kilobyte(n) (((u64)(n)) << 10)
#define Megabyte(n) (((u64)(n)) << 20)
#define Gygabyte(n) (((u64)(n)) << 30)

// [Alignment]

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

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

#define ForEachEnum(Type, It) for(Type It = (Type)0; It < Type##_Count; It = (Type)(It + 1))

// [Disabling Warnings]

#if defined(MSVC_COMPILER)
#define DisableWarning(Code) __pragma (warning(disable: Code))
#endif

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

// NOTE: These are math related.

typedef struct vec2_f32
{
    f32 X, Y;
} vec2_f32;

typedef struct vec4_f32
{
    f32 X, Y, Z, W;
} vec4_f32;

typedef struct vec2_i32
{
    i32 X, Y;
} vec2_i32;

typedef struct mat3x3_f32
{
    void *Data;
} mat3x3_f32;
