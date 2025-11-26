#pragma once

// [Core Types]

typedef struct vec4_float
{
    union
    {
        struct { float X; float Y; float Z; float W; };
        float Values[4];
    };
} vec4_float;

typedef struct matrix_3x3
{
    float c0r0, c0r1, c0r2;
    float c1r0, c1r1, c1r2;
    float c2r0, c2r1, c2r2;
} matrix_3x3;

typedef struct matrix_4x4
{
    float c0r0, c0r1, c0r2, c0r3;
    float c1r0, c1r1, c1r2, c1r3;
    float c2r0, c2r1, c2r2, c2r3;
    float c3r0, c3r1, c3r2, c3r3;
} matrix_4x4;

// rect_float:
//   Simple structure representing a rectangle in 2D space using two vectors.
//
// Rectfloat:
// RectfloatZero:
// TranslatedRectfloat:
//   Helper Constructors. Parameters are self-explanatory
//
// IntersectRectfloat:
//   Returns the geometric intersection of A and B.
//   If B is inside A, B is returned.
//   If A is inside B, A is returned.
//   If they partially overlap, the geometric overlap is returned.
//   If they do not overlap, a 0 rect is returned.
//
// IsPointInRect:
//   returns 1 if a given point is inside the rectangle, else 0.
//
// RoundedRectSDF:
//   Produces a signed distance value representing how far we are from the closest edge of the rectangle.
//   LocalPosition: Must be relative to the rectangle's origin, typically do: (Point-RectOrigin) to get the local point.
//   RectHalfSize:  must be half the rect's size on both axis. If this value is not exact, values may be wrong.
//   Radius:        Give a radius if you care about rounded edges, passing 0 is valid.
//   returns   0 IF The given point is directly on one of the rectangle's edge.
//   returns < 0 IF The given point is inside the rect.
//   returns > 0 IF The given point is outside the rect.
//
// RectsIntersect:
//   returns 1 if the rects instersect in any way, else return 0.
//
// InsetRectfloat:
//   Simply insets a rect by some value. Clamped to 0 if rect ends up negative.
//   Meant to be used by rects that represent a positive 2D rectangle.

static vec4_float   Vec4float            (float X, float Y, float Z, float W);
static matrix_3x3 Mat3x3Zero         (void);
static matrix_3x3 Mat3x3Identity     (void);

// [Ranges]

static bool IsInRangefloat(float Min, float Max, float Value);

// [Helpers]

#define IterateVec4() for(uint32_t Index = 0; Index < 4; Index++)

static bool Mat3x3AreEqual    (matrix_3x3 *m1, matrix_3x3 *m2);

namespace void_math
{

template <typename T>
struct vector2_generic
{
    T X, Y;

    constexpr vector2_generic() noexcept
        : X{T(0)}, Y{T(0)} {}

    constexpr vector2_generic(T X_, T Y_) noexcept
        : X(X_), Y(Y_) {}

    constexpr vector2_generic Absolute() const noexcept
    {
        return {ClampBot(this->X, -this->X), ClampBot(this->Y, -this->Y)};
    }

    constexpr float Length() const noexcept
    {
        return sqrtf(this->X * this->X + this->Y * this->Y);
    }

    constexpr bool IsEmpty(void) const noexcept
    {
        return this->X == 0 && this->Y == 0;
    }

    constexpr vector2_generic operator+(vector2_generic Vec) const noexcept(noexcept(T{} + T{}))
    {
        return {this->X + Vec.X, this->Y + Vec.Y};
    }

    constexpr vector2_generic operator-(vector2_generic Vec) const noexcept(noexcept(T{} - T{}))
    {
        return {this->X - Vec.X, this->Y - Vec.Y};
    }

    constexpr vector2_generic operator*(T Scalar) const noexcept(noexcept(T{} * T{}))
    {
        return {this->X * Scalar, this->Y * Scalar};
    }

    constexpr vector2_generic& operator+=(vector2_generic Vec) noexcept
    {
        this->X += Vec.X;
        this->Y += Vec.Y;

        return *this;
    }

    constexpr vector2_generic& operator-=(vector2_generic Vec) noexcept
    {
        this->X -= Vec.X;
        this->Y -= Vec.Y;

        return *this;
    }

    constexpr vector2_generic& operator*=(T Scalar) noexcept
    {
        this->X *= Scalar;
        this->Y *= Scalar;

        return *this;
    }

    constexpr bool operator==(vector2_generic Vec) const noexcept
    {
        return this->X == Vec.X && this->Y == Vec.Y;
    }
};

using vec2_float = vector2_generic<float>;
using vec2_int = vector2_generic<int>;

template <typename T>
struct rectangle_generic
{
    T Left;
    T Top;
    T Right;
    T Bottom;

    constexpr rectangle_generic() noexcept
        : Left(0), Top(0), Right(0), Bottom(0) {}

    constexpr rectangle_generic(T Left_, T Top_, T Right_, T Bottom_) noexcept
        : Left(Left_), Top(Top_), Right(Right_), Bottom(Bottom_) {}

    static constexpr rectangle_generic FromXYWH(T X, T Y, T Width, T Height) noexcept
    {
        return {X, Y, X + Width, Y + Height};
    }

    static constexpr rectangle_generic FromMinMax(const vector2_generic<T>& Min, const vector2_generic<T>& Max) noexcept
    {
        return {Min.X, Min.Y, Max.X, Max.Y};
    }

    constexpr rectangle_generic Translate(vector2_generic<T> Vec) const noexcept
    {
        return {
            this->Left   + Vec.X,
            this->Top    + Vec.Y,
            this->Right  + Vec.X,
            this->Bottom + Vec.Y
        };
    }

    constexpr rectangle_generic Inset(T Scalar) const noexcept
    {
        return {
            ClampBot(0, this->Left   + Scalar),
            ClampBot(0, this->Top    + Scalar),
            ClampBot(0, this->Right  - Scalar),
            ClampBot(0, this->Bottom - Scalar)
        };
    }

    constexpr rectangle_generic Intersect(rectangle_generic<T> Rect) const noexcept
    {
        vector2_generic Min = {Max(this->Left , Rect.Left ), Max(this->Top   , Rect.Top   )};
        vector2_generic Max = {Min(this->Right, Rect.Right), Min(this->Bottom, Rect.Bottom)};

        if(Max.X <= Min.X || Max.Y <= Min.Y)
        {
            return {};
        }
        else
        {
            return {Min.X, Min.Y, Max.X, Max.Y};
        }
    }

    constexpr bool IsPointInside(vector2_generic<T> Point) const noexcept
    {
        return (Point.X <= this->Right && Point.X >= this->Left && Point.Y <= this->Bottom && Point.Y >= this->Top);
    }

    constexpr bool IsIntersecting(rectangle_generic Rect) const noexcept
    {
        return (this->Right > Rect.Left && this->Left < Rect.Right && this->Bottom > Rect.Top && this->Top < Rect.Bottom);
    }
};

using rect_float = rectangle_generic<float>;
using rect_int   = rectangle_generic<int>;

typedef struct rect_sdf_params
{
    float      Radius;
    vec2_float HalfSize;
    vec2_float PointPosition;
} rect_sdf_params;

}
