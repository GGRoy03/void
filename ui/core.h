// [CONSTANTS]

typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
    UIUnit_Auto    = 3,
} UIUnit_Type;

typedef enum UIIntent_Type
{
    UIIntent_None     = 0,
    UIIntent_Hover    = 1,
    UIIntent_ResizeX  = 2,
    UIIntent_ResizeY  = 3,
    UIIntent_ResizeXY = 4,
    UIIntent_Drag     = 5,
} UIIntent_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_style         ui_style;
typedef struct ui_layout_node   ui_layout_node;
typedef struct ui_layout_tree   ui_layout_tree;
typedef struct ui_node_style    ui_node_style;
typedef struct ui_node_id_table ui_node_id_table;
typedef struct ui_pipeline      ui_pipeline;

typedef void ui_click_callback(ui_layout_node *Node, ui_pipeline *Pipeline);

// [CORE TYPES]

typedef struct ui_color
{
    f32 R;
    f32 G;
    f32 B;
    f32 A;
} ui_color;

typedef struct ui_corner_radius
{
    f32 TopLeft;
    f32 TopRight;
    f32 BotLeft;
    f32 BotRight;
} ui_corner_radius;

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

typedef struct ui_unit
{
    UIUnit_Type Type;
    union
    {
        f32 Float32;
        f32 Percent;
    };
} ui_unit;

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
    rect_f32         TextureSource;
    ui_color         Color;
    ui_corner_radius CornerRadii;
    f32              BorderWidth, Softness, SampleTexture, _P0; // Style Params
} ui_rect;

// ui_node:
//  Main representation of a node in the UI (Button, Window, ...)

typedef struct ui_subtree ui_subtree;
typedef struct ui_node    ui_node;

typedef struct ui_node
{
    b32 CanUse;
    u32 IndexInTree;
    u32 SubtreeId;

    ui_subtree *ChainSubtree;

    // Runtime Properties
    ui_node (*SetTextColor)  (ui_color Color, ui_pipeline *Pipeline);

    // Hierarchy
    ui_node (*FindChild)     (u32 Index, ui_pipeline *Pipeline);

    // Identifiers
    ui_node (*SetId)         (byte_string Id, ui_pipeline *Pipeline);
} ui_node;

typedef struct ui_node_chain
{
    ui_node     Node;
    ui_subtree *Subtree;
} ui_node_chain;

internal void    BeginNodeChain  (ui_node Node, ui_pipeline *Pipeline);

internal ui_node UIGetLast       (ui_pipeline *Pipeline);

// ui_event:

typedef struct ui_layout_node ui_layout_node;

typedef enum UIEvent_Type
{
    UIEvent_Hover  = 1 << 0,
    UIEvent_Click  = 1 << 1,
    UIEvent_Resize = 1 << 2,
    UIEvent_Drag   = 1 << 3,
} UIEvent_Type;

typedef struct ui_hover_event
{
    ui_pipeline *Source;
    ui_node      Node;
    u32          CachedStyleIndex;
} ui_hover_event;

typedef struct ui_click_event
{
    ui_layout_tree *Tree;
    ui_node         Node;
} ui_click_event;

typedef struct ui_resize_event
{
    ui_layout_tree *Tree;
    ui_node         Node;
    vec2_f32        Delta;
} ui_resize_event;

typedef struct ui_drag_event
{
    ui_layout_tree *Tree;
    ui_node         Node;
    vec2_f32        Delta;
} ui_drag_event;

typedef struct ui_scroll_event
{
    ui_layout_tree *Tree;
    ui_node         Node;
    f32             Delta;
} ui_scroll_event;

typedef struct ui_event
{
    UIEvent_Type Type;
    union
    {
        ui_hover_event  Hover;
        ui_click_event  Click;
        ui_resize_event Resize;
        ui_drag_event   Drag;
        ui_scroll_event Scroll;
    };
} ui_event;

typedef struct ui_event_node ui_event_node;
struct ui_event_node
{
    ui_event_node *Next;
    ui_event       Value;
};

