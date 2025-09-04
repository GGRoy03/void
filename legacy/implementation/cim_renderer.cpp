// D3D Interface.
static bool    D3DInitialize (void *UserDevice, void *UserContext);
static void    D3DDrawUI     (cim_i32 ClientWidth, cim_i32 ClientHeight);
// [Font/Text]
static void      D3DTransferGlyph     (stbrp_rect Rect, ui_font *Font);
static ui_font * D3DLoadFont          (const char *FontName, cim_f32 FontSize, UIFont_Mode Mode);
static void      D3DReleaseFont       (ui_font *Font);
static size_t    D3DGetFontFootprint  (glyph_table_params Params, cim_u32 AtlasWidth, UIFont_Mode Mode);

static bool 
InitializeRenderer(UIRenderer_Backend Backend, void *Param1, void *Param2) // WARN: This is a bit goofy.
{
    Cim_Assert(CimCurrent);

    cim_renderer *Renderer    = UIP_RENDERER;
    bool          Initialized = false;

    switch (Backend)
    {

#ifdef _WIN32

    case UIRenderer_D3D:
    {
        Initialized = D3DInitialize(Param1, Param2);
        if (Initialized)
        {
            Renderer->Draw = D3DDrawUI;

            // Text
            Renderer->TransferGlyph = D3DTransferGlyph; // Internal
            Renderer->LoadFont      = D3DLoadFont;      // Public
            Renderer->ReleaseFont   = D3DReleaseFont;   // Public

        }
    } break;

#endif // _WIN32

    }

    return Initialized;
}

// WARN: This is internal only, unsure where to put this.
static void
TransferGlyph(stbrp_rect Rect, ui_font *Font)
{
    Cim_Assert(CimCurrent);
    CimCurrent->Renderer.TransferGlyph(Rect, Font);
}

// [Font Public API]

static ui_font *
UILoadFont(const char *FontName, cim_f32 FontSize, UIFont_Mode Mode)
{
    Cim_Assert(CimCurrent);
    ui_font *Result = CimCurrent->Renderer.LoadFont(FontName, FontSize, Mode);
    return Result;
}

static void
UIUnloadFont(ui_font *Font)
{
    Cim_Assert(CimCurrent);
    CimCurrent->Renderer.ReleaseFont(Font);

    if (Font->FreePointer)
    {
        free(Font->FreePointer);
        Font->FreePointer = NULL;
    }

    Font->AtlasNodes   = NULL;
    Font->AtlasWidth   = 0;
    Font->AtlasHeight  = 0;
    Font->LineHeight   = 0;
    Font->IsValid      = false;
    Font->AtlasContext = {};
}

static void
UISetFont(ui_font *Font)
{
    Cim_Assert(CimCurrent);

    if (Font->IsValid)
    {
        CimCurrent->CurrentFont = Font;
    }
    else
    {
        // TODO: Set a default font/log?
    }
}

// [Agnostic Code]

static ui_draw_batch *
GetDrawBatch(ui_draw_batch_buffer *Buffer, cim_rect ClipRect, UIPipeline_Type PipelineType)
{
    ui_draw_batch *Batch = NULL;

    if (Buffer->BatchCount == Buffer->BatchSize)
    {
        Buffer->BatchSize = Buffer->BatchSize ? Buffer->BatchSize * 2 : 32;

        ui_draw_batch *New = (ui_draw_batch*)malloc(Buffer->BatchSize * sizeof(ui_draw_batch));
        if (!New)
        {
            return NULL;
        }

        if (Buffer->Batches)
        {
            memcpy(New, Buffer->Batches, Buffer->BatchCount * sizeof(ui_draw_batch));
            free(Buffer->Batches);
        }

        // NOTE: Why do we memset?
        memset(New, 0, Buffer->BatchSize * sizeof(ui_draw_batch));
        Buffer->Batches = New;
    }

    if ((Buffer->BatchCount == 0 || !RectAreEqual(Buffer->CurrentClipRect, ClipRect) || Buffer->CurrentPipelineType != PipelineType) && PipelineType != UIPipeline_None)
    {
        Batch = Buffer->Batches + Buffer->BatchCount++;

        Batch->VertexByteOffset = Buffer->FrameVtx.At;
        Batch->StartIndexRead   = Buffer->FrameIdx.At / sizeof(cim_u32);
        Batch->BaseVertexOffset = 0;
        Batch->VtxCount         = 0;
        Batch->IdxCount         = 0;
        Batch->PipelineType     = PipelineType;
        Batch->ClippingRect     = ClipRect;

        Buffer->CurrentPipelineType = PipelineType;
        Buffer->CurrentClipRect     = ClipRect;
    }
    else
    {
        Batch = Buffer->Batches + (Buffer->BatchCount - 1);
    }

    return Batch;
}

static ui_draw_node *
GetDrawNode(ui_tree *Tree, cim_u32 Index)
{
    ui_draw_node *Result = NULL;

    if (Index < Cim_ArrayCount(Tree->Nodes.Draw))
    {
        Result = Tree->Nodes.Draw + Index;

        if(Index == Tree->NodeCount)
        {
            Tree->NodeCount++;
        }
    }

    return Result;
}

