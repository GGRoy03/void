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

// [FORWARD DECLARATIONS]

typedef struct direct_glyph_table direct_glyph_table;

// [CORE TYPES]

// Batch types
// A batch is a linked list of raw byte data

typedef struct render_batch
{
    u8 *Memory;
    u64 ByteCount;
    u64 ByteCapacity;
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

    u64 BatchCount;
    u64 ByteCount;
    u64 BytesPerInstance;
} render_batch_list;

// Params Types
// Specific parameters that must be set by the rendering 
// backend before drawing the corresponding batch.

typedef struct rect_group_params
{
    vec2_f32      AtlasTextureSize;
    matrix_3x3    Transform;
    render_handle AtlasTextureView;
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
} render_pass_params_ui;

// Stats Types
// Used to track resource usage per-pass/per-type.

typedef struct render_pass_ui_stats
{
    u32 BatchCount;
    u32 GroupCount;
    u32 PassCount;
    u64 RenderedDataSize;
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

global render_state RenderState;

// [Globals]

read_only global u32 UIPassDefaultBatchCount       = 10;
read_only global u32 UIPassDefaultGroupCount       = 20;
read_only global u32 UIPassDefaultPassCount        = 5;
read_only global u32 UIPassDefaultRenderedDataSize = Kilobyte(50);
read_only global u32 UIPassDefaultPaddingSize      = Kilobyte(25);

read_only global u64 RenderPassDataSizeTable[] =
{
    80, // Inputs to UI pass (ui_rect)
};

// [FORWARD DECLARATIONS]

typedef struct gpu_font_objects gpu_font_objects;

// [Handles]

internal b32           IsValidRenderHandle    (render_handle Handle);
internal render_handle RenderHandle           (u64 Handle);
internal b32           RenderHandleMatches    (render_handle H1, render_handle H2);

// [Batches]

internal void        * PushDataInBatchList  (memory_arena *Arena, render_batch_list *BatchList);
internal render_pass * GetRenderPass        (memory_arena *Arena, RenderPass_Type Type);
internal b32           CanMergeGroupParams  (rect_group_params *Old, rect_group_params *New);

// [PER-RENDERER API]

internal render_handle InitializeRenderer    (memory_arena *Arena);
internal void          SubmitRenderCommands  (void);

// [Text]

internal b32  CreateGlyphCache      (render_handle BackendHandle, vec2_f32 Size, gpu_font_objects *FontObjects);
internal void ReleaseGlyphCache     (gpu_font_objects *FontObjects);
internal b32  CreateGlyphTransfer   (render_handle Backend, vec2_f32 Size, gpu_font_objects *FontObjects);
internal void ReleaseGlyphTransfer  (gpu_font_objects *FontObjects);
external void TransferGlyph         (rect_f32 Rect, render_handle RendererHandle, gpu_font_objects *FontObjects);
