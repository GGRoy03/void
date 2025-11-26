#define ui_id(Id)  byte_string_literal(Id)
#define UIBlock(X) DeferLoop(X, UIEnd())

// -----------------------------------------------------------------------------------
// UI Context Private Implementation

static ui_pipeline *
GetCurrentPipeline(void)
{
    ui_pipeline *Pipeline = UIState.CurrentPipeline;
    return Pipeline;
}

static ui_subtree *
GetCurrentSubtree(void)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    return Subtree;
}

static ui_subtree *
FindSubtree(ui_node Node)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_subtree *Result = 0;

    IterateLinkedList((&Pipeline->Subtrees), ui_subtree_node *, SubtreeNode)
    {
        ui_subtree *Subtree = &SubtreeNode->Value;
        if(Subtree->Id == Node.SubtreeId)
        {
            Result = Subtree;
            break;
        }
    }

    return Result;
}

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
        // TODO: Show which ID is faulty.

        ConsoleWriteMessage(warn_message("ID could not be set because it already exists for this pipeline"));
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

static ui_node *
UIFindNodeById(byte_string Id, ui_node_table *Table)
{
    ui_node *Result = 0;

    if(IsValidByteString(Id) && IsValidNodeIdTable(Table))
    {
        ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
        ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

        if(Entry)
        {
            // BUG:
            // Does not set subtree ID. But is it needed really?

            Result = UICreateNode(0, 1);
            Result->CanUse = 1;
            Result->IndexInTree = Entry->NodeIndex;
        }
        else
        {
            // TODO: Show which ID is faulty.
            ConsoleWriteMessage(warn_message("Could not find queried node."));
        }
    }
    else
    {
        ConsoleWriteMessage(warn_message("Invalid Arguments Provided. See ui/layout.h for information."));
    }

    return Result;
}

// ----------------------------------------------------------------------------

static ui_color
UIColor(float R, float G, float B, float A)
{
    ui_color Result = { R, G, B, A };
    return Result;
}

static ui_spacing
UISpacing(float Horizontal, float Vertical)
{
    ui_spacing Result = { Horizontal, Vertical };
    return Result;
}

static ui_padding
UIPadding(float Left, float Top, float Right, float Bot)
{
    ui_padding Result = { Left, Top, Right, Bot };
    return Result;
}

static ui_corner_radius
UICornerRadius(float TopLeft, float TopRight, float BotLeft, float BotRight)
{
    ui_corner_radius Result = { TopLeft, TopRight, BotLeft, BotRight };
    return Result;
}

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
MakeResourceKey(UIResource_Type Type, uint32_t NodeIndex, ui_subtree *Subtree)
{
    uint64_t Low  = (uint64_t)Subtree;
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
QueryNodeResource(uint32_t NodeIndex, ui_subtree *Subtree, UIResource_Type Type, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeResourceKey(Type, NodeIndex, Subtree);
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

    ui_resource_key   Key   = MakeGlobalResourceKey(UIResource_ImageGroup, Name);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    VOID_ASSERT(!State.Resource);

    uint64_t   Size   = GetImageGroupFootprint(Width);
    void *Memory = AllocateUIResource(Size, &UIState.ResourceTable->Allocator);

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

        UpdateResourceTable(State.Id, Key, Group, UIState.ResourceTable);
    }
}

static ui_loaded_image
LoadImageInGroup(byte_string GroupName, byte_string Path)
{
    ui_loaded_image Result = {};

    ui_image_group *Group = (ui_image_group *)QueryGlobalResource(GroupName, UIResource_ImageGroup, UIState.ResourceTable);
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
            ConsoleWriteMessage(error_message("Unable to load image inside group."));
        }
    }
    else
    {
        ConsoleWriteMessage(error_message("Could not find image on disk."));
    }

    return Result;
}

// -------------------------------------------------------------
// UI Node Private Implementation

static ui_subtree *
GetSubtreeForNode(ui_node *Node)
{
    ui_subtree *Result = GetCurrentSubtree();
    if(!Result)
    {
        Result = FindSubtree(*Node);
    }

    VOID_ASSERT(Result);
    return Result;
}

