// ------------------------------------------------------------------------------------
// Private Helpers

typedef struct d3d11_format
{
    DXGI_FORMAT Native;
    u32         BytesPerPixel;
} d3d11_format;

internal d3d11_renderer *
D3D11GetRenderer(render_handle HRenderer)
{
    d3d11_renderer *Result = (d3d11_renderer *)HRenderer.u64[0];
    return Result;
}

internal ID3D11Texture2D *
D3D11GetTexture2D(render_handle Handle)
{
    ID3D11Texture2D *Result = (ID3D11Texture2D *)Handle.u64[0];
    return Result;
}

internal ID3D11ShaderResourceView *
D3D11GetShaderView(render_handle Handle)
{
    ID3D11ShaderResourceView *Result = (ID3D11ShaderResourceView *)Handle.u64[0];
    return Result;
}

internal ID3D11Buffer *
D3D11GetVertexBuffer(u64 Size, d3d11_renderer *Renderer)
{
    ID3D11Buffer *Result = Renderer->VBuffer64KB;

    if(Size > Kilobyte(64))
    {
    }

    return Result;
}

internal void
D3D11Release(d3d11_renderer *Renderer)
{
    Assert(Renderer);

    if (Renderer->Device)
    {
        ID3D11Device_Release(Renderer->Device);
        Renderer->Device = 0;
    }

    if (Renderer->DeviceContext)
    {
        ID3D11DeviceContext_Release(Renderer->DeviceContext);
        Renderer->DeviceContext = 0;
    }

    if (Renderer->RenderView)
    {
        ID3D11RenderTargetView_Release(Renderer->RenderView);
        Renderer->RenderView = 0;
    }

    if (Renderer->SwapChain)
    {
       IDXGISwapChain1_Release(Renderer->SwapChain);
       Renderer->SwapChain = 0;
    }
}

internal d3d11_format
D3D11GetFormat(RenderTexture_Type Type)
{
    d3d11_format Result = {0};

    switch(Type)
    {
        case RenderTexture_RGBA:
        {
            Result.Native        = DXGI_FORMAT_R8G8B8A8_UNORM;
            Result.BytesPerPixel = 4;
        } break;

        default:
        {
            Assert(!"Invalid Format Type");
        } break;
    }

    return Result;
}

internal D3D11_TEXTURE_ADDRESS_MODE
D3D11GetTextureAddressMode(RenderTexture_Wrap Type)
{
    D3D11_TEXTURE_ADDRESS_MODE Result = 0;

    switch(Type)
    {
        case RenderTextureWrap_Clamp:  Result = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
        case RenderTextureWrap_Repeat: Result = D3D11_TEXTURE_ADDRESS_WRAP;   break;
        case RenderTextureWrap_Mirror: Result = D3D11_TEXTURE_ADDRESS_MIRROR; break;
    }

    return Result;
}

// [PER-RENDERER API]

