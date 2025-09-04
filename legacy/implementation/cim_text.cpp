// [Internal Types]

typedef struct glyph_entry
{
    glyph_hash HashValue;

    cim_u32 NextWithSameHash;
    cim_u32 NextLRU;
    cim_u32 PrevLRU;

    bool             IsInAtlas;
    ui_texture_coord TexCoord;
    glyph_size       Size;
} glyph_entry;

typedef struct glyph_table
{
    glyph_table_stats Stats;

    cim_u32 HashMask;
    cim_u32 HashCount;
    cim_u32 EntryCount;

    cim_u32     *HashTable;
    glyph_entry *Entries;
} glyph_table;

typedef struct direct_glyph_table
{
    glyph_table_stats Stats;
    glyph_info       *Entries;
    cim_u32           EntryCount;
} direct_glyph_table;

// [Helpers]

static glyph_entry *
GetGlyphEntry(glyph_table *Table, cim_u32 Index)
{
    Cim_Assert(Index < Table->EntryCount);
    glyph_entry *Result = Table->Entries + Index;
    return Result;
}

static cim_u32*
GetSlotPointer(glyph_table *Table, glyph_hash Hash)
{
    cim_u32 HashIndex = Hash.Value;
    cim_u32 HashSlot  = (HashIndex & Table->HashMask);

    Cim_Assert(HashSlot < Table->HashCount);
    cim_u32*Result = &Table->HashTable[HashSlot];

    return Result;
}

static glyph_entry *
GetSentinel(glyph_table *Table)
{
    glyph_entry *Result = Table->Entries;
    return Result;
}

static glyph_table_stats 
GetAndClearStats(glyph_table *Table)
{
    glyph_table_stats Result    = Table->Stats;
    glyph_table_stats ZeroStats = {};

    Table->Stats = ZeroStats;

    return Result;
}

// WARN: This is bad!
static glyph_hash
ComputeGlyphHash(cim_u32 CodePoint)
{
    glyph_hash Result = {};
    Result.Value = CimHash_Block32(&CodePoint, sizeof(CodePoint));

    return Result;
}

// TODO: Update this?
static void
UpdateGlyphCacheEntry(glyph_table *Table, cim_u32 Id, bool NewIsInAtlas, ui_texture_coord TexCoord, glyph_size NewSize)
{
    glyph_entry *Entry = GetGlyphEntry(Table, Id);
    Entry->IsInAtlas = NewIsInAtlas;
    Entry->TexCoord  = TexCoord;
    Entry->Size      = NewSize;
}

#if UIText_ValidateLRU
static void ValidateLRU(glyph_table *Table, int ExpectedCountChange)
{
    cim_u32 EntryCount = 0;

    glyph_entry *Sentinel     = GetSentinel(Table);
    size_t       LastOrdering = Sentinel->Ordering;
    for (cim_u32 EntryIndex = Sentinel->NextLRU; EntryIndex != 0;)
    {
        glyph_entry *Entry = GetGlyphEntry(Table, EntryIndex);
        Cim_Assert(Entry->Ordering < LastOrdering);

        LastOrdering = Entry->Ordering;
        EntryIndex   = Entry->NextLRU;

        ++EntryCount;
    }

    if ((Table->LastLRUCount + ExpectedCountChange) != EntryCount)
    {
        __debugbreak();
    }

     Table->LastLRUCount = EntryCount;
}
#else
#define ValidateLRU(...)
#endif

static void 
RecycleLRU(glyph_table *Table)
{
    glyph_entry *Sentinel = GetSentinel(Table);
    Cim_Assert(Sentinel->PrevLRU);

    cim_u32      EntryIndex = Sentinel->PrevLRU;
    glyph_entry *Entry      = GetGlyphEntry(Table, EntryIndex);
    glyph_entry *Prev       = GetGlyphEntry(Table, Entry->PrevLRU);

    Prev->NextLRU     = 0;
    Sentinel->PrevLRU = Entry->PrevLRU;
    ValidateLRU(Table, -1);

    cim_u32 *NextIndex = GetSlotPointer(Table, Entry->HashValue);
    while (*NextIndex != EntryIndex)
    {
        Cim_Assert(*NextIndex);
        NextIndex = &GetGlyphEntry(Table, *NextIndex)->NextWithSameHash;
    }

    Cim_Assert(*NextIndex == EntryIndex);
    *NextIndex = Entry->NextWithSameHash;

    Entry->NextWithSameHash    = Sentinel->NextWithSameHash;
    Sentinel->NextWithSameHash = EntryIndex;

    UpdateGlyphCacheEntry(Table, EntryIndex, false, {}, {});

    ++Table->Stats.RecycleCount;
}