// -------------------------------------------------------------
// UI Node Public API Implementation

#define UI_NODE_SETTERS(X)                       \
    X(Size,      vec2_unit,      UISetSize)      \
    X(Display,   UIDisplay_Type, UISetDisplay)   \
    X(Color,     ui_color,       UISetColor)     \
    X(TextColor, ui_color,       UISetTextColor)

#define DEFINE_UI_NODE_SETTER(Name, Type, SetFunc) \
void                                               \
ui_node::Set##Name(Type Value)                     \
{                                                  \
    VOID_ASSERT(this->CanUse);                          \
    ui_subtree *Subtree = GetSubtreeForNode(this); \
    VOID_ASSERT(Subtree);                               \
    SetFunc(this->IndexInTree, Value, Subtree);    \
}
UI_NODE_SETTERS(DEFINE_UI_NODE_SETTER)
#undef DEFINE_UI_NODE_SETTER

void ui_node::SetStyle(uint32_t StyleIndex)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    // NOTE:
    // Looks weird..

    ui_node_style *Style = GetNodeStyle(this->IndexInTree, Subtree);
    VOID_ASSERT(Style);

    Style->CachedStyleIndex = StyleIndex;
    Style->IsLastVersion    = 0;

    SetNodeStyle(this->IndexInTree, StyleIndex, Subtree);
}

ui_node * ui_node::FindChild(uint32_t Index)
{
    VOID_ASSERT(this->CanUse);

    ui_node    *Result  = 0;
    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    uint32_t NodeIndex = UITreeFindChild(this->IndexInTree, Index, Subtree);
    if(NodeIndex != InvalidLayoutNodeIndex)
    {
        Result = 0; // TODO: Alloc.
    }

    return Result;
}

void ui_node::AppendChild(ui_node *Child)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    UITreeAppendChild(this->IndexInTree, Child->IndexInTree, Subtree);
}

void ui_node::ReserveChildren(uint32_t Amount)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    UITreeReserve(this->IndexInTree, Amount, Subtree);
}

void ui_node::ClearTextInput(void)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    ui_text       *Text      = (ui_text *)      QueryNodeResource(this->IndexInTree, Subtree, UIResource_Text     , UIState.ResourceTable);
    ui_text_input *TextInput = (ui_text_input *)QueryNodeResource(this->IndexInTree, Subtree, UIResource_TextInput, UIState.ResourceTable);

    VOID_ASSERT(Text);
    VOID_ASSERT(TextInput);

    UITextClear_(Text);
    UITextInputClear_(TextInput);
}

void ui_node::SetText(byte_string Text)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    ui_resource_table *Table = UIState.ResourceTable;

    // WARN:
    // This code is still a bit of a mess, especially if we have mutlipl
    // ways to create text. We should unify somehow.

    ui_resource_key   Key   = MakeResourceKey(UIResource_Text, this->IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    if(!State.Resource)
    {
        uint64_t   Size   = GetUITextFootprint(Text.Size);
        void *Memory = AllocateUIResource(Size, &UIState.ResourceTable->Allocator);

        ui_node_style *Style = GetNodeStyle(this->IndexInTree, Subtree);
        ui_font       *Font  = UIGetFont(Style->Properties[StyleState_Default]);

        ui_text *UIText = PlaceUITextInMemory(Text, Text.Size, Font, Memory);
        VOID_ASSERT(UIText);

        UpdateResourceTable(State.Id, Key, UIText, Table);
        SetLayoutNodeFlags(this->IndexInTree, UILayoutNode_HasText, Subtree);
    }
    else
    {
        // NOTE:
        // This is not an error, but let's not implement it for now.

        VOID_ASSERT(!"Not Implemented");
    }
}

