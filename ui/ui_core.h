// [CONSTANTS]

typedef enum UILayoutBox_Flag
{
    UILayoutBox_NoFlag         = 0,
    UILayoutBox_FlowRow        = 1 << 0,
    UILayoutBox_FlowColumn     = 1 << 1,
    UILayoutBox_DrawBackground = 1 << 2,
    UILayoutBox_DrawBorders    = 1 << 3,
    UILayoutBox_HasClip        = 1 << 4,
} UILayoutBox_Flag;

typedef enum UITree_Type
{
    UITree_None   = 0,
    UITree_Style  = 1,
    UITree_Layout = 2,
} UITree_Type;

typedef enum UINode_Type
{
    UINode_None   = 0,
    UINode_Window = 1,
    UINode_Button = 2,
} UINode_Type;

// [CORE TYPES]

typedef struct ui_layout_box
{
    f32       ClientX;
    f32       ClientY;
    f32       Width;
    f32       Height;
    vec2_f32  Spacing;
    vec4_f32  Padding;
    bit_field Flags;
} ui_layout_box;

typedef struct ui_style
{
    vec4_f32 Color;
    vec4_f32 BorderColor;
    vec4_f32 CornerRadius;
    f32      Softness;
    u32      BorderWidth;
    vec2_f32 Size;
    vec2_f32 Spacing;
    vec4_f32 Padding;
} ui_style;

// [Node Types]
// Many Trees are used throughout the pipeline. Every type of node is held
// in the same tree structure for simplicity.

typedef struct ui_node
{
    u32   Id;
    void *Parent;
    void *First;
    void *Last;
    void *Next;
    void *Prev;

    UINode_Type Type;
    union
    {
        ui_style      Style;
        ui_layout_box Layout;
    };
} ui_node;

typedef struct ui_tree_params
{
    u32         Depth;     // Deepest node in the tree.
    u32         NodeCount; // How many nodes can it hold.
    UITree_Type Type;      // Type
} ui_tree_params;

typedef struct ui_tree
{
    memory_arena *Arena;
    UITree_Type   Type;

    u32      NodeCapacity;
    u32      NodeCount;
    ui_node *Nodes;

    u32       ParentTop;
    u32       MaximumDepth;
    ui_node **ParentStack;
} ui_tree;

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

typedef struct ui_pipeline_params
{
    byte_string   *ThemeFiles;
    u32            ThemeCount;
    
    u32 TreeDepth;
    u32 TreeNodeCount;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    ui_tree            StyleTree;
    ui_tree            LayoutTree;
    ui_style_registery StyleRegistery;
} ui_pipeline;

DEFINE_TYPED_QUEUE(Node, node, ui_node *);

// [Globals]

read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [API]

// [Components]

internal void UIWindow  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UIButton  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UILabel   (byte_string Text, ui_pipeline *Pipeline, ui_font *Font);

// [Style]

internal ui_cached_style * UIGetStyleSentinel            (byte_string Name, ui_style_registery *Registery);
internal ui_style_name     UIGetCachedNameFromStyleName  (byte_string Name, ui_style_registery *Registery);
internal ui_style          UIGetStyleFromCachedName      (ui_style_name Name, ui_style_registery *Registery);

// [Tree]

internal ui_tree  AllocateUITree     (ui_tree_params Params);

internal void      PopParentNodeFromTree  (ui_tree *Tree);
internal void      PushParentNodeInTree   (void *Node, ui_tree *Tree);
internal void    * GetParentNodeFromTree  (ui_tree *Tree);
internal ui_node * UIGetNextNode          (ui_tree *Tree, UINode_Type Type);

// [Pipeline]

internal b32  IsParallelUINode  (ui_node *Node1, ui_node *Node2);
internal b32  IsUINodeALeaf     (UINode_Type Type);

internal ui_pipeline UICreatePipeline           (ui_pipeline_params Params);
internal void        UIPipelineExecute          (ui_pipeline *Pipeline);
internal void        UIPipelineSynchronize      (ui_pipeline *Pipeline, ui_node *Root);
internal void        UIPipelineTopDownLayout    (ui_pipeline *Pipeline);
internal void        UIPipelineCollectDrawList  (ui_pipeline *Pipeline, render_pass *Pass, ui_node *Root);