static void
ExecuteQuadCommand(ui_draw_command_quad *Quad, cim_vector2 Position, cim_vector2 Size, ui_draw_batch *Batch)
{
    cim_f32 X0 = Position.x;
    cim_f32 Y0 = Position.y;
    cim_f32 X1 = Position.x + Size.x;
    cim_f32 Y1 = Position.y + Size.y;

    ui_vertex Body[4];
    Body[0] = {X0, Y0, 0.0f, 1.0f, Quad->Color.x, Quad->Color.y, Quad->Color.z, Quad->Color.w};
    Body[1] = {X0, Y1, 0.0f, 0.0f, Quad->Color.x, Quad->Color.y, Quad->Color.z, Quad->Color.w};
    Body[2] = {X1, Y0, 1.0f, 1.0f, Quad->Color.x, Quad->Color.y, Quad->Color.z, Quad->Color.w};
    Body[3] = {X1, Y1, 1.0f, 0.0f, Quad->Color.x, Quad->Color.y, Quad->Color.z, Quad->Color.w};

    cim_u32 Indices[6];
    Indices[0] = Batch->VtxCount + 0;
    Indices[1] = Batch->VtxCount + 2;
    Indices[2] = Batch->VtxCount + 1;
    Indices[3] = Batch->VtxCount + 2;
    Indices[4] = Batch->VtxCount + 3;
    Indices[5] = Batch->VtxCount + 1;

    WriteToArena(Body, sizeof(Body), UIP_BATCHES.FrameVtx);
    WriteToArena(Indices, sizeof(Indices), UIP_BATCHES.FrameIdx);;

    Batch->VtxCount += 4;
    Batch->IdxCount += 6;
}

static void
ExecuteBorderCommand(ui_draw_command_border *Border, cim_vector2 Position, cim_vector2 Size, ui_draw_batch *Batch)
{
    Cim_Assert(Border->Width > 0);

    cim_f32 X0 = Position.x - Border->Width;
    cim_f32 Y0 = Position.y - Border->Width;
    cim_f32 X1 = Position.x + Size.x + Border->Width;
    cim_f32 Y1 = Position.y + Size.y + Border->Width;

    ui_vertex Borders[4];
    Borders[0] = {X0, Y0, 0.0f, 1.0f, Border->Color.x, Border->Color.y, Border->Color.z, Border->Color.w};
    Borders[1] = {X0, Y1, 0.0f, 0.0f, Border->Color.x, Border->Color.y, Border->Color.z, Border->Color.w};
    Borders[2] = {X1, Y0, 1.0f, 1.0f, Border->Color.x, Border->Color.y, Border->Color.z, Border->Color.w};
    Borders[3] = {X1, Y1, 1.0f, 0.0f, Border->Color.x, Border->Color.y, Border->Color.z, Border->Color.w};

    cim_u32 Indices[6];
    Indices[0] = Batch->VtxCount + 0;
    Indices[1] = Batch->VtxCount + 2;
    Indices[2] = Batch->VtxCount + 1;
    Indices[3] = Batch->VtxCount + 2;
    Indices[4] = Batch->VtxCount + 3;
    Indices[5] = Batch->VtxCount + 1;

    WriteToArena(Borders, sizeof(Borders), UIP_BATCHES.FrameVtx);
    WriteToArena(Indices, sizeof(Indices), UIP_BATCHES.FrameIdx);;

    Batch->VtxCount += 4;
    Batch->IdxCount += 6;
}

static void
ExecuteTextCommand(ui_draw_command_text *Text, cim_vector2 Position, cim_vector2 Size, ui_draw_batch *Batch)
{
    if (Text->Font->Mode == UIFont_ExtendedASCIIDirectMapping)
    {
        cim_f32 MiddleOffsetX = (Size.x - Text->Width) / 2;

        cim_f32 PenX   = Position.x + MiddleOffsetX;
        cim_f32 PenY   = Position.y;
        cim_f32 StartX = PenX;

        for (cim_u32 Idx = 0; Idx < Text->StringLength; Idx++)
        {
            glyph_layout Layout = FindGlyphLayoutByDirectMapping(Text->Font->Table.Direct, Text->String[Idx]);
            glyph_atlas  Atlas  = FindGlyphAtlasByDirectMapping (Text->Font->Table.Direct, Text->String[Idx]);

            // TODO: Double check this.
            if (PenX + Layout.AdvanceX > Position.x + Size.x && (Idx != Text->StringLength - 1))
            {
                PenX = StartX;
                PenY += Text->Font->LineHeight;
            }

            cim_f32 MinX = PenX + Layout.Offsets.x;
            cim_f32 MinY = PenY - Layout.Offsets.y;
            cim_f32 MaxX = MinX + Layout.Size.Width;
            cim_f32 MaxY = MinY + Layout.Size.Height;

            // TODO: Check if this is actually useful.
            MinX = floorf(MinX + 0.5f);
            MinY = floorf(MinY + 0.5f);
            MaxX = floorf(MaxX + 0.5f);
            MaxY = floorf(MaxY + 0.5f);

            ui_text_vertex Quad[4];
            Quad[0] = {MinX, MinY, Atlas.TexCoord.u0, Atlas.TexCoord.v0}; // Top-left
            Quad[1] = {MinX, MaxY, Atlas.TexCoord.u0, Atlas.TexCoord.v1}; // Bottom-left
            Quad[2] = {MaxX, MinY, Atlas.TexCoord.u1, Atlas.TexCoord.v0}; // Top-right
            Quad[3] = {MaxX, MaxY, Atlas.TexCoord.u1, Atlas.TexCoord.v1}; // Bottom-right

            cim_u32 Indices[6];
            Indices[0] = Batch->VtxCount + 0;
            Indices[1] = Batch->VtxCount + 2;
            Indices[2] = Batch->VtxCount + 1;
            Indices[3] = Batch->VtxCount + 2;
            Indices[4] = Batch->VtxCount + 3;
            Indices[5] = Batch->VtxCount + 1;

            WriteToArena(Quad   , sizeof(Quad)   , UIP_BATCHES.FrameVtx);
            WriteToArena(Indices, sizeof(Indices), UIP_BATCHES.FrameIdx);

            Batch->VtxCount += 4;
            Batch->IdxCount += 6;

            PenX += Layout.AdvanceX;
        }
    }

    Batch->Font = Text->Font;
}

