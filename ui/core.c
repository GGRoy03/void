#define ui_id(Id)  byte_string_literal(Id)
#define UIBlock(X) DeferLoop(X, UIEnd())

// -----------------------------------------------------------------------------------
// UI Context Private Implementation

internal ui_pipeline *
GetCurrentPipeline(void)
{
    ui_pipeline *Pipeline = UIState.CurrentPipeline;
    return Pipeline;
}

internal ui_subtree *
GetCurrentSubtree(void)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    return Subtree;
}

internal ui_subtree *
FindSubtree(ui_node Node)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

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

// -----------------------------------------------------------------------------------
// UI Scrolling Implementation

typedef struct ui_scroll_region
{
    vec2_f32    ContentSize;
    f32         ScrollOffset;
    f32         PixelPerLine;
    UIAxis_Type Axis;
} ui_scroll_region;

// ------------------------------------------------------------------------------
// Node Id Map Implementation

#define NodeIdTable_EmptyMask  1 << 0       // 1st bit
#define NodeIdTable_DeadMask   1 << 1       // 2nd bit
#define NodeIdTable_TagMask    0xFF & ~0x03 // High 6 bits

typedef struct ui_node_id_hash
{
    u64 Value;
} ui_node_id_hash;

typedef struct ui_node_id_entry
{
    ui_node_id_hash Hash;
    ui_node         Node;
} ui_node_id_entry;

typedef struct ui_node_id_table
{
    u8               *MetaData;
    ui_node_id_entry *Buckets;

    u64 GroupSize;
    u64 GroupCount;

    u64 HashMask;
} ui_node_id_table;

internal b32
IsValidNodeIdTable(ui_node_id_table *Table)
{
    b32 Result = (Table && Table->MetaData && Table->Buckets && Table->GroupCount);
    return Result;
}

internal u32
GetNodeIdGroupIndexFromHash(ui_node_id_hash Hash, ui_node_id_table *Table)
{
    u32 Result = Hash.Value & Table->HashMask;
    return Result;
}

internal u8
GetNodeIdTagFromHash(ui_node_id_hash Hash)
{
    u8 Result = Hash.Value & NodeIdTable_TagMask;
    return Result;
}

internal ui_node_id_hash
ComputeNodeIdHash(byte_string Id)
{
    // WARN: Again, unsure if this hash is strong enough since hash-collisions
    // are fatal. Fatal in the sense that they will return invalid data.

    ui_node_id_hash Result = {HashByteString(Id)};
    return Result;
}