internal render_handle
InitializeRenderer(void *HWindow, vec2_i32 Resolution, memory_arena *Arena)
{
    render_handle   Result   = { 0 };
    d3d11_renderer *Renderer = PushArray(Arena, d3d11_renderer, 1); 
    HRESULT         Error    = S_OK;

    {
        UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        #ifndef NDEBUG
        CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif
        D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0 };
        Error = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                  CreateFlags, Levels, ARRAYSIZE(Levels),
                                  D3D11_SDK_VERSION, &Renderer->Device, NULL, &Renderer->DeviceContext);
        if (FAILED(Error))
        {
            return Result;
        }
    }

    // TODO: Handle DPI
    {
        // HWND Handle       = OSWin32GetWindowHandle();
        // UINT DPIForWindow = GetDpiForWindow(Handle);
    }

    {
        IDXGIDevice *DXGIDevice = 0;
        Error = ID3D11Device_QueryInterface(Renderer->Device, &IID_IDXGIDevice, (void **)&DXGIDevice);
        if (DXGIDevice)
        {
            IDXGIAdapter *DXGIAdapter = 0;
            IDXGIDevice_GetAdapter(DXGIDevice, &DXGIAdapter);
            if (DXGIAdapter)
            {
                IDXGIFactory2 *Factory = 0;
                IDXGIAdapter_GetParent(DXGIAdapter, &IID_IDXGIFactory2, (void **)&Factory);
                if (Factory)
                {
                    HWND Window = (HWND)HWindow;

                    DXGI_SWAP_CHAIN_DESC1 Desc = { 0 };
                    Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
                    Desc.SampleDesc.Count = 1;
                    Desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    Desc.BufferCount      = 2;
                    Desc.Scaling          = DXGI_SCALING_NONE;
                    Desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                    Error = IDXGIFactory2_CreateSwapChainForHwnd(Factory, (IUnknown *)Renderer->Device,
                                                                 Window, &Desc, 0, 0,
                                                                 &Renderer->SwapChain);

                    IDXGIFactory_MakeWindowAssociation(Factory, Window, DXGI_MWA_NO_ALT_ENTER);
                    IDXGIFactory2_Release(Factory);
                }

                IDXGIAdapter_Release(DXGIAdapter);
            }

            IDXGIDevice_Release(DXGIDevice);
        }

        if (FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Render Target View
    {
        ID3D11Texture2D *BackBuffer = 0;
        Error = IDXGISwapChain1_GetBuffer(Renderer->SwapChain, 0, &IID_ID3D11Texture2D, (void **)&BackBuffer);
        if(BackBuffer)
        {
            Error = ID3D11Device_CreateRenderTargetView(Renderer->Device, (ID3D11Resource*)BackBuffer, NULL, &Renderer->RenderView);

            ID3D11Texture2D_Release(BackBuffer);
        }

        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Buffers
    {
        D3D11_BUFFER_DESC Desc = {0};
        Desc.ByteWidth      = Kilobyte(64);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Renderer->Device->lpVtbl->CreateBuffer(Renderer->Device, &Desc, NULL, &Renderer->VBuffer64KB);
        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Shaders
    {
        ID3D11Device       *Device  = Renderer->Device;
        ID3D11InputLayout  *ILayout = 0;
        ID3D11VertexShader *VShader = 0;
        ID3D11PixelShader  *PShader = 0;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *VShaderSrcBlob = 0;
            ID3DBlob *VShaderErrBlob = 0;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "VSMain", "vs_5_0",
                               0, 0, &VShaderSrcBlob, &VShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = VShaderSrcBlob->lpVtbl->GetBufferPointer(VShaderSrcBlob);
            u64   ByteSize = VShaderSrcBlob->lpVtbl->GetBufferSize(VShaderSrcBlob);
            Error = Device->lpVtbl->CreateVertexShader(Device, ByteCode, ByteSize, 0, &VShader);
            if(FAILED(Error))
            {
            }

            d3d11_input_layout Layout = D3D11ILayoutTable[Type];
            Error = Device->lpVtbl->CreateInputLayout(Device, Layout.Desc, Layout.Count,
                                                      ByteCode, ByteSize,
                                                      &ILayout);
            if(FAILED(Error))
            {
            }

            VShaderSrcBlob->lpVtbl->Release(VShaderSrcBlob);

            Renderer->VShaders[Type] = VShader;
            Renderer->ILayouts[Type] = ILayout;
        }

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *PShaderSrcBlob = 0;
            ID3DBlob *PShaderErrBlob = 0;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "PSMain", "ps_5_0",
                               0, 0, &PShaderSrcBlob, &PShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = PShaderSrcBlob->lpVtbl->GetBufferPointer(PShaderSrcBlob);
            u64   ByteSize = PShaderSrcBlob->lpVtbl->GetBufferSize(PShaderSrcBlob);
            Error = Device->lpVtbl->CreatePixelShader(Device, ByteCode, ByteSize, 0, &PShader);
            if (FAILED(Error))
            {
            }

            PShaderSrcBlob->lpVtbl->Release(PShaderSrcBlob);

            Renderer->PShaders[Type] = PShader;
        }
    }

    // Uniform Buffers
    {
        ID3D11Device *Device = Renderer->Device;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            ID3D11Buffer *Buffer = 0;

            D3D11_BUFFER_DESC Desc = {0};
            Desc.ByteWidth      = D3D11UniformBufferSizeTable[Type];
            Desc.Usage          = D3D11_USAGE_DYNAMIC;
            Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            AlignMultiple(Desc.ByteWidth, 16);

            Device->lpVtbl->CreateBuffer(Device, &Desc, 0, &Buffer);

            Renderer->UBuffers[Type] = Buffer;
        }
    }

    // Blending
    {
        D3D11_BLEND_DESC BlendDesc = {0};
        BlendDesc.RenderTarget[0].BlendEnable           = TRUE;
        BlendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        BlendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        BlendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        BlendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        BlendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(Renderer->Device->lpVtbl->CreateBlendState(Renderer->Device, &BlendDesc, &Renderer->DefaultBlendState)))
        {
        }
    }

    // Samplers
    {
        D3D11_SAMPLER_DESC Desc = {0};
        Desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        Desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.MaxAnisotropy  = 1;
        Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        Desc.MaxLOD         = D3D11_FLOAT32_MAX;

        Error = Renderer->Device->lpVtbl->CreateSamplerState(Renderer->Device, &Desc, &Renderer->AtlasSamplerState);
        if (FAILED(Error))
        {
        }
    }

    // Raster State
    {
        D3D11_RASTERIZER_DESC Desc = {0};
        Desc.FillMode              = D3D11_FILL_SOLID;
        Desc.CullMode              = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable       = TRUE;
        Desc.ScissorEnable         = TRUE; 
        Desc.MultisampleEnable     = FALSE;
        Desc.AntialiasedLineEnable = FALSE;

        Error = Renderer->Device->lpVtbl->CreateRasterizerState(Renderer->Device, &Desc, &Renderer->RasterSt[RenderPass_UI]);
        if(FAILED(Error))
        {
        }
    }

    // Default State
    {
        Renderer->LastResolution = Resolution;
    }

    Result.u64[0] = (u64)Renderer;
    return Result;
}

