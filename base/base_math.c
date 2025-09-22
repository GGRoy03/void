// [API IMPLEMENTATION]

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
	matrix_3x3 Result = {0};
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

internal rect_f32
RectF32(f32 MinX, f32 MinY, f32 MaxX, f32 MaxY)
{
	rect_f32 Result;
	Result.Min.X = MinX;
	Result.Min.Y = MinY;
	Result.Max.X = MaxX;
	Result.Max.Y = MaxY;

	return Result;
}

internal rect_f32
RectF32Zero(void)
{
	rect_f32 Result = {0};
	return Result;
}

// [Rect]

internal rect_f32
IntersectRectF32(rect_f32 R1, rect_f32 R2)
{
	rect_f32 Result;

	Result.Min.X = max(R1.Min.X, R2.Min.X);
	Result.Min.Y = max(R1.Min.Y, R2.Min.Y);
	Result.Max.X = min(R1.Max.X, R2.Max.X);
	Result.Max.Y = min(R1.Max.Y, R2.Max.Y);

	if (Result.Max.X <= Result.Min.X || Result.Max.Y <= Result.Min.Y)
	{
		Result.Min.X = 0;
		Result.Min.Y = 0;
		Result.Max.X = 0;
		Result.Max.Y = 0;
	}
	
	return Result;
}

// [Helpers]

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

internal vec4_f32 
NormalizedColor(vec4_f32 Vector)
{
	f32      Inverse = 1.f / 255;
	vec4_f32 Result  = Vec4F32(Vector.X * Inverse, Vector.Y * Inverse, Vector.Z * Inverse, Vector.W * Inverse);
	return Result;
}