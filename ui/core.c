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
// UI Context Public API Implementation

internal void
UIBeginFrame()
{
    ui_state *State = &UIState;

    // Clear the state if needed
    b32 MouseReleased = OSIsMouseReleased(OSMouseButton_Left);
    if(MouseReleased)
    {
        State->CapturedNode = 0;
    }
}

// -------------------------------------------------------------
// UI Node Private API Implementation

internal ui_node_chain *
GetNodeChain(void)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_node_chain *Result = Pipeline->Chain;
    if(Result)
    {
        Assert(Result->Subtree);
        Assert(Result->Node.CanUse);
    }

    return Result;
}

internal ui_node_chain *
SetTextColorInChain(ui_color Color)
{
    ui_node_chain *Result = GetNodeChain();
    Assert(Result);
    Assert(Result->Subtree);

    UISetTextColor(Result->Node, Color, Result->Subtree);

    return Result;
}

internal ui_node_chain *
SetStyleInChain(u32 StyleIndex)
{
    ui_node_chain *Result = GetNodeChain();
    Assert(Result);
    Assert(Result->Node.CanUse);

    ui_node_style *Style = GetNodeStyle(Result->Node.IndexInTree, Result->Subtree);
    Assert(Style);

    Style->CachedStyleIndex  = StyleIndex;
    Style->LayoutInfoIsBound = 0;

    return Result;
}

internal ui_node_chain *
FindChildInChain(u32 Index)
{
    ui_node_chain *Result = GetNodeChain();
    Assert(Result);
    Assert(Result->Node.CanUse);
    Assert(Result->Subtree);

    ui_node Child = FindLayoutChild(Result->Node, Index, Result->Subtree);
    Result->Node = Child;

    return Result;
}

internal ui_node_chain *
ReserveChildrenInChain(u32 Amount)
{
    ui_node_chain *Result = GetNodeChain();
    Assert(Result);
    Assert(Result->Node.CanUse);
    Assert(Result->Subtree);

    ReserveLayoutChildren(Result->Node, Amount, Result->Subtree);

    return Result;
}

internal ui_node_chain *
SetNodeIdInChain(byte_string Id)
{
    ui_node_chain *Result = GetNodeChain();
    Assert(Result);
    Assert(Result->Node.CanUse);

    SetNodeId(Id, Result->Node, GetCurrentPipeline()->IdTable);

    return Result;
}

// -------------------------------------------------------------
// UI Node Public API Implementation

// NOTE: This could be smarter, we do not have to push a chain in every case.
// If they are from the same subtree then we can just replace the node,
// but we do not have enough information on the node yet.

internal ui_node_chain *
UIChain(ui_node Node)
{
    ui_node_chain *Result  = 0;
    ui_node_chain *Current = GetNodeChain();
    ui_subtree    *Subtree = 0;

    if(!Current)
    {
        ui_pipeline *Pipeline = GetCurrentPipeline();
        Assert(Pipeline);

        Subtree = FindSubtree(Node, Pipeline);
    }
    else
    {
        Subtree = Current->Subtree;
    }

    Assert(Subtree);

    Result = PushStruct(Subtree->FrameData, ui_node_chain);
    Assert(Result);

    Result->Node            = Node;
    Result->Subtree         = Subtree;
    Result->Prev            = Current;
    Result->SetTextColor    = SetTextColorInChain;
    Result->SetStyle        = SetStyleInChain;
    Result->FindChild       = FindChildInChain;
    Result->ReserveChildren = ReserveChildrenInChain;
    Result->SetId           = SetNodeIdInChain;

    GetCurrentPipeline()->Chain = Result;

    return Result;
}

// ----------------------------------------------------------------------------------
// UI Resource Cache Private Implementation

typedef struct ui_resource_entry
{
    ui_resource_key Key;

    u32 NextWithSameHashSlot;
    u32 NextLRU;
    u32 PrevLRU;

    UIResource_Type ResourceType;
    u64             ResourceSize;
    void           *ResourceMemory;
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

            Entry->ResourceType   = UIResource_None;
            Entry->ResourceSize   = 0;
            Entry->ResourceMemory = 0;
        }
    }

    return Result;
}

internal ui_resource_key
MakeUITextResourceKey(byte_string Text)
{
    u64             Hashed = HashByteString(Text);
    ui_resource_key Key    = {.Value = _mm_set_epi64x(0, Hashed)};
    return Key;
}

internal ui_resource_state
FindUIResourceByKey(ui_resource_key Key, ui_resource_table *Table)
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
    Result.Id       = EntryIndex;
    Result.Type     = FoundEntry->ResourceType;
    Result.Resource = FoundEntry->ResourceMemory;

    return Result;
}