void ui_node::SetTextInput(uint8_t *Buffer, uint64_t BufferSize)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    ui_resource_key   Key   = MakeResourceKey(UIResource_TextInput, this->IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    uint64_t   Size   = sizeof(ui_text_input);
    void *Memory = AllocateUIResource(Size, &UIState.ResourceTable->Allocator);

    ui_text_input *TextInput = (ui_text_input *)Memory;
    TextInput->UserBuffer    = ByteString(Buffer, BufferSize);
    TextInput->internalCount = strlen((char *)Buffer);

    UpdateResourceTable(State.Id, Key, TextInput, UIState.ResourceTable);

    // NOTE:
    // I mean... Maybe this is fine. But at least centralize it when we update
    // the resource table perhaps? And then we kinda have the index from the key.
    // Just need to make global keys recognizable.

    SetLayoutNodeFlags(this->IndexInTree, UILayoutNode_HasTextInput, Subtree);

    this->SetText(ByteString(Buffer, BufferSize));
}

void ui_node::SetScroll(float ScrollSpeed, UIAxis_Type Axis)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    ui_resource_key   Key   = MakeResourceKey(UIResource_ScrollRegion, this->IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    uint64_t   Size   = GetScrollRegionFootprint();
    void *Memory = AllocateUIResource(Size, &UIState.ResourceTable->Allocator);

    scroll_region_params Params =
    {
        .PixelPerLine = ScrollSpeed,
        .Axis         = Axis,
    };

    ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory);
    if(ScrollRegion)
    {
        UpdateResourceTable(State.Id, Key, ScrollRegion, UIState.ResourceTable);

        // WARN:
        // Still don't know how to feel about this.
        // It's not great, that's for sure. Again this idea of centralizing
        SetLayoutNodeFlags(this->IndexInTree, UILayoutNode_HasScrollRegion, Subtree);
    }
}

// NOTE:
// Then we at least need to specify that group names must be internal strings.

void ui_node::SetImage(byte_string Path, byte_string Group)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    ui_loaded_image Image = LoadImageInGroup(Group, Path);
    if(Image.IsLoaded)
    {
        uint64_t   Size   = sizeof(ui_image);
        void *Memory = AllocateUIResource(Size, &UIState.ResourceTable->Allocator);

        if(Memory)
        {
            ui_image *ImageResource = (ui_image *)Memory;
            ImageResource->Source    = Image.Source;
            ImageResource->GroupName = Group;

            ui_resource_key   Key   = MakeResourceKey(UIResource_Image, this->IndexInTree, Subtree);
            ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

            UpdateResourceTable(State.Id, Key, ImageResource, UIState.ResourceTable);
            SetLayoutNodeFlags(this->IndexInTree, UILayoutNode_HasImage, Subtree);
        }
        else
        {
            ConsoleWriteMessage(error_message("Failed to allocate memory: Image."));
        }
    }
}

