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
    u32 Depth;
    u32 NodeCount;
} ui_layout_tree_params;

typedef struct ui_layout_tree
{
    memory_arena *Arena;

    u32             NodeCapacity;
    u32             NodeCount;
    ui_layout_node *Nodes;

    u32              ParentTop;
    u32              MaximumDepth;
    ui_layout_node **ParentStack;
} ui_layout_tree;

// [Globals]

read_only global u32 InvalidLayoutNodeIndex    = 0xFFFFFFFF;
read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [Tree Mutations]

internal void DragUISubtree    (vec2_f32 Delta, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void ResizeUISubtree  (vec2_f32 Delta, ui_layout_node *LayoutNode, ui_pipeline *Pipeline);

// [Layout Pass]

internal ui_hit_test HitTestLayout     (vec2_f32 MousePosition, ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PreOrderMeasure   (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);
internal void        PostOrderMeasure  (ui_layout_node *LayoutRoot);
internal void        PreOrderPlace     (ui_layout_node *LayoutRoot, ui_pipeline *Pipeline);

// [Layout Tree/Nodes]

internal b32              IsValidLayoutNode     (ui_layout_node *Node);
internal ui_layout_tree * AllocateLayoutTree    (ui_layout_tree_params Params);
internal void             PopLayoutNodeParent   (ui_layout_tree *Tree);
internal void             PushLayoutNodeParent  (ui_layout_node *Node, ui_layout_tree*Tree);
internal ui_layout_node * InitializeLayoutNode  (ui_cached_style *Style, UILayoutNode_Type Type, bit_field ConstantFlags, ui_layout_tree *Tree);
internal ui_layout_node * GetLayoutNodeParent   (ui_layout_tree *Tree);
internal ui_layout_node * GetFreeLayoutNode     (ui_layout_tree *Tree, UILayoutNode_Type Type);

// [Binds]

internal void BindText  (ui_layout_node *Node, byte_string Text, ui_font *Font, memory_arena *Arena);
