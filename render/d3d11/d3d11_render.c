// [Internal Renderer API]

internal void
D3D11Release(d3d11_backend *Backend)
{
    if (Backend)
    {
        if (Backend->Device)
        {
            ID3D11Device_Release(Backend->Device);
            Backend->Device = 0;
        }

        if (Backend->DeviceContext)
        {
            ID3D11DeviceContext_Release(Backend->DeviceContext);
            Backend->DeviceContext = 0;
        }

        if (Backend->RenderView)
        {
            ID3D11RenderTargetView_Release(Backend->RenderView);
            Backend->RenderView = 0;
        }

        if (Backend->SwapChain)
        {
            IDXGISwapChain1_Release(Backend->SwapChain);
            Backend->SwapChain = 0;
        }
    }
}

internal ID3D11Buffer *
D3D11GetVertexBuffer(u64 Size, d3d11_backend *Backend)
{
    ID3D11Buffer *Result = Backend->VBuffer64KB;

    if(Size > Kilobyte(64))
    {
        OSLogMessage(byte_string_literal("Failed to acquire D3D11 Vertex Buffer."), OSMessage_Fatal);
    }

    return Result;
}

// [PER-RENDERER API]

internal render_handle
InitializeRenderer(memory_arena *Arena)
{
    render_handle  Result  = { 0 };
    d3d11_backend *Backend = PushArray(Arena, d3d11_backend, 1); 
    HRESULT        Error   = S_OK;

    {
        UINT CreateFlags = 0;
        #ifndef NDEBUG
        CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif
        D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0 };
        Error = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                  CreateFlags, Levels, ARRAYSIZE(Levels),
                                  D3D11_SDK_VERSION, &Backend->Device, NULL, &Backend->DeviceContext);
        if (FAILED(Error))
        {
            return Result;
        }
    }

    {
        IDXGIDevice *DXGIDevice = 0;
        Error = ID3D11Device_QueryInterface(Backend->Device, &IID_IDXGIDevice, (void **)&DXGIDevice);
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
                    HWND WindowHandle = OSWin32GetWindowHandle();

                    DXGI_SWAP_CHAIN_DESC1 Desc = { 0 };
                    Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
                    Desc.SampleDesc.Count = 1;
                    Desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    Desc.BufferCount      = 2;
                    Desc.Scaling          = DXGI_SCALING_NONE;
                    Desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                    Error = IDXGIFactory2_CreateSwapChainForHwnd(Factory, (IUnknown *)Backend->Device, WindowHandle,
                                                                 &Desc, 0, 0, &Backend->SwapChain);
                    IDXGIFactory_MakeWindowAssociation(Factory, WindowHandle, DXGI_MWA_NO_ALT_ENTER);
                    IDXGIFactory2_Release(Factory);
                }

                IDXGIAdapter_Release(DXGIAdapter);
            }

            IDXGIDevice_Release(DXGIDevice);
        }

        if (FAILED(Error))
        {
            D3D11Release(Backend);
            return Result;
        }
    }

    {
        ID3D11Texture2D *BackBuffer = 0;
        Error = IDXGISwapChain1_GetBuffer(Backend->SwapChain, 0, &IID_ID3D11Texture2D, (void **)&BackBuffer);
        if(BackBuffer)
        {
            Error = ID3D11Device_CreateRenderTargetView(Backend->Device, (ID3D11Resource*)BackBuffer, NULL, &Backend->RenderView);

            ID3D11Texture2D_Release(BackBuffer);
        }

        if(FAILED(Error))
        {
            D3D11Release(Backend);
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

        Error = Backend->Device->lpVtbl->CreateBuffer(Backend->Device, &Desc, NULL, &Backend->VBuffer64KB);
        if(FAILED(Error))
        {
            D3D11Release(Backend);
            return Result;
        }
    }

    // Default Shaders
    {
        ID3D11Device       *Device  = Backend->Device;
        ID3D11InputLayout  *ILayout = 0;
        ID3D11VertexShader *VShader = 0;
        ID3D11PixelShader  *PShader = 0;

        ForEachEnum(RenderPass_Type, Type)
        {
            byte_string Source = D3D11VShaderSourceTable[Type];

            ID3DBlob *VShaderSrcBlob = 0;
            ID3DBlob *VShaderErrBlob = 0;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "VSMain", "vs_5_0",
                               0, 0, &VShaderSrcBlob, &VShaderErrBlob);
            if(FAILED(Error))
            {
                OSLogMessage(byte_string_literal("Failed to compile D3D11 vertex shader."), OSMessage_Fatal);
            }

            void *ByteCode = VShaderSrcBlob->lpVtbl->GetBufferPointer(VShaderSrcBlob);
            u64   ByteSize = VShaderSrcBlob->lpVtbl->GetBufferSize(VShaderSrcBlob);
            Error = Device->lpVtbl->CreateVertexShader(Device, ByteCode, ByteSize, 0, &VShader);
            if(FAILED(Error))
            {
                OSLogMessage(byte_string_literal("Failed to create D3D11 vertex shader."), OSMessage_Fatal);
            }

            d3d11_input_layout Layout = D3D11ILayoutTable[Type];
            Error = Device->lpVtbl->CreateInputLayout(Device, Layout.Desc, Layout.Count,
                                                      ByteCode, ByteSize,
                                                      &ILayout);
            if(FAILED(Error))
            {
                OSLogMessage(byte_string_literal("Failed to create D3D11 input layout."), OSMessage_Fatal);
            }

            VShaderSrcBlob->lpVtbl->Release(VShaderSrcBlob);

            Backend->VShaders[Type] = VShader;
            Backend->ILayouts[Type] = ILayout;
        }

        ForEachEnum(RenderPass_Type, Type)
        {
            byte_string Source = D3D11PShaderSourceTable[Type];

            ID3DBlob *PShaderSrcBlob = 0;
            ID3DBlob *PShaderErrBlob = 0;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "PSMain", "ps_5_0",
                               0, 0, &PShaderSrcBlob, &PShaderErrBlob);
            if(FAILED(Error))
            {
                OSAbort(1);
            }

            void *ByteCode = PShaderSrcBlob->lpVtbl->GetBufferPointer(PShaderSrcBlob);
            u64   ByteSize = PShaderSrcBlob->lpVtbl->GetBufferSize(PShaderSrcBlob);
            Error = Device->lpVtbl->CreatePixelShader(Device, ByteCode, ByteSize, 0, &PShader);
            if (FAILED(Error))
            {
                OSAbort(1);
            }

            PShaderSrcBlob->lpVtbl->Release(PShaderSrcBlob);

            Backend->PShaders[Type] = PShader;
        }
    }

    // Uniform Buffers
    {
        ID3D11Device *Device = Backend->Device;

        ForEachEnum(RenderPass_Type, Type)
        {
            ID3D11Buffer *Buffer = 0;

            D3D11_BUFFER_DESC Desc = {0};
            Desc.ByteWidth      = D3D11UniformBufferSizeTable[Type];
            Desc.Usage          = D3D11_USAGE_DYNAMIC;
            Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            AlignMultiple(Desc.ByteWidth, 16);

            Device->lpVtbl->CreateBuffer(Device, &Desc, 0, &Buffer);

            Backend->UBuffers[Type] = Buffer;
        }
    }

    // Default State
    {
        Backend->LastResolution = OSGetClientSize();
    }

    Result.u64[0] = (u64)Backend;
    return Result;
}