void ui_node::ListenOnKey(ui_text_input_onkey Callback, void *UserData)
{
    VOID_ASSERT(this->CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(this);
    VOID_ASSERT(Subtree);

    // NOTE:
    // For now these callbacks are limited to text inputs. Unsure if I want to expose
    // them more generally. It's also not clear that it's limited to text input.

    ui_text_input *TextInput = (ui_text_input *)QueryNodeResource(this->IndexInTree, Subtree, UIResource_TextInput, UIState.ResourceTable);
    VOID_ASSERT(TextInput);

    TextInput->OnKey         = Callback;
    TextInput->OnKeyUserData = UserData;
}

void ui_node::DebugBox(uint32_t Flag, bool Draw)
{
    VOID_ASSERT(this->CanUse);
    VOID_ASSERT(Flag == UILayoutNode_DebugOuterBox || Flag == UILayoutNode_DebugInnerBox|| Flag == UILayoutNode_DebugContentBox);

    ui_subtree *Subtree = GetCurrentSubtree();
    VOID_ASSERT(Subtree);

    if(Draw)
    {
        SetLayoutNodeFlags(this->IndexInTree, Flag, Subtree);
    }
    else
    {
        ClearLayoutNodeFlags(this->IndexInTree, Flag, Subtree);
    }
}

void ui_node::SetId(byte_string Id, ui_node_table *Table)
{
    VOID_ASSERT(this->CanUse);
    VOID_ASSERT(IsValidByteString(Id));
    VOID_ASSERT(Table);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    SetNodeId(Id, this->IndexInTree, Table);
}

static ui_node *
UICreateNode(uint32_t Flags, bool IsFrameNode)
{
    ui_subtree *Subtree = GetCurrentSubtree();
    VOID_ASSERT(Subtree);

    ui_node *Node = PushStruct(Subtree->Transient, ui_node);

    if(Node && !IsFrameNode)
    {
        Node->IndexInTree = AllocateLayoutNode(Flags, Subtree);
        Node->CanUse      = true;
        Node->SubtreeId   = Subtree->Id;
    }

    return Node;
}

// ----------------------------------------------------------------------------------
// Context internal Implementation

static uint64_t
GetSubtreeinternalFootprint(uint64_t NodeCount)
{
    uint64_t TreeSize  = GetLayoutTreeFootprint(NodeCount);
    uint64_t StyleSize = NodeCount * sizeof(ui_node_style);
    uint64_t Result    = sizeof(ui_subtree) + TreeSize + StyleSize;

    return Result;
}

static bool
IsValidHoveredNode(ui_hovered_node *Node)
{
    VOID_ASSERT(Node);

    bool Result = (Node->Subtree && Node->Index < Node->Subtree->NodeCount);
    return Result;
}

static void
ClearHoveredNode(ui_hovered_node *Node)
{
    VOID_ASSERT(Node);

    Node->Index   = 0;
    Node->Subtree = 0;
}

static bool
IsValidFocusedNode(ui_focused_node *Node)
{
    VOID_ASSERT(Node);

    bool Result = (Node->Subtree && Node->Index < Node->Subtree->NodeCount);
    return Result;
}

static void
ClearFocusedNode(ui_focused_node *Node)
{
    VOID_ASSERT(Node);

    Node->Index       = 0;
    Node->Subtree     = 0;
    Node->IsTextInput = 0;
    Node->Intent      = UIIntent_None;
}

// ----------------------------------------------------------------------------------
// Context Public API Implementation

static void
UIBeginFrame(vec2_int WindowSize)
{
    bool      MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    bool      MouseClicked  = OSIsMouseClicked(OSMouseButton_Left);
    vec2_float MousePosition = OSGetMousePosition();

    ui_focused_node Focused = UIState.Focused;
    if(IsValidFocusedNode(&Focused))
    {
        if(MouseClicked && Focused.IsTextInput && !IsMouseInsideOuterBox(MousePosition, Focused.Index, Focused.Subtree))
        {
            ClearFocusedNode(&UIState.Focused);
        } else
        if(MouseReleased && !Focused.IsTextInput)
        {
            ClearFocusedNode(&UIState.Focused);
        }
    }

    UIState.WindowSize = WindowSize;
}

static void
UIEndFrame(void)
{
    ClearHoveredNode(&UIState.Hovered);
}

static void
UISetNodeHover(uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(!IsValidHoveredNode(&UIState.Hovered));

    UIState.Hovered.Index   = NodeIndex;
    UIState.Hovered.Subtree = Subtree;
}

static bool
UIHasNodeHover(void)
{
    bool Result = IsValidHoveredNode(&UIState.Hovered);
    return Result;
}

static ui_hovered_node
UIGetNodeHover(void)
{
    VOID_ASSERT(IsValidHoveredNode(&UIState.Hovered));

    ui_hovered_node Result = UIState.Hovered;
    return Result;
}

static void
UISetNodeFocus(uint32_t NodeIndex, ui_subtree *Subtree, bool IsTextInput, UIIntent_Type Intent)
{
    VOID_ASSERT(!IsValidFocusedNode(&UIState.Focused));

    UIState.Focused.Index       = NodeIndex;
    UIState.Focused.Subtree     = Subtree;
    UIState.Focused.IsTextInput = IsTextInput;
    UIState.Focused.Intent      = Intent;
}

static bool
UIHasNodeFocus(void)
{
    bool Result = IsValidFocusedNode(&UIState.Focused);
    return Result;
}

static ui_focused_node
UIGetNodeFocus(void)
{
    VOID_ASSERT(IsValidFocusedNode(&UIState.Focused));

    ui_focused_node Result = UIState.Focused;
    return Result;
}

static void
UIBeginSubtree(ui_subtree_params Params)
{
    if(Params.CreateNew)
    {
        VOID_ASSERT(Params.NodeCount && "Cannot create a subtree with 0 nodes.");

        memory_arena *Persistent = 0;
        {
            uint64_t Footprint = GetSubtreeinternalFootprint(Params.NodeCount);

            memory_arena_params Params = {};
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;
            Params.ReserveSize       = Footprint;
            Params.CommitSize        = Footprint;

            Persistent = AllocateArena(Params);
        }

        memory_arena *Transient = 0;
        {
            memory_arena_params Params = {};
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;
            Params.ReserveSize       = VOID_KILOBYTE(32);
            Params.CommitSize        = VOID_KILOBYTE(16);

            Transient = AllocateArena(Params);
        }

        if(Persistent && Transient)
        {
            ui_subtree_node *SubtreeNode = PushStruct(Persistent, ui_subtree_node);
            ui_subtree      *Subtree     = &SubtreeNode->Value;

            Subtree->Persistent = Persistent;
            Subtree->Transient  = Transient;
            Subtree->NodeCount  = Params.NodeCount;

            Subtree->ComputedStyles = PushArray(Subtree->Persistent, ui_node_style, Params.NodeCount);

            ui_layout_tree *Tree = 0;
            {
                uint64_t   Footprint = GetLayoutTreeFootprint(Params.NodeCount);
                void *Memory    = PushArray(Subtree->Persistent, uint8_t, Footprint);

                Tree = PlaceLayoutTreeInMemory(Params.NodeCount, Memory);
            }
            Subtree->LayoutTree = Tree;

            // NOTE:
            // What the fuck is this? Not great.

            ui_pipeline *Pipeline = GetCurrentPipeline();
            VOID_ASSERT(Pipeline);
            ui_subtree_list *SubtreeList = &Pipeline->Subtrees;
            AppendToLinkedList(SubtreeList, SubtreeNode, SubtreeList->Count);
            Pipeline->CurrentSubtree = Subtree;
        }
    }
    else
    {
        // NOTE: Not sure..
    }
}

static void
UIEndSubtree(ui_subtree_params Params)
{
    VOID_UNUSED(Params);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);
}

