// [CONSTANTS]

typedef enum UILayoutBox_Flag
{
    UILayoutBox_NoFlag         = 0,
    UILayoutBox_FlowRow        = 1 << 0,
    UILayoutBox_FlowColumn     = 1 << 1,
    UILayoutBox_DrawBackground = 1 << 2,
} UILayoutBox_Flag;

// [CORE TYPES]

typedef struct ui_layout_box
{
    f32       Width;
    f32       Height;
    vec2_f32  Spacing;
    vec4_f32  Padding;
    vec4_f32  Color;
    
    bit_field Flags;
} ui_layout_box;

typedef struct ui_layout_node ui_layout_node;
struct ui_layout_node
{
    ui_layout_box LayoutBox;
    f32           ClientX;
    f32           ClientY;

    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;
};

typedef struct ui_layout_tree_params
{
    u32 Depth;     // Deepest node in the tree.
    u32 NodeCount; // How many nodes can it hold.
} ui_layout_tree_params;

typedef struct ui_layout_tree
{
    memory_arena *Arena;

    ui_layout_node *Nodes;
    u32             NodeCapacity;
    u32             NodeCount;

    ui_layout_node **ParentStack;
    u32              ParentTop;

    u32 MaximumDepth;
} ui_layout_tree;

typedef struct ui_style
{
    vec4_f32 Color;
    vec4_f32 BorderColor;
    u32      BorderWidth;
    vec2_f32 Size;
    vec2_f32 Spacing;
    vec4_f32 Padding;
} ui_style;

typedef struct ui_style_name
{
    byte_string Value;
} ui_style_name;

typedef struct ui_cached_style ui_cached_style;
struct ui_cached_style
{
    u32              Index;
    ui_cached_style *Next;
    ui_style         Style;
};

typedef struct ui_style_registery
{
    u32 Count;
    u32 Capacity;

    ui_style_name   *CachedName;
    ui_cached_style *Sentinels;
    ui_cached_style *CachedStyles;
    memory_arena    *Arena;
} ui_style_registery;

typedef struct ui_state
{
    ui_layout_tree     LayoutTree;     // NOTE: Unclear if we want an array of those?
    ui_style_registery StyleRegistery;
} ui_state;

// [Globals]

read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [API]

// [Components]

internal void UIWindow  (ui_style_name StyleName, ui_layout_tree *LayoutTree, ui_style_registery *StyleRegistery);
internal void UIButton  (ui_style_name StyleName, ui_layout_tree *LayoutTree, ui_style_registery *StyleRegistery);

// [Style]

internal ui_cached_style * UIGetStyleSentinel            (byte_string Name, ui_style_registery *Registery);
internal ui_style_name     UIGetCachedNameFromStyleName  (byte_string Name, ui_style_registery *Registery);
internal ui_style          UIGetStyleFromCachedName      (ui_style_name Name, ui_style_registery *Registery);

// [Layout]

internal ui_layout_tree   UIAllocateLayoutTree      (ui_layout_tree_params Params);
internal ui_layout_node * UIGetParentForLayoutNode  (ui_layout_tree *Tree);
internal void             UIPopParentLayoutNode     (ui_layout_tree *Tree);
internal void             UICreateLayoutNode        (vec2_f32 Position, ui_layout_box LayoutBox, b32 IsAlwaysLeaf, ui_layout_tree *Tree);
internal void             UIComputeLayout           (ui_layout_tree *Tree, render_context *RenderContext);
