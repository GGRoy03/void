#pragma once

// [Macros and linking]

#define COBJMACROS
#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

// [Core Types]

typedef struct gpu_font_context
{
    ID3D11Texture2D          *GlyphCache;
    ID3D11ShaderResourceView *GlyphCacheView;

    ID3D11Texture2D          *GlyphTransfer;
    ID3D11ShaderResourceView *GlyphTransferView;
    IDXGISurface             *GlyphTransferSurface;
} gpu_font_context;

typedef struct d3d11_rect_uniform_buffer
{
    vec4_float Transform[3];
    vec2_float ViewportSizeInPixel;
    vec2_float AtlasSizeInPixel;
} d3d11_rect_uniform_buffer;

typedef struct d3d11_input_layout
{
    const D3D11_INPUT_ELEMENT_DESC *Desc;
              uint32_t                       Count;
} d3d11_input_layout;

typedef struct d3d11_renderer
{
    // Base Objects
    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;
    ID3D11BlendState       *DefaultBlendState;
    ID3D11SamplerState     *AtlasSamplerState;

    // Buffers
    ID3D11Buffer *VBuffer64KB;

    // Pipelines
    ID3D11InputLayout     *ILayouts[RenderPass_Count];
    ID3D11VertexShader    *VShaders[RenderPass_Count];
    ID3D11PixelShader     *PShaders[RenderPass_Count];
    ID3D11Buffer          *UBuffers[RenderPass_Count];
    ID3D11RasterizerState *RasterSt[RenderPass_Count];

    // State
    vec2_int LastResolution;
} d3d11_renderer;

// [Globals]

