#pragma once

// [Macros and linking]

#define COBJMACROS
DisableWarning(4201)
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

// [Core Types]

typedef struct d3d11_rect_uniform_buffer
{
    vec4_f32 Transform[3];
    vec2_f32 ViewportSize;
} d3d11_rect_uniform_buffer;

typedef struct d3d11_input_layout
{
    read_only D3D11_INPUT_ELEMENT_DESC *Desc;
              u32                       Count;
} d3d11_input_layout;

typedef struct d3d11_backend
{
    // Base Objects
    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;

    // Buffers
    ID3D11Buffer *VBuffer64KB;

    // Pipelines
    ID3D11InputLayout  *ILayouts[RenderPass_Type_Count];
    ID3D11VertexShader *VShaders[RenderPass_Type_Count];
    ID3D11PixelShader  *PShaders[RenderPass_Type_Count];
    ID3D11Buffer       *UBuffers[RenderPass_Type_Count];

    // State
    vec2_i32 LastResolution;
} d3d11_backend;

// [Globals]

read_only global D3D11_INPUT_ELEMENT_DESC D3D11RectILayout[] =
{
    {"POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
};

read_only global u8 D3D11RectVShader[] =
"cbuffer Constants : register(b0)                                                              \n"
"{                                                                                             \n"
"    float3x3 Transform;                                                                       \n"
"    float2   ViewportSizePixel;                                                               \n"
"};                                                                                            \n"
"                                                                                              \n"
"struct VertexInput                                                                            \n"
"{                                                                                             \n"
"    float4 DestRectPixel : POS;                                                               \n"
"    float4 Col           : COL;                                                               \n"
"    uint   VertexId      : SV_VertexID;                                                       \n"
"};                                                                                            \n"
"                                                                                              \n"
"struct VertexOutput                                                                           \n"
"{                                                                                             \n"
"   float4 Position : SV_POSITION;                                                             \n"
"   float4 Col      : COLOR;                                                                   \n"
"};                                                                                            \n"
"                                                                                              \n"
"VertexOutput VSMain(VertexInput Input)                                                        \n"
"{                                                                                             \n"
"    float2 TopL_Dest_Pixel = Input.DestRectPixel.xy;                                          \n"
"    float2 BotR_Dest_Pixel = Input.DestRectPixel.zw;                                          \n"
"                                                                                              \n"
"    float2 CornerPositionPixel[] =                                                            \n"
"    {                                                                                         \n"
"        float2(TopL_Dest_Pixel.x, BotR_Dest_Pixel.y),                                         \n"
"        float2(TopL_Dest_Pixel.x, TopL_Dest_Pixel.y),                                         \n"
"        float2(BotR_Dest_Pixel.x, BotR_Dest_Pixel.y),                                         \n"
"        float2(BotR_Dest_Pixel.x, TopL_Dest_Pixel.y),                                         \n"
"    };                                                                                        \n"
"                                                                                              \n"
"    float2 Transformed = mul(Transform, float3(CornerPositionPixel[Input.VertexId], 1.f)).xy; \n"
"    Transformed.y = ViewportSizePixel.y - Transformed.y;                                      \n"
"                                                                                              \n"
"    VertexOutput Output;                                                                      \n"
"    Output.Position.xy = ((2.f * Transformed) / ViewportSizePixel) - 1.f;                     \n"
"    Output.Position.z  = 0.f;                                                                 \n"
"    Output.Position.w  = 1.f;                                                                 \n"
"    Output.Col         = Input.Col;                                                           \n"
"                                                                                              \n"
"    return Output;                                                                            \n"
"}                                                                                             \n"
;

read_only global u8 D3D11RectPShader[] =
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

read_only global byte_string D3D11VShaderSourceTable[] =
{
    byte_string_compile(D3D11RectVShader),
};

read_only global byte_string D3D11PShaderSourceTable[] =
{
    byte_string_compile(D3D11RectPShader),
};

read_only global d3d11_input_layout D3D11ILayoutTable[] =
{
    {D3D11RectILayout, ArrayCount(D3D11RectILayout)},
};

read_only global u32 D3D11UniformBufferSizeTable[] =
{
    {(u32)sizeof(d3d11_rect_uniform_buffer)},
};