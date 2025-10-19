#pragma once

// [Core Types]

DisableWarning(4201)
typedef struct vec2_f32
{
    union
    {
        struct {f32 X; f32 Y;};
        f32 Values[2];
    };
} vec2_f32;

DisableWarning(4201)
typedef struct vec4_f32
{
    union
    {
        struct { f32 X; f32 Y; f32 Z; f32 W; };
        f32 Values[4];
    };
} vec4_f32;

typedef struct vec2_i32
{
    i32 X, Y;
} vec2_i32;

typedef struct matrix_3x3
{
    f32 c0r0, c0r1, c0r2;
    f32 c1r0, c1r1, c1r2;
    f32 c2r0, c2r1, c2r2;
} matrix_3x3;

typedef struct matrix_4x4
{
    f32 c0r0, c0r1, c0r2, c0r3;
    f32 c1r0, c1r1, c1r2, c1r3;
    f32 c2r0, c2r1, c2r2, c2r3;
    f32 c3r0, c3r1, c3r2, c3r3;
} matrix_4x4;

// rect_f32:
//   Simple structure representing a rectangle in 2D space using two vectors.
//
// RectF32:
// RectF32Zero:
// TranslatedRectF32:
//   Helper Constructors. Parameters are self-explanatory
//
// IntersectRectF32:
//
// IsPointInRect:
//   returns 1 if a given point is inside the rectangle, else 0.
//
// RoundedRectSDF:
//   Produces a signed distance value representing how far we are from the closest edge of the rectangle.
//   LocalPosition: Must be relative to the rectangle's origin, typically do: (Point-RectOrigin) to get the local point.
//   RectHalfSize:  must be half the rect's size on both axis. If this value is not exact, values may be wrong.
//   Radius:        Give a radius if you care about rounded edges, passing 0 is valid.
//   returns  0 IF The given point is directly on one of the rectangle's edge.
//   returns <0 IF The given point is inside the rect.
//   returns >0 IF The given point is outside the rect.
//
// RectsIntersect:
//   returns 1 if the rects instersect in any way, else return 0.

typedef struct rect_f32
{
    vec2_f32 Min;
    vec2_f32 Max;
} rect_f32;

internal rect_f32 RectF32            (f32 MinX, f32 MinY, f32 Width, f32 Height);
internal rect_f32 RectF32Zero        (void);
internal rect_f32 TranslatedRectF32  (rect_f32 Rect, vec2_f32 Translation);

internal rect_f32 IntersectRectF32  (rect_f32 R1, rect_f32 R2);
internal b32      IsPointInRect     (rect_f32 Target, vec2_f32 Point);
internal f32      RoundedRectSDF    (vec2_f32 LocalPosition, vec2_f32 RectHalfSize, f32 Radius);
internal b32      RectsIntersect    (rect_f32 A, rect_f32 B);
internal rect_f32 InsetRectF32      (rect_f32 Rect, f32 Size);

// [Constructors]

internal vec2_f32   Vec2F32            (f32 X, f32 Y);
internal vec4_f32   Vec4F32            (f32 X, f32 Y, f32 Z, f32 W);
internal vec2_i32   Vec2I32            (i32 X, i32 Y);
internal vec2_f32   Vec2I32ToVec2F32   (vec2_i32 Vec);
internal matrix_3x3 Mat3x3Zero         (void);
internal matrix_3x3 Mat3x3Identity     (void);

// [Vector OPs]

internal vec2_f32 Vec2F32Add     (vec2_f32 Vec1, vec2_f32 Vec2);
internal vec2_f32 Vec2F32Sub     (vec2_f32 Vec1, vec2_f32 Vec2);
internal vec2_f32 Vec2F32Abs     (vec2_f32 Vec);
internal f32      Vec2F32Length  (vec2_f32 Vec);

// [Ranges]

internal b32 IsInRangeF32(f32 Min, f32 Max, f32 Value);

// [Helpers]

#define IterateVec4() for(u32 Index = 0; Index < 4; Index++)

internal b32 Vec2I32IsEqual    (vec2_i32 Vec1, vec2_i32 Vec2);
internal b32 Vec2I32IsEmpty    (vec2_i32 Vec);
internal b32 Vec2F32IsEqual    (vec2_f32 Vec1, vec2_f32 Vec2);
internal b32 Vec2F32IsEmpty    (vec2_f32 Vec);
internal b32 Mat3x3AreEqual    (matrix_3x3 *m1, matrix_3x3 *m2);
