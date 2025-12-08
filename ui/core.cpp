// ------------------------------------------------------------------------------
// Node Id Map Implementation

#define NodeIdTable_EmptyMask  1 << 0       // 1st bit
#define NodeIdTable_DeadMask   1 << 1       // 2nd bit
#define NodeIdTable_TagMask    0xFF & ~0x03 // High 6 bits

typedef struct ui_node_id_hash
{
    uint64_t Value;
} ui_node_id_hash;

typedef struct ui_node_id_entry
{
    ui_node_id_hash Hash;
    uint32_t             NodeIndex;
} ui_node_id_entry;

typedef struct ui_node_table
{
    uint8_t               *MetaData;
    ui_node_id_entry *Buckets;

    uint64_t GroupSize;
    uint64_t GroupCount;

    uint64_t HashMask;
} ui_node_table;

static bool
IsValidNodeIdTable(ui_node_table *Table)
{
    bool Result = (Table && Table->MetaData && Table->Buckets && Table->GroupCount);
    return Result;
}

static uint32_t
GetNodeIdGroupIndexFromHash(ui_node_id_hash Hash, ui_node_table *Table)
{
    uint32_t Result = Hash.Value & Table->HashMask;
    return Result;
}

static uint8_t
GetNodeIdTagFromHash(ui_node_id_hash Hash)
{
    uint8_t Result = Hash.Value & NodeIdTable_TagMask;
    return Result;
}

static ui_node_id_hash
ComputeNodeIdHash(byte_string Id)
{
    // WARN: Again, unsure if this hash is strong enough since hash-collisions
    // are fatal. Fatal in the sense that they will return invalid data.

    ui_node_id_hash Result = {HashByteString(Id)};
    return Result;
}

static ui_node_id_entry *
FindNodeIdEntry(ui_node_id_hash Hash, ui_node_table *Table)
{
    uint32_t ProbeCount = 0;
    uint32_t GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        uint8_t *Meta = Table->MetaData + (GroupIndex * Table->GroupSize);
        uint8_t  Tag  = GetNodeIdTagFromHash(Hash);

        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);
        __m128i TagVector  = _mm_set1_epi8(Tag);

        // Uses a 6 bit tags to search a matching tag thorugh the meta-data
        // using vectors of bytes instead of comparing full hashes directly.

        int TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, TagVector));
        while(TagMask)
        {
            uint32_t Lane  = FindFirstBit(TagMask);
            uint32_t Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            if(Hash.Value == Entry->Hash.Value)
            {
                return Entry;
            }

            TagMask &= TagMask - 1;
        }

        // Check for slots that have never been used. If we find any it means that
        // our value has never been inserted in the map, since, otherwise, it would
        // have been inserted in that free slot. Keep proving if no never-used slot
        // have been found.

        __m128i EmptyVector = _mm_set1_epi8(NodeIdTable_EmptyMask);
        int     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

        if(!EmptyMask)
        {
            ProbeCount++;
            GroupIndex = (GroupIndex + (ProbeCount * ProbeCount)) & (Table->GroupCount - 1);
        }
        else
        {
            break;
        }
    }

    return 0;
}

static void
InsertNodeId(ui_node_id_hash Hash, uint32_t NodeIndex, ui_node_table *Table)
{
    uint32_t ProbeCount = 0;
    uint32_t GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        uint8_t     *Meta       = Table->MetaData + (GroupIndex * Table->GroupSize);
        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);

        // First check for empty entries (never-used), if one is found insert it and
        // set the meta-data to the tag (which will clear all state bits)

        __m128i EmptyVector = _mm_set1_epi8(NodeIdTable_EmptyMask);
        int     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

        if(EmptyMask)
        {
            uint32_t Lane  = FindFirstBit(EmptyMask);
            uint32_t Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash      = Hash;
            Entry->NodeIndex = NodeIndex;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        // Then check for dead entries in the meta-vector, if one is found insert it and
        // set the meta-data to the tag which will clear all state bits

        __m128i DeadVector = _mm_set1_epi8(NodeIdTable_DeadMask);
        int     DeadMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, DeadVector));

        if(DeadMask)
        {
            uint32_t Lane  = FindFirstBit(DeadMask);
            uint32_t Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash      = Hash;
            Entry->NodeIndex = NodeIndex;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        ProbeCount++;
        GroupIndex = (GroupIndex + (ProbeCount * ProbeCount)) & (Table->GroupCount - 1);
    }
}

