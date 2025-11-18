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
    vec2_f32      TextureSize;
    render_handle Texture;
    matrix_3x3    Transform;
    rect_f32      Clip;
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
    u32              Count;
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

read_only global u64 RenderPassDataSizeTable[] =
{
    80, // Inputs to UI pass (ui_rect)
};

// [Handles]

internal b32           IsValidRenderHandle    (render_handle Handle);
internal render_handle RenderHandle           (u64 Handle);
internal b32           RenderHandleMatches    (render_handle H1, render_handle H2);

// [Batches]

internal void        * PushDataInBatchList      (memory_arena *Arena, render_batch_list *BatchList);
internal render_pass * GetRenderPass            (memory_arena *Arena, RenderPass_Type Type);
internal b32           CanMergeRectGroupParams  (rect_group_params *Old, rect_group_params *New);

// [PER-RENDERER API]

internal render_handle InitializeRenderer    (void *HWindow, vec2_i32 Resolution, memory_arena *Arena);
internal void          SubmitRenderCommands  (render_handle HRenderer, vec2_i32 Resolution, render_pass_list *RenderPassList);

// ------------------------------------------------------------------------------------
// Textures

typedef enum RenderTexture_Type
{
    RenderTexture_None  = 0,
    RenderTexture_RGBA  = 1,
} RenderTexture_Type;

typedef enum RenderTexture_Filter
{
    RenderTextureFilter_Nearest = 0,
    RenderTextureFilter_Linear  = 1,
} RenderTexture_Filter;

typedef enum RenderTexture_Wrap
{
    RenderTextureWrap_Clamp  = 1,
    RenderTextureWrap_Repeat = 2,
    RenderTextureWrap_Mirror = 3,
} RenderTexture_Wrap;

typedef struct render_texture_params
{
    RenderTexture_Type   Type;
    u32                  Width;
    u32                  Height;
    RenderTexture_Filter MinFilter;
    RenderTexture_Filter MagFilter;
    RenderTexture_Wrap   WrapU;
    RenderTexture_Wrap   WrapV;
    void                *InitialData;
    u32                  InitialDataSize;
    b32                  GenerateMipmaps;
} render_texture_params;

typedef struct render_texture
{
    render_handle Texture;
    render_handle View;
    vec2_f32      Size;
} render_texture;

internal render_texture CreateRenderTexture    (render_texture_params Params);
internal void           CopyIntoRenderTexture  (render_texture Texture, rect_f32 Source, u8 *Pixels, u32 Pitch);

// CreateGlyphCache:
//   Creates the GPU resource used as the persistent glyph cache.
//   Populates FontContext with the cache-related resources.
//   Returns 1 on success, 0 on failure.
//   The caller is responsible for freeing the resources allocated by using ReleaseGlyphCache if this function fails.
//
// CreateGlyphTransfer:
//   Creates the GPU resource used as the transfer/render target for rasterizing glyphs.
//   Populates FontContext with the transfer-related resources.
//   Returns 1 on success, 0 on failure.
//   The caller is responsible for freeing the resources allocated by using ReleaseGlyphTransfer if this function fails.
//
// ReleaseGlyphCache:
//   Releases/cleans any resources associated with the glyph cache stored in FontContext.
//   Safe to call with NULL. After this call the cache-related fields in FontContext are invalid.
//
// ReleaseGlyphTransfer:
//   Releases/cleans any resources associated with the transfer/render-target in FontContext.
//   Safe to call with NULL. After this call the transfer-related fields in FontContext are invalid.
//
// TransferGlyph:
//   Copies a rectangular region from the transfer resource into the persistent glyph cache.
//   Rect describes the area to copy and the destination offset inside the cache.
//   Typical flow: create transfer resource -> rasterize glyphs into it -> call TransferGlyph to copy into cache.
//   You may generate the rects however you want, but they must not overlap.

typedef struct gpu_font_context gpu_font_context;

internal b32  CreateGlyphCache      (render_handle HRenderer, vec2_f32 TextureSize, gpu_font_context *FontContext);
internal b32  CreateGlyphTransfer   (render_handle HRenderer, vec2_f32 TextureSize, gpu_font_context *FontContext);
internal void ReleaseGlyphCache     (gpu_font_context *FontContext);
internal void ReleaseGlyphTransfer  (gpu_font_context *FontContext);
external void TransferGlyph         (rect_f32 Rect, render_handle HRenderer, gpu_font_context *FontContext);
