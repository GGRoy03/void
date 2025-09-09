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

read_only global u8 D3D11RectVShader[] =
"cbuffer PerFrame : register(b0)                                        \n"
"{                                                                      \n"
"    matrix SpaceMatrix;                                                \n"
"};                                                                     \n"
"                                                                       \n"
"struct VertexInput                                                     \n"
"{                                                                      \n"
"    float2 Pos : POSITION;                                             \n"
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

read_only global shader_source D3D11VShaderSourceTable[] =
{
    {D3D11RectVShader, sizeof(D3D11RectVShader)},
};

read_only global shader_source D3D11PShaderSourceTable[] =
{
    {D3D11RectPShader, sizeof(D3D11RectPShader)},
};

