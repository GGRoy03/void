// [Constants]

typedef enum Renderer_Backend
{
    Renderer_None  = 0,
    Renderer_D3D11 = 1,
} Renderer_Backend;

typedef enum RenderPass_Type
{
    RenderPass_UI = 0,

    RenderPass_Type_Count = 1,
} RenderPass_Type;

// [Core Types]

typedef struct render_handle
{
    u64 u64[1];
} render_handle;

typedef struct shader_source
{
    read_only u8 *Data;
              u64 Size;
} shader_source;

// Batch types
// A batch is a linked list of data.
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

// Group Types
// Group are logical grouping of batches as well as specific
// parameters that must be set by the rendering backend before
// drawing those batches.
typedef struct render_rect_group_node render_rect_group_node;
struct render_rect_group_node
{
    render_rect_group_node *Next;
    render_batch_list       BatchList;

    mat3x3_f32 Transform;
};

// Params Types
// Used to batch groups of data within a pass.

typedef struct render_pass_params_ui
{
    render_rect_group_node *First;
    render_rect_group_node *Last;
} render_pass_params_ui;

// Stats Types
// Used to infer the size of the allocated arena at the beginning of every frame.
// This structure must be filled correctly throughout the frame to achieve better
// allocations. Also useful for the user.

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
        void                  *Params;
        render_pass_params_ui *UI;
    } Params;
} render_pass;

typedef struct render_pass_node render_pass_node;
struct render_pass_node
{
    render_pass_node *Next;
    render_pass       Pass;
};

typedef struct render_context
{
    // UI
    render_pass_ui_stats UIStats;
    memory_arena        *UIPassArena;
    render_pass_node    *UIPassNode;
} render_context;

// [Globals]

global u32 UIPassDefaultBatchCount       = 10;
global u32 UIPassDefaultGroupCount       = 20;
global u32 UIPassDefaultPassCount        = 5;
global u32 UIPassDefaultRenderedDataSize = Kilobyte(50);
global u32 UIPassPaddingSize             = Kilobyte(25);

// [CORE API]

internal void BeginRenderingContext  (render_context *Context);
internal b32  IsValidRenderHandle    (render_handle Handle);

// [PER-RENDERER API]

internal render_handle InitializeRenderer    (memory_arena *Arena);
internal void          EndRendererFrame      (void);
internal void          SubmitRenderCommands  (render_context *RenderContext, render_handle BackendHandle);
