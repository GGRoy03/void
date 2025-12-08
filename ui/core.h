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

// [FORWARD DECLARATIONS]

typedef struct ui_layout_node   ui_layout_node;
typedef struct ui_layout_tree   ui_layout_tree;
typedef struct ui_node_table ui_node_id_table;
typedef struct ui_pipeline      ui_pipeline;

typedef void ui_click_callback(ui_layout_node *Node, ui_pipeline *Pipeline);

// [CORE TYPES]

struct ui_color
{
    float R;
    float G;
    float B;
    float A;
};

typedef struct ui_corner_radius
{
    float TL;
    float TR;
    float BR;
    float BL;
} ui_corner_radius;

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
    uint32_t Index;

    // Style
    void     SetStyle         (uint32_t Style, ui_pipeline &Pipeline);

    // Layout
    ui_node  Find             (uint32_t Index , ui_pipeline &Pipeline);
    void     Append           (ui_node  Child , ui_pipeline &Pipeline);
    void     Reserve          (uint32_t Amount, ui_pipeline &Pipeline);

    // Resource
    void     SetText          (byte_string Text, ui_pipeline &Pipeline);
    void     SetTextInput     (uint8_t *Buffer, uint64_t BufferSize, ui_pipeline &Pipeline);
    void     SetScroll        (float ScrollSpeed, UIAxis_Type Axis, ui_pipeline &Pipeline);
    void     SetImage         (byte_string Path, byte_string Group, ui_pipeline &Pipeline);

    // Debug
    void     DebugBox         (uint32_t Flag, bool Draw, ui_pipeline &Pipeline);

    // Misc
    void     SetId            (byte_string Id, ui_pipeline &Pipeline);
};

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

typedef enum NodeIdTable_Size
{
    NodeIdTable_128Bits = 16,
} NodeIdTable_Size;

typedef struct ui_node_table_params
{
    NodeIdTable_Size GroupSize;
    uint64_t         GroupCount;
} ui_node_table_params;

// NOTE: I believe most of this code can be hidden. Is it not meant to be used by the user.

static uint64_t        UIGetNodeTableFootprint   (ui_node_table_params Params);
static ui_node_table * UIPlaceNodeTableInMemory  (ui_node_table_params Params, void *Memory);
static ui_node         UIFindNodeById            (byte_string Id, ui_node_table *Table);

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
    uint32_t        Id;
    UIResource_Type ResourceType;
    void           *Resource;
} ui_resource_state;

static uint64_t            GetResourceTableFootprint   (ui_resource_table_params Params);
static ui_resource_table * PlaceResourceTableInMemory  (ui_resource_table_params Params, void *Memory);

// Keys:
//   Opaque handles to resources. Use a resource table to retrieve the associated data
//   MakeNodeResourceKey   is used for node-based resources
//   MakeGlobalResourceKey is used for node-less  resources

static bool            IsValidResourceKey    (ui_resource_key Key);
static ui_resource_key MakeNodeResourceKey   (UIResource_Type Type, uint32_t NodeIndex, ui_layout_tree *Tree);
static ui_resource_key MakeGlobalResourceKey (UIResource_Type Type, byte_string Name);

// Resources:
//   Use FindResourceByKey to retrieve some resource with a key created from MakeResourceKey.
//   If the resource doesn't exist yet, the returned state will contain: .ResourceType = UIResource_None AND .Resource = NULL.
//   You may update the table using UpdateResourceTable by passing the relevant updated data. The id is retrieved in State.Id.

static ui_resource_state FindResourceByKey     (ui_resource_key Key, ui_resource_table *Table);
static void              UpdateResourceTable   (uint32_t Id, ui_resource_key Key, void *Memory, ui_resource_table *Table);

// Queries:
//   Queries both compute a key and retrieve the corresponding resource type.
//   When querying a resource it is expected that the resource already exists and
//   is initialized with the requested type. On failure trigger an assertion.

static void * QueryNodeResource    (uint32_t NodeIndex, ui_layout_tree *Tree, UIResource_Type Type, ui_resource_table *Table);
static void * QueryGlobalResource  (byte_string Name, UIResource_Type Type, ui_resource_table *Table);

// -----------------------------------------------------------------------------------

static void UIBeginFrame  (vec2_int WindowSize);
static void UIEndFrame    (void);

// -----------------------------------------------------------------------------------

struct ui_cached_style;

enum class UIPipeline : uint32_t
{
    Default = 0,
    Count   = 1,
};

constexpr uint32_t PipelineCount = static_cast<uint32_t>(UIPipeline::Count);

struct ui_pipeline_params
{
    byte_string      VtxShaderByteCode;
    byte_string      PxlShaderByteCode;

    uint64_t         NodeCount;
    uint64_t         FrameBudget;

    UIPipeline       Pipeline;
    ui_cached_style *StyleArray;
    uint32_t         StyleIndexMin;
    uint32_t         StyleIndexMax;
};

struct ui_pipeline
{
    // Render State
    render_handle        VtxShader;
    render_handle        PxlShader;

    // UI State
    ui_layout_tree      *Tree;
    ui_node_table       *NodeTable;

    // User State
    ui_cached_style     *StyleArray;
    uint32_t             StyleIndexMin;
    uint32_t             StyleIndexMax;

    // Memory
    memory_arena *StateArena;
    memory_arena *FrameArena;

    // Misc
    uint32_t ZIndex;
    bool     Bound;
    uint64_t NodeCount;
};


static void               UICreatePipeline            (const ui_pipeline_params &Params);
static ui_pipeline&       UIBindPipeline              (UIPipeline Pipeline);
static void               UIUnbindPipeline            (UIPipeline Pipeline);
static ui_pipeline_params UIGetDefaultPipelineParams  (void);

// ----------------------------------------

// NOTE:
// Only here, because of the D3D11 debug layer bug? Really?
// Because it could fit in the global resource pattern as far as I understand?

struct ui_font;
typedef struct ui_font_list
{
    ui_font *First;
    ui_font *Last;
    uint32_t Count;
} ui_font_list;

struct void_context
{
    // Memory
    memory_arena      *StateArena;

    // State
    ui_resource_table *ResourceTable;
    ui_pipeline        PipelineArray[PipelineCount];
    uint32_t           PipelineCount;

    ui_font_list     Fonts; // TODO: Find a solution such that this is a global resource.

    // State
    vec2_int   WindowSize;
};

static void_context GlobalVoidContext;

static void_context & GetVoidContext     (void);
static ui_pipeline  & GetBoundPipeline   (void);
static void           CreateVoidContext  (void);
