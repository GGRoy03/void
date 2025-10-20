// [CONSTANTS]

typedef enum UILayoutNode_Flag
{
    // Draws
    UILayoutNode_DrawBackground = 1 << 0,
    UILayoutNode_DrawBorders    = 1 << 1,
    UILayoutNode_DrawText       = 1 << 2,

    // Batching
    UILayoutNode_HasClip        = 1 << 3,

    // State
    UILayoutNode_IsHovered      = 1 << 4,
    UILayoutNode_IsClicked      = 1 << 5,
    UILayoutNode_IsDraggable    = 1 << 6,
    UILayoutNode_IsResizable    = 1 << 7,

    // Layout Info
    UILayoutNode_PlaceChildrenX  = 1 << 8,
    UILayoutNode_PlaceChildrenY  = 1 << 9,
    UILayoutNode_IsParent        = 1 << 10,
    UILayoutNode_IsScrollRegion  = 1 << 11,

    // Binds
    UILayoutNode_TextIsBound     = 1 << 12,

    // Dirty/Stale/Pruned
    UILayoutNode_IsPruned        = 1 << 13,
} UILayoutNode_Flag;

typedef enum LayoutTree_Constant
{
    LayoutTree_DefaultNodeCount = 4000,
    LayoutTree_DefaultNodeDepth = 16,
    LayoutTree_InvalidNodeIndex = 0xFFFFFFFF,
} LayoutTree_Constant;

typedef enum ScrollAxis_Type
{
    ScrollAxis_X = 0,
    ScrollAxis_Y = 1,
} ScrollAxis_Type;

// [CORE TYPES]

typedef struct ui_scroll_context
{
    vec2_f32        ContentSize;       // Logical Size for all of the content inside
    vec2_f32        ContentWindowSize; // Actual size of the widget, what we see
    f32             ScrollOffset;      // Scroll Offset in pixel 0..ScrollAxis.Max
    f32             PixelPerLine;      // How many pixels to scroll per line.
    ScrollAxis_Type Axis;              // Which Axis the scroll is applied to
} ui_scroll_context;

typedef struct ui_hit_test
{
    ui_layout_node *Node;
    UIIntent_Type   Intent;
    b32             Success;
} ui_hit_test;

typedef struct ui_layout_box
{
    // Output
    f32 FinalX;
    f32 FinalY;
    f32 FinalWidth;
    f32 FinalHeight;

    // Layout Info
    ui_unit    Width;
    ui_unit    Height;
    ui_spacing Spacing;
    ui_padding Padding;

    // Params
    rect_f32 Clip;

    // Binds
    ui_glyph_run      *DisplayText;
    ui_scroll_context *ScrollContext;
} ui_layout_box;

// A simple tree is used for representing a layout.
// Each pipeline posseses a tree.

typedef struct ui_layout_node ui_layout_node;
struct ui_layout_node
{
    // Hierarchy
    u32             Index;
    u32             ChildCount;
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    ui_layout_box Value;
    bit_field     Flags;
};

typedef struct ui_layout_tree_params
{
    u64 NodeCount;
    u64 NodeDepth;
} ui_layout_tree_params;

DEFINE_TYPED_STACK(LayoutNode, layout_node, ui_layout_node *)

typedef struct ui_layout_tree
{
    // Nodes
    u64             NodeDepth;
    u64             NodeCapacity;
    u64             NodeCount;
    ui_layout_node *Nodes;

    // Depth
    layout_node_stack Parents;
} ui_layout_tree;

// [Tree Mutations]

internal void DragUISubtree    (vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void ResizeUISubtree  (vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline);

// [Layout Pass]

internal ui_hit_test HitTestLayout     (vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PreOrderMeasure   (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PostOrderMeasure  (ui_layout_node *LayoutRoot);
internal void        PreOrderPlace     (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);

// [Layout Tree/Nodes]

internal rect_f32 MakeRectFromNode(ui_layout_node *Node, vec2_f32 Translation);
internal ui_layout_node      * GetLastAddedLayoutNode   (ui_layout_tree *Tree);
internal b32                   IsValidLayoutNode        (ui_layout_node *Node);
internal u64                   GetLayoutTreeFootprint   (ui_layout_tree_params Params);
internal ui_layout_tree_params SetDefaultTreeParams     (ui_layout_tree_params Params);
internal ui_layout_tree      * PlaceLayoutTreeInMemory  (ui_layout_tree_params Params, void *Memory);
internal b32                   IsValidLayoutNode        (ui_layout_node *Node);
internal ui_layout_node      * InitializeLayoutNode     (ui_cached_style *Style, bit_field ConstantFlags, byte_string Id, ui_pipeline *Pipeline);
internal ui_layout_node      * GetFreeLayoutNode        (ui_layout_tree *Tree);

// [Binds]

internal void BindText  (ui_layout_node *Node, byte_string Text, ui_font *Font, memory_arena *Arena);

// [Context]

internal void UIBeginSubtree  (ui_layout_node *Parent, ui_pipeline *Pipeline);
internal void UIEndSubtree    (ui_pipeline *Pipeline);

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
//   Opaque pointer.
//
// SetNodeId:
//  Given a byte_string we try to insert and a node we try to insert the entry into the table.
//  If the table or the node are invalid, this function has no effect. (Log)
//  If an entry with the same name already exists, this function has no effect. (Log)
//  Otherwise, the id will be linked to the name in the table and you can find it with UIFindNodeById
//
// UIFindNodeById:
//  Given a byte string we try to query the corresponding node.
//  Returns the node if found (Must be inserted via SetNodeId) or 0 if not.
//  If the table or the byte_string is invalid, this function has no effect.
// -------------------------------------------------------------------------------------------------------------------

typedef struct ui_node_id_table ui_node_id_table;

internal void               SetNodeId       (byte_string Id, ui_layout_node *Node, ui_node_id_table *Table);
internal ui_layout_node   * UIFindNodeById  (byte_string Id, ui_node_id_table *Table);

// [Scrolling]

internal void     ApplyScrollToContext     (f32 ScrollDelta, ui_scroll_context *Context);
internal void     PruneScrollContextNodes  (ui_layout_node *Node);
internal vec2_f32 GetScrollTranslation     (ui_layout_node *Node);