// SO FUCKING BAD.

static void
UIEndDraw(ui_tree *DrawTree)
{
    cim_u32 Queue[256];
    cim_u32 QueueHead  = 0;
    cim_u32 QueueTail  = 0;

    Queue[QueueTail++] = 0;

    while(QueueHead != QueueTail)
    {
        ui_draw_node  *Node  = GetDrawNode(DrawTree, Queue[QueueHead++]);
        ui_draw_batch *Batch = GetDrawBatch(UIP_BATCHES, Node->ClippingRect, Node->Pipeline);

        cim_u32       ChildIndex = Node->FirstChild;
        ui_draw_node *ChildNode  = GetDrawNode(DrawTree, ChildIndex);
        while (ChildIndex != UILayout_InvalidNode)
        {
            Queue[QueueTail++] = ChildIndex;
            Cim_Assert(QueueTail < 256);

            ChildIndex = ChildNode->Next;
            ChildNode  = GetDrawNode(DrawTree, ChildIndex);
        }

        switch (Node->Type)
        {

        case UIDrawNode_None:
        {
            // NOTE: Expected from invisible elements.
        } break;

        case UIDrawNode_Window:
        {
            ui_draw_command_border *BorderCommand     = &Node->Data.Window.BorderCommand;
            ui_draw_command_quad   *BackgroundCommand = &Node->Data.Window.BodyCommand;

            if (BorderCommand->IsVisible)
            {
                ExecuteBorderCommand(BorderCommand, Node->Position, Node->Size, Batch);
            }

            if (BackgroundCommand->IsVisible)
            {
                ExecuteQuadCommand(BackgroundCommand, Node->Position, Node->Size, Batch);
            }

        } break;

        case UIDrawNode_Button:
        {
            ui_draw_command_border *BorderCommand     = &Node->Data.Button.BorderCommand;
            ui_draw_command_quad   *BackgroundCommand = &Node->Data.Button.BodyCommand;
            ui_draw_command_text   *TextCommand       = &Node->Data.Button.TextCommand;

            if (BorderCommand->IsVisible)
            {
                ExecuteBorderCommand(BorderCommand, Node->Position, Node->Size, Batch);
            }

            if (BackgroundCommand->IsVisible)
            {
                ExecuteQuadCommand(BackgroundCommand, Node->Position, Node->Size, Batch);
            }

            // WARN: There's something tricky here. Since a button can also draw text, we need to
            // change the pipeline and request a new batch... Since our text drawing is still really
            // naive, we can simply force it here. We'd like to be able to batch text drawing more 
            // aggressively. This is ugly but it should work.
            if (TextCommand->IsVisible)
            {
                Batch = GetDrawBatch(UIP_BATCHES, {}, UIPipeline_Text);
                ExecuteTextCommand(&Node->Data.Button.TextCommand, Node->Position, Node->Size, Batch);
            }
        } break;

        case UIDrawNode_Text:
        {
            ui_draw_command_text *TextCommand = &Node->Data.Button.TextCommand;
            
            if (TextCommand->IsVisible)
            {
                ExecuteTextCommand(TextCommand, Node->Position, Node->Size, Batch);
            }

        } break;

        default:
        {
            Cim_Assert(!"Invalid draw node.");
        } break;

        }
    }
}


// [Uber Shaders]
// NOTE: I still don't really know what I want to do with this.
// It's kind of annoying to maintain?

static char D3DTextShader[] =
"cbuffer PerFrame : register(b0)                                        \n"
"{                                                                      \n"
"    matrix SpaceMatrix;                                                \n"
"};                                                                     \n"
"                                                                       \n"
"// Vertex Shader                                                       \n"
"                                                                       \n"
"struct VertexInput                                                     \n"
"{                                                                      \n"
"    float2 Pos : POSITION;                                             \n"
"    float2 Tex : TEXCOORD0;                                            \n"
"};                                                                     \n"
"                                                                       \n"
"struct VertexOutput                                                    \n"
"{                                                                      \n"
"    float4 Position : SV_POSITION;                                     \n"
"    float2 Tex      : TEXCOORD0;                                       \n"
"};                                                                     \n"
"                                                                       \n"
"VertexOutput VSMain(VertexInput Input)                                 \n"
"{                                                                      \n"
"    VertexOutput Output;                                               \n"
"    Output.Position = mul(SpaceMatrix, float4(Input.Pos, 1.0f, 1.0f)); \n"
"    Output.Tex      = Input.Tex;                                       \n"
"    return Output;                                                     \n"
"}                                                                      \n"
"                                                                       \n"
"// Pixel Shader                                                        \n"
"                                                                       \n"
"Texture2D    FontTexture : register(t0);                               \n"
"SamplerState FontSampler : register(s0);                               \n"
"                                                                       \n"
"float4 PSMain(VertexOutput Input) : SV_TARGET                          \n"
"{                                                                      \n"
"    return FontTexture.Sample(FontSampler, Input.Tex);                 \n"
"}                                                                      \n"
;

