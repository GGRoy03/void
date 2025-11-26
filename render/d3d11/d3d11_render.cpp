// ------------------------------------------------------------------------------------
// Private Helpers

typedef struct d3d11_format
{
    DXGI_FORMAT Native;
    uint32_t         BytesPerPixel;
} d3d11_format;

static d3d11_renderer *
D3D11GetRenderer(render_handle HRenderer)
{
    d3d11_renderer *Result = (d3d11_renderer *)HRenderer.uint64_t[0];
    return Result;
}

static ID3D11Texture2D *
D3D11GetTexture2D(render_handle Handle)
{
    ID3D11Texture2D *Result = (ID3D11Texture2D *)Handle.uint64_t[0];
    return Result;
}

static ID3D11ShaderResourceView *
D3D11GetShaderView(render_handle Handle)
{
    ID3D11ShaderResourceView *Result = (ID3D11ShaderResourceView *)Handle.uint64_t[0];
    return Result;
}

static ID3D11Buffer *
D3D11GetVertexBuffer(uint64_t Size, d3d11_renderer *Renderer)
{
    ID3D11Buffer *Result = Renderer->VBuffer64KB;

    if(Size > VOID_KILOBYTE(64))
    {
    }

    return Result;
}

static void
D3D11Release(d3d11_renderer *Renderer)
{
    VOID_ASSERT(Renderer);

    if (Renderer->Device)
    {
        Renderer->Device->Release();
        Renderer->Device = nullptr;
    }

    if (Renderer->DeviceContext)
    {
        Renderer->DeviceContext->Release();
        Renderer->DeviceContext = nullptr;
    }

    if (Renderer->RenderView)
    {
        Renderer->RenderView->Release();
        Renderer->RenderView = nullptr;
    }

    if (Renderer->SwapChain)
    {
       Renderer->SwapChain->Release();
       Renderer->SwapChain = nullptr;
    }
}

static d3d11_format
D3D11GetFormat(RenderTexture_Type Type)
{
    d3d11_format Result = {};

    switch(Type)
    {
        case RenderTexture_RGBA:
        {
            Result.Native        = DXGI_FORMAT_R8G8B8A8_UNORM;
            Result.BytesPerPixel = 4;
        } break;

        default:
        {
            VOID_ASSERT(!"Invalid Format Type");
        } break;
    }

    return Result;
}

static D3D11_TEXTURE_ADDRESS_MODE
D3D11GetTextureAddressMode(RenderTexture_Wrap Type)
{
    D3D11_TEXTURE_ADDRESS_MODE Result = D3D11_TEXTURE_ADDRESS_CLAMP;

    switch(Type)
    {
        case RenderTextureWrap_Clamp:  Result = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
        case RenderTextureWrap_Repeat: Result = D3D11_TEXTURE_ADDRESS_WRAP;   break;
        case RenderTextureWrap_Mirror: Result = D3D11_TEXTURE_ADDRESS_MIRROR; break;
    }

    return Result;
}

// [PER-RENDERER API]

