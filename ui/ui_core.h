// [CONSTANTS]

typedef enum UIStyleNode_Flag
{
    UIStyleNode_NoFlag       = 0,
    UIStyleNode_DirtySubtree = 1 << 0,
    UIStyleNode_DirtySpine   = 1 << 1,
} UIStyleNode_Flag;

typedef enum UILayoutBox_Flag
{
    UILayoutBox_NoFlag                  = 0,
    UILayoutBox_PlaceChildrenVertically = 1 << 0,
    UILayoutBox_DrawBackground          = 1 << 1,
    UILayoutBox_DrawBorders             = 1 << 2,
    UILayoutBox_DrawText                = 1 << 3,
    UILayoutBox_HasClip                 = 1 << 4,
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
    UINode_Label  = 3,
} UINode_Type;

// [TYPED MACROS]

typedef struct ui_node ui_node;
typedef struct ui_font ui_font;
DEFINE_TYPED_QUEUE(Node, node, ui_node *);

// [CORE TYPES]

typedef struct ui_character
{
    os_glyph_layout Layout;
    rect_f32        SampleSource;
    rect_f32        Position;
} ui_character;

typedef struct ui_text
{
    f32           LineHeight;
    vec2_f32      AtlasTextureSize;
    render_handle AtlasTexture;
    ui_character *Characters;
    u32           Size;
} ui_text;

typedef struct ui_layout_box
{
    // Output
    f32 ClientX;
    f32 ClientY;

    // Layout Info
    f32       Width;
    f32       Height;
    vec2_f32  Spacing;
    vec4_f32  Padding;

    // Params
    rect_f32   Clip;
    matrix_3x3 Transform;

    // Extra-Draw
    ui_text *Text;

    // Misc
    bit_field Flags;
} ui_layout_box;

typedef struct ui_style
{
    // Color
    vec4_f32 Color;
    f32      Opacity;

    // Layout
    vec4_f32 Padding;
    vec2_f32 Size;
    vec2_f32 Spacing;

    // Borders/Corners
    vec4_f32 BorderColor;
    vec4_f32 CornerRadius;
    f32      Softness;
    u32      BorderWidth;

    // Font
    f32 FontSize;
    union
    {
        ui_font    *Ref;
        byte_string Name;
    } Font;

    // Misc
    bit_field Flags;
} ui_style;

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
    UITree_Type Type;
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
    byte_string  *ThemeFiles;
    u32           ThemeCount;
    u32           TreeDepth;
    u32           TreeNodeCount;
    u32           FontCount;
    render_handle RendererHandle;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    ui_tree            StyleTree;
    ui_tree            LayoutTree;
    ui_style_registery StyleRegistery;

    // Handles
    render_handle RendererHandle;

    // TODO: Make something that is able to create a static arena inside an arena.
    memory_arena *FrameArena;
    memory_arena *StaticArena;
} ui_pipeline;

// [Globals]

read_only global u32 InvalidNodeId = 0xFFFFFFFF;

read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [API]

// [Components]

internal void UIWindow  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UIButton  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UILabel   (ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline);

// [Style]

internal ui_cached_style * UIGetStyleSentinel            (byte_string   Name, ui_style_registery *Registery);
internal ui_style_name     UIGetCachedNameFromStyleName  (byte_string   Name, ui_style_registery *Registery);
internal ui_style          UIGetStyleFromCachedName      (ui_style_name Name, ui_style_registery *Registery);

// [Tree]

internal ui_tree   UIAllocateTree                (ui_tree_params Params);
internal void      UIPopParentNodeFromTree       (ui_tree *Tree);
internal void      UIPushParentNodeInTree        (void *Node, ui_tree *Tree);
internal void    * UIGetParentNodeFromTree       (ui_tree *Tree);
internal ui_node * UIGetNextNode                 (ui_tree *Tree, UINode_Type Type);
internal ui_node * UIGetLayoutNodeFromStyleNode  (ui_node *Node, ui_pipeline *Pipeline);

// [Bindings]

internal void UISetTextBinding  (ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_node *Node);
internal void UISetFlagBinding  (ui_node *Node, b32 Set, UILayoutBox_Flag Flag, ui_pipeline *Pipeline);

// [Pipeline]

internal b32         UIIsParallelNode         (ui_node *Node1, ui_node *Node2);
internal b32         UIIsNodeALeaf            (UINode_Type Type);
internal void        UILinkNodes              (ui_node *Node, ui_node *Parent);

internal ui_pipeline UICreatePipeline         (ui_pipeline_params Params);
internal void        UIPipelineBegin          (ui_pipeline *Pipeline);
internal void        UIPipelineExecute        (ui_pipeline *Pipeline, render_pass_list *PassList);

internal void      UIPipelineSynchronize    (ui_pipeline *Pipeline, ui_node *Root);
internal void      UIPipelineTopDownLayout  (ui_pipeline *Pipeline);
internal ui_node * UIPipelineHitTest        (ui_pipeline *Pipeline, vec2_f32 MousePosition, ui_node *LRoot);
internal void      UIPipelineBuildDrawList  (ui_pipeline *Pipeline, render_pass *Pass, ui_node *SRoot, ui_node *LRoot);