internal ui_node_id_entry *
FindNodeIdEntry(ui_node_id_hash Hash, ui_node_id_table *Table)
{
    u32 ProbeCount = 0;
    u32 GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        u8 *Meta = Table->MetaData + (GroupIndex * Table->GroupSize);
        u8  Tag  = GetNodeIdTagFromHash(Hash);

        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);
        __m128i TagVector  = _mm_set1_epi8(Tag);

        // Uses a 6 bit tags to search a matching tag thorugh the meta-data
        // using vectors of bytes instead of comparing full hashes directly.

        i32 TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, TagVector));
        while(TagMask)
        {
            u32 Lane  = FindFirstBit(TagMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

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
        i32     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

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

internal void
InsertNodeId(ui_node_id_hash Hash, ui_node Node, ui_node_id_table *Table)
{
    u32 ProbeCount = 0;
    u32 GroupIndex = GetNodeIdGroupIndexFromHash(Hash, Table);

    while(1)
    {
        u8     *Meta       = Table->MetaData + (GroupIndex * Table->GroupSize);
        __m128i MetaVector = _mm_loadu_si128((__m128i *)Meta);

        // First check for empty entries (never-used), if one is found insert it and
        // set the meta-data to the tag (which will clear all state bits)

        __m128i EmptyVector = _mm_set1_epi8(NodeIdTable_EmptyMask);
        i32     EmptyMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, EmptyVector));

        if(EmptyMask)
        {
            u32 Lane  = FindFirstBit(EmptyMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash = Hash;
            Entry->Node = Node;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        // Then check for dead entries in the meta-vector, if one is found insert it and
        // set the meta-data to the tag which will clear all state bits

        __m128i DeadVector = _mm_set1_epi8(NodeIdTable_DeadMask);
        i32     DeadMask   = _mm_movemask_epi8(_mm_cmpeq_epi8(MetaVector, DeadVector));

        if(DeadMask)
        {
            u32 Lane  = FindFirstBit(DeadMask);
            u32 Index = Lane + (GroupIndex * Table->GroupSize);

            ui_node_id_entry *Entry = Table->Buckets + Index;
            Entry->Hash = Hash;
            Entry->Node = Node;

            Meta[Lane] = GetNodeIdTagFromHash(Hash);

            break;
        }

        ProbeCount++;
        GroupIndex = (GroupIndex + (ProbeCount * ProbeCount)) & (Table->GroupCount - 1);
    }
}

internal u64
GetNodeIdTableFootprint(ui_node_id_table_params Params)
{
    u64 GroupTotal = Params.GroupSize * Params.GroupCount;

    u64 MetaDataSize = GroupTotal * sizeof(u8);
    u64 BucketsSize  = GroupTotal * sizeof(ui_node_id_entry);
    u64 Result       = sizeof(ui_node_id_table) + MetaDataSize + BucketsSize;

    return Result;
}

internal ui_node_id_table *
PlaceNodeIdTableInMemory(ui_node_id_table_params Params, void *Memory)
{
    Assert(Params.GroupSize == 16);
    Assert(Params.GroupCount > 0 && IsPowerOfTwo(Params.GroupCount));

    ui_node_id_table *Result = 0;

    if(Memory)
    {
        u64 GroupTotal = Params.GroupSize * Params.GroupCount;

        u8 *              MetaData = (u8 *)Memory;
        ui_node_id_entry *Entries  = (ui_node_id_entry *)(MetaData + GroupTotal);

        Result = (ui_node_id_table *)(Entries + GroupTotal);
        Result->MetaData   = MetaData;
        Result->Buckets    = Entries;
        Result->GroupSize  = Params.GroupSize;
        Result->GroupCount = Params.GroupCount;
        Result->HashMask   = Params.GroupCount - 1;

        for(u32 Idx = 0; Idx < GroupTotal; ++Idx)
        {
            Result->MetaData[Idx] = NodeIdTable_EmptyMask;
        }
    }

    return Result;
}

internal void
SetNodeId(byte_string Id, ui_node Node, ui_node_id_table *Table)
{
    if(!IsValidNodeIdTable(Table))
    {
        return;
    }

    ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
    ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

    if(!Entry)
    {
        InsertNodeId(Hash, Node, Table);
    }
    else
    {
        // TODO: Show which ID is faulty.

        ConsoleWriteMessage(warn_message("ID could not be set because it already exists for this pipeline"));
    }
}

internal ui_node
FindNodeById(byte_string Id, ui_node_id_table *Table)
{
    ui_node Result = {0};

    if(IsValidByteString(Id) && IsValidNodeIdTable(Table))
    {
        ui_node_id_hash   Hash  = ComputeNodeIdHash(Id);
        ui_node_id_entry *Entry = FindNodeIdEntry(Hash, Table);

        if(Entry)
        {
            Result = Entry->Node;
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

// [Helpers]

internal ui_color

UIColor(f32 R, f32 G, f32 B, f32 A)
{
    ui_color Result = { R, G, B, A };
    return Result;
}

internal ui_spacing
UISpacing(f32 Horizontal, f32 Vertical)
{
    ui_spacing Result = { Horizontal, Vertical };
    return Result;
}

internal ui_padding
UIPadding(f32 Left, f32 Top, f32 Right, f32 Bot)
{
    ui_padding Result = { Left, Top, Right, Bot };
    return Result;
}

internal ui_corner_radius
UICornerRadius(f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight)
{
    ui_corner_radius Result = { TopLeft, TopRight, BotLeft, BotRight };
    return Result;
}

internal vec2_unit
Vec2Unit(ui_unit U0, ui_unit U1)
{
    vec2_unit Result = { .X = U0, .Y = U1 };
    return Result;
}

internal b32
IsNormalizedColor(ui_color Color)
{
    b32 Result = (Color.R >= 0.f && Color.R <= 1.f) &&
                 (Color.G >= 0.f && Color.G <= 1.f) &&
                 (Color.B >= 0.f && Color.B <= 1.f) &&
                 (Color.A >= 0.f && Color.A <= 1.f);

    return Result;
}

// -------------------------------------------------------------
// UI Node Private Implementation

internal ui_subtree *
GetSubtreeForNode(ui_node Node)
{
    ui_subtree *Result = GetCurrentSubtree();
    if(!Result)
    {
        Result = FindSubtree(Node);
    }

    Assert(Result);
    return Result;
}

// -------------------------------------------------------------
// UI Node Public API Implementation

internal void
UINodeSetDisplay(ui_node Node, UIDisplay_Type Type)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    UISetDisplay(Node.IndexInTree, Type, Subtree);
}

internal void
UINodeSetTextColor(ui_node Node, ui_color Color)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    UISetTextColor(Node.IndexInTree, Color, Subtree);
}

internal void
UINodeSetStyle(ui_node Node, u32 StyleIndex)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(Node.IndexInTree, Subtree);
    Assert(Style);

    Style->CachedStyleIndex = StyleIndex;
    Style->IsLastVersion    = 0;

    UpdateNodeIfNeeded(Node.IndexInTree, Subtree);
}

internal ui_node
UINodeFindChild(ui_node Node, u32 Index)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_node Result = FindLayoutChild(Node.IndexInTree, Index, Subtree);
    return Result;
}

internal void
UINodeReserveChildren(ui_node Node, u32 Amount)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ReserveLayoutChildren(Node, Amount, Subtree); // TODO: Pass index.
}