static char D3DVertexShader[] =
"cbuffer PerFrame : register(b0)                                        \n"
"{                                                                      \n"
"    matrix SpaceMatrix;                                                \n"
"};                                                                     \n"
"                                                                       \n"
"struct VertexInput                                                     \n"
"{                                                                      \n"
"    float2 Pos : POSITION;                                             \n"
"    float2 Tex : TEXCOORD0;                                            \n"
"    float4 Col : COLOR;                                                \n"
"};                                                                     \n"
"                                                                       \n"
"struct VertexOutput                                                    \n"
"{                                                                      \n"
"   float4 Position : SV_POSITION;                                      \n"
"   float4 Col      : COLOR;                                            \n"
"};                                                                     \n"
"                                                                       \n"
"VertexOutput VSMain(VertexInput Input)                                 \n"
"{                                                                      \n"
"    VertexOutput Output;                                               \n"
"                                                                       \n"
"    Output.Position = mul(SpaceMatrix, float4(Input.Pos, 1.0f, 1.0f)); \n"
"    Output.Col      = Input.Col;                                       \n"
"                                                                       \n"
"    return Output;                                                     \n"
"}                                                                      \n"
;

static char D3DPixelShader[] =
"struct PSInput                           \n"
"{                                        \n"
"    float4 Position : SV_POSITION;       \n"
"    float4 Col      : COLOR;             \n"
"};                                       \n"
"                                         \n"
"float4 PSMain(PSInput Input) : SV_TARGET \n"
"{                                        \n"
"    float4 Color = Input.Col;            \n"
"    return Color;                        \n"
"}                                        \n"
;

// [D3D Backend]

#ifdef _WIN32

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler")
#pragma comment (lib, "dxguid.lib")

#define D3DSafeRelease(Object) if(Object) Object->Release(); Object = NULL;
#define D3DReturnVoid(Error)  if(FAILED(Error)) {return;}
#define D3DReturnError(Error) if(FAILED(Error)) {return Error;}

// [D3D Internal Structs]

typedef struct d3d_pipeline
{
    ID3D11InputLayout  *Layout;
    ID3D11VertexShader *VtxShader;
    ID3D11PixelShader  *PxlShader;
    UINT                Stride;
} d3d_pipeline;

typedef struct d3d_shared_data
{
    cim_f32 SpaceMatrix[4][4];
} d3d_shared_data;

typedef struct d3d_backend
{
    // User
    ID3D11Device        *Device;
    ID3D11DeviceContext *DeviceContext;

    // Frame Data
    ID3D11Buffer *SharedFrameData;
    ID3D11Buffer *VtxBuffer;
    ID3D11Buffer *IdxBuffer;
    size_t        VtxBufferSize;
    size_t        IdxBufferSize;

    // Internal Render State
    ID3D11BlendState   *BlendState;
    ID3D11SamplerState *SamplerState;
} d3d_backend;

// NOTE: This is part of the interface implementation.
typedef struct gpu_font_backend
{
    ID3D11Texture2D          *GlyphCache;
    ID3D11ShaderResourceView *GlyphCacheView;

    ID3D11Texture2D          *GlyphTransfer;
    ID3D11ShaderResourceView *GlyphTransferView;
    IDXGISurface             *GlyphTransferSurface;
} gpu_font_backend;

// Temporary for testing. These are default pipelines.
static d3d_pipeline Pipelines[UIPipeline_Count] = {};

// [D3D Private Implementation]

static UINT
GetD3DFormatSize(DXGI_FORMAT Format)
{
    switch (Format)
    {

    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        return 4;

    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return 8;

    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
        return 12;

    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return 16;

    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
        return 2;

    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_TYPELESS:
        return 4;

    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return 8;

    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_TYPELESS:
        return 1;

    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 2;

    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        return 4;

    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
        return 2;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return 4;

    default:
        return 0;

    }
}

