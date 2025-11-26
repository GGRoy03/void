typedef enum UIUnit_Type
{
    UIUnit_None    = 0,
    UIUnit_Float32 = 1,
    UIUnit_Percent = 2,
    UIUnit_Auto    = 3,
} UIUnit_Type;

typedef enum UIDisplay_Type
{
    UIDisplay_Normal = 0,
    UIDisplay_None   = 1,
    UIDisplay_Flex   = 2,
} UIDisplay_Type;

typedef enum UIAxis_Type
{
    UIAxis_None = 0,
    UIAxis_X    = 1,
    UIAxis_Y    = 2,
    UIAxis_XY   = 3,
} UIAxis_Type;

typedef enum UIAlign_Type
{
    UIAlign_Start   = 0,
    UIAlign_Center  = 1,
    UIAlign_End     = 2,
    UIAlign_Stretch = 3,
    UIAlign_None    = 4,
} UIAlign_Type;

// Flex:
//   UIFlexDirection_Type:
//     Which axis is the main axis when placing flex items.
//     Will default to UIFlexDirection_Row if nothing is specified.
//
//   UIFlexJustify:
//
//   UIFlexAlign:

typedef enum UIFlexDirection_Type
{
    UIFlexDirection_Row    = 0,
    UIFlexDirection_Column = 1,
} UIFlexDirection_Type;

typedef enum UIJustifyContent_Type
{
    UIJustifyContent_Start        = 0,
    UIJustifyContent_Center       = 1,
    UIJustifyContent_End          = 2,
    UIJustifyContent_SpaceBetween = 3,
    UIJustifyContent_SpaceAround  = 4,
    UIJustifyContent_SpaceEvenly  = 5,
} UIJustifyContent_Type;

typedef enum UIAlignItems_Type
{
    UIAlignItems_None    = 0,
    UIAlignItems_Start   = 1,
    UIAlignItems_Center  = 2,
    UIAlignItems_End     = 3,
    UIAlignItems_Stretch = 4,
} UIAlignItems_Type;

// [FORWARD DECLARATIONS]

typedef struct ui_style         ui_style;
typedef struct ui_layout_node   ui_layout_node;
typedef struct ui_layout_tree   ui_layout_tree;
typedef struct ui_node_style    ui_node_style;
typedef struct ui_node_table ui_node_id_table;
typedef struct ui_pipeline      ui_pipeline;

typedef void ui_click_callback(ui_layout_node *Node, ui_pipeline *Pipeline);

// [CORE TYPES]

typedef struct ui_color
{
    float R;
    float G;
    float B;
    float A;
} ui_color;

typedef struct ui_corner_radius
{
    float TopLeft;
    float TopRight;
    float BotLeft;
    float BotRight;
} ui_corner_radius;

typedef struct ui_spacing
{
    float Horizontal;
    float Vertical;
} ui_spacing;

typedef struct ui_padding
{
    float Left;
    float Top;
    float Right;
    float Bot;
} ui_padding;

