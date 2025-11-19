// [Constructors]

internal vec2_f32
Vec2F32(f32 X, f32 Y)
{
    vec2_f32 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

internal vec4_f32
Vec4F32(f32 X, f32 Y, f32 Z, f32 W)
{
    vec4_f32 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;

    return Result;
}

internal vec2_i32
Vec2I32(i32 X, i32 Y)
{
    vec2_i32 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

internal vec2_f32
Vec2I32ToVec2F32(vec2_i32 Vec)
{
    vec2_f32 Result;
    Result.X = (f32)Vec.X;
    Result.Y = (f32)Vec.Y;

    return Result;
}

internal matrix_3x3 
Mat3x3Zero(void)
{
    matrix_3x3 Result = {};
    return Result;
}

internal matrix_3x3
Mat3x3Identity(void)
{
    matrix_3x3 Result;
    Result.c0r0 = 1;
    Result.c0r1 = 0;
    Result.c0r2 = 0;
    Result.c1r0 = 0;
    Result.c1r1 = 1;
    Result.c1r2 = 0;
    Result.c2r0 = 0;
    Result.c2r1 = 0;
    Result.c2r2 = 1;

    return Result;
}

// [Rect]

internal rect_f32
RectF32(f32 MinX, f32 MinY, f32 Width, f32 Height)
{
    rect_f32 Result;
    Result.Min.X = MinX;
    Result.Min.Y = MinY;
    Result.Max.X = MinX + Width;
    Result.Max.Y = MinY + Height;

    return Result;
}

internal rect_f32
RectF32Zero(void)
{
    rect_f32 Result = {};
    return Result;
}

internal rect_f32
TranslatedRectF32(rect_f32 Rect, vec2_f32 Translation)
{
    rect_f32 Result = Rect;
    Result.Min.X += Translation.X;
    Result.Min.Y += Translation.Y;
    Result.Max.X += Translation.X;
    Result.Max.Y += Translation.Y;

    return Result;
}

internal rect_f32
IntersectRectF32(rect_f32 A, rect_f32 B)
{
    vec2_f32 Min    = Vec2F32(max(A.Min.X, B.Min.X), max(A.Min.Y, B.Min.Y));
    vec2_f32 Max    = Vec2F32(min(A.Max.X, B.Max.X), min(A.Max.Y, B.Max.Y));
    rect_f32 Result = {Min, Max};

    if (Result.Max.X <= Result.Min.X || Result.Max.Y <= Result.Min.Y)
    {
        Result = (rect_f32){0};
    }

    return Result;
}

internal b32
IsPointInRect(rect_f32 Target, vec2_f32 Point)
{
    b32 Result = (Point.X <= Target.Max.X && Point.X >= Target.Min.X) &&
                 (Point.Y <= Target.Max.Y && Point.Y >= Target.Min.Y);
    return Result;
}

internal b32
RectsIntersect(rect_f32 A, rect_f32 B)
{
    b32 Result = !((A.Max.X <= B.Min.X) || (A.Min.X >= B.Max.X) ||
                   (A.Max.Y <= B.Min.Y) || (A.Min.Y >= B.Max.Y));
    return Result;
}

internal f32
RoundedRectSDF(rect_sdf_params Params)
{
    // Abuse the symmetry and fold every point into the first quadrant.
    // Offset these points by RectHalfSize, to figure out if they still lay inside the quadrant.
    // If it still does, then we know the point was outside the rect, else it was inside.

    vec2_f32 RadiusVector  = Vec2F32(Params.Radius, Params.Radius);
    vec2_f32 FirstQuadrant = Vec2F32Add(Vec2F32Sub(Vec2F32Abs(Params.PointPosition), Params.HalfSize), RadiusVector);

    // OuterDistance: If any axis is positive, take its length to figure out the closest distance to the boundary.
    // InnerDistance: If any axis is positive, this results in a 0. Else return the "less negative" value (Closest to edge)

    f32 OuterDistance = Vec2F32Length(Vec2F32(Max(FirstQuadrant.X, 0.f), Max(FirstQuadrant.Y, 0.f)));
    f32 InnerDistance = Min(Max(FirstQuadrant.X, FirstQuadrant.Y), 0.f);
    f32 Result        = OuterDistance + InnerDistance - Params.Radius;

    return Result;
}

internal rect_f32
InsetRectF32(rect_f32 Rect, f32 Size)
{
    rect_f32 Result = Rect;
    Result.Min.X = ClampBot(0.f, Rect.Min.X + Size);
    Result.Min.Y = ClampBot(0.f, Rect.Min.Y + Size);
    Result.Max.X = ClampBot(0.f, Rect.Max.X - Size);
    Result.Max.Y = ClampBot(0.f, Rect.Max.Y - Size);

    return Result;
}

internal rect_f32
TranslateRectF32(rect_f32 Rect, vec2_f32 Translation)
{
    rect_f32 Result;
    Result.Min.X = Rect.Min.X + Translation.X;
    Result.Min.Y = Rect.Min.Y + Translation.Y;
    Result.Max.X = Rect.Max.X + Translation.X;
    Result.Max.Y = Rect.Max.Y + Translation.Y;

    return Result;
}

// [Vector OPs]

internal vec2_f32
Vec2F32Add(vec2_f32 Vec1, vec2_f32 Vec2)
{
    vec2_f32 Result = Vec2F32(Vec1.X + Vec2.X, Vec1.Y + Vec2.Y);
    return Result;
}

internal vec2_f32
Vec2F32Sub(vec2_f32 Vec1, vec2_f32 Vec2)
{
    vec2_f32 Result = Vec2F32((Vec1.X - Vec2.X), (Vec1.Y - Vec2.Y));
    return Result;
}

internal vec2_f32
Vec2F32Abs(vec2_f32 Vec)
{
    vec2_f32 Result = Vec2F32(ClampBot(Vec.X, -Vec.X), ClampBot(Vec.Y, -Vec.Y));
    Assert(Result.X >= 0 && Result.Y >= 0);

    return Result;
}

internal f32
Vec2F32Length(vec2_f32 Vec)
{
    f32 Result = sqrtf(Vec.X * Vec.X + Vec.Y + Vec.Y);
    return Result;
}

// [Ranges]

internal b32
IsInRangeF32(f32 Min, f32 Max, f32 Value)
{
    b32 Result = (Value >= Min && Value <= Max);
    return Result;
}

internal b32
Vec2I32IsEqual(vec2_i32 Vec1, vec2_i32 Vec2)
{
	b32 Result = (Vec1.X == Vec2.X) && (Vec1.Y == Vec2.Y);
	return Result;
}

internal b32
Vec2I32IsEmpty(vec2_i32 Vec)
{
	b32 Result = (Vec.X == 0) && (Vec.Y == 0);
	return Result;
}

internal b32
Vec2F32IsEqual(vec2_f32 Vec1, vec2_f32 Vec2)
{
	b32 Result = (Vec1.X == Vec2.X) && (Vec1.Y == Vec2.Y);
	return Result;
}

internal b32
Vec2F32IsEmpty(vec2_f32 Vec)
{
	b32 Result = (Vec.X == 0) && (Vec.Y == 0);
	return Result;
}

internal b32
Mat3x3AreEqual(matrix_3x3 *m1, matrix_3x3 *m2)
{
	b32 Result = MemoryCompare(m1, m2, sizeof(matrix_3x3)) == 0;
	return Result;
}

// [Misc]