static render_handle
InitializeRenderer(void *HWindow, vec2_int Resolution, memory_arena *Arena)
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
        IDXGIDevice *DXGIDevice = nullptr;
        Error = Renderer->Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice);
        if (DXGIDevice)
        {
            IDXGIAdapter *DXGIAdapter = nullptr;
            DXGIDevice->GetAdapter(&DXGIAdapter);
            if (DXGIAdapter)
            {
                IDXGIFactory2 *Factory = nullptr;
                DXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&Factory);
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

                    Error = Factory->CreateSwapChainForHwnd((IUnknown *)Renderer->Device,
                                                            Window, &Desc, 0, 0,
                                                            &Renderer->SwapChain);

                    Factory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER);
                    Factory->Release();
                }

                DXGIAdapter->Release();
            }

            DXGIDevice->Release();
        }

        if (FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Render Target View
    {
        ID3D11Texture2D *BackBuffer = nullptr;
        Error = Renderer->SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
        if(BackBuffer)
        {
            Error = Renderer->Device->CreateRenderTargetView((ID3D11Resource*)BackBuffer, NULL, &Renderer->RenderView);

            BackBuffer->Release();
        }

        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Buffers
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = VOID_KILOBYTE(64);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Renderer->Device->CreateBuffer(&Desc, NULL, &Renderer->VBuffer64KB);
        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Shaders
    {
        ID3D11Device       *Device  = Renderer->Device;
        ID3D11InputLayout  *ILayout = nullptr;
        ID3D11VertexShader *VShader = nullptr;
        ID3D11PixelShader  *PShader = nullptr;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *VShaderSrcBlob = nullptr;
            ID3DBlob *VShaderErrBlob = nullptr;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "VSMain", "vs_5_0",
                               0, 0, &VShaderSrcBlob, &VShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = VShaderSrcBlob->GetBufferPointer();
            uint64_t   ByteSize = VShaderSrcBlob->GetBufferSize();
            Error = Device->CreateVertexShader(ByteCode, ByteSize, 0, &VShader);
            if(FAILED(Error))
            {
            }

            d3d11_input_layout Layout = D3D11ILayoutTable[Type];
            Error = Device->CreateInputLayout(Layout.Desc, Layout.Count, ByteCode, ByteSize, &ILayout);
            if(FAILED(Error))
            {
            }

            VShaderSrcBlob->Release();

            Renderer->VShaders[Type] = VShader;
            Renderer->ILayouts[Type] = ILayout;
        }

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *PShaderSrcBlob = nullptr;
            ID3DBlob *PShaderErrBlob = nullptr;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "PSMain", "ps_5_0",
                               0, 0, &PShaderSrcBlob, &PShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = PShaderSrcBlob->GetBufferPointer();
            uint64_t   ByteSize = PShaderSrcBlob->GetBufferSize();
            Error = Device->CreatePixelShader(ByteCode, ByteSize, 0, &PShader);
            if (FAILED(Error))
            {
            }

            PShaderSrcBlob->Release();

            Renderer->PShaders[Type] = PShader;
        }
    }

    // Uniform Buffers
    {
        ID3D11Device *Device = Renderer->Device;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            ID3D11Buffer *Buffer = nullptr;

            D3D11_BUFFER_DESC Desc = {};
            Desc.ByteWidth      = D3D11UniformBufferSizeTable[Type];
            Desc.Usage          = D3D11_USAGE_DYNAMIC;
            Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            AlignMultiple(Desc.ByteWidth, 16);

            Device->CreateBuffer(&Desc, 0, &Buffer);

            Renderer->UBuffers[Type] = Buffer;
        }
    }

    // Blending
    {
        D3D11_BLEND_DESC BlendDesc = {};
        BlendDesc.RenderTarget[0].BlendEnable           = TRUE;
        BlendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        BlendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        BlendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        BlendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        BlendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(Renderer->Device->CreateBlendState(&BlendDesc, &Renderer->DefaultBlendState)))
        {
        }
    }

    // Samplers
    {
        D3D11_SAMPLER_DESC Desc = {};
        Desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        Desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.MaxAnisotropy  = 1;
        Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        Desc.MaxLOD         = D3D11_FLOAT32_MAX;

        Error = Renderer->Device->CreateSamplerState(&Desc, &Renderer->AtlasSamplerState);
        if (FAILED(Error))
        {
        }
    }

    // Raster State
    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode              = D3D11_FILL_SOLID;
        Desc.CullMode              = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable       = TRUE;
        Desc.ScissorEnable         = TRUE; 
        Desc.MultisampleEnable     = FALSE;
        Desc.AntialiasedLineEnable = FALSE;

        Error = Renderer->Device->CreateRasterizerState(&Desc, &Renderer->RasterSt[RenderPass_UI]);
        if(FAILED(Error))
        {
        }
    }

    // Default State
    {
        Renderer->LastResolution = Resolution;
    }

    Result.uint64_t[0] = (uint64_t)Renderer;
    return Result;
}