typedef struct ui_unit
{
    UIUnit_Type Type;
    union
    {
        float Float32;
        float Percent;
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
    rect_float         RectBounds;
    rect_float         TextureSource;
    ui_color         Color;
    ui_corner_radius CornerRadii;
    float              BorderWidth, Softness, SampleTexture, _P0; // Style Params
} ui_rect;

// ------------------------------------------------------------------------------------
// User Callbacks

typedef enum UIEvent_State
{
    UIEvent_Untouched = 0,
    UIEvent_Rejected  = 1,
    UIEvent_Handled   = 2,
} UIEvent_State;

typedef UIEvent_State (*ui_text_input_onchar)  (uint8_t Char, void *UserData);
typedef UIEvent_State (*ui_text_input_onkey)   (OSInputKey_Type Key, void *UserData);

// ui_node:
//  Main representation of a node in the UI (Button, Window, ...)
//  A node can be anything you want. Nodes are only valid for a single frame, do
//  not keep them in memory.

struct ui_node
{
    bool CanUse;
    uint64_t  IndexInTree;
    uint64_t  SubtreeId;

    // Style
    void SetStyle      (uint32_t Style);
    void SetSize       (vec2_unit Size);
    void SetDisplay    (UIDisplay_Type Type);
    void SetColor      (ui_color Color);
    void SetTextColor  (ui_color Color);

    // Layout
    ui_node* FindChild        (uint32_t Index);
    void     AppendChild      (ui_node *Child);
    void     ReserveChildren  (uint32_t Amount);

    // Resource
    void SetText         (byte_string Text);
    void SetTextInput    (uint8_t *Buffer, uint64_t BufferSize);
    void SetScroll       (float ScrollSpeed, UIAxis_Type Axis);
    void SetImage        (byte_string Path, byte_string Group);
    void ClearTextInput  (void);

    // Callbacks
    void ListenOnKey        (ui_text_input_onkey Callback, void *UserData);

    // Debug
    void DebugBox           (uint32_t Flag, bool Draw);

    // Misc
    void SetId              (byte_string Id, ui_node_table *Table);
};


static ui_node * UICreateNode  (uint32_t Flags, bool IsFrameNode);

// -----------------------------------------------------------------------------------
// Image API

static void UICreateImageGroup  (byte_string Name, int Width, int Height);

// -----------------------------------------------------------------------------------
// NodeIdTable_Size:
//  NodeIdTable_128Bits is the default size for this table which uses 128 SIMD to find/insert in the table
//
// ui_node_table_params
//  GroupSize : How many values per "groups" this must be one of NodeIDTableSize
//  GroupCount: How many groups the table contains, this must be a power of two (VOID_ASSERTed in PlaceNodeIdTableInMemory)
//  This table never resizes and the amount of slots must acount for the worst case scenario.
//  Number of slots is computed by GroupSize * GroupCount.
//
// GetNodeIdTableFootprint:
//   Return the number of bytes required to store a node-id table for `Params`.
//   The caller must allocate at least this many bytes (aligned for ui_node_id_entry/ui_node_table)
//   before calling PlaceNodeIdTableInMemory.
//
// PlaceNodeIdTableInMemory:
//   Initialize a ui_node_table inside the caller-supplied Memory block and return a pointer
//   to the placed ui_node_table. Does NOT allocate memory. If Memory == NULL the function
//   returns NULL, thus caller must only check that the returned memory is non-null.
//   Caller owns the memory and is responsible for managing it.
// -------------------------------------------------------------------------------------------------------------------

// NOTE:
// This could be a user managed attachment? Because not every subtree needs this.

typedef enum NodeIdTable_Size
{
    NodeIdTable_128Bits = 16,
} NodeIdTable_Size;

typedef struct ui_node_table_params
{
    NodeIdTable_Size GroupSize;
    uint64_t              GroupCount;
} ui_node_table_params;

static uint64_t             UIGetNodeTableFootprint   (ui_node_table_params Params);
static ui_node_table * UIPlaceNodeTableInMemory  (ui_node_table_params Params, void *Memory);
static ui_node       * UIFindNodeById            (byte_string Id, ui_node_table *Table);

#include <immintrin.h>

typedef struct ui_resource_table ui_resource_table;
typedef struct ui_text ui_text;

typedef enum UIResource_Type
{
    UIResource_None         = 0,
    UIResource_Text         = 1,
    UIResource_TextInput    = 2,
    UIResource_ScrollRegion = 3,
    UIResource_Image        = 4,
    UIResource_ImageGroup   = 5,
} UIResource_Type;

typedef struct ui_resource_key
{
    __m128i Value;
} ui_resource_key;

typedef struct ui_resource_stats
{
    uint64_t CacheHitCount;
    uint64_t CacheMissCount;
} ui_resource_stats;

typedef struct ui_resource_table_params
{
    uint32_t HashSlotCount;
    uint32_t EntryCount;
} ui_resource_table_params;

typedef struct ui_resource_state
{
    uint32_t             Id;
    UIResource_Type ResourceType;
    void           *Resource;
} ui_resource_state;

static uint64_t                 GetResourceTableFootprint   (ui_resource_table_params Params);
static ui_resource_table * PlaceResourceTableInMemory  (ui_resource_table_params Params, void *Memory);

// Keys:
//   Opaque handles to resources. Use a resource table to retrieve the associated data
//   MakeResourceKey is used for node-based resources (Text, Scroll Region, Images)
//   MakeGlobalResourceKey is used for node-less resources (Image Group, ...)

typedef struct ui_subtree ui_subtree;

static ui_resource_key MakeResourceKey       (UIResource_Type Type, uint32_t NodeIndex, ui_subtree *Subtree);
static ui_resource_key MakeGlobalResourceKey (UIResource_Type Type, byte_string Name);

// Resources:
//   Use FindResourceByKey to retrieve some resource with a key created from MakeResourceKey.
//   If the resource doesn't exist yet, the returned state will contain: .ResourceType = UIResource_None AND .Resource = NULL.
//   You may update the table using UpdateResourceTable by passing the relevant updated data. The id is retrieved in State.Id.

static ui_resource_state FindResourceByKey     (ui_resource_key Key, ui_resource_table *Table);
static void              UpdateResourceTable   (uint32_t Id, ui_resource_key Key, void *Memory, ui_resource_table *Table);

// Queries:
//   Queries both compute a key and retrieve the corresponding resource type.
//   When querying a resource it is excpected that the resource already exists and
//   is initialized with the requested type. On failure trigger an assertion.

static void * QueryNodeResource    (uint32_t NodeIndex, ui_subtree *Subtree, UIResource_Type Type, ui_resource_table *Table);
static void * QueryGlobalResource  (byte_string Name, UIResource_Type Type, ui_resource_table *Table);

// ------------------------------------------------------------------------------------

typedef struct ui_style_registry ui_style_registry;
typedef struct ui_font ui_font;

typedef enum UIIntent_Type
{
    UIIntent_None          = 0,
    UIIntent_Drag          = 1,
    UIIntent_ResizeX       = 2,
    UIIntent_ResizeY       = 3,
    UIIntent_ResizeXY      = 4,
    UIIntent_EditTextInput = 5,
} UIIntent_Type;

typedef struct ui_font_list
{
    ui_font *First;
    ui_font *Last;
    uint32_t      Count;
} ui_font_list;

typedef struct ui_pipeline_buffer
{
    ui_pipeline *Values;
    uint32_t          Count;
    uint32_t          Size;
} ui_pipeline_buffer;

typedef struct ui_hovered_node
{
    uint32_t         Index;
    ui_subtree *Subtree;
} ui_hovered_node;

typedef struct ui_focused_node
{
    uint32_t           Index;
    ui_subtree   *Subtree;
    bool           IsTextInput;
    UIIntent_Type Intent;
} ui_focused_node;

typedef struct ui_state
{
    // Resources
    ui_resource_table  *ResourceTable;
    ui_pipeline_buffer  Pipelines;

    // internal State
    ui_hovered_node Hovered;
    ui_focused_node Focused;

    console_queue Console;

    // NOTE: What about this?
    ui_font_list     Fonts;
    memory_arena    *StaticData;

    // State
    ui_pipeline *CurrentPipeline;
    vec2_int     WindowSize;
} ui_state;

static ui_state UIState;

static void UIBeginFrame               (vec2_int WindowSize);
static void UIEndFrame                 (void);

static void            UISetNodeHover  (uint32_t NodeIndex, ui_subtree *Subtree);
static bool             UIHasNodeHover  (void);
static ui_hovered_node UIGetNodeHover  (void);

static void            UISetNodeFocus  (uint32_t NodeIndex, ui_subtree *Subtree, bool IsTextInput, UIIntent_Type Intent);
static bool             UIHasNodeFocus  (void);
static ui_focused_node UIGetNodeFocus  (void);

// [Helpers]

static ui_color         UIColor            (float R, float G, float B, float A);
static ui_spacing       UISpacing          (float Horizontal, float Vertical);
static ui_padding       UIPadding          (float Left, float Top, float Right, float Bot);
static ui_corner_radius UICornerRadius     (float TopLeft, float TopRight, float BotLeft, float BotRight);
static vec2_unit        Vec2Unit           (ui_unit U0, ui_unit U1);
static bool              IsNormalizedColor  (ui_color Color);

// ------------------

typedef struct ui_event_node ui_event_node;
typedef struct ui_layout_node ui_layout_node;

typedef struct ui_event_list
{
    ui_event_node *First;
    ui_event_node *Last;
    uint32_t            Count;
} ui_event_list;

typedef struct ui_subtree_params
{
    bool CreateNew;
    uint64_t NodeCount;
} ui_subtree_params;

typedef struct ui_subtree
{
    // Persistent
    memory_arena    *Persistent;
    ui_layout_tree  *LayoutTree;
    ui_node_style   *ComputedStyles;

    // Semi-Transient
    ui_event_list Events;

    // Transient
    memory_arena *Transient;

    // Info
    uint64_t NodeCount;
    uint64_t Id;

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
    uint32_t              Count;
} ui_subtree_list;

typedef struct ui_pipeline_params
{
    byte_string ThemeFile;
} ui_pipeline_params;

typedef struct ui_pipeline
{
    // User State (WIP)
    void *VShader;
    void *PShader;

    // WIP
    ui_style_registry *Registry;         // NOTE: Linked list this.
    uint64_t                NextSubtreeId;
    ui_subtree        *CurrentSubtree;
    ui_subtree_list    Subtrees;
    memory_arena      *internalArena;
} ui_pipeline;

#define UISubtree(Params) DeferLoop(UIBeginSubtree(Params), UIEndSubtree(Params))

static void          UIBeginSubtree       (ui_subtree_params Params);
static void          UIEndSubtree         (ui_subtree_params Params);

static ui_pipeline * UICreatePipeline      (ui_pipeline_params Params);
static void          UIBeginAllSubtrees    (ui_pipeline *Pipeline);
static void          UIExecuteAllSubtrees  (ui_pipeline *Pipeline);
// ----------------------------------------