internal void 
SubmitRenderCommands(render_context *RenderContext, render_handle BackendHandle)
{
    d3d11_backend          *Backend       = (d3d11_backend *)BackendHandle.u64[0];
    ID3D11DeviceContext    *DeviceContext = Backend->DeviceContext;
    ID3D11RenderTargetView *RenderView    = Backend->RenderView; 
    IDXGISwapChain1        *SwapChain     = Backend->SwapChain;

    // Update State
    vec2_i32 Resolution = Backend->LastResolution;
    {
        // NOTE: Probably don't want to call this every frame. We can check a boolean that is
        // set by WM_SIZE. Maybe it's OKAY?

        vec2_i32 CurrentResolution = OSGetClientSize();

        if (!Vec2I32IsEqual(Resolution, CurrentResolution))
        {
            // TODO: Resize swap chain and whatever else we need.

            Resolution = CurrentResolution;
        }
    }

    // Render Game
    {
        const FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        DeviceContext->lpVtbl->ClearRenderTargetView(DeviceContext, RenderView, ClearColor);
    }

    // Render UI (Single Pass)
    {
        render_pass_params_ui Params = RenderContext->UIParams;

        // Rasterizer
        D3D11_VIEWPORT Viewport = {0.0f, 0.0f, (f32)Resolution.X, (f32)Resolution.Y, 0.0f, 1.0f};
        DeviceContext->lpVtbl->RSSetViewports(DeviceContext, 1, &Viewport);

        for (render_rect_group_node *RectGroupNode = Params.First; RectGroupNode != 0; RectGroupNode = RectGroupNode->Next)
        {
            render_batch_list BatchList = RectGroupNode->BatchList;

            ID3D11Buffer *VBuffer = D3D11GetVertexBuffer(BatchList.ByteCount, Backend);
            {
                D3D11_MAPPED_SUBRESOURCE Resource = {0};
                DeviceContext->lpVtbl->Map(DeviceContext, (ID3D11Resource*)VBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);

                u8 *WritePointer = Resource.pData;
                u64 WriteOffset  = 0;
                for(render_batch_node *Batch = BatchList.First; Batch != 0; Batch = Batch->Next)
                {
                    u64 WriteSize = Batch->Value.ByteCount;
                    MemoryCopy(WritePointer + WriteOffset, Batch->Value.Memory, WriteSize);
                    WriteOffset += WriteSize;
                }

                DeviceContext->lpVtbl->Unmap(DeviceContext, (ID3D11Resource *)VBuffer, 0);
            }

            // Uniform Buffers
            ID3D11Buffer *UniformBuffer = Backend->UBuffers[RenderPass_UI];
            {
                d3d11_rect_uniform_buffer Uniform = { 0 };
                Uniform.Transform[0] = Vec4F32(1, 0, 0, 0);
                Uniform.Transform[1] = Vec4F32(0, 1, 0, 0);
                Uniform.Transform[2] = Vec4F32(0, 0, 1, 0);
                Uniform.ViewportSize = Vec2F32((f32)Resolution.X, (f32)Resolution.Y);

                D3D11_MAPPED_SUBRESOURCE Resource = { 0 };
                DeviceContext->lpVtbl->Map(DeviceContext, (ID3D11Resource *)UniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
                MemoryCopy(Resource.pData, &Uniform, sizeof(Uniform));
                DeviceContext->lpVtbl->Unmap(DeviceContext, (ID3D11Resource *)UniformBuffer, 0);
            }

            // Pipeline Info
            ID3D11InputLayout  *ILayout = Backend->ILayouts[RenderPass_UI];
            ID3D11VertexShader *VShader = Backend->VShaders[RenderPass_UI];
            ID3D11PixelShader  *PShader = Backend->PShaders[RenderPass_UI];

            // OM
            DeviceContext->lpVtbl->OMSetRenderTargets(DeviceContext, 1, &Backend->RenderView, 0);

            // IA
            u32 Stride = (u32)BatchList.BytesPerInstance;
            u32 Offset = 0;
            DeviceContext->lpVtbl->IASetVertexBuffers(DeviceContext, 0, 1, &VBuffer, &Stride, &Offset);
            DeviceContext->lpVtbl->IASetInputLayout(DeviceContext, ILayout);
            DeviceContext->lpVtbl->IASetPrimitiveTopology(DeviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            // Shaders
            DeviceContext->lpVtbl->VSSetShader(DeviceContext, VShader, 0, 0);
            DeviceContext->lpVtbl->PSSetShader(DeviceContext, PShader, 0, 0);
            DeviceContext->lpVtbl->VSSetConstantBuffers(DeviceContext, 0, 1, &UniformBuffer);

            // Draw
            u32 InstanceCount = (u32)(BatchList.ByteCount / BatchList.BytesPerInstance);
            DeviceContext->lpVtbl->DrawInstanced(DeviceContext, 4, InstanceCount, 0, 0);
        }
    }
    

    // Present
    HRESULT Error = SwapChain->lpVtbl->Present(SwapChain, 0, 0);
    if (FAILED(Error))
    {
        OSAbort(1);
    }
    DeviceContext->lpVtbl->ClearState(DeviceContext);
}
