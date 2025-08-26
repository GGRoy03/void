static void
PopParent()
{
    Cim_Assert(CimCurrent);

    cim_layout      *Layout = UIP_LAYOUT;
    ui_layout_tree *Tree   = &Layout->Tree; // This would access the current tree.

    if (Tree->AtParent > 0)
    {
        cim_u32 IdToPop = Tree->Nodes[Tree->ParentStack[Tree->AtParent--]].Id;
        if (IdToPop == Tree->FirstDragNode)
        {
            Tree->DragTransformX = 0;
            Tree->DragTransformY = 0;
        }
    }
    else
    {
        CimLog_Error("Unable to pop parent since we are already at depth 0");
    }
}

static cim_rect
MakeRectFromNode(ui_layout_node *Node)
{
    cim_rect Rect;
    Rect.MinX = Node->X;
    Rect.MinY = Node->Y;
    Rect.MaxX = Node->X + Node->Width;
    Rect.MaxY = Node->Y + Node->Height;

    return Rect;
}

static ui_layout_node *
GetUILayoutNode(cim_u32 Index)
{
    Cim_Assert(CimCurrent);

    cim_layout      *Layout = UIP_LAYOUT;
    ui_layout_tree *Tree   = &Layout->Tree; // This would access the current tree.

    if (Index < Tree->NodeCount)
    {
        ui_layout_node *Node = Tree->Nodes + Index;

        return Node;
    }
    else
    {
        CimLog_Error("Invalid index used when accesing layout node: %u", Index);
        return NULL;
    }
}

static ui_layout_node *
PushLayoutNode(bool IsContainer, ui_component_state *State)
{
    Cim_Assert(CimCurrent);

    cim_layout     *Layout = UIP_LAYOUT;
    ui_layout_tree *Tree   = &Layout->Tree; // This would access the current tree.

    if (Tree->NodeCount == Tree->NodeSize)
    {
        size_t NewSize = Tree->NodeSize ? (size_t)Tree->NodeSize * 2u : 8u;

        if (NewSize > (SIZE_MAX / sizeof(*Tree->Nodes)))
        {
            CimLog_Fatal("Requested allocation size too large.");
            return NULL;
        }

        ui_layout_node *Temp = (ui_layout_node *)realloc(Tree->Nodes, NewSize * sizeof(*Temp));
        if (!Temp)
        {
            CimLog_Fatal("Failed to heap-allocate.");
            return NULL;
        }

        Tree->Nodes    = Temp;
        Tree->NodeSize = (cim_u32)NewSize;
    }

    cim_u32         NodeId = Tree->NodeCount;
    ui_layout_node *Node   = Tree->Nodes + NodeId;
    cim_u32         Parent = Tree->ParentStack[Tree->AtParent];

    Node->Id          = NodeId;
    Node->FirstChild  = CimLayout_InvalidNode;
    Node->LastChild   = CimLayout_InvalidNode;
    Node->NextSibling = CimLayout_InvalidNode;
    Node->Parent      = CimLayout_InvalidNode;
    Node->State       = State;

    if (Parent != CimLayout_InvalidNode)
    {
        ui_layout_node *ParentNode = Tree->Nodes + Parent;

        if (ParentNode->FirstChild == CimLayout_InvalidNode)
        {
            ParentNode->FirstChild = NodeId;
            ParentNode->LastChild  = NodeId;
        }
        else
        {
            ui_layout_node *LastChild = Tree->Nodes + ParentNode->LastChild;
            LastChild->NextSibling = NodeId;
            ParentNode->LastChild = NodeId;
        }

        Node->Parent = Parent;
    }

    if (IsContainer)
    {
        if (Tree->AtParent + 1 < CimLayout_MaxNestDepth)
        {
            Tree->ParentStack[++Tree->AtParent] = NodeId;
        }
        else
        {
            CimLog_Error("Maximum nest depth reached: %u", CimLayout_MaxNestDepth);
        }
    }

    ++Tree->NodeCount;

    return Node;
}

static ui_component *
FindComponent(const char *Key)
{
    Cim_Assert(CimCurrent);
    ui_component_hashmap *Hashmap = &UI_LAYOUT.Components;

    if (!Hashmap->IsInitialized)
    {
        Hashmap->GroupCount = 32;

        cim_u32 BucketCount = Hashmap->GroupCount * CimBucketGroupSize;
        size_t  BucketSize = BucketCount * sizeof(ui_component_entry);
        size_t  MetadataSize = BucketCount * sizeof(cim_u8);

        Hashmap->Buckets = (ui_component_entry *)malloc(BucketSize);
        Hashmap->Metadata = (cim_u8 *)malloc(MetadataSize);

        if (!Hashmap->Buckets || !Hashmap->Metadata)
        {
            return NULL;
        }

        memset(Hashmap->Buckets, 0, BucketSize);
        memset(Hashmap->Metadata, CimEmptyBucketTag, MetadataSize);

        Hashmap->IsInitialized = true;
    }

    cim_u32 ProbeCount = 0;
    cim_u32 Hashed = CimHash_String32(Key);
    cim_u32 GroupIdx = Hashed & (Hashmap->GroupCount - 1);

    while (true)
    {
        cim_u8 *Meta = Hashmap->Metadata + GroupIdx * CimBucketGroupSize;
        cim_u8  Tag = (Hashed & 0x7F);

        __m128i Mv = _mm_loadu_si128((__m128i *)Meta);
        __m128i Tv = _mm_set1_epi8(Tag);

        cim_i32 TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Tv));
        while (TagMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(TagMask);
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            if (strcmp(E->Key, Key) == 0)
            {
                return &E->Value;
            }

            TagMask &= TagMask - 1;
        }

        __m128i Ev = _mm_set1_epi8(CimEmptyBucketTag);
        cim_i32 EmptyMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Ev));
        if (EmptyMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(EmptyMask);
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            Meta[Lane] = Tag;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            strcpy_s(E->Key, sizeof(E->Key), Key);
            E->Key[sizeof(E->Key) - 1] = 0;

            return &E->Value;
        }

        ProbeCount++;
        GroupIdx = (GroupIdx + ProbeCount * ProbeCount) & (Hashmap->GroupCount - 1);
    }

    return NULL;
}