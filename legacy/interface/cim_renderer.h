#pragma once

// [Constants]

#define UITree_NestingDepth 32
#define UITree_InvalidNode 0xFFFFFFFF

typedef enum UIRenderer_Backend
{
    UIRenderer_D3D = 0,
} UIRenderer_Backend;

typedef enum UIShader_Type
{
    UIShader_Vertex = 0,
    UIShader_Pixel  = 1,
} UIShader_Type;

typedef enum UIPipeline_Type
{
    UIPipeline_None    = 0,
    UIPipeline_Default = 1,
    UIPipeline_Text    = 2,
    UIPipeline_Count   = 3,
} UIPipeline_Type;

typedef enum UIDrawNode_Type
{
    UIDrawNode_None   = 0,
    UIDrawNode_Window = 1,
    UIDrawNode_Button = 2,
    UIDrawNode_Text   = 3,
} UIDrawNode_Type;

typedef enum UIDrawCommand_Type
{
    UIDrawCommand_None   = 0,
    UIDrawCommand_Header = 1, // NOTE: Not a header like window header, but more like a command header.
    UIDrawCommand_Quad   = 2,
    UIDrawCommand_Border = 3,
    UIDrawCommand_Text   = 4,
} UIDrawCommand_Type;

// [Forward Declarations]

typedef struct ui_font ui_font;

// [Types]

typedef struct ui_draw_command_quad
{
    bool        IsVisible;
    cim_vector4 Color;
    cim_u32     Index;
} ui_draw_command_quad;

typedef struct ui_draw_command_border
{
    bool        IsVisible;
    cim_vector4 Color;
    cim_f32     Width;
} ui_draw_command_border;

typedef struct ui_draw_command_text
{
    bool     IsVisible;
    ui_font *Font;
    cim_f32  Width;
    cim_u32  StringLength;
    char    *String;       // WARN: This is very fragile
} ui_draw_command_text;

typedef struct ui_draw_node_window
{
    ui_draw_command_quad   BodyCommand;
    ui_draw_command_border BorderCommand;
} ui_draw_node_window;

typedef struct ui_draw_node_button
{
    ui_draw_command_border BorderCommand;
    ui_draw_command_quad   BodyCommand;
    ui_draw_command_text   TextCommand;
} ui_draw_node_button;

typedef struct ui_draw_node_text
{
    ui_draw_command_text TextCommand;
} ui_draw_node_text;

typedef struct ui_draw_node
{
    UIDrawNode_Type Type;
    union
    {
        ui_draw_node_window Window;
        ui_draw_node_button Button;
        ui_draw_node_text   Text;
    } Data;

    // Written to by layout
    cim_vector2 Position;
    cim_vector2 Size;

    // Batch
    cim_rect        ClippingRect;
    UIPipeline_Type Pipeline;

    // Tree
    cim_u32 Id;
    cim_u32 Next;
    cim_u32 FirstChild;
} ui_draw_node;

// NOTE: Experimenting above.

typedef struct ui_shader_desc
{
    char         *SourceCode;
    UIShader_Type Type;
} ui_shader_desc;

typedef struct ui_vertex
{
    cim_f32 PosX, PosY;
    cim_f32 U, V;
    cim_f32 R, G, B, A;
} ui_vertex;

typedef struct ui_text_vertex
{
    cim_f32 PosX, PosY;
    cim_f32 U, V;
}  ui_text_vertex;

typedef struct ui_draw_batch
{
    size_t VertexByteOffset;
    size_t BaseVertexOffset;
    size_t StartIndexRead;

    cim_u32 IdxCount;
    cim_u32 VtxCount;

    cim_rect        ClippingRect;
    cim_bit_field   Features;
    UIPipeline_Type PipelineType;
    ui_font        *Font;
} ui_draw_batch;

typedef struct ui_draw_batch_buffer
{
    cim_u32   GlobalVtxOffset;
    cim_u32   GlobalIdxOffset;
    cim_arena FrameVtx;
    cim_arena FrameIdx;

    ui_draw_batch *Batches;
    cim_u32        BatchCount;
    cim_u32        BatchSize;

    cim_rect        CurrentClipRect;
    UIPipeline_Type CurrentPipelineType;
} ui_draw_batch_buffer;

// V-Table
typedef void DrawUI (cim_i32 Width, cim_i32 Height);

// Text
typedef void      draw_glyph    (stbrp_rect Rect, ui_font *Font);
typedef ui_font * load_font     (const char *FontName, cim_f32 FontSize, UIFont_Mode Mode);
typedef void      release_font  (ui_font *Font);

typedef struct cim_renderer 
{
	DrawUI *Draw;
    
    // Text
    draw_glyph   *TransferGlyph;
    load_font    *LoadFont;
    release_font *ReleaseFont;
} cim_renderer;

// NOTE: This is an internal proxy only, unsure how to build the interface.
static void TransferGlyph (stbrp_rect Rect, ui_font *Font);
