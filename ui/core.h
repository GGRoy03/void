// [CONSTANTS]

typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
    UIUnit_Auto    = 3,
} UIUnit_Type;

typedef enum UIDisplay_Type
{
    UIDisplay_Normal = 0, // NOTE: Placeholder, so we get something that isn't None.
    UIDisplay_None   = 1,
} UIDisplay_Type;

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
} ui_node;

typedef struct ui_node_chain ui_node_chain;
struct ui_node_chain
{
    ui_node        Node;
    ui_subtree    *Subtree;
    ui_node_chain *Prev;

    // Style
    ui_node_chain * (*SetDisplay)       (UIDisplay_Type Display);
    ui_node_chain * (*SetTextColor)     (ui_color Color);
    ui_node_chain * (*SetStyle)         (u32 StyleIndex);

    // Layout
    ui_node_chain * (*FindChild)        (u32 Index);
    ui_node_chain * (*ReserveChildren)  (u32 Count);

    // Resource
    ui_node_chain * (*SetText)          (byte_string Text);

    // Misc
    ui_node_chain * (*SetId)            (byte_string Id);
};

internal ui_node_chain * UIChain  (ui_node Node);

// ------------------------------------------------------------------------------------

#include <immintrin.h>

typedef struct ui_resource_table ui_resource_table;
typedef struct ui_glyph_run      ui_glyph_run;

typedef enum UIResource_Type
{
    UIResource_None = 0,
    UIResource_Text = 1,
} UIResource_Type;

typedef struct ui_resource_key
{
    __m128i Value;
} ui_resource_key;

typedef struct ui_resource_stats
{
    u64 CacheHitCount;
    u64 CacheMissCount;
} ui_resource_stats;

typedef struct ui_resource_table_params
{
    u32 HashSlotCount;
    u32 EntryCount;
} ui_resource_table_params;

typedef struct ui_resource_state
{
    u32 Id;

    UIResource_Type Type;
    void            *Resource;
} ui_resource_state;

internal u64                 GetResourceTableFootprint   (ui_resource_table_params Params);
internal ui_resource_table * PlaceResourceTableInMemory  (ui_resource_table_params Params, void *Memory);

internal ui_resource_key  GetNodeResource   (u32 NodeId, ui_subtree *Subtree);
internal void             SetNodeResource   (u32 NodeIndex, ui_resource_key Key, ui_subtree *Subtree);
internal ui_glyph_run   * GetTextResource   (ui_resource_key Key, ui_resource_table *Table);

internal ui_resource_key   MakeTextResourceKey  (byte_string Text);
internal ui_resource_state FindResourceByKey    (ui_resource_key Key, ui_resource_table *Table);

internal void              UpdateTextResource   (u32 Id, byte_string Text, ui_font *Font, ui_resource_table *Table);

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
    ui_resource_table *ResourceTable;

    // NOTE: What about this?
    ui_font_list     Fonts;
    ui_pipeline_list Pipelines;

    memory_arena    *StaticData;

    // State
    ui_pipeline *CurrentPipeline;
    vec2_i32     WindowSize;

    // Systems
    console_queue Console;
} ui_state;

global ui_state UIState;

internal ui_pipeline * GetCurrentPipeline  ();

// [Helpers]

internal ui_color         UIColor            (f32 R, f32 G, f32 B, f32 A);
internal ui_spacing       UISpacing          (f32 Horizontal, f32 Vertical);
internal ui_padding       UIPadding          (f32 Left, f32 Top, f32 Right, f32 Bot);
internal ui_corner_radius UICornerRadius     (f32 TopLeft, f32 TopRight, f32 BotLeft, f32 BotRight);
internal vec2_unit        Vec2Unit           (ui_unit U0, ui_unit U1);
internal b32              IsNormalizedColor  (ui_color Color);

// ------------------

typedef struct ui_event_list ui_event_list;

typedef enum UIIntent_Type
{
    UIIntent_None     = 0,
    UIIntent_Drag     = 1,
    UIIntent_ResizeX  = 2,
    UIIntent_ResizeY  = 3,
    UIIntent_ResizeXY = 4,
} UIIntent_Type;

typedef struct ui_subtree_params
{
    b32 CreateNew;
    u64 NodeCount;
} ui_subtree_params;

typedef struct ui_subtree
{
    // Persistent
    u64              Id;
    ui_layout_tree  *LayoutTree;
    ui_node_style   *ComputedStyles;
    ui_resource_key *Resources;

    // Semi-Transient
    ui_layout_node  *CapturedNode;
    UIIntent_Type    Intent;

    // Transient
    memory_arena  *FrameData;
    ui_event_list *Events;

    // Info
    u64 NodeCount;

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
    ui_node_id_table  *IdTable;  // NOTE: Should this be stored on the tree? Think so

    // Frame/State
    ui_node_chain *Chain;

    // Subtree
    u64             NextSubtreeId;
    ui_subtree     *CurrentSubtree;
    ui_subtree_list Subtrees;

    // Memory
    memory_arena *StaticArena;
};

#define UISubtree(Params) DeferLoop(UIBeginSubtree(Params), UIEndSubtree(Params))

internal b32 IsValidSubtree  (ui_subtree *Subtree);

internal void          UIBeginSubtree       (ui_subtree_params Params);
internal void          UIEndSubtree         (ui_subtree_params Params);

internal ui_pipeline * UICreatePipeline      (ui_pipeline_params Params);
internal void          UIBeginAllSubtrees    (ui_pipeline *Pipeline);
internal void          UIExecuteAllSubtrees  (ui_pipeline *Pipeline);

internal ui_subtree  * FindSubtree           (ui_node Node, ui_pipeline *Pipeline);

// ----------------------------------------