static ui_pipeline *
GetFreeUIPipeline(void)
{
    ui_pipeline_buffer *Buffer = &UIState.Pipelines;
    VOID_ASSERT(Buffer);
    VOID_ASSERT(Buffer->Count < Buffer->Size); // NOTE: Wrong ui_config.h!

    ui_pipeline *Result = Buffer->Values + Buffer->Count++;

    return Result;
}

static ui_pipeline *
UICreatePipeline(ui_pipeline_params Params)
{
    ui_state    *State  = &UIState;
    ui_pipeline *Result = GetFreeUIPipeline();
    VOID_ASSERT(Result);

    // internal Data
    {
        memory_arena_params ArenaParams = { 0 };
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = VOID_MEGABYTE(1);
        ArenaParams.CommitSize        = VOID_KILOBYTE(1);

        Result->internalArena = AllocateArena(ArenaParams);
    }

    // Registry - Maybe just return a buffer?
    {
        Result->Registry = CreateStyleRegistry(Params.ThemeFile, Result->internalArena);
    }

    State->CurrentPipeline = Result;

    return Result;
}

// NOTE:
// Perhaps we do not need this?

static void
UIBeginAllSubtrees(ui_pipeline *Pipeline)
{
    VOID_ASSERT(Pipeline);

    ui_subtree_list *List  = &Pipeline->Subtrees;
    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree *Subtree = &Node->Value;

        ClearArena(Subtree->Transient);

        UpdateSubtreeState(Subtree);
    }

    UIState.CurrentPipeline = Pipeline;
}

static void
UIExecuteAllSubtrees(ui_pipeline *Pipeline)
{
    VOID_ASSERT(Pipeline);

    ui_subtree_list *List = &Pipeline->Subtrees;
    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree *Subtree = &Node->Value;

        ComputeSubtreeLayout(Subtree);
        PaintSubtree(Subtree);
    }
}