static void 
SubmitRenderCommands(render_handle HRenderer, vec2_int Resolution, render_pass_list *RenderPassList)
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
    vec2_int CurrentResolution = Renderer->LastResolution;
    {
        if (!(Resolution == CurrentResolution))
        {
            // TODO: Resize swap chain and whatever else we need.

            Resolution = CurrentResolution;
        }
    }

    // Temporary Clear Screen
    {
        const FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        DeviceContext->ClearRenderTargetView(RenderView, ClearColor);
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
            D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (float)Resolution.X, (float)Resolution.Y, 0.0f, 1.0f };
            DeviceContext->RSSetState(Renderer->RasterSt[RenderPass_UI]);
            DeviceContext->RSSetViewports(1, &Viewport);

            for (rect_group_node *Node = Params.First; Node != 0; Node = Node->Next)
            {
                render_batch_list BatchList  = Node->BatchList;
                rect_group_params NodeParams = Node->Params;

                // Vertex Buffer
                ID3D11Buffer *VBuffer = D3D11GetVertexBuffer(BatchList.ByteCount, Renderer);
                {
                    D3D11_MAPPED_SUBRESOURCE Resource = { 0 };
                    DeviceContext->Map((ID3D11Resource *)VBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);

                    uint8_t *WritePointer = static_cast<uint8_t*>(Resource.pData);
                    uint64_t WriteOffset  = 0;
                    for (render_batch_node *Batch = BatchList.First; Batch != 0; Batch = Batch->Next)
                    {
                        uint64_t WriteSize = Batch->Value.ByteCount;
                        MemoryCopy(WritePointer + WriteOffset, Batch->Value.Memory, WriteSize);
                        WriteOffset += WriteSize;
                    }

                    DeviceContext->Unmap((ID3D11Resource *)VBuffer, 0);
                }

                // Uniform Buffers
                ID3D11Buffer *UniformBuffer = Renderer->UBuffers[RenderPass_UI];
                {
                    d3d11_rect_uniform_buffer Uniform = {};
                    Uniform.Transform[0]        = Vec4float(NodeParams.Transform.c0r0, NodeParams.Transform.c0r1, NodeParams.Transform.c0r2, 0);
                    Uniform.Transform[1]        = Vec4float(NodeParams.Transform.c1r0, NodeParams.Transform.c1r1, NodeParams.Transform.c1r2, 0);
                    Uniform.Transform[2]        = Vec4float(NodeParams.Transform.c2r0, NodeParams.Transform.c2r1, NodeParams.Transform.c2r2, 0);
                    Uniform.ViewportSizeInPixel = vec2_float((float)Resolution.X, (float)Resolution.Y);
                    Uniform.AtlasSizeInPixel    = NodeParams.TextureSize;

                    D3D11_MAPPED_SUBRESOURCE Resource = {};
                    DeviceContext->Map((ID3D11Resource *)UniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
                    MemoryCopy(Resource.pData, &Uniform, sizeof(Uniform));
                    DeviceContext->Unmap((ID3D11Resource *)UniformBuffer, 0);
                }

                // Pipeline Info
                ID3D11InputLayout        *ILayout   = Renderer->ILayouts[RenderPass_UI];
                ID3D11VertexShader       *VShader   = Renderer->VShaders[RenderPass_UI];
                ID3D11PixelShader        *PShader   = Renderer->PShaders[RenderPass_UI];
                ID3D11ShaderResourceView *AtlasView = D3D11GetShaderView(NodeParams.Texture);

                // OM
                DeviceContext->OMSetRenderTargets(1, &Renderer->RenderView, 0);
                DeviceContext->OMSetBlendState(Renderer->DefaultBlendState, 0, 0xFFFFFFFF);

                // IA
                uint32_t Stride = (uint32_t)BatchList.BytesPerInstance;
                uint32_t Offset = 0;
                DeviceContext->IASetVertexBuffers(0, 1, &VBuffer, &Stride, &Offset);
                DeviceContext->IASetInputLayout(ILayout);
                DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                // Shaders
                DeviceContext->VSSetShader(VShader, 0, 0);
                DeviceContext->VSSetConstantBuffers(0, 1, &UniformBuffer);
                DeviceContext->PSSetShader(PShader, 0, 0);
                DeviceContext->PSSetShaderResources(0, 1, &AtlasView);
                DeviceContext->PSSetSamplers(0, 1, &Renderer->AtlasSamplerState);

                // Scissor
                {
                    rect_float   Clip = NodeParams.Clip;
                    D3D11_RECT Rect = {};

                    if (Clip.Left == 0 && Clip.Top == 0 && Clip.Right == 0 && Clip.Bottom == 0)
                    {
                        Rect.left   = 0;
                        Rect.right  = Resolution.X;
                        Rect.top    = 0;
                        Rect.bottom = Resolution.Y;
                    } else
                    if (Clip.Left > Clip.Right || Clip.Top > Clip.Bottom)
                    {
                        Rect.left   = 0;
                        Rect.right  = 0;
                        Rect.top    = 0;
                        Rect.bottom = 0;
                    }
                    else
                    {
                        Rect.left   = Clip.Left;
                        Rect.right  = Clip.Right;
                        Rect.top    = Clip.Top;
                        Rect.bottom = Clip.Bottom;
                    }

                    DeviceContext->RSSetScissorRects(1, &Rect);
                }

                // Draw
                uint32_t InstanceCount = (uint32_t)(BatchList.ByteCount / BatchList.BytesPerInstance);
                DeviceContext->DrawInstanced(4, InstanceCount, 0, 0);
            }
        } break;

        default: break;

        }
    }

    // Present
    HRESULT Error = SwapChain->Present(0, 0);
    if (FAILED(Error))
    {
    }
    DeviceContext->ClearState();

    // Clear
    RenderPassList->First = 0;
    RenderPassList->Last  = 0;
}