static HRESULT
CreateD3DInputLayout(d3d_pipeline *Pipeline, d3d_backend *Backend, ID3DBlob *VtxBlob)
{
    HRESULT                 Error      = S_OK;
    ID3D11ShaderReflection *Reflection = NULL;

    Error = D3DReflect(VtxBlob->GetBufferPointer(), VtxBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void **)&Reflection);
    if (FAILED(Error))
    {
        return Error;
    }

    D3D11_SHADER_DESC ShaderDesc;
    Error = Reflection->GetDesc(&ShaderDesc);
    if (FAILED(Error))
    {
        return Error;
    }

    D3D11_INPUT_ELEMENT_DESC Desc[32] = {};
    UINT                     Offset   = 0;

    for (cim_u32 InputIdx = 0; InputIdx < ShaderDesc.InputParameters; InputIdx++)
    {
        D3D11_SIGNATURE_PARAMETER_DESC Signature;
        Error = Reflection->GetInputParameterDesc(InputIdx, &Signature);
        if (FAILED(Error))
        {
            return Error;
        }

        D3D11_INPUT_ELEMENT_DESC *InputDesc = Desc + InputIdx;
        InputDesc->SemanticName      = Signature.SemanticName;
        InputDesc->SemanticIndex     = Signature.SemanticIndex;
        InputDesc->InputSlotClass    = D3D11_INPUT_PER_VERTEX_DATA;
        InputDesc->AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;

        if      (Signature.Mask == 1)
        {
            if (Signature.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32_UINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32_SINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32_FLOAT;
            }
        }
        else if (Signature.Mask <= 3)
        {
            if (Signature.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32_UINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32_SINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32_FLOAT;
            }
        }
        else if (Signature.Mask <= 7)
        {
            if (Signature.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32_UINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32_SINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32_FLOAT;
            }
        }
        else if (Signature.Mask <= 15)
        {
            if (Signature.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32A32_UINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32A32_SINT;
            }
            else if (Signature.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
            {
                InputDesc->Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
        }

        Offset += GetD3DFormatSize(InputDesc->Format);
    }

    Pipeline->Stride = Offset;

    Error = Backend->Device->CreateInputLayout(Desc, ShaderDesc.InputParameters,
                                               VtxBlob->GetBufferPointer(), VtxBlob->GetBufferSize(),
                                               &Pipeline->Layout);
    return Error;
}

static HRESULT
CreateD3DShaders(d3d_pipeline *Pipeline, ui_shader_desc *ShaderDesc, cim_u32 Count)
{
    Cim_Assert(CimCurrent && ShaderDesc);

    HRESULT      Error   = S_OK;
    d3d_backend *Backend = (d3d_backend *)CimCurrent->Backend;

    ID3DBlob *ErrorBlob    = NULL;
    ID3DBlob *ShaderBlob   = NULL;
    ID3DBlob *VtxBlob      = NULL;
    UINT      CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    cim_u32 DescIdx = 0;
    while (SUCCEEDED(Error) && DescIdx < Count)
    {
        ui_shader_desc *Desc = ShaderDesc + DescIdx;

        switch (Desc->Type)
        {

        case UIShader_Vertex:
        {
            const char  *EntryPoint = "VSMain";
            const char  *Profile    = "vs_5_0";
            const SIZE_T SourceSize = strlen(Desc->SourceCode);

            Error = D3DCompile(Desc->SourceCode, SourceSize,
                               NULL, NULL, NULL,
                               EntryPoint, Profile,
                               CompileFlags, 0,
                               &VtxBlob, &ErrorBlob);

            if (SUCCEEDED(Error) && VtxBlob)
            {
                Error = Backend->Device->CreateVertexShader(VtxBlob->GetBufferPointer(), VtxBlob->GetBufferSize(),
                                                            NULL, &Pipeline->VtxShader);
            }

        } break;

        case UIShader_Pixel:
        {
            const char *EntryPoint  = "PSMain";
            const char *Profile     = "ps_5_0";
            const SIZE_T SourceSize = strlen(Desc->SourceCode);

            Error = D3DCompile(Desc->SourceCode, SourceSize,
                               NULL, NULL, NULL,
                               EntryPoint, Profile,
                               CompileFlags, 0,
                               &ShaderBlob, &ErrorBlob);

            if (SUCCEEDED(Error) && ShaderBlob)
            {
                Error = Backend->Device->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
                                                           NULL, &Pipeline->PxlShader);
            }
        } break;

        }

        D3DSafeRelease(ShaderBlob);
        ++DescIdx;
    } 

    if (SUCCEEDED(Error) && VtxBlob)
    {
        Error = CreateD3DInputLayout(Pipeline, Backend, VtxBlob);
    }

    return Error;
}

static HRESULT
CreateD3DPipeline(d3d_pipeline *Pipeline, ui_shader_desc *ShaderDesc, cim_u32 Count)
{
    HRESULT Error  = S_OK;

    Error = CreateD3DShaders(Pipeline, ShaderDesc, Count);

    return Error;
}

static void
ReleaseD3DPipeline(d3d_pipeline *Pipeline)
{
    if (Pipeline->Layout)
    {
        Pipeline->Layout->Release();
        Pipeline->Layout = NULL;
    }

    if (Pipeline->PxlShader)
    {
        Pipeline->PxlShader->Release();
        Pipeline->PxlShader = NULL;
    }

    if (Pipeline->VtxShader)
    {
        Pipeline->VtxShader->Release();
        Pipeline->VtxShader = NULL;
    }
}

