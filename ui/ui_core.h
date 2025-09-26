// [CONSTANTS]

typedef enum UIStyleNode_Flag
{
    UIStyleNode_NoFlag            = 0,
    UIStyleNode_HasSize           = 1 << 0,
    UIStyleNode_HasColor          = 1 << 1,
    UIStyleNode_HasPadding        = 1 << 2,
    UIStyleNode_HasSpacing        = 1 << 3,
    UIStyleNode_HasFontName       = 1 << 4,
    UIStyleNode_HasFontSize       = 1 << 5,
    UIStyleNode_HasSoftness       = 1 << 6,
    UIStyleNode_HasBorderColor    = 1 << 7,
    UIStyleNode_HasBorderWidth    = 1 << 8,
    UIStyleNode_HasCornerRadius   = 1 << 9,
    UIStyleNode_DirtySubtree      = 1 << 10,
    UIStyleNode_DirtySpine        = 1 << 11,
    UIStyleNode_StyleSetAtRuntime = 1 << 12,
} UIStyleNode_Flag;

typedef enum UILayoutNode_Flag
{
    UILayoutNode_NoFlag                  = 0,
    UILayoutNode_PlaceChildrenVertically = 1 << 0,
    UILayoutNode_DrawBackground          = 1 << 1,
    UILayoutNode_DrawBorders             = 1 << 2,
    UILayoutNode_DrawText                = 1 << 3,
    UILayoutNode_HasClip                 = 1 << 4,
    UILayoutNode_IsHovered               = 1 << 5,
    UILayoutNode_IsClicked               = 1 << 6,
    UILayoutNode_IsDraggable             = 1 << 7,
} UILayoutNode_Flag;

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
    UINode_Header = 4,
} UINode_Type;

typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
} UIUnit_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_node      ui_node;
typedef struct ui_font      ui_font;
typedef struct ui_text      ui_text;
typedef struct ui_style     ui_style;
typedef struct ui_character ui_character;
typedef struct ui_pipeline  ui_pipeline;

// [CORE TYPES]

DEFINE_TYPED_QUEUE(Node, node, ui_node *);

// [Callbacks Types]

typedef void ui_click_callback(ui_node *Node, ui_pipeline *Pipeline);

typedef struct ui_unit
{
    UIUnit_Type Type;
    union
    {
        f32 Float32;
        f32 Percent;
    };
} ui_unit;

typedef struct ui_color
{
    f32 R;
    f32 G;
    f32 B;
    f32 A;
} ui_color;

typedef struct ui_spacing
{
    f32 Horizontal;
    f32 Vertical;
} ui_spacing;

typedef struct ui_padding
{
    f32 Left;
    f32 Top;
    f32 Right;
    f32 Bot;
} ui_padding;

typedef struct ui_corner_radius
{
    f32 TopLeft;
    f32 TopRight;
    f32 BotLeft;
    f32 BotRight;
} ui_corner_radius;

typedef struct vec2_unit
{
    union
    {
        struct { ui_unit X; ui_unit Y; };
        ui_unit Values[2];
    };
} vec2_unit;

typedef struct vec4_unit
{
    union
    {
        struct { ui_unit X; ui_unit Y; ui_unit Z; ui_unit W; };
        ui_unit Values[4];
    };
} vec4_unit;

// NOTE: Must be padded to 16 bytes alignment.
typedef struct ui_rect
{
    rect_f32         RectBounds;
    rect_f32         AtlasSampleSource;
    ui_color         Color;
    ui_corner_radius CornerRadii;
    f32              BorderWidth, Softness, SampleAtlas, _P0; // Style Params
} ui_rect;

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
    rect_f32   Clip;
    matrix_3x3 Transform;

    // Misc
    bit_field Flags;

    // Misc (Should text be callback based?)
    ui_text           *Text;
    ui_click_callback *ClickCallback;
} ui_layout_box;

typedef struct ui_style
{
    // Color
    ui_color Color;
    f32      Opacity;

    // Layout
    vec2_unit  Size;
    ui_padding Padding;
    ui_spacing Spacing;

    // Borders/Corners
    ui_color         BorderColor;
    ui_corner_radius CornerRadius;
    f32              Softness;
    f32              BorderWidth;

    // Font
    f32 FontSize;
    union
    {
        ui_font    *Ref;
        byte_string Name;
    } Font;

    // Effects
    ui_style *ClickOverride;
    ui_style *HoverOverride;

    // Misc
    u32       Version;
    bit_field Flags;
} ui_style;

struct ui_node
{
    u32      Id;
    ui_node *Parent;
    ui_node *First;
    ui_node *Last;
    ui_node *Next;
    ui_node *Prev;

    UINode_Type Type;
    union
    {
        ui_style      Style;
        ui_layout_box Layout;
    };
};

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