// -----------------------------------------------------------------------------------
// Glyph Public Implementation

static bool
IsValidGPUFontContext(gpu_font_context *Context)
{
    bool Result = (Context) && (Context->GlyphCache) && (Context->GlyphCacheView) && (Context->GlyphTransfer) && (Context->GlyphTransferView) && (Context->GlyphTransferSurface);
    return Result;
}

static bool
CreateGlyphCache(render_handle HRenderer, vec2_float TextureSize, gpu_font_context *FontContext)
{
    bool Result  = 0;

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

        Renderer->Device->CreateTexture2D(&Desc, 0, &FontContext->GlyphCache);
        if (FontContext->GlyphCache)
        {
            Renderer->Device->CreateShaderResourceView((ID3D11Resource *)FontContext->GlyphCache, 0, &FontContext->GlyphCacheView);
            Result = (FontContext->GlyphCacheView != 0);
        }
    }

    return Result;
}

static void
ReleaseGlyphCache(gpu_font_context *FontContext)
{
    if (FontContext)
    {
        if (FontContext->GlyphCache)
        {
            FontContext->GlyphCache->Release();
            FontContext->GlyphCache = nullptr;
        }

        if (FontContext->GlyphCacheView)
        {
            FontContext->GlyphCacheView->Release();
            FontContext->GlyphCacheView = nullptr;
        }
    }
}

static bool
CreateGlyphTransfer(render_handle HRenderer, vec2_float TextureSize, gpu_font_context *FontContext)
{
    bool Result  = 0;

    if (IsValidRenderHandle(HRenderer))
    {
        d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width            = (UINT)TextureSize.X;
        Desc.Height           = (UINT)TextureSize.Y;
        Desc.MipLevels        = 1;
        Desc.ArraySize        = 1;
        Desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.Usage            = D3D11_USAGE_DEFAULT;
        Desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        Renderer->Device->CreateTexture2D(&Desc, 0, &FontContext->GlyphTransfer);
        if (FontContext->GlyphTransfer)
        {
            Renderer->Device->CreateShaderResourceView((ID3D11Resource *)FontContext->GlyphTransfer, 0, &FontContext->GlyphTransferView);
            if (FontContext->GlyphTransferView)
            {
                FontContext->GlyphTransfer->QueryInterface(__uuidof(IDXGISurface), (void **)&FontContext->GlyphTransferSurface);
                Result = (FontContext->GlyphTransferSurface != 0);
            }
        }
    }

    return Result;
}

static void
ReleaseGlyphTransfer(gpu_font_context *FontContext)
{
    if (FontContext)
    {
        if (FontContext->GlyphTransfer)
        {
            FontContext->GlyphTransfer->Release();
            FontContext->GlyphTransfer = nullptr;
        }

        if (FontContext->GlyphTransferView)
        {
            FontContext->GlyphTransferView->Release();
            FontContext->GlyphTransferView = nullptr;
        }

        if (FontContext->GlyphTransferSurface)
        {
            FontContext->GlyphTransferSurface->Release();
            FontContext->GlyphTransferSurface = nullptr;
        }
    }
}