typedef struct ui_event_list
{
    ui_event_node *First;
    ui_event_node *Last;
    u32            Count;
} ui_event_list;

internal void RecordUIEvent       (ui_event Event, ui_event_list *Events, memory_arena *Arena);
internal void ProcessUIEventList  (ui_event_list *Events);

internal void RecordUIHoverEvent  (ui_node Node, ui_pipeline *Source, ui_event_list *Events, memory_arena *Arena);

// ------------------------------------------------------------------------------------

typedef struct ui_style_registry ui_style_registry;

typedef struct ui_pipeline_list
{
    ui_pipeline *First;
    ui_pipeline *Last;
    u32          Count;
} ui_pipeline_list;

typedef struct ui_font ui_font;
typedef struct ui_font_list
{
    ui_font *First;
    ui_font *Last;
    u32      Count;
} ui_font_list;

typedef struct ui_state
{
    // Resources
    ui_font_list     Fonts;
    ui_pipeline_list Pipelines;
    memory_arena    *StaticData;

    // State
    ui_pipeline    *CurrentPipeline;
    ui_layout_node *CapturedNode;

    // Systems
    console_queue Console;
} ui_state;

global ui_state UIState;

// [Helpers]

internal ui_color         UIColor            (f32 R, f32 G, f32 B, f32 A);
internal ui_spacing       UISpacing          (f32 Horizontal, f32 Vertical);
internal ui_padding       UIPadding          (f32 Left, f32 Top, f32 Right, f32 Bot);
internal ui_corner_radius UICornerRadius     (f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight);
internal vec2_unit        Vec2Unit           (ui_unit U0, ui_unit U1);
internal b32              IsNormalizedColor  (ui_color Color);

// ui_subtree & ui_subtree_node & ui_subtree_list:
//
// ui_pipeline_params & ui_pipeline:
//
// UIBeginPipeline:
//
// UICreatePipeline:

typedef struct ui_subtree_params
{
    b32 CreateNew;
    u64 LayoutNodeCount;
    u64 LayoutNodeDepth;
} ui_subtree_params;

typedef struct ui_subtree
{
    // Persistent
    u64             Id;
    ui_layout_tree *LayoutTree;
    ui_node_style  *ComputedStyles;

    // State
    ui_node LastNode;
} ui_subtree;

typedef struct ui_subtree_node ui_subtree_node;
struct ui_subtree_node
{
    ui_subtree_node *Next;
    ui_subtree       Value;
};

typedef struct ui_subtree_list
{
    ui_subtree_node *First;
    ui_subtree_node *Last;
    u32              Count;
} ui_subtree_list;

typedef struct ui_pipeline_params
{
    byte_string ThemeFile;
} ui_pipeline_params;

typedef struct ui_pipeline ui_pipeline;
struct ui_pipeline
{
    ui_pipeline *Next;

    // State
    ui_style_registry *Registry;
    ui_node_id_table  *IdTable;     // NOTE: Should this be stored on the tree?

    // Frame/State
    ui_event_list Events;
    ui_node_chain Chain;

    // Subtree
    u64             NextSubtreeId;
    ui_subtree     *CurrentSubtree;
    ui_subtree_list Subtrees;

    // Memory
    memory_arena *StaticArena;
    memory_arena *FrameArena;
};

#define UISubtree(Params, Pipeline) DeferLoop(UIBeginSubtree(Params, Pipeline), UIEndSubtree(Params));

internal void          UIBeginSubtree       (ui_subtree_params Params, ui_pipeline *Pipeline);
internal void          UIEndSubtree         (ui_subtree_params Params);

internal void          UIBeginPipeline       (ui_pipeline *Pipeline);
internal ui_pipeline * UICreatePipeline      (ui_pipeline_params Params);
internal void          UIExecuteAllSubtrees  (ui_pipeline *Pipeline);

internal ui_subtree  * FindSubtree           (ui_node Node, ui_pipeline *Pipeline);

// ----------------------------------------