static void
SetNodeId(byte_string Id, uint32_t NodeIndex, ui_node_table *Table)
{
    if(!IsValidNodeIdTable(Table))
    {
        return;
    }

    ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
    ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

    if(!Entry)
    {
        InsertNodeId(Hash, NodeIndex, Table);
    }
    else
    {
    }
}

static uint64_t
GetNodeIdTableFootprint(ui_node_table_params Params)
{
    uint64_t GroupTotal = Params.GroupSize * Params.GroupCount;

    uint64_t MetaDataSize = GroupTotal * sizeof(uint8_t);
    uint64_t BucketsSize  = GroupTotal * sizeof(ui_node_id_entry);
    uint64_t Result       = sizeof(ui_node_table) + MetaDataSize + BucketsSize;

    return Result;
}

static ui_node_table *
PlaceNodeIdTableInMemory(ui_node_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.GroupSize == 16);
    VOID_ASSERT(Params.GroupCount > 0 && VOID_ISPOWEROFTWO(Params.GroupCount));

    ui_node_table *Result = 0;

    if(Memory)
    {
        uint64_t GroupTotal = Params.GroupSize * Params.GroupCount;

        uint8_t *              MetaData = (uint8_t *)Memory;
        ui_node_id_entry *Entries  = (ui_node_id_entry *)(MetaData + GroupTotal);

        Result = (ui_node_table *)(Entries + GroupTotal);
        Result->MetaData   = MetaData;
        Result->Buckets    = Entries;
        Result->GroupSize  = Params.GroupSize;
        Result->GroupCount = Params.GroupCount;
        Result->HashMask   = Params.GroupCount - 1;

        for(uint32_t Idx = 0; Idx < GroupTotal; ++Idx)
        {
            Result->MetaData[Idx] = NodeIdTable_EmptyMask;
        }
    }

    return Result;
}

static ui_node
UIFindNodeById(byte_string Id, ui_node_table *Table)
{
    ui_node Result = {InvalidLayoutNodeIndex};

    if(IsValidByteString(Id) && IsValidNodeIdTable(Table))
    {
        ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
        ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

        if(Entry)
        {
            Result = {Entry->NodeIndex};
        }
        else
        {
        }
    }
    else
    {
    }

    return Result;
}

// ----------------------------------------------------------------------------

static vec2_unit
Vec2Unit(ui_unit U0, ui_unit U1)
{
    vec2_unit Result = { .X = U0, .Y = U1 };
    return Result;
}

static bool
IsNormalizedColor(ui_color Color)
{
    bool Result = (Color.R >= 0.f && Color.R <= 1.f) &&
                 (Color.G >= 0.f && Color.G <= 1.f) &&
                 (Color.B >= 0.f && Color.B <= 1.f) &&
                 (Color.A >= 0.f && Color.A <= 1.f);

    return Result;
}

// ----------------------------------------------------------------------------------
// UI Resource Cache Private Implementation

typedef struct ui_resource_allocator
{
    uint64_t AllocatedCount;
    uint64_t AllocatedBytes;
} ui_resource_allocator;

typedef struct ui_resource_entry
{
    ui_resource_key Key;

    uint32_t NextWithSameHashSlot;
    uint32_t NextLRU;
    uint32_t PrevLRU;

    UIResource_Type ResourceType; // NOTE: VOID_UNUSED?
    void           *Memory;
} ui_resource_entry;

typedef struct ui_resource_table
{
    ui_resource_stats     Stats;
    ui_resource_allocator Allocator;

    uint32_t HashMask;
    uint32_t HashSlotCount;
    uint32_t EntryCount;

    uint32_t               *HashTable;
    ui_resource_entry *Entries;
} ui_resource_table;