internal void 
SubmitRenderCommands(render_handle HRenderer, vec2_i32 Resolution, render_pass_list *RenderPassList)
{
    d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

    if(!Renderer || !RenderPassList)
    {
        return;
    }

    ID3D11DeviceContext    *DeviceContext = Renderer->DeviceContext;
    ID3D11RenderTargetView *RenderView    = Renderer->RenderView; 
    IDXGISwapChain1        *SwapChain     = Renderer->SwapChain;

    // Update State
    vec2_i32 CurrentResolution = Renderer->LastResolution;
    {
        if (!Vec2I32IsEqual(Resolution, CurrentResolution))
        {
            // TODO: Resize swap chain and whatever else we need.

            Resolution = CurrentResolution;
        }
    }

    // Temporary Clear Screen
    {
        const FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        DeviceContext->lpVtbl->ClearRenderTargetView(DeviceContext, RenderView, ClearColor);
    }

    for (render_pass_node *PassNode = RenderPassList->First; PassNode != 0; PassNode = PassNode->Next)
    {
        render_pass Pass = PassNode->Value;

        switch (Pass.Type)
        {

        case RenderPass_UI:
        {
            render_pass_params_ui Params = Pass.Params.UI.Params;

            // Rasterizer
            D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (f32)Resolution.X, (f32)Resolution.Y, 0.0f, 1.0f };
            DeviceContext->lpVtbl->RSSetState(DeviceContext, Renderer->RasterSt[RenderPass_UI]);
            DeviceContext->lpVtbl->RSSetViewports(DeviceContext, 1, &Viewport);

            for (rect_group_node *Node = Params.First; Node != 0; Node = Node->Next)
            {
                render_batch_list BatchList  = Node->BatchList;
                rect_group_params NodeParams = Node->Params;

                // Vertex Buffer
                ID3D11Buffer *VBuffer = D3D11GetVertexBuffer(BatchList.ByteCount, Renderer);
                {
                    D3D11_MAPPED_SUBRESOURCE Resource = { 0 };
                    DeviceContext->lpVtbl->Map(DeviceContext, (ID3D11Resource *)VBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);

                    u8 *WritePointer = Resource.pData;
                    u64 WriteOffset  = 0;
                    for (render_batch_node *Batch = BatchList.First; Batch != 0; Batch = Batch->Next)
                    {
                        u64 WriteSize = Batch->Value.ByteCount;
                        MemoryCopy(WritePointer + WriteOffset, Batch->Value.Memory, WriteSize);
                        WriteOffset += WriteSize;
                    }

                    DeviceContext->lpVtbl->Unmap(DeviceContext, (ID3D11Resource *)VBuffer, 0);
                }

                // Uniform Buffers
                ID3D11Buffer *UniformBuffer = Renderer->UBuffers[RenderPass_UI];
                {
                    d3d11_rect_uniform_buffer Uniform = { 0 };
                    Uniform.Transform[0]        = Vec4F32(NodeParams.Transform.c0r0, NodeParams.Transform.c0r1, NodeParams.Transform.c0r2, 0);
                    Uniform.Transform[1]        = Vec4F32(NodeParams.Transform.c1r0, NodeParams.Transform.c1r1, NodeParams.Transform.c1r2, 0);
                    Uniform.Transform[2]        = Vec4F32(NodeParams.Transform.c2r0, NodeParams.Transform.c2r1, NodeParams.Transform.c2r2, 0);
                    Uniform.ViewportSizeInPixel = Vec2F32((f32)Resolution.X, (f32)Resolution.Y);
                    Uniform.AtlasSizeInPixel    = NodeParams.TextureSize;

                    D3D11_MAPPED_SUBRESOURCE Resource = { 0 };
                    DeviceContext->lpVtbl->Map(DeviceContext, (ID3D11Resource *)UniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
                    MemoryCopy(Resource.pData, &Uniform, sizeof(Uniform));
                    DeviceContext->lpVtbl->Unmap(DeviceContext, (ID3D11Resource *)UniformBuffer, 0);
                }

                // Pipeline Info
                ID3D11InputLayout        *ILayout   = Renderer->ILayouts[RenderPass_UI];
                ID3D11VertexShader       *VShader   = Renderer->VShaders[RenderPass_UI];
                ID3D11PixelShader        *PShader   = Renderer->PShaders[RenderPass_UI];
                ID3D11ShaderResourceView *AtlasView = D3D11GetShaderView(NodeParams.Texture);

                // OM
                DeviceContext->lpVtbl->OMSetRenderTargets(DeviceContext, 1, &Renderer->RenderView, 0);
                DeviceContext->lpVtbl->OMSetBlendState(DeviceContext, Renderer->DefaultBlendState, 0, 0xFFFFFFFF);

                // IA
                u32 Stride = (u32)BatchList.BytesPerInstance;
                u32 Offset = 0;
                DeviceContext->lpVtbl->IASetVertexBuffers(DeviceContext, 0, 1, &VBuffer, &Stride, &Offset);
                DeviceContext->lpVtbl->IASetInputLayout(DeviceContext, ILayout);
                DeviceContext->lpVtbl->IASetPrimitiveTopology(DeviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                // Shaders
                DeviceContext->lpVtbl->VSSetShader(DeviceContext, VShader, 0, 0);
                DeviceContext->lpVtbl->VSSetConstantBuffers(DeviceContext, 0, 1, &UniformBuffer);
                DeviceContext->lpVtbl->PSSetShader(DeviceContext, PShader, 0, 0);
                DeviceContext->lpVtbl->PSSetShaderResources(DeviceContext, 0, 1, &AtlasView);
                DeviceContext->lpVtbl->PSSetSamplers(DeviceContext, 0, 1, &Renderer->AtlasSamplerState);

                // Scissor
                {
                    rect_f32   Clip = NodeParams.Clip;
                    D3D11_RECT Rect = {0};

                    if (Clip.Min.X == 0 && Clip.Min.Y == 0 && Clip.Max.X == 0 && Clip.Max.Y == 0)
                    {
                        Rect.left   = 0;
                        Rect.right  = Resolution.X;
                        Rect.top    = 0;
                        Rect.bottom = Resolution.Y;
                    } else
                    if (Clip.Min.X > Clip.Max.X || Clip.Min.Y > Clip.Max.Y)
                    {
                        Rect.left   = 0;
                        Rect.right  = 0;
                        Rect.top    = 0;
                        Rect.bottom = 0;
                    }
                    else
                    {
                        Rect.left   = Clip.Min.X;
                        Rect.right  = Clip.Max.X;
                        Rect.top    = Clip.Min.Y;
                        Rect.bottom = Clip.Max.Y;
                    }

                    DeviceContext->lpVtbl->RSSetScissorRects(DeviceContext, 1, &Rect);
                }

                // Draw
                u32 InstanceCount = (u32)(BatchList.ByteCount / BatchList.BytesPerInstance);
                DeviceContext->lpVtbl->DrawInstanced(DeviceContext, 4, InstanceCount, 0, 0);
            }
        } break;

        default: break;

        }
    }

    // Present
    HRESULT Error = SwapChain->lpVtbl->Present(SwapChain, 0, 0);
    if (FAILED(Error))
    {
    }
    DeviceContext->lpVtbl->ClearState(DeviceContext);

    // Clear
    RenderPassList->First = 0;
    RenderPassList->Last  = 0;
}

// -----------------------------------------------------------------------------------
// Glyph Public Implementation

internal b32
IsValidGPUFontContext(gpu_font_context *Context)
{
    b32 Result = (Context) && (Context->GlyphCache) && (Context->GlyphCacheView) && (Context->GlyphTransfer) && (Context->GlyphTransferView) && (Context->GlyphTransferSurface);
    return Result;
}

internal b32
CreateGlyphCache(render_handle HRenderer, vec2_f32 TextureSize, gpu_font_context *FontContext)
{
    b32 Result  = 0;

    if (IsValidRenderHandle(HRenderer))
    {
        d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

        D3D11_TEXTURE2D_DESC Desc = { 0 };
        Desc.Width            = (UINT)TextureSize.X;
        Desc.Height           = (UINT)TextureSize.Y;
        Desc.MipLevels        = 1;
        Desc.ArraySize        = 1;
        Desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.Usage            = D3D11_USAGE_DEFAULT;
        Desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        Renderer->Device->lpVtbl->CreateTexture2D(Renderer->Device, &Desc, 0, &FontContext->GlyphCache);
        if (FontContext->GlyphCache)
        {
            Renderer->Device->lpVtbl->CreateShaderResourceView(Renderer->Device, (ID3D11Resource *)FontContext->GlyphCache, 0, &FontContext->GlyphCacheView);
            Result = (FontContext->GlyphCacheView != 0);
        }
    }

    return Result;
}

internal void
ReleaseGlyphCache(gpu_font_context *FontContext)
{
    if (FontContext)
    {
        if (FontContext->GlyphCache)
        {
            FontContext->GlyphCache->lpVtbl->Release(FontContext->GlyphCache);
            FontContext->GlyphCache = 0;
        }

        if (FontContext->GlyphCacheView)
        {
            FontContext->GlyphCacheView->lpVtbl->Release(FontContext->GlyphCacheView);
            FontContext->GlyphCacheView = 0;
        }
    }
}

internal b32
CreateGlyphTransfer(render_handle HRenderer, vec2_f32 TextureSize, gpu_font_context *FontContext)
{
    b32 Result  = 0;

    if (IsValidRenderHandle(HRenderer))
    {
        d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

        D3D11_TEXTURE2D_DESC Desc = {0};
        Desc.Width            = (UINT)TextureSize.X;
        Desc.Height           = (UINT)TextureSize.Y;
        Desc.MipLevels        = 1;
        Desc.ArraySize        = 1;
        Desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.Usage            = D3D11_USAGE_DEFAULT;
        Desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        Renderer->Device->lpVtbl->CreateTexture2D(Renderer->Device, &Desc, 0, &FontContext->GlyphTransfer);
        if (FontContext->GlyphTransfer)
        {
            Renderer->Device->lpVtbl->CreateShaderResourceView(Renderer->Device, (ID3D11Resource *)FontContext->GlyphTransfer, 0, &FontContext->GlyphTransferView);
            if (FontContext->GlyphTransferView)
            {
                FontContext->GlyphTransfer->lpVtbl->QueryInterface(FontContext->GlyphTransfer, &IID_IDXGISurface, (void **)&FontContext->GlyphTransferSurface);
                Result = (FontContext->GlyphTransferSurface != 0);
            }
        }
    }

    return Result;
}

internal void
ReleaseGlyphTransfer(gpu_font_context *FontContext)
{
    if (FontContext)
    {
        if (FontContext->GlyphTransfer)
        {
            FontContext->GlyphTransfer->lpVtbl->Release(FontContext->GlyphTransfer);
            FontContext->GlyphTransfer = 0;
        }

        if (FontContext->GlyphTransferView)
        {
            FontContext->GlyphTransferView->lpVtbl->Release(FontContext->GlyphTransferView);
            FontContext->GlyphTransferView = 0;
        }

        if (FontContext->GlyphTransferSurface)
        {
            FontContext->GlyphTransferSurface->lpVtbl->Release(FontContext->GlyphTransferSurface);
            FontContext->GlyphTransferSurface = 0;
        }
    }
}

internal void
TransferGlyph(rect_f32 Rect, render_handle HRenderer, gpu_font_context *FontContext)
{
    if (IsValidRenderHandle(HRenderer) && IsValidGPUFontContext(FontContext))
    {
        d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

        D3D11_BOX SourceBox;
        SourceBox.left   = 0;
        SourceBox.top    = 0;
        SourceBox.front  = 0;
        SourceBox.right  = (UINT)Rect.Max.X;
        SourceBox.bottom = (UINT)Rect.Max.Y;
        SourceBox.back   = 1;

        Renderer->DeviceContext->lpVtbl->CopySubresourceRegion(Renderer->DeviceContext, (ID3D11Resource *)FontContext->GlyphCache,
                                                              0, (UINT)Rect.Min.X, (UINT)Rect.Min.Y, 0,
                                                              (ID3D11Resource *)FontContext->GlyphTransfer, 0, &SourceBox);
    }
}

// -----------------------------------------------------------------------------------
// Texture Private Implementation

internal b32
D3D11IsValidTexture(render_texture RenderTexture)
{
    b32 Result = IsValidRenderHandle(RenderTexture.Texture) && IsValidRenderHandle(RenderTexture.View) && !Vec2F32IsEmpty(RenderTexture.Size);
    return Result;
}

// -----------------------------------------------------------------------------------
// Texture Public Implementation

internal render_texture
CreateRenderTexture(render_texture_params Params)
{
    Assert(Params.Width > 0 && Params.Height > 0);

    d3d11_renderer *Backend = D3D11GetRenderer(RenderState.Renderer);
    Assert(Backend);

    d3d11_format Format = D3D11GetFormat(Params.Type);

    D3D11_TEXTURE2D_DESC TextureDesc = {0};
    TextureDesc.Width              = Params.Width;
    TextureDesc.Height             = Params.Height;
    TextureDesc.MipLevels          = Params.GenerateMipmaps ? 0 : 1;
    TextureDesc.ArraySize          = 1;
    TextureDesc.Format             = Format.Native;
    TextureDesc.SampleDesc.Count   = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage              = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags     = 0;
    TextureDesc.MiscFlags          = Params.GenerateMipmaps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

    D3D11_SUBRESOURCE_DATA  InitData        = {0};
    D3D11_SUBRESOURCE_DATA *InitDataPointer = 0;

    if(Params.InitialData)
    {
        InitData.pSysMem          = Params.InitialData;
        InitData.SysMemPitch      = Params.Width * Format.BytesPerPixel;
        InitData.SysMemSlicePitch = 0;

        InitDataPointer = &InitData;
    }

    ID3D11Texture2D *Texture = 0;
    HRESULT HR = ID3D11Device_CreateTexture2D(Backend->Device, &TextureDesc, InitDataPointer, &Texture);
    Assert(SUCCEEDED(HR));

    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc = {0};
    ViewDesc.Format                    = Format.Native;
    ViewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    ViewDesc.Texture2D.MostDetailedMip = 0;
    ViewDesc.Texture2D.MipLevels       = Params.GenerateMipmaps ? -1 : 1;

    ID3D11ShaderResourceView *View = 0;
    HR = ID3D11Device_CreateShaderResourceView(Backend->Device, (ID3D11Resource*)Texture, &ViewDesc, &View);
    Assert(SUCCEEDED(HR));

    if(Params.GenerateMipmaps && Params.InitialData)
    {
        ID3D11DeviceContext_GenerateMips(Backend->DeviceContext, View);
    }

    render_texture Result = {0};
    Result.Texture = RenderHandle((u64)Texture);
    Result.View    = RenderHandle((u64)View);
    Result.Size    = Vec2F32(Params.Width, Params.Height);

    return Result;
}

internal void
CopyIntoRenderTexture(render_texture RenderTexture, rect_f32 Source, u8 *Pixels, u32 Pitch)
{
    Assert(D3D11IsValidTexture(RenderTexture));
    Assert(Pixels);

    D3D11_BOX Box =
    {
        .left   = (UINT)Source.Min.X,
        .top    = (UINT)Source.Min.Y,
        .right  = (UINT)Source.Max.X,
        .bottom = (UINT)Source.Max.Y,
        .front  = 0,
        .back   = 1,
    };

    d3d11_renderer *Renderer = D3D11GetRenderer(RenderState.Renderer);
    Assert(Renderer);

    ID3D11Texture2D *Texture = D3D11GetTexture2D(RenderTexture.Texture);
    Assert(Texture);

    ID3D11DeviceContext_UpdateSubresource(Renderer->DeviceContext, (ID3D11Resource *)Texture, 0, &Box, Pixels, Pitch, 0);
}
