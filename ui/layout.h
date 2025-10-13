// [CONSTANTS]

typedef enum UILayoutNode_Type
{
    UILayoutNode_None       = 0,
    UILayoutNode_Window     = 1,
    UILayoutNode_Button     = 2,
    UILayoutNode_Label      = 3,
    UILayoutNode_Header     = 4,
    UILayoutNode_ScrollView = 5,
} UILayoutNode_Type;

typedef enum UILayoutNode_Flag
{
    UILayoutNode_NoFlag         = 0,

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
    UILayoutNode_PlaceChildrenX = 1 << 8,
    UILayoutNode_PlaceChildrenY = 1 << 9,
    UILayoutNode_IsParent       = 1 << 10,

    // Binds
    UILayoutNode_TextIsBound    = 1 << 11,
} UILayoutNode_Flag;

typedef enum LayoutTree_Constant
{
    LayoutTree_DefaultDepth     = 16,
    LayoutTree_DefaultCapacity  = 500,
    LayoutTree_InvalidNodeIndex = 0xFFFFFFFF,
} LayoutTree_Constant;

typedef enum NodeIdTable_Constant
{
    NodeIdTable_GroupSize = 16,
    NodeIdTable_EmptyMask = 1 << 0,      // 1st bit
    NodeIdTable_DeadMask  = 1 << 1,      // 2nd bit
    NodeIdTable_TagMask   = 0xFF & ~0x03 // High 6 bits
} NodeIdTable_Constant;

// [CORE TYPES]

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
    ui_glyph_run *DisplayText;
} ui_layout_box;

// A simple tree is used for representing a layout.
// Each pipeline posseses a tree.

typedef struct ui_layout_node ui_layout_node;
struct ui_layout_node
{
    u32             Index;
    u32             ChildCount;
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    UILayoutNode_Type Type;
    ui_layout_box     Value;

    bit_field Flags;

    ui_cached_style *CachedStyle; // NOTE: Really?
};

typedef struct ui_layout_tree_params
{
    // NOTE: Both of these are problematic, because they set a hard limit on trees.
    // Unsure what the correct approach is.

    u32 Depth;
    u32 NodeCount;
} ui_layout_tree_params;

typedef struct ui_layout_tree
{
    // Nodes
    u32             NodeCapacity;
    u32             NodeCount;
    ui_layout_node *Nodes;

    // Depth
    u32              ParentTop;
    u32              MaximumDepth;
    ui_layout_node **ParentStack;
} ui_layout_tree;

// A hashmap is used to query nodes per ID
// An id is simply a string that can we be set
// to whatever the user wants it to be.

typedef struct ui_node_id_hash
{
    u64 Value;
} ui_node_id_hash;

typedef struct ui_node_id_entry
{
    ui_node_id_hash Hash;
    ui_layout_node *Target;
} ui_node_id_entry;

typedef struct ui_node_id_table
{
    u8               *MetaData;
    ui_node_id_entry *Buckets;

    u64 GroupSize;
    u64 GroupCount;

    u64 HashMask;
} ui_node_id_table;

typedef struct ui_node_id_table_params
{
    u64 GroupSize;
    u64 GroupCount;
} ui_node_id_table_params;

// [Tree Mutations]

internal void DragUISubtree    (vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void ResizeUISubtree  (vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline);

// [Layout Pass]

internal ui_hit_test HitTestLayout     (vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PreOrderMeasure   (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PostOrderMeasure  (ui_layout_node *LayoutRoot);
internal void        PreOrderPlace     (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);

// [Layout Tree/Nodes]

internal u64              GetLayoutTreeFootprint   (ui_layout_tree_params Params);
internal ui_layout_tree * PlaceLayoutTreeInMemory  (ui_layout_tree_params Params, void *Memory);
internal b32              IsValidLayoutNode        (ui_layout_node *Node);
internal void             PopLayoutNodeParent      (ui_layout_tree *Tree);
internal void             PushLayoutNodeParent     (ui_layout_node *Node, ui_layout_tree*Tree);
internal ui_layout_node * InitializeLayoutNode     (ui_cached_style *Style, UILayoutNode_Type Type, ui_layout_node *Parent, bit_field ConstantFlags, byte_string Id, ui_pipeline *Pipeline);
internal ui_layout_node * GetLayoutNodeParent      (ui_layout_tree *Tree);
internal ui_layout_node * GetFreeLayoutNode        (ui_layout_tree *Tree, UILayoutNode_Type Type);

// [Binds]

internal void BindText  (ui_layout_node *Node, byte_string Text, ui_font *Font, memory_arena *Arena);

// [Node Id]

internal u64                GetNodeIdTableFootprint      (ui_node_id_table_params Params);
internal ui_node_id_table * PlaceNodeIdTableInMemory     (ui_node_id_table_params Params, void *Memory);
internal b32                IsValidNodeIdTable           (ui_node_id_table *Table);
internal u32                GetNodeIdGroupIndexFromHash  (ui_node_id_hash Hash, ui_node_id_table *Table);
internal u8                 GetNodeIdTagFromHash         (ui_node_id_hash Hash);
internal ui_node_id_hash    ComputeNodeIdHash            (byte_string Id);
internal ui_node_id_entry * FindNodeIdEntry              (ui_node_id_hash Hash, ui_node_id_table *Table);
internal void               InsertNodeId                 (ui_node_id_hash Hash, ui_layout_node *Node, ui_node_id_table *Table);
internal void               SetNodeId                    (byte_string Id, ui_layout_node *Node, ui_node_id_table *Table);
internal ui_layout_node   * UIFindNodeById               (byte_string Id, ui_pipeline *Pipeline);