static void
TransferGlyph(rect_float Rect, render_handle HRenderer, gpu_font_context *FontContext)
{
    if (IsValidRenderHandle(HRenderer) && IsValidGPUFontContext(FontContext))
    {
        d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

        D3D11_BOX SourceBox;
        SourceBox.left   = 0;
        SourceBox.top    = 0;
        SourceBox.front  = 0;
        SourceBox.right  = (UINT)Rect.Right;
        SourceBox.bottom = (UINT)Rect.Bottom;
        SourceBox.back   = 1;

        Renderer->DeviceContext->CopySubresourceRegion((ID3D11Resource *)FontContext->GlyphCache,
                                                      0, (UINT)Rect.Left, (UINT)Rect.Top, 0,
                                                      (ID3D11Resource *)FontContext->GlyphTransfer, 0, &SourceBox);
    }
}

// -----------------------------------------------------------------------------------
// Texture Private Implementation

static bool
D3D11IsValidTexture(render_texture RenderTexture)
{
    bool Result = IsValidRenderHandle(RenderTexture.Texture) && IsValidRenderHandle(RenderTexture.View) && !RenderTexture.Size.IsEmpty();
    return Result;
}

// -----------------------------------------------------------------------------------
// Texture Public Implementation

static render_texture
CreateRenderTexture(render_texture_params Params)
{
    VOID_ASSERT(Params.Width > 0 && Params.Height > 0);

    d3d11_renderer *Backend = D3D11GetRenderer(RenderState.Renderer);
    VOID_ASSERT(Backend);

    d3d11_format Format = D3D11GetFormat(Params.Type);

    D3D11_TEXTURE2D_DESC TextureDesc = {};
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

    D3D11_SUBRESOURCE_DATA  InitData        = {};
    D3D11_SUBRESOURCE_DATA *InitDataPointer = 0;

    if(Params.InitialData)
    {
        InitData.pSysMem          = Params.InitialData;
        InitData.SysMemPitch      = Params.Width * Format.BytesPerPixel;
        InitData.SysMemSlicePitch = 0;

        InitDataPointer = &InitData;
    }

    ID3D11Texture2D *Texture = nullptr;
    HRESULT HR = Backend->Device->CreateTexture2D(&TextureDesc, InitDataPointer, &Texture);
    VOID_ASSERT(SUCCEEDED(HR));

    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc = {};
    ViewDesc.Format                    = Format.Native;
    ViewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    ViewDesc.Texture2D.MostDetailedMip = 0;
    ViewDesc.Texture2D.MipLevels       = Params.GenerateMipmaps ? -1 : 1;

    ID3D11ShaderResourceView *View = nullptr;
    HR = Backend->Device->CreateShaderResourceView((ID3D11Resource*)Texture, &ViewDesc, &View);
    VOID_ASSERT(SUCCEEDED(HR));

    if(Params.GenerateMipmaps && Params.InitialData)
    {
        Backend->DeviceContext->GenerateMips(View);
    }

    render_texture Result = {};
    Result.Texture = RenderHandle((uint64_t)Texture);
    Result.View    = RenderHandle((uint64_t)View);
    Result.Size    = vec2_float(Params.Width, Params.Height);

    return Result;
}

static void
CopyIntoRenderTexture(render_texture RenderTexture, rect_float Source, uint8_t *Pixels, uint32_t Pitch)
{
    VOID_ASSERT(D3D11IsValidTexture(RenderTexture));
    VOID_ASSERT(Pixels);

    D3D11_BOX Box =
    {
        .left   = (UINT)Source.Left,
        .top    = (UINT)Source.Top,
        .front  = 0,
        .right  = (UINT)Source.Right,
        .bottom = (UINT)Source.Bottom,
        .back   = 1,
    };

    d3d11_renderer *Renderer = D3D11GetRenderer(RenderState.Renderer);
    VOID_ASSERT(Renderer);

    ID3D11Texture2D *Texture = D3D11GetTexture2D(RenderTexture.Texture);
    VOID_ASSERT(Texture);

    Renderer->DeviceContext->UpdateSubresource((ID3D11Resource *)Texture, 0, &Box, Pixels, Pitch, 0);
}