// [Pipeline Types]

typedef struct ui_pipeline_params
{
    byte_string  *ThemeFiles;
    u32           ThemeCount;
    u32           TreeDepth;
    u32           TreeNodeCount;
    render_handle RendererHandle;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    ui_tree            StyleTree;
    ui_tree            LayoutTree;
    ui_style_registery StyleRegistery;

    // State
    ui_node *CurrentDragCaptureNode;

    // Handles
    render_handle RendererHandle;

    // TODO: Make something that is able to create a static arena inside an arena.
    memory_arena *FrameArena;
    memory_arena *StaticArena;
} ui_pipeline;

// [State]

typedef struct ui_state
{
    ui_font *First;
    ui_font *Last;
    u32      FontCount;

    memory_arena *StaticData;
} ui_state;

// [GLOBALS]

read_only global u32 InvalidNodeId = 0xFFFFFFFF;

read_only global u32 LayoutTreeDefaultCapacity = 500;
read_only global u32 LayoutTreeDefaultDepth    = 16;

// [API]

// [Helpers]

internal ui_color         UIColor         (f32 R, f32 G, f32 B, f32 A);
internal ui_spacing       UISpacing       (f32 Horizontal, f32 Vertical);
internal ui_padding       UIPadding       (f32 Left, f32 Top, f32 Right, f32 Bot);
internal ui_corner_radius UICornerRadius  (f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight);
internal vec2_unit        Vec2Unit        (ui_unit U0, ui_unit U1);

internal b32 IsNormalizedColor(ui_color Color);

// [Components]

internal void UIWindow  (ui_style_name StyleName, ui_pipeline *Pipeline);
internal void UIButton  (ui_style_name StyleName, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void UILabel   (ui_style_name StyleName, byte_string Text, ui_pipeline *Pipeline);
internal void UIHeader  (ui_style_name StyleName, ui_pipeline *Pipeline);

#define UIHeaderEx(StyleName, Pipeline) DeferLoop(UIHeader(StyleName, Pipeline), UIPopParentNode(Pipeline.StyleTree))

// [Style]

internal ui_cached_style * UIGetStyleSentinel  (byte_string   Name, ui_style_registery *Registery);
internal ui_style_name     UIGetCachedName     (byte_string   Name, ui_style_registery *Registery);
internal ui_style          UIGetStyle          (ui_style_name Name, ui_style_registery *Registery);

internal void UISetColor(ui_node *Node, ui_color Color);

// [Tree]

internal ui_tree   UIAllocateTree                (ui_tree_params Params);
internal void      UIPopParentNode               (ui_tree *Tree);
internal void      UIPushParentNode              (void *Node, ui_tree *Tree);
internal ui_node * UIGetParentNode               (ui_tree *Tree);
internal ui_node * UIGetNextNode                 (ui_tree *Tree, UINode_Type Type);
internal ui_node * UIGetLayoutNodeFromStyleNode  (ui_node *Node, ui_pipeline *Pipeline);
internal ui_node * UIGetStyleNodeFromLayoutNode  (ui_node *Node, ui_pipeline *Pipeline);

// [Bindings]

internal void UISetTextBinding           (ui_pipeline *Pipeline, ui_character *Characters, u32 Count, ui_font *Font, ui_node *Node);
internal void UISetFlagBinding           (ui_node *Node, b32 Set, UILayoutNode_Flag Flag, ui_pipeline *Pipeline);
internal void UISetClickCallbackBinding  (ui_node *Node, ui_click_callback *Callback, ui_pipeline *Pipeline);

// [Pipeline]

internal b32         UIIsParallelNode         (ui_node *Node1, ui_node *Node2);
internal b32         UIIsNodeALeaf            (UINode_Type Type);
internal void        UILinkNodes              (ui_node *Node, ui_node *Parent);

internal ui_pipeline UICreatePipeline         (ui_pipeline_params Params, ui_state *UIState);
internal void        UIPipelineBegin          (ui_pipeline *Pipeline);
internal void        UIPipelineExecute        (ui_pipeline *Pipeline, render_pass_list *PassList);

internal void      UIPipelineSynchronize    (ui_pipeline *Pipeline, ui_node *Root);
internal void      UIPipelineDragNodes      (vec2_f32 MouseDelta, ui_pipeline *Pipeline, ui_node *LRoot);
internal void      UIPipelineTopDownLayout  (ui_pipeline *Pipeline);
internal ui_node * UIPipelineHitTest        (ui_pipeline *Pipeline, vec2_f32 MousePosition, ui_node *LRoot);
internal void      UIPipelineBuildDrawList  (ui_pipeline *Pipeline, render_pass *Pass, ui_node *SRoot, ui_node *LRoot);
