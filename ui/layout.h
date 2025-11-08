// ui_layout_node:
// ui_layout_tree:
//   Opaque types the user does not need to know about.
//
// GetLayoutTreeFootprint:
//   Returns the size of the memory needed to call PlaceLayoutTreeInMemory.
//   Parameters will apply their default values if needed.
//
// PlaceLayoutTreeInMemory:
//   Places the tree in memory given the parameters and the memory YOU allocated.
//   You are responsible for managing this memory.
//   If memory is 0, it will fall through and 0 will be returned, thus you must only check that this function doesn't return 0.
//
// UIEnterSubtree & UILeaveSubtree & UISubtree:
//   ...

typedef struct ui_layout_node  ui_layout_node;
typedef struct ui_layout_tree  ui_layout_tree;

#define InvalidUINodeIndex 0xFFFFFFFF

typedef enum UILayoutNode_Flag
{
    // Painting
    UILayoutNode_DoNotPaint      = 1 << 1,
    UILayoutNode_HasClip         = 1 << 2,

    // State
    UILayoutNode_IsDraggable     = 1 << 4,
    UILayoutNode_IsResizable     = 1 << 5,

    // Layout Info (Should not exist most likely)
    UILayoutNode_PlaceChildrenX  = 1 << 6,
    UILayoutNode_PlaceChildrenY  = 1 << 7,
    UILayoutNode_IsParent        = 1 << 8,

    // Frame State (Do not like this)
    UILayoutNode_IsHovered       = 1 << 9,

    // Resources
    UILayoutNode_HasText         = 1 << 10,
    UILayoutNode_HasTextInput    = 1 << 11,
    UILayoutNode_HasScrollRegion = 1 << 12,
} UILayoutNode_Flag;

internal u64              GetLayoutTreeFootprint   (u64 NodeCount);
internal ui_layout_tree * PlaceLayoutTreeInMemory  (u64 NodeCount, void *Memory);
internal ui_node          AllocateUINode           (bit_field Flags, ui_subtree *Subtree);
internal void             UIEnd                    (void);

internal ui_node FindLayoutChild        (u32 NodeIndex, u32 ChildIndex, ui_subtree *Subtree);
internal void    ReserveLayoutChildren  (ui_node Node, u32 Amount, ui_subtree *Subtree);

internal void UpdateNodeIfNeeded    (u32 NodeIndex, ui_subtree *Subtree);
internal void SetLayoutNodeFlags    (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);
internal void ClearLayoutNodeFlags  (u32 NodeIndex, bit_field Flags, ui_subtree *Subtree);

// -------------------------------------------------------------------------------------------------------------------
// ui_hit_test:
//
// HitTestLayout:
//
// ComputeLayout:

internal void ComputeSubtreeLayout  (ui_subtree *Subtree);
internal void UpdateSubtreeState    (ui_subtree *Subtree);

internal void PaintSubtree           (ui_subtree *Subtree);

// -------------------------------------------------------------------------------------------------------------------
// NodeIdTable_Size:
//  NodeIdTable_128Bits is the default size for this table which uses 128 SIMD to find/insert in the table
//
// ui_node_id_table_params
//  GroupSize : How many values per "groups" this must be one of NodeIDTableSize
//  GroupCount: How many groups the table contains, this must be a power of two (Asserted in PlaceNodeIdTableInMemory)
//  This table never resizes and the amount of slots must acount for the worst case scenario.
//  Number of slots is computed by GroupSize * GroupCount.
//
// GetNodeIdTableFootprint:
//   Return the number of bytes required to store a node-id table for `Params`.
//   The caller must allocate at least this many bytes (aligned for ui_node_id_entry/ui_node_id_table)
//   before calling PlaceNodeIdTableInMemory.
//
// PlaceNodeIdTableInMemory:
//   Initialize a ui_node_id_table inside the caller-supplied Memory block and return a pointer
//   to the placed ui_node_id_table. Does NOT allocate memory. If Memory == NULL the function
//   returns NULL, thus caller must only check that the returned memory is non-null.
//   Caller owns the memory and is responsible for managing it.
// -------------------------------------------------------------------------------------------------------------------

typedef enum NodeIdTable_Size
{
    NodeIdTable_128Bits = 16,
} NodeIdTable_Size;

typedef struct ui_node_id_table_params
{
    NodeIdTable_Size GroupSize;
    u64              GroupCount;
} ui_node_id_table_params;

internal u64                GetNodeIdTableFootprint   (ui_node_id_table_params Params);
internal ui_node_id_table * PlaceNodeIdTableInMemory  (ui_node_id_table_params Params, void *Memory);

// -------------------------------------------------------------------------------------------------------------------
// ui_node_id_table:
//   Opaque pointer to the table.
//
// SetNodeId:
//  If the table or the node are invalid, this function has no effect.
//  If an entry with the same name already exists, this function has no effect.
//
// FindNodeById:
//  Returns the node if found (Must be inserted via SetNodeId) or an unusable one if not.
// -------------------------------------------------------------------------------------------------------------------

typedef struct ui_node_id_table ui_node_id_table;

internal void    SetNodeId     (byte_string Id, ui_node Node, ui_node_id_table *Table);
internal ui_node FindNodeById  (byte_string Id, ui_node_id_table *Table);

// -------------------------------------------------------------------------------------------------------------------
