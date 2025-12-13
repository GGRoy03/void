// [CONSTANTS]

typedef enum Renderer_Backend
{
    Renderer_None  = 0,
    Renderer_D3D11 = 1,
} Renderer_Backend;

typedef enum RenderPass_Type
{
    RenderPass_UI = 0,

    RenderPass_Count = 1,
} RenderPass_Type;

typedef struct render_handle
{
    uint64_t Value[1];
} render_handle;


// Batch types
// A batch is a linked list of raw byte data

typedef struct render_batch
{
    uint8_t *Memory;
    uint64_t ByteCount;
    uint64_t ByteCapacity;
} render_batch;

typedef struct render_batch_node render_batch_node;
struct render_batch_node
{
    render_batch_node *Next;
    render_batch       Value;
};

typedef struct render_batch_list
{
    render_batch_node *First;
    render_batch_node *Last;

    uint64_t BatchCount;
    uint64_t ByteCount;
    uint64_t BytesPerInstance;
} render_batch_list;

// Params Types
// Specific parameters that must be set by the rendering 
// backend before drawing the corresponding batch.

typedef struct rect_group_params
{
    vec2_uint16   TextureSize;
    render_handle Texture;
    matrix_3x3    Transform;
    rect_float    Clip;
} rect_group_params;

// Group Types
// Group are logical grouping of batches as well as specific
// parameters that must be set by the rendering backend before
// drawing those batches.

typedef struct rect_group_node rect_group_node;
struct rect_group_node
{
    rect_group_node  *Next;
    render_batch_list BatchList;
    rect_group_params Params;
};

// Params Types
// Used to batch groups of data within a pass.

typedef struct render_pass_params_ui
{
    rect_group_node *First;
    rect_group_node *Last;
    uint32_t              Count;
} render_pass_params_ui;

// Stats Types
// Used to track resource usage per-pass/per-type.

typedef struct render_pass_ui_stats
{
    uint32_t BatchCount;
    uint32_t GroupCount;
    uint32_t PassCount;
    uint64_t RenderedDataSize;
} render_pass_ui_stats;

// Render Pass Types
// The game contains multiple render passes (UI, 3D Geometry, ...). Each is batched
// and has a set of allocated memory (which they can overflow). Implemented as a linked
// list.

typedef struct render_pass
{
    RenderPass_Type Type;
    union
    {
        struct
        {
            render_pass_params_ui Params;
            render_pass_ui_stats  Stats;
        } UI;
    } Params;
} render_pass;

typedef struct render_pass_node render_pass_node;
struct render_pass_node
{
    render_pass_node *Next;
    render_pass       Value;
};

typedef struct render_pass_list
{
    render_pass_node *First;
    render_pass_node *Last;
} render_pass_list;

// One of the three globals (GAME, UI, RENDERER)

typedef struct render_state
{
    render_handle    Renderer;
    render_pass_list PassList;
} render_state;

static render_state RenderState;

// [Globals]

const static uint64_t RenderPassDataSizeTable[] =
{
    128, // Inputs to UI pass (ui_rect)
};

// [Handles]

static bool          IsValidRenderHandle    (render_handle Handle);
static render_handle RenderHandle           (uint64_t Handle);
static bool          RenderHandleMatches    (render_handle H1, render_handle H2);

// [Batches]

static void        * PushDataInBatchList      (memory_arena *Arena, render_batch_list *BatchList);
static render_pass * GetRenderPass            (memory_arena *Arena, RenderPass_Type Type);
static bool          CanMergeRectGroupParams  (rect_group_params *Old, rect_group_params *New);

// [PER-RENDERER API]

static render_handle InitializeRenderer    (void *HWindow, vec2_int Resolution, memory_arena *Arena);
static void          SubmitRenderCommands  (render_handle HRenderer, vec2_int Resolution, render_pass_list *RenderPassList);

// ------------------------------------------------------------------------------------
// @Internal : Textures

enum class RenderTexture
{
    None = 0,
    RGBA = 1,
};

static render_handle CreateRenderTexture      (uint16_t SizeX, uint16_t SizeY, RenderTexture Type);
static render_handle CreateRenderTextureView  (render_handle TextureHandle, RenderTexture Type);