const static D3D11_INPUT_ELEMENT_DESC D3D11RectILayout[] =
{
    {"POS" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"FONT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COL" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"CORR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"STY" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
};

const static uint8_t D3D11RectShader[] =
"// [Inputs/Outputs]                                                                               \n"
"                                                                                                  \n"
"cbuffer Constants : register(b0)                                                                  \n"
"{                                                                                                 \n"
"    float3x3 Transform;                                                                           \n"
"    float2   ViewportSizeInPixel;                                                                 \n"
"    float2   AtlasSizeInPixel;                                                                    \n" 
"};                                                                                                \n"
"                                                                                                  \n"
"struct CPUToVertex                                                                                \n"
"{                                                                                                 \n"
"    float4 RectInPixel         : POS;                                                             \n"
"    float4 AtlasSrcInPixel     : FONT;                                                            \n"
"    float4 Color               : COL;                                                             \n"
"    float4 CornderRadiiInPixel : CORR;                                                            \n"
"    float4 StyleParams         : STY;                                                             \n" // X: BorderWidth, Y: Softness, Z: Sample Atlas
"    uint   VertexId            : SV_VertexID;                                                     \n"
"};                                                                                                \n"
"                                                                                                  \n"
"struct VertexToPixel                                                                              \n"
"{                                                                                                 \n"
"   float4 Position            : SV_POSITION;                                                      \n"
"   float4 Color               : COL;                                                              \n"
"   float2 TexCoordInPercent   : TXP;                                                              \n"
"   float2 SDFSamplePos        : SDF;                                                              \n"
"   float  CornerRadiusInPixel : COR;                                                              \n"
"                                                                                                  \n"
"   nointerpolation float2 RectHalfSizeInPixel : RHS;                                              \n"
"   nointerpolation float  SoftnessInPixel     : SFT;                                              \n"
"   nointerpolation float  BorderWidthInPixel  : BDW;                                              \n"
"   nointerpolation float  MustSampleAtlas     : MSA;                                              \n"
"};                                                                                                \n"
"                                                                                                  \n"
"Texture2D    AtlasTexture : register(t0);                                                         \n"
"SamplerState AtlasSampler : register(s0);                                                         \n"
"                                                                                                  \n"
"// [Helpers]                                                                                      \n"
"                                                                                                  \n"
"float RectSDF(float2 SamplePosition, float2 RectHalfSize, float Radius)                           \n" // Everything is solved in the first quadrant because of symmetry on both axes.
"{                                                                                                 \n"
"    float2 FirstQuadrantPos = abs(SamplePosition) - RectHalfSize + Radius;                        \n" // Quadrant Folding Inwards offset by corner radius | > 0 == Outside
"    float  OuterDistance    = length(max(FirstQuadrantPos, 0.0));                                 \n" // > 0 == Outside, on each axis. Length(p) == Closest Edge from outside
"    float  InnerDistance    = min(max(FirstQuadrantPos.x, FirstQuadrantPos.y), 0.0);              \n" // < = == Inside. If any axis > 0, Inner == 0.
"                                                                                                  \n"
"    return OuterDistance + InnerDistance - Radius;                                                \n"
"}                                                                                                 \n" 
"                                                                                                  \n"
"VertexToPixel VSMain(CPUToVertex Input)                                                           \n"
"{                                                                                                 \n"
"    float2 RectTopLeftInPixel   = Input.RectInPixel.xy;                                           \n"
"    float2 RectBotRightInPixel  = Input.RectInPixel.zw;                                           \n"
"    float2 AtlasTopLeftInPixel  = Input.AtlasSrcInPixel.xy;                                       \n"
"    float2 AtlasBotRightInPixel = Input.AtlasSrcInPixel.zw;                                       \n"
"    float2 RectSizeInPixel      = abs(RectBotRightInPixel - RectTopLeftInPixel);                  \n"
"                                                                                                  \n"
"    float2 CornerPositionInPixel[] =                                                              \n"
"    {                                                                                             \n"
"        float2(RectTopLeftInPixel.x , RectBotRightInPixel.y),                                     \n"
"        float2(RectTopLeftInPixel.x , RectTopLeftInPixel.y ),                                     \n"
"        float2(RectBotRightInPixel.x, RectBotRightInPixel.y),                                     \n"
"        float2(RectBotRightInPixel.x, RectTopLeftInPixel.y ),                                     \n"
"    };                                                                                            \n"
"                                                                                                  \n"
"    float CornderRadiusInPixel[] =                                                                \n"
"    {                                                                                             \n"
"        Input.CornderRadiiInPixel.y,                                                              \n"
"        Input.CornderRadiiInPixel.x,                                                              \n"
"        Input.CornderRadiiInPixel.w,                                                              \n"
"        Input.CornderRadiiInPixel.z,                                                              \n"
"    };                                                                                            \n"
"                                                                                                  \n"
"    float2 AtlasSourceInPixel[] =                                                                 \n"
"    {                                                                                             \n"
"        float2(AtlasTopLeftInPixel.x , AtlasBotRightInPixel.y),                                   \n"
"        float2(AtlasTopLeftInPixel.x , AtlasTopLeftInPixel.y ),                                   \n"
"        float2(AtlasBotRightInPixel.x, AtlasBotRightInPixel.y),                                   \n"
"        float2(AtlasBotRightInPixel.x, AtlasTopLeftInPixel.y ),                                   \n"
"    };                                                                                            \n"
"                                                                                                  \n"
"    float2 CornerAxisPercent;                                                                     \n"
"    CornerAxisPercent.x = (Input.VertexId >> 1) ? 1.f : 0.f;                                      \n"
"    CornerAxisPercent.y = (Input.VertexId &  1) ? 0.f : 1.f;                                      \n"
"                                                                                                  \n"
"    float2 Transformed = mul(Transform, float3(CornerPositionInPixel[Input.VertexId], 1.f)).xy;   \n"
"    Transformed.y = ViewportSizeInPixel.y - Transformed.y;                                        \n"
"                                                                                                  \n"
"    VertexToPixel Output;                                                                         \n"
"    Output.Position.xy         = ((2.f * Transformed) / ViewportSizeInPixel) - 1.f;               \n"
"    Output.Position.z          = 0.f;                                                             \n"
"    Output.Position.w          = 1.f;                                                             \n"
"    Output.CornerRadiusInPixel = CornderRadiusInPixel[Input.VertexId];                            \n"
"    Output.BorderWidthInPixel  = Input.StyleParams.x;                                             \n"
"    Output.SoftnessInPixel     = Input.StyleParams.y;                                             \n"
"    Output.RectHalfSizeInPixel = RectSizeInPixel / 2.f;                                           \n"
"    Output.SDFSamplePos        = (2.f * CornerAxisPercent - 1.f) * Output.RectHalfSizeInPixel;    \n"
"    Output.Color               = Input.Color;                                                     \n"
"    Output.MustSampleAtlas     = Input.StyleParams.z;                                             \n"
"    Output.TexCoordInPercent   = AtlasSourceInPixel[Input.VertexId] / AtlasSizeInPixel;           \n"
"                                                                                                  \n"
"    return Output;                                                                                \n"
"}                                                                                                 \n"
"                                                                                                  \n"
"float4 PSMain(VertexToPixel Input) : SV_TARGET                                                    \n"
"{                                                                                                 \n"
"                                                                                                  \n"
"    float4 AlbedoSample = float4(1, 1, 1, 1);                                                     \n"
"    if(Input.MustSampleAtlas > 0)                                                                 \n"
"    {                                                                                             \n"
"        AlbedoSample = AtlasTexture.Sample(AtlasSampler, Input.TexCoordInPercent);                \n"
"    }                                                                                             \n"
"                                                                                                  \n"
"    float BorderSDF = 1;                                                                          \n"
"    if(Input.BorderWidthInPixel > 0)                                                              \n"
"    {                                                                                             \n"
"        float2 SamplePosition = Input.SDFSamplePos;                                               \n"
"        float2 Softness       = float2(2.f * Input.SoftnessInPixel, 2.f * Input.SoftnessInPixel); \n"
"        float2 RectHalfSize   = Input.RectHalfSizeInPixel - Input.BorderWidthInPixel - Softness;  \n"
"        float  Radius         = max(Input.CornerRadiusInPixel - Input.BorderWidthInPixel, 0);     \n" // Prevent negative radius
"                                                                                                  \n"
"        BorderSDF = RectSDF(SamplePosition, RectHalfSize, Radius);                                \n"
"        BorderSDF = smoothstep(0, 2 * Input.SoftnessInPixel, BorderSDF);                          \n" // 0->1 clamping based on softness. BorderSDF == 0 if inside, BorderSDF == 1 if far.
"    }                                                                                             \n"
"                                                                                                  \n"
"    if(BorderSDF < 0.001f) discard;                                                               \n" // If it is not part of the border mask discard it (Hollow Center)
"                                                                                                  \n"
"    float CornerSDF = 1;                                                                          \n"
"    if(Input.CornerRadiusInPixel > 0 || Input.SoftnessInPixel > 0.75f)                            \n"
"    {                                                                                             \n"
"        float2 SamplePosition = Input.SDFSamplePos;                                               \n"
"        float2 Softness       = float2(2.f * Input.SoftnessInPixel, 2.f * Input.SoftnessInPixel); \n"
"        float2 RectHalfSize   = Input.RectHalfSizeInPixel - Softness;                             \n"
"        float  CornerRadius   = Input.CornerRadiusInPixel;                                        \n"
"                                                                                                  \n"
"        CornerSDF = RectSDF(SamplePosition, RectHalfSize, CornerRadius);                          \n"
"        CornerSDF = 1.0 - smoothstep(0, 2 * Input.SoftnessInPixel, CornerSDF);                    \n" // 0->1 clamping based on softness. == 1 if inside, < 1 if outside.
"    }                                                                                             \n"
"                                                                                                  \n"
"    float4 Output = AlbedoSample;                                                                 \n"
"    Output   *= Input.Color;                                                                      \n"
"    Output.a *= CornerSDF;                                                                        \n"
"    Output.a *= BorderSDF;                                                                        \n"
"                                                                                                  \n"
"    return Output;                                                                                \n"
"}                                                                                                 \n"
;


const static byte_string D3D11ShaderSourceTable[] =
{
    byte_string_compile(D3D11RectShader),
};

const static d3d11_input_layout D3D11ILayoutTable[] =
{
    {D3D11RectILayout, VOID_ARRAYCOUNT(D3D11RectILayout)},
};

const static uint64_t D3D11UniformBufferSizeTable[] =
{
    sizeof(d3d11_rect_uniform_buffer),
};