static cim_u32 
PopFreeEntry(glyph_table *Table)
{
    glyph_entry *Sentinel = GetSentinel(Table);

    if (!Sentinel->NextWithSameHash)
    {
        RecycleLRU(Table);
    }

    cim_u32 Result = Sentinel->NextWithSameHash;
    Cim_Assert(Result);

    glyph_entry *Entry = GetGlyphEntry(Table, Result);
    Sentinel->NextWithSameHash = Entry->NextWithSameHash;
    Entry->NextWithSameHash    = 0;

    Cim_Assert(Entry);
    Cim_Assert(Entry != Sentinel);
    Cim_Assert(Entry->NextWithSameHash == 0);
    Cim_Assert(Entry == GetGlyphEntry(Table, Result));

    return Result;
}

// [API]

static glyph_info 
FindGlyphEntryByHash(glyph_table *Table, glyph_hash Hash)
{
    glyph_entry *Result = NULL;

    cim_u32 *Slot       = GetSlotPointer(Table, Hash);
    cim_u32  EntryIndex = *Slot;
    while (EntryIndex)
    {
        glyph_entry *Entry = GetGlyphEntry(Table, EntryIndex);
        if (Entry->HashValue.Value == Hash.Value)
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHash;
    }

    if (Result)
    {
        Cim_Assert(EntryIndex);

        glyph_entry *Prev = GetGlyphEntry(Table, Result->PrevLRU);
        glyph_entry *Next = GetGlyphEntry(Table, Result->NextLRU);

        Prev->NextLRU = Result->NextLRU;
        Next->PrevLRU = Result->PrevLRU;

        ValidateLRU(Table, -1);

        ++Table->Stats.HitCount;
    }
    else
    {
        EntryIndex = PopFreeEntry(Table);
        Cim_Assert(EntryIndex);

        Result = GetGlyphEntry(Table, EntryIndex);
        Cim_Assert(Result->NextWithSameHash == 0);

        Result->NextWithSameHash = *Slot;
        Result->HashValue        = Hash;

        *Slot = EntryIndex;

        ++Table->Stats.MissCount;
    }

    glyph_entry *Sentinel = GetSentinel(Table);
    Cim_Assert(Result != Sentinel);
    Result->NextLRU = Sentinel->NextLRU;
    Result->PrevLRU = 0;

    glyph_entry *NextLRU = GetGlyphEntry(Table, Sentinel->NextLRU);
    NextLRU->PrevLRU  = EntryIndex;
    Sentinel->NextLRU = EntryIndex;

#if UIText_ValidateLRU
    Result->Ordering = Sentinel->Ordering++;
#endif
    ValidateLRU(Table, 1);

    glyph_info Info;
    Info.MapId     = EntryIndex;
    Info.IsInAtlas = Result->IsInAtlas;
    Info.Size      = Result->Size;
    Info.TexCoord  = Result->TexCoord;

    return Info;
}