static ui_resource_entry *
GetResourceSentinel(ui_resource_table *Table)
{
    ui_resource_entry *Result = Table->Entries;
    return Result;
}

static uint32_t *
GetResourceSlotPointer(ui_resource_key Key, ui_resource_table *Table)
{
    uint32_t HashIndex = _mm_cvtsi128_si32(Key.Value);
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    VOID_ASSERT(HashSlot < Table->HashSlotCount);

    uint32_t *Result = &Table->HashTable[HashSlot];
    return Result;
}

static ui_resource_entry *
GetResourceEntry(uint32_t Index, ui_resource_table *Table)
{
    VOID_ASSERT(Index < Table->EntryCount);

    ui_resource_entry *Result = Table->Entries + Index;
    return Result;
}

static bool
ResourceKeyAreEqual(ui_resource_key A, ui_resource_key B)
{
    __m128i Compare = _mm_cmpeq_epi32(A.Value, B.Value);
    bool     Result  = (_mm_movemask_epi8(Compare) == 0xffff);

    return Result;
}

static uint32_t
PopFreeResourceEntry(ui_resource_table *Table)
{
    ui_resource_entry *Sentinel = GetResourceSentinel(Table);

    // At initialization we populate sentinel's the hash chain such that:
    // (Sentinel) -> (Slot) -> (Slot) -> (Slot)
    // If (Sentinel) -> (Nothing), then we have no more slots available.

    if(!Sentinel->NextWithSameHashSlot)
    {
        VOID_ASSERT(!"Not Implemented");
    }

    uint32_t Result = Sentinel->NextWithSameHashSlot;

    ui_resource_entry *Entry = GetResourceEntry(Result, Table);
    Sentinel->NextWithSameHashSlot = Entry->NextWithSameHashSlot;
    Entry->NextWithSameHashSlot    = 0;

    return Result;
}

static UIResource_Type
GetResourceTypeFromKey(ui_resource_key Key)
{
    uint64_t             High = _mm_extract_epi64(Key.Value, 1);
    UIResource_Type Type = (UIResource_Type)(High >> 32);
    return Type;
}

static void *
AllocateUIResource(uint64_t Size, ui_resource_allocator *Allocator)
{
    void *Result = OSReserveMemory(Size);

    if(Result)
    {
        bool Committed = OSCommitMemory(Result, Size);
        VOID_ASSERT(Committed);

        Allocator->AllocatedCount += 1;
        Allocator->AllocatedBytes += Size;
    }

    return Result;
}

// ----------------------------------------------------------------------------------
// UI Resource Cache Public API

static uint64_t
GetResourceTableFootprint(ui_resource_table_params Params)
{
    uint64_t HashTableSize  = Params.HashSlotCount  * sizeof(uint32_t);
    uint64_t EntryArraySize = Params.EntryCount * sizeof(ui_resource_entry);
    uint64_t Result         = sizeof(ui_resource_table) + HashTableSize + EntryArraySize;

    return Result;
}

static ui_resource_table *
PlaceResourceTableInMemory(ui_resource_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.EntryCount);
    VOID_ASSERT(Params.HashSlotCount);
    VOID_ASSERT(VOID_ISPOWEROFTWO(Params.HashSlotCount));

    ui_resource_table *Result = 0;

    if(Memory)
    {
        uint32_t               *HashTable = (uint32_t *)Memory;
        ui_resource_entry *Entries   = (ui_resource_entry *)(HashTable + Params.HashSlotCount);

        Result = (ui_resource_table *)(Entries + Params.EntryCount);
        Result->HashTable     = HashTable;
        Result->Entries       = Entries;
        Result->EntryCount    = Params.EntryCount;
        Result->HashSlotCount = Params.HashSlotCount;
        Result->HashMask      = Params.HashSlotCount - 1;

        for(uint32_t Idx = 0; Idx < Params.EntryCount; ++Idx)
        {
            ui_resource_entry *Entry = GetResourceEntry(Idx, Result);
            if((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHashSlot = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHashSlot = 0;
            }

            Entry->ResourceType = UIResource_None;
            Entry->Memory       = 0;
        }
    }

    return Result;
}

