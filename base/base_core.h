#pragma once

#define internal      static
#define global        static
#define local_persist static

#define Kilobyte(n) (((u64)(n)) << 10)
#define Megabyte(n) (((u64)(n)) << 20)
#define Gygabyte(n) (((u64)(n)) << 30)

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)

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

typedef struct vec2_f32
{
    f32 X, Y;
} vec2_f32;

typedef struct vec4_f32
{
    f32 X, Y, Z, W;
} vec4_f32;