internal void
UINodeClearText(ui_node Node)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_text *Text = QueryTextResource(Node.IndexInTree, Subtree, UIState.ResourceTable);
    Assert(Text);
    UITextClear_(Text);
}

internal void
UINodeSetText(ui_node Node, byte_string Text)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_resource_table *Table = UIState.ResourceTable;

    // BUG:
    // One problem is that this key is not strong enough and might collide
    // with other text keys. What even happens if two keys collide?

    // WARN: 
    // This code is still a bit of a mess, especially if we have mutliple
    // ways to create text. We should unify somehow. There's no way I am doing
    // manual allocations. Really bad!

    ui_resource_key   Key   = MakeResourceKey(UIResource_Text, Node.IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    if(!State.Resource)
    {
        u64   Size     = GetUITextFootprint(Text.Size);
        void *Memory   = OSReserveMemory(Size);
        b32   Commited = OSCommitMemory(Memory, Size);
        Assert(Memory && Commited);

        ui_node_style *Style = GetNodeStyle(Node.IndexInTree, Subtree);
        ui_font       *Font  = UIGetFont(Style->Properties[StyleState_Basic]);

        ui_text *UIText = PlaceUITextInMemory(Text, Text.Size, Font, Memory);
        Assert(UIText);

        UpdateResourceTable(State.Id, Key, UIText, UIResource_Text, Table);
        SetLayoutNodeFlags(Node.IndexInTree, UILayoutNode_HasText, Subtree);
    }
    else
    {
        // NOTE:
        // This is not an error, but let's not implement it for now.

        Assert(!"Not Implemented");
    }

    UpdateNodeIfNeeded(Node.IndexInTree, Subtree);
}

internal void
UINodeSetTextInput(ui_node Node, u8 *Buffer, u64 BufferSize)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_resource_key   Key   = MakeResourceKey(UIResource_TextInput, Node.IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    // NOTE:
    // Do we even care if the resource is set or not? Kind of confused.
    // I mean, we could reuse the same memory, but we can't just stomp it
    // and the resource cache will handle deallocation.

    // WARN:
    // Still don't know about these allocations. Haven't found a better way yet.

    u64   Size     = sizeof(ui_text_input);
    void *Memory   = OSReserveMemory(Size);
    b32   Commited = OSCommitMemory(Memory, Size);
    Assert(Memory && Commited);

    ui_text_input *TextInput = (ui_text_input *)Memory;
    TextInput->UserData = ByteString(Buffer, strlen((char *)Buffer));
    TextInput->Size     = BufferSize;

    UpdateResourceTable(State.Id, Key, TextInput, UIResource_TextInput, UIState.ResourceTable);

    // WARN:
    // Still don't know how to feel about this.
    // It's not great, that's for sure.
    SetLayoutNodeFlags(Node.IndexInTree, UILayoutNode_HasTextInput, Subtree);
}

