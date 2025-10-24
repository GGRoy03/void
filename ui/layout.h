typedef enum ScrollAxis_Type
{
    ScrollAxis_X = 0,
    ScrollAxis_Y = 1,
} ScrollAxis_Type;

typedef struct ui_scroll_context
{
    vec2_f32        ContentSize;       // Logical Size for all of the content inside
    vec2_f32        ContentWindowSize; // Actual size of the widget, what we see
    f32             ScrollOffset;      // Scroll Offset in pixel 0..ScrollAxis.Max
    f32             PixelPerLine;      // How many pixels to scroll per line.
    ScrollAxis_Type Axis;              // Which Axis the scroll is applied to
} ui_scroll_context;

// TODO: Cleanup this API

internal void     BindScrollContext        (ui_node Node, ScrollAxis_Type Axis, ui_layout_tree *Tree, memory_arena *Arena);
internal void     ApplyScrollToContext     (f32 ScrollDelta, ui_scroll_context *Context);
internal void     PruneScrollContextNodes  (ui_layout_node *Node);
internal vec2_f32 GetScrollTranslation     (ui_layout_node *Node);

// ui_layout_node:
// ui_layout_tree:
//   Opaque types the user does not need to know about.
//
// ui_layout_tree_params:
//   Parameters used when allocating trees.
//   If NodeCount == 0 -> It will default to some value.
//   If NodeDepth == 0 -> It will default to some value.
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
//   ui_layout_tree_params Params    = {0};                                                ---> Default Parameters will be set!
//   u64                   Footprint = GetLayoutTreeFootprint(Params);                     ---> Query the footprint!
//   ui_layout_tree        *Tree     = PlaceLayoutTreeInMemory(Params, malloc(Footprint)); ---> Place in memory!
//   if(Tree)                                                                              ---> Check if the memory is valid!
//   {
//      ...
//   }
//
// GetTreeSize:
//   Query the tree size after placing in memory. Useful is it's possible the tree params contained default
//   values. Make sure the tree is initialized before calling this.
//
// UIBuildNode:
//   aaa
//
// UIEnterSubtree & UILeaveSubtree & UISubtree:
//   ...

typedef struct ui_layout_node  ui_layout_node;
typedef struct ui_layout_tree  ui_layout_tree;

typedef enum UILayoutNode_Flag
{
    // Draws
    UILayoutNode_DrawBackground  = 1 << 0,
    UILayoutNode_DrawBorders     = 1 << 1,
    UILayoutNode_DrawText        = 1 << 2,
    UILayoutNode_DoNotDraw       = 1 << 3,

    // Batching
    UILayoutNode_HasClip         = 1 << 4,

    // State
    UILayoutNode_IsHoverable     = 1 << 5,
    UILayoutNode_IsClickable     = 1 << 6,
    UILayoutNode_IsDraggable     = 1 << 7,
    UILayoutNode_IsResizable     = 1 << 8,
    UILayoutNode_IsScrollable    = 1 << 9,

    // Layout Info
    UILayoutNode_PlaceChildrenX  = 1 << 10,
    UILayoutNode_PlaceChildrenY  = 1 << 11,
    UILayoutNode_IsParent        = 1 << 12,
} UILayoutNode_Flag;

typedef enum LayoutTree_Constant
{
    LayoutTree_DefaultNodeCount = 4000,
    LayoutTree_DefaultNodeDepth = 16,
    LayoutTree_InvalidNodeIndex = 0xFFFFFFFF,
} LayoutTree_Constant;

typedef struct ui_layout_tree_params
{
    u64 NodeCount;
    u64 NodeDepth;
} ui_layout_tree_params;

internal u64              GetLayoutTreeFootprint   (ui_layout_tree_params Params);
internal ui_layout_tree * PlaceLayoutTreeInMemory  (ui_layout_tree_params Params, void *Memory);
internal u64              GetTreeSize              (ui_layout_tree *Tree);

internal ui_node          UIFindChild              (ui_node Node, u32 Index, ui_layout_tree *Tree);


internal ui_node          UIBuildLayoutNode       (style_property BaseProperties[StyleProperty_Count], bit_field Flags, ui_subtree *Subtree, memory_arena *Arena);
internal void             UIEnd                    (ui_pipeline *Pipeline);

// -------------------------------------------------------------------------------------------------------------------
// ui_hit_test:
//
// HitTestLayout:
//
// ComputeLayout:

internal void HitTestLayout  (ui_layout_tree *Tree, memory_arena *Arena);
internal void ComputeLayout  (ui_layout_tree *Tree, memory_arena *Arena);
internal void DrawLayout     (ui_subtree *Subtree, memory_arena *Arena);

// [Tree Mutations]

internal void DragUISubtree    (vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void ResizeUISubtree  (vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline);


// [Binds]

internal void BindText(ui_node Node, byte_string Text, ui_font *Font, ui_layout_tree *Tree, memory_arena *Arena);

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
// UISetNodeId:
//  If the table or the node are invalid, this function has no effect.
//  If an entry with the same name already exists, this function has no effect.
//
// UIFindNodeById:
//  Returns the node if found (Must be inserted via SetNodeId) or 0 if not.
// -------------------------------------------------------------------------------------------------------------------

typedef struct ui_node_id_table ui_node_id_table;

internal void    UISetNodeId     (byte_string Id, ui_node Node, ui_node_id_table *Table);
internal ui_node UIFindNodeById  (byte_string Id, ui_node_id_table *Table);

// -------------------------------------------------------------------------------------------------------------------