internal void
UpdateUITextResource(u32 Id, byte_string Text, ui_font *Font, ui_resource_table *Table)
{
    ui_resource_entry *Entry = GetResourceEntry(Id, Table);
    Assert(Entry && "Resource Id is invalid");

    // Then we use the Glyph Run API to malloc some memory. At least for now. And we update the entry.
    // What happens when we update an entry that already has memory? Can just free it for now.

    if(!Entry->ResourceMemory)
    {
        u64   Size   = GetGlyphRunFootprint(Text);
        void *Memory = OSReserveMemory(Size);

        if(Memory && OSCommitMemory(Memory, Size))
        {
            Entry->ResourceSize   = Size;
            Entry->ResourceMemory = CreateGlyphRun(Text, Font, Memory);
        }
    }
    else
    {
        Assert(!"Not implemented!");
    }
}

internal ui_resource_key
GetNodeResource(u32 NodeId, ui_subtree *Subtree)
{
    ui_resource_key Resource = Subtree->Resources[NodeId];
    return Resource;
}

// -----------------------------------------------------------------------------------
// UI Context Public Implementation

internal ui_pipeline *
GetCurrentPipeline()
{
    ui_pipeline *Pipeline = UIState.CurrentPipeline;
    return Pipeline;
}

// ----------------------------------------------------------------------------------
// Pipeline Public API Implementation

internal b32
IsValidSubtree(ui_subtree *Subtree)
{
    b32 Result = (Subtree) && (Subtree->FrameData) && (Subtree->LayoutTree);
    return Result;
}

internal void
UIBeginSubtree(ui_subtree_params Params)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    memory_arena *Persist = Pipeline->StaticArena;
    Assert(Persist);

    if(Params.CreateNew)
    {
        Assert(Params.NodeCount && "Cannot create a subtree with 0 nodes.");

        ui_subtree_node *SubtreeNode = PushStruct(Persist, ui_subtree_node);
        if(SubtreeNode)
        {
            ui_subtree_list *SubtreeList = &Pipeline->Subtrees;
            AppendToLinkedList(SubtreeList, SubtreeNode, SubtreeList->Count);

            ui_layout_tree *LayoutTree = 0;
            {
                u64   Footprint = GetLayoutTreeFootprint(Params.NodeCount);
                void *Memory    = PushArray(Persist, u8, Footprint);

                LayoutTree = PlaceLayoutTreeInMemory(Params.NodeCount, Memory);
            }

            memory_arena *Arena = 0;
            {
                memory_arena_params Params = {0};
                Params.AllocatedFromFile = __FILE__;
                Params.AllocatedFromLine = __LINE__;
                Params.ReserveSize       = Megabyte(1);
                Params.CommitSize        = Kilobyte(16);

                Arena = AllocateArena(Params);
            }

            SubtreeNode->Value.NodeCount      = Params.NodeCount;
            SubtreeNode->Value.FrameData      = Arena;
            SubtreeNode->Value.LayoutTree     = LayoutTree;
            SubtreeNode->Value.ComputedStyles = PushArray(Persist, ui_node_style  , Params.NodeCount);
            SubtreeNode->Value.Resources      = PushArray(Persist, ui_resource_key, Params.NodeCount);
            SubtreeNode->Value.Id             = Pipeline->NextSubtreeId++;

            Pipeline->CurrentSubtree = &SubtreeNode->Value;
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

    Pipeline->Chain = 0;
}

// NOTE: This is thinning out which is good. Soon I won't need these allocations
// I am guessing. This construct may still be useful.

internal ui_pipeline *
UICreatePipeline(ui_pipeline_params Params)
{
    ui_state    *State  = &UIState;
    ui_pipeline *Result = PushArray(State->StaticData, ui_pipeline, 1);

    if(Result)
    {
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

        // Registry
        {
            Result->Registry = CreateStyleRegistry(Params.ThemeFile, Result->StaticArena);
        }

        AppendToLinkedList((&State->Pipelines), Result, State->Pipelines.Count);

        State->CurrentPipeline = Result;
    }

    return Result;
}

internal void
UIBeginAllSubtrees(ui_pipeline *Pipeline)
{
    Assert(Pipeline && "Pipeline Pointer is NULL.");

    ui_subtree_list *List  = &Pipeline->Subtrees;
    IterateLinkedList(List, ui_subtree_node *, Node)
    {
        ui_subtree *Subtree = &Node->Value;

        ClearArena(Subtree->FrameData);

        Subtree->Events = 0;
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

        ComputeLayout(Subtree, Subtree->FrameData);
        HitTestLayout(Subtree, Subtree->FrameData);
        DrawLayout   (Subtree, Subtree->FrameData);
    }
}

internal ui_subtree *
FindSubtree(ui_node Node, ui_pipeline *Pipeline)
{
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
