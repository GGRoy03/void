// D3D Interface.
static bool    D3DInitialize     (void *UserDevice, void *UserContext);
static void    D3DDrawUI         (cim_i32 ClientWidth, cim_i32 ClientHeight);
static void    D3DTransferGlyph  (stbrp_rect Rect, ui_font Font);
static ui_font D3DLoadFont       (const char *FontName, cim_f32 FontSize);
static void    D3DReleaseFont    (ui_font *Font);

static bool 
InitializeRenderer(CimRenderer_Backend Backend, void *Param1, void *Param2) // WARN: This is a bit goofy.
{
    cim_renderer *Renderer    = UIP_RENDERER;
    bool          Initialized = false;

    switch (Backend)
    {

#ifdef _WIN32

    case CimRenderer_D3D:
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

// TODO: Check if these are inlined.

// WARN: This is internal only, unsure where to put this.
static void
TransferGlyph(stbrp_rect Rect, ui_font Font)
{
    Cim_Assert(CimCurrent);
    CimCurrent->Renderer.TransferGlyph(Rect, Font);
}

// [Font Public API]

// TODO: Check if this is inlined.
static ui_font
UILoadFont(const char *FileName, cim_f32 FontSize)
{
    Cim_Assert(CimCurrent);
    ui_font Result = CimCurrent->Renderer.LoadFont(FileName, FontSize);
    return Result;
}

static void
UIUnloadFont(ui_font *Font)
{
    Cim_Assert(CimCurrent);
    CimCurrent->Renderer.ReleaseFont(Font);

    if (Font->HeapBase)
    {
        free(Font->HeapBase);
        Font->HeapBase = NULL;
    }

    Font->Table        = NULL;
    Font->AtlasNodes   = NULL;
    Font->AtlasWidth   = 0;
    Font->AtlasHeight  = 0;
    Font->LineHeight   = 0;
    Font->IsValid      = false;
    Font->AtlasContext = {};
}

static void
UISetFont(ui_font Font)
{
    Cim_Assert(CimCurrent);

    if (Font.IsValid)
    {
        CimCurrent->CurrentFont = Font;
    }
    else
    {
        // TODO: Set a default font?
    }
}

// [Agnostic Code]

static ui_draw_command *
GetDrawCommand(cim_cmd_buffer *Buffer, cim_rect ClipRect, UIPipeline_Type PipelineType)
{
    ui_draw_command *Command = NULL;

    if (Buffer->CommandCount == Buffer->CommandSize)
    {
        Buffer->CommandSize = Buffer->CommandSize ? Buffer->CommandSize * 2 : 32;

        ui_draw_command *New = (ui_draw_command *)malloc(Buffer->CommandSize * sizeof(ui_draw_command));
        if (!New)
        {
            return NULL;
        }

        if (Buffer->Commands)
        {
            memcpy(New, Buffer->Commands, Buffer->CommandCount * sizeof(ui_draw_command));
            free(Buffer->Commands);
        }

        // NOTE: Why do we memset?
        memset(New, 0, Buffer->CommandSize * sizeof(ui_draw_command));
        Buffer->Commands = New;
    }

    if (Buffer->CommandCount == 0 || !RectAreEqual(Buffer->CurrentClipRect, ClipRect) || Buffer->CurrentPipelineType != PipelineType)
    {
        Command = Buffer->Commands + Buffer->CommandCount++;

        Command->VertexByteOffset = Buffer->FrameVtx.At;
        Command->StartIndexRead   = Buffer->FrameIdx.At / sizeof(cim_u32); // Assuming 32 bits indices.
        Command->BaseVertexOffset = 0;
        Command->VtxCount         = 0;
        Command->IdxCount         = 0;
        Command->PipelineType     = PipelineType;
        Command->ClippingRect     = ClipRect;

        Buffer->CurrentPipelineType = PipelineType;
        Buffer->CurrentClipRect     = ClipRect;
    }
    else
    {
        Command = Buffer->Commands + (Buffer->CommandCount - 1);
    }

    return Command;
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
typedef struct renderer_font_objects
{
    ID3D11Texture2D          *GlyphCache;
    ID3D11ShaderResourceView *GlyphCacheView;

    ID3D11Texture2D          *GlyphTransfer;
    ID3D11ShaderResourceView *GlyphTransferView;
    IDXGISurface             *GlyphTransferSurface;
} renderer_font_objects;

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

// And these are the new functions that go with the new fonts.
static HRESULT
CreateD3DGlyphCache(d3d_backend *Backend, renderer_font_objects *Objects, cim_u32 Width, cim_u32 Height)
{
    HRESULT Error = E_FAIL;

    if (Backend->Device)
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

        Error = Backend->Device->CreateTexture2D(&TextureDesc, NULL, &Objects->GlyphCache);
        if (SUCCEEDED(Error))
        {
            Error = Backend->Device->CreateShaderResourceView(Objects->GlyphCache, NULL, &Objects->GlyphCacheView);
        }
    }

    return Error;
}

static void
ReleaseD3DGlyphCache(renderer_font_objects *Objects)
{
    if (Objects->GlyphCache)
    {
        Objects->GlyphCache->Release();
        Objects->GlyphCache = NULL;
    }

    if (Objects->GlyphCacheView)
    {
        Objects->GlyphCacheView->Release();
        Objects->GlyphCacheView = NULL;
    }
}

static HRESULT
CreateD3DGlyphTransfer(d3d_backend *Backend, renderer_font_objects *Objects, cim_u32 Width, cim_u32 Height)
{
    HRESULT Error = E_FAIL;
    if (Backend->Device)
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

        Error = Backend->Device->CreateTexture2D(&TextureDesc, NULL, &Objects->GlyphTransfer);
        if (SUCCEEDED(Error))
        {
            Error = Backend->Device->CreateShaderResourceView(Objects->GlyphTransfer, NULL, &Objects->GlyphTransferView);
            if (SUCCEEDED(Error))
            {
                Error = Objects->GlyphTransfer->QueryInterface(IID_IDXGISurface, (void **)&Objects->GlyphTransferSurface);
            }

        }
    }

    return Error;
}

static void 
ReleaseD3DGlyphTransfer(renderer_font_objects *Objects)
{
    if (Objects->GlyphTransferSurface)
    {
        Objects->GlyphTransferSurface->Release();
        Objects->GlyphTransferSurface = NULL;
    }

    if (Objects->GlyphTransferView)
    {
        Objects->GlyphTransferView->Release();
        Objects->GlyphTransferView = NULL;
    }

    if (Objects->GlyphTransfer)
    {
        Objects->GlyphTransfer->Release();
        Objects->GlyphTransfer = NULL;
    }
}

//static void
//ReleaseD3DFont(ui_font *Font)
//{
//
//}

// [D3D Public Implementation]

static void
D3DReleaseFont(ui_font *Font)
{
    if (Font->FontObjects)
    {
        ReleaseD3DGlyphCache(Font->FontObjects);
        ReleaseD3DGlyphTransfer(Font->FontObjects);
    }

    if (Font->OSFontObjects)
    {
        ReleaseFontObjects(Font->OSFontObjects);
    }
}

// TODO: Clenaup this function.
static ui_font
D3DLoadFont(const char *FontName, cim_f32 FontSize)
{
    Cim_Assert(CimCurrent && CimCurrent->Backend);

    ui_font      Font    = {};
    d3d_backend *Backend = (d3d_backend *)CimCurrent->Backend;
    HRESULT      Error   = S_OK;

    // NOTE: We use hardcoded numbers for everything for now. Probably should depend
    // on the user parameter.
    Font.AtlasWidth  = 1024;
    Font.AtlasHeight = 1024;

    // NOTE: I still don't know how to query/set to appropriate amount.
    // Maybe the user controls that. We also probably need to ask the OS
    // for some of that information.
    glyph_table_params Params;
    Params.HashCount  = 256;
    Params.EntryCount = 1024;

    size_t OSFontObjectsSize  = GetFontObjectsFootprint();
    size_t GlyphTableSize     = GetGlyphTableFootprint(Params);
    size_t FontObjectsSize    = sizeof(renderer_font_objects);
    size_t AtlasAllocatorSize = Font.AtlasWidth * sizeof(stbrp_node);

    size_t HeapSize    = OSFontObjectsSize + GlyphTableSize + FontObjectsSize + AtlasAllocatorSize;
    char  *HeapPointer = (char *)malloc(HeapSize);
    if (!HeapPointer)
    {
        return Font;
    }
    memset(HeapPointer, 0, HeapSize);
    Font.HeapBase = HeapPointer;

    // WARN: Misaligned stuff? Need to check that. Maybe causing some crashes.
    Font.Table         = PlaceGlyphTableInMemory(Params, HeapPointer);
    Font.FontObjects   = (renderer_font_objects *) (HeapPointer + GlyphTableSize);
    Font.AtlasNodes    = (stbrp_node *)            (HeapPointer + GlyphTableSize + FontObjectsSize);
    Font.OSFontObjects = (os_font_objects *)       (HeapPointer + GlyphTableSize + FontObjectsSize + AtlasAllocatorSize);

    // NOTE: The glyph cache and the glyph transfer are created with some hardcoded numbers
    // for now I probable need to know more about the font and use the table params to set
    // the appropriate size.
    Error = CreateD3DGlyphCache(Backend, Font.FontObjects, Font.AtlasWidth, Font.AtlasHeight);
    if(FAILED(Error))
    {
        ReleaseD3DGlyphCache(Font.FontObjects);
        ReleaseD3DGlyphTransfer(Font.FontObjects);
        free(HeapPointer);
        return Font;
    }

    Error = CreateD3DGlyphTransfer(Backend, Font.FontObjects, Font.AtlasWidth, Font.AtlasHeight);
    if (FAILED(Error))
    {
        ReleaseD3DGlyphCache(Font.FontObjects);
        ReleaseD3DGlyphTransfer(Font.FontObjects);
        free(HeapPointer);
        return Font;
    }

    if (!CreateFontObjects(FontName, FontSize, (void *)Font.FontObjects->GlyphTransferSurface, &Font))
    {
        ReleaseD3DGlyphCache(Font.FontObjects);
        ReleaseD3DGlyphTransfer(Font.FontObjects);
        free(HeapPointer);
        return Font;
    }

    // Seems to crash if there is a mistake.
    stbrp_init_target(&Font.AtlasContext, Font.AtlasWidth, Font.AtlasHeight, Font.AtlasNodes, Font.AtlasWidth);

    Font.IsValid = true;
    return Font;
}

// TODO: Make a backend cleanup function probably.
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
        Desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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

    HRESULT              Error     = S_OK;
    ID3D11Device        *Device    = Backend->Device;        Cim_Assert(Device);
    ID3D11DeviceContext *DeviceCtx = Backend->DeviceContext; Cim_Assert(DeviceCtx);
    cim_cmd_buffer      *CmdBuffer = UIP_COMMANDS;

    if (ClientWidth == 0 || ClientHeight == 0)
    {
        CimArena_Reset(&CmdBuffer->FrameVtx);
        CimArena_Reset(&CmdBuffer->FrameIdx);
        CmdBuffer->CommandCount = 0;
        return;
    }

    if (!Backend->VtxBuffer || CmdBuffer->FrameVtx.At > Backend->VtxBufferSize)
    {
        D3DSafeRelease(Backend->VtxBuffer);

        Backend->VtxBufferSize = CmdBuffer->FrameVtx.At + 1024;

        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = Backend->VtxBufferSize;
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Device->CreateBuffer(&Desc, NULL, &Backend->VtxBuffer);
        D3DReturnVoid(Error);
    }

    if (!Backend->IdxBuffer || CmdBuffer->FrameIdx.At > Backend->IdxBufferSize)
    {
        D3DSafeRelease(Backend->IdxBuffer);

        Backend->IdxBufferSize = CmdBuffer->FrameIdx.At + 1024;

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
    memcpy(VtxResource.pData, CmdBuffer->FrameVtx.Memory, CmdBuffer->FrameVtx.At);
    DeviceCtx->Unmap(Backend->VtxBuffer, 0);

    D3D11_MAPPED_SUBRESOURCE IdxResource = {};
    Error = DeviceCtx->Map(Backend->IdxBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &IdxResource);
    D3DReturnVoid(Error)
    memcpy(IdxResource.pData, CmdBuffer->FrameIdx.Memory, CmdBuffer->FrameIdx.At);
    DeviceCtx->Unmap(Backend->IdxBuffer, 0);

    Error = SetupD3DRenderState(ClientWidth, ClientHeight, Backend);
    D3DReturnVoid(Error)

    for (cim_u32 CmdIdx = 0; CmdIdx < CmdBuffer->CommandCount; CmdIdx++)
    {
        ui_draw_command *Command  = CmdBuffer->Commands + CmdIdx;
        d3d_pipeline     *Pipeline = &Pipelines[Command->PipelineType];

        // BUG: When drawing text we never bind the texture, since we don't even know how.
        // What do we even do then? We need the access to the font associated with the pipeline.
        // Let's just do something really bad for now. And maybe refactor this to command based
        // renderer?

        DeviceCtx->IASetInputLayout(Pipeline->Layout);
        DeviceCtx->IASetVertexBuffers(0, 1, &Backend->VtxBuffer, &Pipeline->Stride, (UINT *)&Command->VertexByteOffset);

        DeviceCtx->VSSetShader(Pipeline->VtxShader, NULL, 0);
        if (Command->PipelineType == UIPipeline_Text)
        {
            DeviceCtx->PSSetShaderResources(0, 1, &Command->Font.FontObjects->GlyphCacheView);
        }

        DeviceCtx->PSSetShader(Pipeline->PxlShader, NULL, 0);

        DeviceCtx->DrawIndexed(Command->IdxCount, Command->StartIndexRead, Command->BaseVertexOffset);
    }

    CimArena_Reset(&CmdBuffer->FrameVtx);
    CimArena_Reset(&CmdBuffer->FrameIdx);
    CmdBuffer->CommandCount = 0;
}

static void
D3DTransferGlyph(stbrp_rect Rect, ui_font Font)
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

        Backend->DeviceContext->CopySubresourceRegion(Font.FontObjects->GlyphCache, 0, Rect.x, Rect.y, 0,
                                                      Font.FontObjects->GlyphTransfer, 0, &SourceBox);
    }
}

#endif