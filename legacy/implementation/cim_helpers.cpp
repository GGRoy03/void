// [Geometry]

static bool
IsInsideRect(cim_rect Rect)
{
    cim_u32 MousePosX = (cim_u32)UI_INPUT.MouseX;
    cim_u32 MousePosY = (cim_u32)UI_INPUT.MouseY;

    bool MouseIsInside = (MousePosX > Rect.MinX) && (MousePosX < Rect.MaxX) &&
                         (MousePosY > Rect.MinY) && (MousePosY < Rect.MaxY);

    return MouseIsInside;
}

static bool
RectAreEqual(cim_rect A, cim_rect B)
{
    bool Result = (A.MinX == B.MinX) && (A.MinY == B.MinY) &&
                  (A.MaxX == B.MaxX) && (A.MaxY == B.MaxY);
    return Result;
}

// [Arenas]

static void
WriteToArena(void *Data, size_t Size, cim_arena *Arena)
{
    if (Arena->At + Size > Arena->Capacity)
    {
        Arena->Capacity = Arena->Capacity ? Arena->Capacity * 2 : 1024;

        while (Arena->Capacity < Arena->At + Size)
        {
            Arena->Capacity += (Arena->Capacity >> 1);
        }

        void *New = malloc(Arena->Capacity);
        if (!New)
        {
            Cim_Assert(!"Malloc failure: OOM?");
            return;
        }
        else
        {
            memset((char *)New + Arena->At, 0, Arena->Capacity - Arena->At);
        }

        if (Arena->Memory)
        {
            memcpy(New, Arena->Memory, Arena->At);
            free(Arena->Memory);
        }

        Arena->Memory = New;
    }

    memcpy((cim_u8 *)Arena->Memory + Arena->At, Data, Size);
    Arena->At += Size;
}

void *
CimArena_Push(size_t Size, cim_arena *Arena)
{
    if (Arena->At + Size > Arena->Capacity)
    {
        Arena->Capacity = Arena->Capacity ? Arena->Capacity * 2 : 1024;

        while (Arena->Capacity < Arena->At + Size)
        {
            Arena->Capacity += (Arena->Capacity >> 1);
        }

        void *New = malloc(Arena->Capacity);
        if (!New)
        {
            Cim_Assert(!"Malloc failure: OOM?");
            return NULL;
        }
        else
        {
            memset((char *)New + Arena->At, 0, Arena->Capacity - Arena->At);
        }

        if (Arena->Memory)
        {
            memcpy(New, Arena->Memory, Arena->At);
            free(Arena->Memory);
        }

        Arena->Memory = New;
    }

    void *Ptr = (char *)Arena->Memory + Arena->At;
    Arena->At += Size;

    return Ptr;
}

void *
CimArena_GetLast(size_t TypeSize, cim_arena *Arena)
{
    Cim_Assert(Arena->Memory);
    return (char *)Arena->Memory + (Arena->At - TypeSize);
}

cim_u32
CimArena_GetCount(size_t TypeSize, cim_arena *Arena)
{
    return (cim_u32)(Arena->At / TypeSize);
}

void
CimArena_Reset(cim_arena *Arena)
{
    Arena->At = 0;
}

void
CimArena_End(cim_arena *Arena)
{
    if (Arena->Memory)
    {
        free(Arena->Memory);

        Arena->Memory = NULL;
        Arena->At = 0;
        Arena->Capacity = 0;
    }
}

// [Hashing]

#define FNV32Prime 0x01000193
#define FNV32Basis 0x811C9DC5
#define CimEmptyBucketTag  0x80
#define CimBucketGroupSize 16

cim_u32
CimHash_FindFirstBit32(cim_u32 Mask)
{
    return (cim_u32)__builtin_ctz(Mask);
}

cim_u32
CimHash_String32(const char *String)
{
    if (!String)
    {
        Cim_Assert(!"Cannot hash empty string.");
        return 0;
    }

    cim_u32 Result = FNV32Basis;
    cim_u8  Character;

    while ((Character = *String++))
    {
        Result = FNV32Prime * (Result ^ Character);
    }

    return Result;
}

cim_u32
CimHash_Block32(void *ToHash, cim_u32 ToHashLength)
{
    cim_u8 *BytePointer = (cim_u8 *)ToHash;
    cim_u32 Result = FNV32Basis;

    if (!ToHash)
    {
        CimLog_Error("Called Hash32Bits with NULL memory adress as value.");
        return 0;
    }

    if (ToHashLength < 1)
    {
        CimLog_Error("Called Hash32Bits with length inferior to 1.");
        return 0;
    }

    while (ToHashLength--)
    {
        Result = FNV32Prime * (Result ^ *BytePointer++);
    }

    return Result;
}

// [Buffers]

static bool
IsValidBuffer(buffer *Buffer)
{
    bool Result = Buffer->At < Buffer->Size;
    return Result;
}

static buffer
AllocateBuffer(size_t Size)
{
    buffer Result;
    Result.Data = (cim_u8*)malloc(Size);
    Result.Size = Size;
    Result.At   = 0;

    if (Result.Data)
    {
        memset(Result.Data, 0, Result.Size);
    }

    return Result;
}

static void
FreeBuffer(buffer *Buffer)
{
    if (Buffer->Data)
    {
        free(Buffer->Data);

        Buffer->At   = 0;
        Buffer->Size = 0;
        Buffer->Data = NULL;
    }
    else
    {
        CimLog_Warn("Could not free the buffer. Potential memory leak.");
    }
}