static glyph_table *
PlaceGlyphTableInMemory(glyph_table_params Params, void *Memory)
{
    Cim_Assert(Params.HashCount >= 1);
    Cim_Assert(Params.EntryCount >= 2);
    Cim_Assert(Cim_IsPowerOfTwo(Params.HashCount));

    glyph_table *Result = NULL;

    if (Memory)
    {
        glyph_entry *Entries = (glyph_entry *)Memory;
        Result = (glyph_table *)(Entries + Params.EntryCount);
        Result->HashTable = (cim_u32 *)(Result + 1);
        Result->Entries   = Entries;

        Result->HashMask   = Params.HashCount - 1;
        Result->HashCount  = Params.HashCount;
        Result->EntryCount = Params.EntryCount;

        memset(Result->HashTable, 0, Result->HashCount * sizeof(Result->HashTable[0]));

        for (cim_u32 EntryIndex = 0; EntryIndex < Params.EntryCount; ++EntryIndex)
        {
             glyph_entry *Entry = GetGlyphEntry(Result, EntryIndex);
             if ((EntryIndex + 1) < Params.EntryCount)
             {
                 Entry->NextWithSameHash = EntryIndex + 1;
             }
             else
             {
                 Entry->NextWithSameHash = 0;
             }

             Entry->IsInAtlas = false;
             Entry->TexCoord  = {};
             Entry->Size      = {};
        }

        GetAndClearStats(Result);
    }

    return Result;
}

static size_t 
GetGlyphTableFootprint(glyph_table_params Params)
{
    size_t HashSize  = Params.HashCount  * sizeof(cim_u32);
    size_t EntrySize = Params.EntryCount * sizeof(glyph_entry);
    size_t Result    = (sizeof(glyph_table) + HashSize + EntrySize);

    return Result;
}

// [Direct Glyph Tables]

static glyph_atlas
FindGlyphAtlasByDirectMapping(direct_glyph_table *Table, char Character)
{
    Cim_Assert(Table && Table->Entries);

    cim_u32     MapIndex = (cim_u32)Character;
    glyph_info *Result   = Table->Entries + MapIndex;

    glyph_atlas AtlasInfo;
    AtlasInfo.TableId   = MapIndex;
    AtlasInfo.IsInAtlas = Result->IsInAtlas;
    AtlasInfo.TexCoord  = Result->TexCoord;

    return AtlasInfo;
}

static glyph_layout
FindGlyphLayoutByDirectMapping(direct_glyph_table *Table, char Character)
{
    Cim_Assert(Table && Table->Entries);

    cim_u32     MapIndex = (cim_u32)Character;
    glyph_info *Result   = Table->Entries + MapIndex;

    glyph_layout LayoutInfo;
    LayoutInfo.Size     = Result->Size;
    LayoutInfo.Offsets.x  = Result->Offsets.x;
    LayoutInfo.Offsets.y  = Result->Offsets.y;
    LayoutInfo.AdvanceX = Result->AdvanceX;

    return LayoutInfo;
}

static void
UpdateDirectGlyphTable(direct_glyph_table *Table, cim_u32 MapId, bool IsInAtlas,
                       ui_texture_coord Coord, cim_vector2 Offsets, glyph_size Size, cim_f32 AdvanceX)
{
    Cim_Assert(Table);

    if(MapId < Table->EntryCount)
    {
        glyph_info *Entry = Table->Entries + MapId;
        Cim_Assert(Entry->IsInAtlas == false);
        Cim_Assert(Entry->AdvanceX  == 0);

        Entry->IsInAtlas = IsInAtlas;
        Entry->TexCoord  = Coord;
        Entry->Size      = Size;
        Entry->Offsets   = Offsets;
        Entry->AdvanceX  = AdvanceX;
    }
}

static direct_glyph_table *
PlaceDirectGlyphTableInMemory(glyph_table_params Params, void *Memory)
{
    Cim_Assert(Params.EntryCount == UIText_ExtendedASCIITableSize);

    direct_glyph_table *Result = NULL;

    if(Memory)
    {
        glyph_info *Entries = (glyph_info*)Memory;
        Result          = (direct_glyph_table *)(Entries + Params.EntryCount);
        Result->Entries = Entries;

        memset(Result->Entries, 0, Params.EntryCount * sizeof(glyph_info));

        Result->EntryCount = Params.EntryCount;
    }

    return Result;
}

static size_t
GetDirectGlyphTableFootPrint(glyph_table_params Params)
{
    size_t EntrySize = Params.EntryCount * sizeof(glyph_info);
    size_t Result    = sizeof(direct_glyph_table) + EntrySize;

    return Result;
}