static ui_resource_key
MakeNodeResourceKey(UIResource_Type Type, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    uint64_t Low  = (uint64_t)Tree;
    uint64_t High = ((uint64_t)Type << 32) | NodeIndex;

    ui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

static ui_resource_key
MakeGlobalResourceKey(UIResource_Type Type, byte_string Name)
{
    uint64_t Low  = HashByteString(Name);
    uint64_t High = ((uint64_t)Type << 32);

    ui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

static bool
IsValidResourceKey(ui_resource_key Key)
{
    return true;
}

static ui_resource_state
FindResourceByKey(ui_resource_key Key, ui_resource_table *Table)
{
    ui_resource_entry *FoundEntry = 0;

    uint32_t *Slot       = GetResourceSlotPointer(Key, Table);
    uint32_t  EntryIndex = Slot[0];

    while(EntryIndex)
    {
        ui_resource_entry *Entry = GetResourceEntry(EntryIndex, Table);
        if(ResourceKeyAreEqual(Entry->Key, Key))
        {
            FoundEntry = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHashSlot;
    }

    if(FoundEntry)
    {
        // If we hit an already existing entry we must pop it off the LRU chain.
        // (Prev) -> (Entry) -> (Next)
        // (Prev) -> (Next)

        ui_resource_entry *Prev = GetResourceEntry(FoundEntry->PrevLRU, Table);
        ui_resource_entry *Next = GetResourceEntry(FoundEntry->NextLRU, Table);

        Prev->NextLRU = FoundEntry->NextLRU;
        Next->PrevLRU = FoundEntry->PrevLRU;

        ++Table->Stats.CacheHitCount;
    }
    else
    {
        // If we miss an entry we have to first allocate a new one.
        // If we have some hash slot at X: Hash[X]
        // Hash[X] -> (Index) -> (Index)
        // (Entry) -> Hash[X] -> (Index) -> (Index)
        // Hash[X] -> (Index) -> (Index) -> (Index)

        EntryIndex = PopFreeResourceEntry(Table);
        VOID_ASSERT(EntryIndex);

        FoundEntry = GetResourceEntry(EntryIndex, Table);
        FoundEntry->NextWithSameHashSlot = Slot[0];
        FoundEntry->Key                  = Key;

        Slot[0] = EntryIndex;

        ++Table->Stats.CacheMissCount;
    }

    // Every time we access an entry we must assure that this new entry is now
    // the most recent one in the LRU chain.
    // What we have: (Sentinel) -> (Entry)    -> (Entry) -> (Entry)
    // What we want: (Sentinel) -> (NewEntry) -> (Entry) -> (Entry) -> (Entry)

    ui_resource_entry *Sentinel = GetResourceSentinel(Table);
    FoundEntry->NextLRU = Sentinel->NextLRU;
    FoundEntry->PrevLRU = 0;

    ui_resource_entry *NextLRU = GetResourceEntry(Sentinel->NextLRU, Table);
    NextLRU->PrevLRU  = EntryIndex;
    Sentinel->NextLRU = EntryIndex;

    ui_resource_state Result = {};
    Result.Id           = EntryIndex;
    Result.ResourceType = FoundEntry->ResourceType;
    Result.Resource     = FoundEntry->Memory;

    return Result;
}

// NOTE:
// The type can always be inferred from the key.

static void
UpdateResourceTable(uint32_t Id, ui_resource_key Key, void *Memory, ui_resource_table *Table)
{
    ui_resource_entry *Entry = GetResourceEntry(Id, Table);
    VOID_ASSERT(Entry);

    if(Entry->Memory && Entry->Memory != Memory)
    {
        OSRelease(Entry->Memory);
    }

    Entry->Key          = Key;
    Entry->Memory       = Memory;
    Entry->ResourceType = GetResourceTypeFromKey(Key);
}

static void *
QueryNodeResource(uint32_t NodeIndex, ui_layout_tree *Tree, UIResource_Type Type, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeNodeResourceKey(Type, NodeIndex, Tree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    VOID_ASSERT(State.Resource && State.ResourceType == Type);

    void *Result = State.Resource;
    return Result;
}

static void *
QueryGlobalResource(byte_string Name, UIResource_Type Type, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeGlobalResourceKey(Type, Name);
    ui_resource_state State = FindResourceByKey(Key, Table);

    VOID_ASSERT(State.Resource && State.ResourceType == Type);

    void *Result = State.Resource;
    return Result;
}

// ------------------------------------------------------------
// UI Image Processing internal Implementation

typedef struct ui_image
{
    rect_float    Source;
    byte_string GroupName;
} ui_image;

typedef struct ui_loaded_image
{
    rect_float Source;
    bool      IsLoaded;
} ui_loaded_image;

typedef struct ui_image_group
{
    vec2_float       Size;
    render_texture RenderTexture;
    stbrp_context  Allocator;
    stbrp_node    *AllocatorNodes;
} ui_image_group;

static uint64_t
GetImageGroupFootprint(float Width)
{
    uint64_t NodeSize = Width * sizeof(stbrp_node);
    uint64_t Result   = NodeSize + sizeof(ui_image_group);
    return Result;
}

// ------------------------------------------------------------
// UI Image Processing Public Implementation

static void
UICreateImageGroup(byte_string Name, int Width, int Height)
{
    VOID_ASSERT(Width  > 0);
    VOID_ASSERT(Height > 0);

    void_context &Context = GetVoidContext();

    ui_resource_key   Key   = MakeGlobalResourceKey(UIResource_ImageGroup, Name);
    ui_resource_state State = FindResourceByKey(Key, Context.ResourceTable);

    VOID_ASSERT(!State.Resource);

    uint64_t   Size   = GetImageGroupFootprint(Width);
    void *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

    if(Memory)
    {
        render_texture_params Texture =
        {
            .Type   = RenderTexture_RGBA,
            .Width  = Width,
            .Height = Height,
        };

        stbrp_node *AllocatorNodes = (stbrp_node *)Memory;

        ui_image_group *Group = (ui_image_group *)(AllocatorNodes + Width);
        Group->Size           = vec2_float(Width, Height);
        Group->RenderTexture  = CreateRenderTexture(Texture);
        Group->AllocatorNodes = AllocatorNodes;

        stbrp_init_target(&Group->Allocator, Width, Height, Group->AllocatorNodes, Width);

        UpdateResourceTable(State.Id, Key, Group, Context.ResourceTable);
    }
}

static ui_loaded_image
LoadImageInGroup(byte_string GroupName, byte_string Path)
{
    void_context   &Context = GetVoidContext();
    ui_loaded_image Result  = {};

    ui_image_group *Group = (ui_image_group *)QueryGlobalResource(GroupName, UIResource_ImageGroup, Context.ResourceTable);
    VOID_ASSERT(Group);

    int Width, Height, Channels;
    uint8_t *Pixels = stbi_load((char *)Path.String, &Width, &Height, &Channels, 4);

    if(Pixels)
    {
        stbrp_rect Rect = {};
        Rect.w = (uint16_t)(Width);
        Rect.h = (uint16_t)(Height);

        VOID_ASSERT(Rect.w == Width);
        VOID_ASSERT(Rect.h == Height);
        stbrp_pack_rects(&Group->Allocator, &Rect, 1);

        if(Rect.was_packed)
        {
            Result.Source   = rect_float::FromXYWH(Rect.x, Rect.y, Rect.w, Rect.h);
            Result.IsLoaded = 1;

            CopyIntoRenderTexture(Group->RenderTexture, Result.Source, Pixels, Width * Channels);

            stbi_image_free(Pixels);
        }
        else
        {
        }
    }
    else
    {
    }

    return Result;
}

// -------------------------------------------------------------
// @Public: Frame Node API

void ui_node::SetStyle(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    if(StyleIndex >= Pipeline.StyleIndexMin && StyleIndex <= Pipeline.StyleIndexMax)
    {
        SetNodeProperties(Index, StyleIndex, Pipeline.StyleArray[StyleIndex], Pipeline.Tree);
    }
}

ui_node ui_node::Find(uint32_t FindIndex, ui_pipeline &Pipeline)
{
    ui_node Result = { UITreeFindChild(Index, FindIndex, Pipeline.Tree) };
    return Result;
}

void ui_node::Append(ui_node Child, ui_pipeline &Pipeline)
{
    UITreeAppendChild(Index, Child.Index, Pipeline.Tree);
}

void ui_node::Reserve(uint32_t Amount, ui_pipeline &Pipeline)
{
    UITreeReserve(Index, Amount, Pipeline.Tree);
}

void ui_node::SetText(byte_string Text, ui_pipeline &Pipeline)
{
    void_context &Context  = GetVoidContext();

    ui_resource_table *Table = Context.ResourceTable;
    ui_resource_key    Key   = MakeNodeResourceKey(UIResource_Text, Index, Pipeline.Tree);
    ui_resource_state  State = FindResourceByKey(Key, Table);

    if(!State.Resource)
    {
        uint64_t  Size   = GetUITextFootprint(Text.Size);
        void     *Memory = AllocateUIResource(Size, &Table->Allocator);

        // TODO: RE-IMPLEMENT
    }
    else
    {
        VOID_ASSERT(!"Not Implemented");
    }
}

void ui_node::SetTextInput(uint8_t *Buffer, uint64_t BufferSize, ui_pipeline &Pipeline)
{
    void_context &Context  = GetVoidContext();

    ui_resource_key   Key   = MakeNodeResourceKey(UIResource_TextInput, Index, Pipeline.Tree);
    ui_resource_state State = FindResourceByKey(Key, Context.ResourceTable);

    uint64_t Size    = sizeof(ui_text_input);
    void     *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

    ui_text_input *TextInput = (ui_text_input *)Memory;
    TextInput->UserBuffer    = ByteString(Buffer, BufferSize);
    TextInput->internalCount = strlen((char *)Buffer);

    UpdateResourceTable(State.Id, Key, TextInput, Context.ResourceTable);

    // NOTE:
    // I mean... Maybe this is fine. But at least centralize it when we update
    // the resource table perhaps? And then we kinda have the index from the key.
    // Just need to make global keys recognizable.

    SetLayoutNodeFlags(Index, UILayoutNode_HasTextInput, Pipeline.Tree);

    // pass Pipeline through to SetText
    SetText(ByteString(Buffer, BufferSize), Pipeline);
}

void ui_node::SetScroll(float ScrollSpeed, UIAxis_Type Axis, ui_pipeline &Pipeline)
{
    void_context &Context  = GetVoidContext();

    ui_resource_key   Key   = MakeNodeResourceKey(UIResource_ScrollRegion, Index, Pipeline.Tree);
    ui_resource_state State = FindResourceByKey(Key, Context.ResourceTable);

    uint64_t  Size   = GetScrollRegionFootprint();
    void     *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

    scroll_region_params Params =
    {
        .PixelPerLine = ScrollSpeed,
        .Axis         = Axis,
    };

    ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory);
    if(ScrollRegion)
    {
        UpdateResourceTable(State.Id, Key, ScrollRegion, Context.ResourceTable);

        // WARN:
        // Still don't know how to feel about this.
        // It's not great, that's for sure. Again this idea of centralizing
        SetLayoutNodeFlags(Index, UILayoutNode_HasScrollRegion, Pipeline.Tree);
    }
}

void ui_node::SetImage(byte_string Path, byte_string Group, ui_pipeline &Pipeline)
{
    void_context &Context = GetVoidContext();

    ui_loaded_image Image = LoadImageInGroup(Group, Path);
    if(Image.IsLoaded)
    {
        uint64_t  Size   = sizeof(ui_image);
        void     *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

        if(Memory)
        {
            ui_image *ImageResource = (ui_image *)Memory;
            ImageResource->Source    = Image.Source;
            ImageResource->GroupName = Group;

            ui_resource_key   Key   = MakeNodeResourceKey(UIResource_Image, Index, Pipeline.Tree);
            ui_resource_state State = FindResourceByKey(Key, Context.ResourceTable);

            UpdateResourceTable(State.Id, Key, ImageResource, Context.ResourceTable);
            SetLayoutNodeFlags(Index, UILayoutNode_HasImage, Pipeline.Tree);
        }
        else
        {
        }
    }
}

void ui_node::DebugBox(uint32_t Flag, bool Draw, ui_pipeline &Pipeline)
{
    VOID_ASSERT(Flag == UILayoutNode_DebugOuterBox || Flag == UILayoutNode_DebugInnerBox || Flag == UILayoutNode_DebugContentBox);

    if(Draw)
    {
        SetLayoutNodeFlags(Index, Flag, Pipeline.Tree);
    }
    else
    {
        ClearLayoutNodeFlags(Index, Flag, Pipeline.Tree);
    }
}

void ui_node::SetId(byte_string Id, ui_pipeline &Pipeline)
{
    VOID_ASSERT(IsValidByteString(Id)); // Makes no sense.

    SetNodeId(Id, Index, Pipeline.NodeTable);
}

// ----------------------------------------------------------------------------------
// Context Public API Implementation

// NOTE:
// Do we expose these types? I don't know.

enum class PointerEvent
{
    None    = 0,
    Move    = 1,
    Click   = 2,
    Release = 3,
    Hover   = 4,
};

struct pointer_event
{
    PointerEvent Type;
    uint32_t     PointerId;
    vec2_float   Position;
    uint32_t     ButtonMask;
};

struct pointer_event_node
{
    pointer_event_node *Next;
    pointer_event       Value;
};

struct pointer_event_list
{
    pointer_event_node *First;
    pointer_event_node *Last;
    uint32_t            Count;
};

static void
EnqueuePointerEvent(PointerEvent Type, uint32_t PointerId, vec2_float Position, uint32_t ButtonMask, pointer_event_list &List)
{
    VOID_ASSERT(Type != PointerEvent::None);  // Internal Corruption

    pointer_event_node *Node = static_cast<pointer_event_node *>(malloc(sizeof(pointer_event_node))); // TODO: Arena.
    if(Node)
    {
        Node->Next             = 0;
        Node->Value.Type       = Type;
        Node->Value.PointerId  = PointerId;
        Node->Value.Position   = Position;
        Node->Value.ButtonMask = ButtonMask;

        AppendToLinkedList((&List), Node, List.Count);
    }
}

static void
UIBeginFrame(vec2_int WindowSize)
{
    void_context &Context = GetVoidContext();
    os_inputs    *Inputs  = OSGetInputs();

    pointer_event_list EventList = {};

    for(uint32_t Idx = 0; Idx < 1; ++Idx)
    {
        input_pointer &Pointer = Inputs->Pointers[Idx];

        uint32_t PressedButtonMask = Pointer.ButtonMask & ~Pointer.LastButtonMask;
        if(PressedButtonMask)
        {
            EnqueuePointerEvent(PointerEvent::Click, Pointer.Id, Pointer.Position, PressedButtonMask, EventList);
        }

        uint32_t ReleasedButtonMask = Pointer.LastButtonMask & ~Pointer.ButtonMask;
        if(ReleasedButtonMask)
        {
            EnqueuePointerEvent(PointerEvent::Release, Pointer.Id, Pointer.Position, ReleasedButtonMask, EventList);
        }

        if(Pointer.ButtonMask == BUTTON_NONE)
        {
            EnqueuePointerEvent(PointerEvent::Hover, Pointer.Id, Pointer.Position, ReleasedButtonMask, EventList);
        }

        // NOTE: Do we really just do this here?
        Pointer.LastButtonMask = Pointer.ButtonMask;
    }

    for(uint32_t Idx = 0; Idx < Context.PipelineCount; ++Idx)
    {
        ui_pipeline &Pipeline = Context.PipelineArray[Idx];

        ConsumePointerEvents(0, Pipeline.Tree, &EventList);
    }

    Context.WindowSize = WindowSize;
}

static void
UIEndFrame(void)
{
    // TODO: Something.
}

// ==================================================================================
// @Public : Context

static void_context &
GetVoidContext(void)
{
    return GlobalVoidContext;
}

// NOTE:
// Context parameters? Unsure.

static void
CreateVoidContext(void)
{
    void_context &Context = GetVoidContext();

    // Memory
    {
        Context.StateArena = AllocateArena({});

        VOID_ASSERT(Context.StateArena);
    }

    // State
    {
        ui_resource_table_params TableParams =
        {
            .HashSlotCount = 512,
            .EntryCount    = 2048,
        };

        uint64_t TableFootprint = GetResourceTableFootprint(TableParams);
        void    *TableMemory    = PushArena(Context.StateArena, TableFootprint, 0);

        Context.ResourceTable =  PlaceResourceTableInMemory(TableParams, TableMemory);

        VOID_ASSERT(Context.ResourceTable);
    }
}

static uint64_t
GetPipelineStateFootprint(const ui_pipeline_params &Params)
{
    uint64_t TreeSize = GetLayoutTreeFootprint(Params.NodeCount);
    uint64_t Result   = TreeSize;

    return Result;
}

// ==================================================================================
// @Public : Pipeline API

static void
UICreatePipeline(const ui_pipeline_params &Params)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(Params.Pipeline)];

    // Memory
    {
        Pipeline.StateArena = AllocateArena({.ReserveSize = GetPipelineStateFootprint(Params)});
        Pipeline.FrameArena = AllocateArena({.ReserveSize = Params.FrameBudget});

        VOID_ASSERT(Pipeline.StateArena && Pipeline.FrameArena);
    }

    // UI State
    {
        uint64_t  TreeFootprint = GetLayoutTreeFootprint(Params.NodeCount);
        void     *TreeMemory    = PushArena(Pipeline.StateArena, TreeFootprint, GetLayoutTreeAlignment());

        Pipeline.Tree = PlaceLayoutTreeInMemory(Params.NodeCount, TreeMemory);

        VOID_ASSERT(Pipeline.Tree);
    }

    // User State
    {
        Pipeline.StyleArray    = Params.StyleArray;
        Pipeline.StyleIndexMin = Params.StyleIndexMin;
        Pipeline.StyleIndexMax = Params.StyleIndexMax;

        VOID_ASSERT(Pipeline.StyleArray && Pipeline.StyleIndexMin <= Pipeline.StyleIndexMax);
    }

    // Render State
    {
        // NOTE:
        // Unsure how/if I want this yet, but don't need it right now.
    }

    ++Context.PipelineCount;
}

static ui_pipeline &
UIBindPipeline(UIPipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(!Pipeline.Bound)
    {
        PopArenaTo(Pipeline.FrameArena, 0);

        Pipeline.Bound = true;
    }

    return Pipeline;
}

static void
UIUnbindPipeline(UIPipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(Pipeline.Bound)
    {
        PreOrderMeasureTree   (Pipeline.Tree, Pipeline.FrameArena);
        PostOrderMeasureTree  (0            , Pipeline.Tree);          // WARN: Passing 0 is not always correct.
        PlaceLayoutTree       (Pipeline.Tree, Pipeline.FrameArena);

        // NOTE: Not a fan of this flow. But it does seem to be better than what we had.

        ui_paint_buffer Buffer = GeneratePaintBuffer(Pipeline.Tree, Pipeline.StyleArray, Pipeline.FrameArena);
        if(Buffer.Commands && Buffer.Size)
        {
            ExecutePaintCommands(Buffer, Pipeline.FrameArena);
        }

        Pipeline.Bound = false;
    }
}

static ui_pipeline_params
UIGetDefaultPipelineParams(void)
{
    ui_pipeline_params Params =
    {
        .VtxShaderByteCode = GetDefaultVtxShader(),
        .PxlShaderByteCode = GetDefaultPxlShader(),
    };

    return Params;
}