internal void
UINodeSetScroll(ui_node Node, UIAxis_Type Axis)
{
    Assert(Node.CanUse);

    ui_subtree *Subtree = GetSubtreeForNode(Node);
    Assert(Subtree);

    ui_resource_key   Key   = MakeResourceKey(UIResource_ScrollRegion, Node.IndexInTree, Subtree);
    ui_resource_state State = FindResourceByKey(Key, UIState.ResourceTable);

    // WARN:
    // Same annoying allocation!

    u64   Size     = sizeof(ui_scroll_region);
    void *Memory   = OSReserveMemory(Size);
    b32   Commited = OSCommitMemory(Memory, Size);
    Assert(Memory && Commited);

    ui_scroll_region *ScrollRegion = (ui_scroll_region *)Memory;
    ScrollRegion->PixelPerLine = 8.f;
    ScrollRegion->Axis         = Axis;

    UpdateResourceTable(State.Id, Key, ScrollRegion, UIResource_ScrollRegion, UIState.ResourceTable);

    // WARN:
    // Still don't know how to feel about this.
    // It's not great, that's for sure.
    SetLayoutNodeFlags(Node.IndexInTree, UILayoutNode_HasScrollRegion, Subtree);
}

internal void
UINodeSetId(ui_node Node, byte_string Id)
{
    Assert(Node.CanUse);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    SetNodeId(Id, Node, Pipeline->IdTable);
}

internal ui_node
UINode(bit_field Flags)
{
    ui_subtree *Subtree = GetCurrentSubtree();
    Assert(Subtree);

    ui_node Node = AllocateUINode(Flags, Subtree);
    Assert(Node.CanUse);

    return Node;
}

// -------------------------------------------------------------
// UI Node Public API Implementation

// ----------------------------------------------------------------------------------
// UI Resource Cache Private Implementation

typedef struct ui_resource_entry
{
    ui_resource_key Key;

    u32 NextWithSameHashSlot;
    u32 NextLRU;
    u32 PrevLRU;

    UIResource_Type ResourceType;
    void           *Memory;
} ui_resource_entry;

typedef struct ui_resource_table
{
    ui_resource_stats Stats;

    u32 HashMask;
    u32 HashSlotCount;
    u32 EntryCount;

    u32               *HashTable;
    ui_resource_entry *Entries;
} ui_resource_table;

internal ui_resource_entry *
GetResourceSentinel(ui_resource_table *Table)
{
    ui_resource_entry *Result = Table->Entries;
    return Result;
}

internal u32 *
GetResourceSlotPointer(ui_resource_key Key, ui_resource_table *Table)
{
    u32 HashIndex = _mm_cvtsi128_si32(Key.Value);
    u32 HashSlot  = (HashIndex & Table->HashMask);

    Assert(HashSlot < Table->HashSlotCount);

    u32 *Result = &Table->HashTable[HashSlot];
    return Result;
}

internal ui_resource_entry *
GetResourceEntry(u32 Index, ui_resource_table *Table)
{
    Assert(Index < Table->EntryCount);

    ui_resource_entry *Result = Table->Entries + Index;
    return Result;
}

internal b32
ResourceKeyAreEqual(ui_resource_key A, ui_resource_key B)
{
    __m128i Compare = _mm_cmpeq_epi32(A.Value, B.Value);
    b32     Result  = (_mm_movemask_epi8(Compare) == 0xffff);

    return Result;
}

internal u32
PopFreeResourceEntry(ui_resource_table *Table)
{
    ui_resource_entry *Sentinel = GetResourceSentinel(Table);

    // At initialization we populate sentinel's the hash chain such that:
    // (Sentinel) -> (Slot) -> (Slot) -> (Slot)
    // If (Sentinel) -> (Nothing), then we have no more slots available.

    if(!Sentinel->NextWithSameHashSlot)
    {
        Assert(!"Not Implemented");
    }

    u32 Result = Sentinel->NextWithSameHashSlot;

    ui_resource_entry *Entry = GetResourceEntry(Result, Table);
    Sentinel->NextWithSameHashSlot = Entry->NextWithSameHashSlot;
    Entry->NextWithSameHashSlot    = 0;

    return Result;
}