static HRESULT
SetupD3DRenderState(cim_i32 ClientWidth, cim_i32 ClientHeight, d3d_backend *Backend)
{
    HRESULT              Error     = S_OK;
    ID3D11Device        *Device    = Backend->Device;
    ID3D11DeviceContext *DeviceCtx = Backend->DeviceContext;

    if (!Backend->SharedFrameData)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = sizeof(d3d_shared_data);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Device->CreateBuffer(&Desc, NULL, &Backend->SharedFrameData);
        if (FAILED(Error))
        {
            return Error;
        }
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    Error = DeviceCtx->Map(Backend->SharedFrameData, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (SUCCEEDED(Error))
    {
        d3d_shared_data *SharedData = (d3d_shared_data *)MappedResource.pData;

        cim_f32 Left = 0;
        cim_f32 Right = ClientWidth;
        cim_f32 Top = 0;
        cim_f32 Bot = ClientHeight;

        cim_f32 SpaceMatrix[4][4] =
        {
            { 2.0f / (Right - Left)          , 0.0f                     , 0.0f, 0.0f },
            { 0.0f                           , 2.0f / (Top - Bot)       , 0.0f, 0.0f },
            { 0.0f                           , 0.0f                     , 0.5f, 0.0f },
            { (Right + Left) / (Left - Right), (Top + Bot) / (Bot - Top), 0.5f, 1.0f },
        };

        memcpy(&SharedData->SpaceMatrix, SpaceMatrix, sizeof(SpaceMatrix));
        DeviceCtx->Unmap(Backend->SharedFrameData, 0);
    }
    else
    {
        return Error;
    }

    DeviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceCtx->IASetIndexBuffer(Backend->IdxBuffer, DXGI_FORMAT_R32_UINT, 0);

    DeviceCtx->VSSetConstantBuffers(0, 1, &Backend->SharedFrameData);

    DeviceCtx->PSSetSamplers(0, 1, &Backend->SamplerState); // Is this global?

    DeviceCtx->OMSetBlendState(Backend->BlendState, NULL, 0xFFFFFFFF);

    D3D11_VIEWPORT Viewport;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width    = ClientWidth;
    Viewport.Height   = ClientHeight;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    DeviceCtx->RSSetViewports(1, &Viewport);

    return Error;
}

static HRESULT
CreateD3DGlyphCache(d3d_backend *D3DBackend, gpu_font_backend *FontBackend, cim_u32 Width, cim_u32 Height)
{
    Cim_Assert(FontBackend);

    HRESULT Error = E_FAIL;

    if (D3DBackend->Device)
    {
        D3D11_TEXTURE2D_DESC TextureDesc = {};
        TextureDesc.Width      = Width;
        TextureDesc.Height     = Height;
        TextureDesc.MipLevels  = 1;
        TextureDesc.ArraySize  = 1;
        TextureDesc.Format     = DXGI_FORMAT_B8G8R8A8_UNORM;
        TextureDesc.SampleDesc = { 1, 0 };
        TextureDesc.Usage      = D3D11_USAGE_DEFAULT;
        TextureDesc.BindFlags  = D3D11_BIND_SHADER_RESOURCE;

        Error = D3DBackend->Device->CreateTexture2D(&TextureDesc, NULL, &FontBackend->GlyphCache);
        if (SUCCEEDED(Error))
        {
            Error = D3DBackend->Device->CreateShaderResourceView(FontBackend->GlyphCache, NULL, &FontBackend->GlyphCacheView);
        }
    }

    return Error;
}

static void
ReleaseD3DGlyphCache(gpu_font_backend *Backend)
{
    if (Backend->GlyphCache)
    {
        Backend->GlyphCache->Release();
        Backend->GlyphCache = NULL;
    }

    if (Backend->GlyphCacheView)
    {
        Backend->GlyphCacheView->Release();
        Backend->GlyphCacheView = NULL;
    }
}

static HRESULT
CreateD3DGlyphTransfer(d3d_backend *D3DBackend, gpu_font_backend *FontBackend, cim_u32 Width, cim_u32 Height)
{
    HRESULT Error = E_FAIL;
    if (D3DBackend->Device)
    {
        D3D11_TEXTURE2D_DESC TextureDesc = {};
        TextureDesc.Width      = Width;
        TextureDesc.Height     = Height;
        TextureDesc.MipLevels  = 1;
        TextureDesc.ArraySize  = 1;
        TextureDesc.Format     = DXGI_FORMAT_B8G8R8A8_UNORM;
        TextureDesc.SampleDesc = { 1, 0 };
        TextureDesc.Usage      = D3D11_USAGE_DEFAULT;
        TextureDesc.BindFlags  = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        Error = D3DBackend->Device->CreateTexture2D(&TextureDesc, NULL, &FontBackend->GlyphTransfer);
        if (SUCCEEDED(Error))
        {
            Error = D3DBackend->Device->CreateShaderResourceView(FontBackend->GlyphTransfer, NULL, &FontBackend->GlyphTransferView);
            if (SUCCEEDED(Error))
            {
                Error = FontBackend->GlyphTransfer->QueryInterface(IID_IDXGISurface, (void **)&FontBackend->GlyphTransferSurface);
            }

        }
    }

    return Error;
}

static void 
ReleaseD3DGlyphTransfer(gpu_font_backend *Backend)
{
    if (Backend->GlyphTransferSurface)
    {
        Backend->GlyphTransferSurface->Release();
        Backend->GlyphTransferSurface = NULL;
    }

    if (Backend->GlyphTransferView)
    {
        Backend->GlyphTransferView->Release();
        Backend->GlyphTransferView = NULL;
    }

    if (Backend->GlyphTransfer)
    {
        Backend->GlyphTransfer->Release();
        Backend->GlyphTransfer = NULL;
    }
}

// [D3D Public Implementation]

// [Text/Font]
static ui_font *
D3DLoadFont(const char *FontName, cim_f32 FontSize, UIFont_Mode Mode)
{
    Cim_Assert(CimCurrent && CimCurrent->Backend);

    ui_font           *Result  = NULL;
    d3d_backend       *Backend = (d3d_backend *)CimCurrent->Backend;
    glyph_table_params Params  = {};

    cim_u32 AtlasWidth  = 0;
    cim_u32 AtlasHeight = 0;
    if (Mode == UIFont_ExtendedASCIIDirectMapping)
    {
        // WARN: Still unsure about all of this.
        AtlasWidth  = 1024;
        AtlasHeight = 1024;

        Params.HashCount  = 0;
        Params.EntryCount = UIText_ExtendedASCIITableSize;
    }

    if (AtlasWidth > 0 && AtlasHeight > 0)
    {
        size_t HeapSize = D3DGetFontFootprint(Params, AtlasWidth, Mode);
        char*  HeapBase = (char*)malloc(HeapSize);

        if (HeapBase)
        {
            memset(HeapBase, 0, HeapSize);

            Result   = (ui_font *)HeapBase;
            HeapBase = (char *)(Result + 1);

            if (Mode == UIFont_ExtendedASCIIDirectMapping)
            {
                Result->Table.Direct = PlaceDirectGlyphTableInMemory(Params, HeapBase);
                HeapBase            += GetDirectGlyphTableFootPrint(Params);
            }
            Cim_Assert(Result->Table.Direct || Result->Table.LRU);

            Result->AtlasNodes = (stbrp_node*)HeapBase;
            HeapBase          += AtlasWidth * sizeof(stbrp_node);

            Result->OSBackend = (os_font_backend *)HeapBase;
            HeapBase          = (char *)(Result->OSBackend + 1);

            Result->GPUBackend = (gpu_font_backend *)HeapBase;
            HeapBase           = (char *)(Result->GPUBackend + 1);

            Result->FreePointer = HeapBase - HeapSize;
        }

        Result->AtlasWidth  = AtlasWidth;
        Result->AtlasHeight = AtlasHeight;
        stbrp_init_target(&Result->AtlasContext, Result->AtlasWidth, Result->AtlasHeight, Result->AtlasNodes, Result->AtlasWidth);
    }

    if (Result)
    {
        HRESULT Error = S_OK;

        Error = CreateD3DGlyphCache(Backend, Result->GPUBackend, Result->AtlasWidth, Result->AtlasHeight);
        if (SUCCEEDED(Error))
        {
            Error = CreateD3DGlyphTransfer(Backend, Result->GPUBackend, Result->AtlasWidth, Result->AtlasHeight);
            if (SUCCEEDED(Error))
            {
                Error = OSAcquireFontBackend(FontName, FontSize, Result->GPUBackend->GlyphTransferSurface, Result) ? S_OK : E_FAIL;
            }
        }

        if (FAILED(Error))
        {
            ReleaseD3DGlyphCache(Result->GPUBackend);
            ReleaseD3DGlyphTransfer(Result->GPUBackend);
            OSReleaseFontBackend(Result->OSBackend);
            free(Result->FreePointer);
        }
        else
        {
            Result->IsValid = true;
            Result->Size    = FontSize;
        }
    }

    return Result;
}

static void
D3DReleaseFont(ui_font *Font)
{
    if (Font->GPUBackend)
    {
        ReleaseD3DGlyphCache(Font->GPUBackend);
        ReleaseD3DGlyphTransfer(Font->GPUBackend);
    }

    if (Font->OSBackend)
    {
        OSReleaseFontBackend(Font->OSBackend);
    }
}

static size_t
D3DGetFontFootprint(glyph_table_params Params, cim_u32 AtlasWidth, UIFont_Mode Mode)
{
    size_t Result = 0;

    switch(Mode)
    {

    case UIFont_ExtendedASCIIDirectMapping:
    {
        size_t GlyphTableSize = GetDirectGlyphTableFootPrint(Params);
        size_t AtlasNodeSize  = AtlasWidth * sizeof(stbrp_node);
        size_t GPUBackendSize = sizeof(gpu_font_backend);
        size_t OSBackendSize  = sizeof(os_font_backend);
        
        Result = sizeof(ui_font) + GlyphTableSize + AtlasNodeSize + GPUBackendSize + OSBackendSize;
    } break;

    default: break;

    }

    return Result;
}


// TODO: Make a backend cleanup function probably.
// TODO: Rework this function slightly. (Mostly Okay)
static bool
D3DInitialize(void *UserDevice, void *UserContext)
{
    Cim_Assert(CimCurrent);

    if (!UserDevice || !UserContext || !CimCurrent)
    {
        return false;
    }

    d3d_backend *Backend = (d3d_backend *)calloc(1, sizeof * Backend);
    if (!Backend)
    {
        return false;
    }

    CimCurrent->Backend = Backend;
    Backend->Device        = (ID3D11Device *)UserDevice;
    Backend->DeviceContext = (ID3D11DeviceContext *)UserContext;

    HRESULT Error = S_OK;

    {
        ui_shader_desc Shaders[2] =
        {
            {D3DVertexShader, UIShader_Vertex},
            {D3DPixelShader , UIShader_Pixel },
        };

        Error = CreateD3DPipeline(&Pipelines[UIPipeline_Default], Shaders, 2);
        if (FAILED(Error))
        {
            free(Backend);
            CimCurrent->Backend = NULL;

            return false;
        }
    }

    {
        ui_shader_desc Shaders[2] =
        {
            {D3DTextShader, UIShader_Vertex},
            {D3DTextShader, UIShader_Pixel },
        };

        Error = CreateD3DPipeline(&Pipelines[UIPipeline_Text], Shaders, 2);
        if (FAILED(Error))
        {
            ReleaseD3DPipeline(&Pipelines[UIPipeline_Text]);

            free(Backend);
            CimCurrent->Backend = NULL;

            return false;
        }

        D3D11_SAMPLER_DESC Desc = {};
        Desc.Filter         = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        Desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.MaxAnisotropy  =  1;
        Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        Desc.MaxLOD         = D3D11_FLOAT32_MAX;

        Error = Backend->Device->CreateSamplerState(&Desc, &Backend->SamplerState);
        if (FAILED(Error))
        {
            ReleaseD3DPipeline(&Pipelines[UIPipeline_Default]);
            ReleaseD3DPipeline(&Pipelines[UIPipeline_Text]);

            free(Backend);
            CimCurrent->Backend = NULL;

            return false;
        }
    }

    {
        D3D11_BLEND_DESC Desc = {};
        Desc.RenderTarget[0].BlendEnable           = TRUE;
        Desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        Desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        Desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        Desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        Desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        Desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        Error = Backend->Device->CreateBlendState(&Desc, &Backend->BlendState);
        if (FAILED(Error))
        {
            ReleaseD3DPipeline(&Pipelines[UIPipeline_Default]);
            ReleaseD3DPipeline(&Pipelines[UIPipeline_Text]);

            free(Backend);
            CimCurrent->Backend = NULL;

            Backend->SamplerState->Release();
            Backend->SamplerState = NULL;

            return false;
        }
    }

    return true;
}

static void
D3DDrawUI(cim_i32 ClientWidth, cim_i32 ClientHeight)
{
    Cim_Assert(CimCurrent);
    d3d_backend *Backend = (d3d_backend *)CimCurrent->Backend; Cim_Assert(Backend);

    HRESULT               Error       = S_OK;
    ID3D11Device         *Device      = Backend->Device;        Cim_Assert(Device);
    ID3D11DeviceContext  *DeviceCtx   = Backend->DeviceContext; Cim_Assert(DeviceCtx);
    ui_draw_batch_buffer *BatchBuffer = UIP_BATCHES;

    if (ClientWidth == 0 || ClientHeight == 0)
    {
        CimArena_Reset(&BatchBuffer->FrameVtx);
        CimArena_Reset(&BatchBuffer->FrameIdx);
        BatchBuffer->BatchCount = 0;
        return;
    }

    if (!Backend->VtxBuffer || BatchBuffer->FrameVtx.At > Backend->VtxBufferSize)
    {
        D3DSafeRelease(Backend->VtxBuffer);

        Backend->VtxBufferSize = BatchBuffer->FrameVtx.At + 1024;

        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = Backend->VtxBufferSize;
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Device->CreateBuffer(&Desc, NULL, &Backend->VtxBuffer);
        D3DReturnVoid(Error);
    }

    if (!Backend->IdxBuffer || BatchBuffer->FrameIdx.At > Backend->IdxBufferSize)
    {
        D3DSafeRelease(Backend->IdxBuffer);

        Backend->IdxBufferSize = BatchBuffer->FrameIdx.At + 1024;

        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = Backend->IdxBufferSize;
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Device->CreateBuffer(&Desc, NULL, &Backend->IdxBuffer);
        D3DReturnVoid(Error)
    }

    D3D11_MAPPED_SUBRESOURCE VtxResource = {};
    Error = DeviceCtx->Map(Backend->VtxBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &VtxResource); 
    D3DReturnVoid(Error)
    memcpy(VtxResource.pData, BatchBuffer->FrameVtx.Memory, BatchBuffer->FrameVtx.At);
    DeviceCtx->Unmap(Backend->VtxBuffer, 0);

    D3D11_MAPPED_SUBRESOURCE IdxResource = {};
    Error = DeviceCtx->Map(Backend->IdxBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &IdxResource);
    D3DReturnVoid(Error)
    memcpy(IdxResource.pData, BatchBuffer->FrameIdx.Memory, BatchBuffer->FrameIdx.At);
    DeviceCtx->Unmap(Backend->IdxBuffer, 0);

    Error = SetupD3DRenderState(ClientWidth, ClientHeight, Backend);
    D3DReturnVoid(Error)

    for (cim_u32 CmdIdx = 0; CmdIdx < BatchBuffer->BatchCount; CmdIdx++)
    {
        ui_draw_batch *Batch    = BatchBuffer->Batches + CmdIdx;
        d3d_pipeline  *Pipeline = &Pipelines[Batch->PipelineType];

        DeviceCtx->IASetInputLayout(Pipeline->Layout);
        DeviceCtx->IASetVertexBuffers(0, 1, &Backend->VtxBuffer, &Pipeline->Stride, (UINT *)&Batch->VertexByteOffset);

        DeviceCtx->VSSetShader(Pipeline->VtxShader, NULL, 0);

        DeviceCtx->PSSetShader(Pipeline->PxlShader, NULL, 0);

        // TODO: Change this cluster fuck (Pipelines included)
        if (Batch->PipelineType == UIPipeline_Text)
        {
            DeviceCtx->PSSetShaderResources(0, 1, &Batch->Font->GPUBackend->GlyphCacheView);
        }

        DeviceCtx->DrawIndexed(Batch->IdxCount, Batch->StartIndexRead, Batch->BaseVertexOffset);
    }

    CimArena_Reset(&BatchBuffer->FrameVtx);
    CimArena_Reset(&BatchBuffer->FrameIdx);
    BatchBuffer->BatchCount = 0;
}

static void
D3DTransferGlyph(stbrp_rect Rect, ui_font *Font)
{
    Cim_Assert(CimCurrent);
    Cim_Assert(CimCurrent->Backend);

    d3d_backend *Backend = (d3d_backend *)CimCurrent->Backend;

    if (Backend && Backend->DeviceContext)
    {
        D3D11_BOX SourceBox;
        SourceBox.left   = 0;
        SourceBox.top    = 0;
        SourceBox.front  = 0;
        SourceBox.right  = Rect.w;
        SourceBox.bottom = Rect.h;
        SourceBox.back   = 1;

        Backend->DeviceContext->CopySubresourceRegion(Font->GPUBackend->GlyphCache, 0, Rect.x, Rect.y, 0,
                                                      Font->GPUBackend->GlyphTransfer, 0, &SourceBox);
    }
}

#endif