// ----------------------------------------------------------------------------------
// UI Resource Cache Public API

internal u64
GetResourceTableFootprint(ui_resource_table_params Params)
{
    u64 HashTableSize  = Params.HashSlotCount  * sizeof(u32);
    u64 EntryArraySize = Params.EntryCount * sizeof(ui_resource_entry);
    u64 Result         = sizeof(ui_resource_table) + HashTableSize + EntryArraySize;

    return Result;
}

internal ui_resource_table *
PlaceResourceTableInMemory(ui_resource_table_params Params, void *Memory)
{
    Assert(Params.EntryCount);
    Assert(Params.HashSlotCount);
    Assert(IsPowerOfTwo(Params.HashSlotCount));

    ui_resource_table *Result = 0;

    if(Memory)
    {
        u32               *HashTable = (u32 *)Memory;
        ui_resource_entry *Entries   = (ui_resource_entry *)(HashTable + Params.HashSlotCount);

        Result = (ui_resource_table *)(Entries + Params.EntryCount);
        Result->HashTable     = HashTable;
        Result->Entries       = Entries;
        Result->EntryCount    = Params.EntryCount;
        Result->HashSlotCount = Params.HashSlotCount;
        Result->HashMask      = Params.HashSlotCount - 1;

        for(u32 Idx = 0; Idx < Params.EntryCount; ++Idx)
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

internal ui_resource_key
MakeResourceKey(UIResource_Type Type, u32 NodeIndex, ui_subtree *Subtree)
{
    u64 Low  = (u64)Subtree;
    u64 High = ((u64)Type << 32) | NodeIndex;

    ui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

internal ui_resource_state
FindResourceByKey(ui_resource_key Key, ui_resource_table *Table)
{
    ui_resource_entry *FoundEntry = 0;

    u32 *Slot       = GetResourceSlotPointer(Key, Table);
    u32  EntryIndex = Slot[0];

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
        Assert(EntryIndex);

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

    ui_resource_state Result = {0};
    Result.Id           = EntryIndex;
    Result.ResourceType = FoundEntry->ResourceType;
    Result.Resource     = FoundEntry->Memory;

    return Result;
}

internal void
UpdateResourceTable(u32 Id, ui_resource_key Key, void *Memory, UIResource_Type Type, ui_resource_table *Table)
{
    ui_resource_entry *Entry = GetResourceEntry(Id, Table);
    Assert(Entry);

    if(Entry->Memory && Entry->Memory != Memory)
    {
        OSRelease(Entry->Memory);
    }

    if(Entry->Memory)
    {
        Assert(Type == Entry->ResourceType);
    }

    Entry->Key          = Key;
    Entry->Memory       = Memory;
    Entry->ResourceType = Type;
}

internal ui_text *
QueryTextResource(u32 NodeIndex, ui_subtree *Subtree, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeResourceKey(UIResource_Text, NodeIndex, Subtree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    Assert(State.Resource && State.ResourceType == UIResource_Text);

    ui_text *Result = (ui_text *)State.Resource;
    return Result;
}

internal ui_text_input *
QueryTextInputResource(u32 NodeIndex, ui_subtree *Subtree, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeResourceKey(UIResource_TextInput, NodeIndex, Subtree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    Assert(State.Resource && State.ResourceType == UIResource_TextInput);

    ui_text_input *Result = (ui_text_input *)State.Resource;
    return Result;
}

internal ui_scroll_region *
QueryScrollRegion(u32 NodeIndex, ui_subtree *Subtree, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeResourceKey(UIResource_ScrollRegion, NodeIndex, Subtree);
    ui_resource_state State = FindResourceByKey(Key, Table);

    Assert(State.Resource && State.ResourceType == UIResource_ScrollRegion);

    ui_scroll_region *Result = (ui_scroll_region *)State.Resource;
    return Result;
}

// ----------------------------------------------------------------------------------
// Pipeline Internal Implementation

internal u64
GetSubtreeStaticFootprint(u64 NodeCount)
{
    u64 TreeSize  = GetLayoutTreeFootprint(NodeCount);
    u64 StyleSize = NodeCount * sizeof(ui_node_style);
    u64 Result    = sizeof(ui_subtree) + TreeSize + StyleSize;

    return Result;
}

// ----------------------------------------------------------------------------------
// Pipeline Public API Implementation

// NOTE:
// Is this even used?

internal b32
IsValidSubtree(ui_subtree *Subtree)
{
    b32 Result = (Subtree) && (Subtree->Persistent) && (Subtree->Transient) && (Subtree->LayoutTree);
    return Result;
}

internal void
UIBeginSubtree(ui_subtree_params Params)
{
    if(Params.CreateNew)
    {
        Assert(Params.NodeCount && "Cannot create a subtree with 0 nodes.");

        memory_arena *Persistent = 0;
        {
            u64 Footprint = GetSubtreeStaticFootprint(Params.NodeCount);

            memory_arena_params Params = {0};
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;
            Params.ReserveSize       = Footprint;
            Params.CommitSize        = Footprint;

            Persistent = AllocateArena(Params);
        }

        memory_arena *Transient = 0;
        {
            memory_arena_params Params = {0};
            Params.AllocatedFromFile = __FILE__;
            Params.AllocatedFromLine = __LINE__;
            Params.ReserveSize       = Kilobyte(64);
            Params.CommitSize        = Kilobyte(8);

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
                u64   Footprint = GetLayoutTreeFootprint(Params.NodeCount);
                void *Memory    = PushArray(Subtree->Persistent, u8, Footprint);

                Tree = PlaceLayoutTreeInMemory(Params.NodeCount, Memory);
            }
            Subtree->LayoutTree = Tree;

            // NOTE:
            // What the fuck is this?

            ui_pipeline *Pipeline = GetCurrentPipeline();
            Assert(Pipeline);
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

internal void
UIEndSubtree(ui_subtree_params Params)
{
    Useless(Params);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);
}

internal ui_pipeline *
GetFreeUIPipeline(void)
{
    ui_pipeline_buffer *Buffer = &UIState.Pipelines;
    Assert(Buffer);
    Assert(Buffer->Count < Buffer->Size); // NOTE: Wrong ui_config.h!

    ui_pipeline *Result = Buffer->Values + Buffer->Count++;

    return Result;
}

internal ui_pipeline *
UICreatePipeline(ui_pipeline_params Params)
{
    ui_state    *State  = &UIState;
    ui_pipeline *Result = GetFreeUIPipeline();
    Assert(Result);

    // Static Data
    {
        memory_arena_params ArenaParams = { 0 };
        ArenaParams.AllocatedFromFile = __FILE__;
        ArenaParams.AllocatedFromLine = __LINE__;
        ArenaParams.ReserveSize       = Megabyte(1);
        ArenaParams.CommitSize        = Kilobyte(1);

        Result->StaticArena = AllocateArena(ArenaParams);
    }

    // Node Id Table
    {
        ui_node_id_table_params Params = { 0 };
        Params.GroupSize  = NodeIdTable_128Bits;
        Params.GroupCount = 32;

        u64   Footprint = GetNodeIdTableFootprint(Params);
        void *Memory    = PushArray(Result->StaticArena, u8, Footprint);

        Result->IdTable = PlaceNodeIdTableInMemory(Params, Memory);
    }

    // Registry - Maybe just return a buffer?
    {
        Result->Registry = CreateStyleRegistry(Params.ThemeFile, Result->StaticArena);
    }

    State->CurrentPipeline = Result;

    return Result;
}

// NOTE:
// Perhaps we do not need this?

internal void
UIBeginAllSubtrees(ui_pipeline *Pipeline)
{
    Assert(Pipeline && "Pipeline Pointer is NULL.");

    ui_subtree_list *List  = &Pipeline->Subtrees;
    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree *Subtree = &Node->Value;

        ClearArena(Subtree->Transient);
    }

    UIState.CurrentPipeline = Pipeline;
}

internal void
UIExecuteAllSubtrees(ui_pipeline *Pipeline)
{
    Assert(Pipeline && "Pipeline Pointer is NULL.");

    ui_subtree_list *List = &Pipeline->Subtrees;
    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree *Subtree = &Node->Value;

        ComputeSubtreeLayout(Subtree);
        UpdateSubtreeState(Subtree);
        PaintSubtree(Subtree);
    }
}
