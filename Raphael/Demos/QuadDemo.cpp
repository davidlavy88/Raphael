#include "QuadDemo.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "GPUStructs.h"


using namespace raphael;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Initialization process
// 1. Create application window
// 2. Create device
// 3. Create descriptor heaps (DSV, RTV, CBV/SRV/UAV if needed)
// 4. Create swap chain + depth buffer
// 5. Create geometry resources (vertex/index buffers, views)
// 6. Constant buffers (per-frame upload buffers)
// 7. Create root signature (define shader resource bindings)
// 8. Create pipeline state (compile shaders, create PSO)
// 9. Create command objects (command allocators, command lists)
bool QuadDemo::Initialize()
{
    // -- 1. Create application window --
    if (!CreateAppWindow())
        return false;
    
    // -- 2. Create device --
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

	// -- 3. Create descriptor heaps --
	CreateDescriptorHeaps();

	// -- 4. Create swap chain and depth buffer --
	CreateSwapChainAndDepthBuffer();

	// -- 5. Create geometry resources --
	CreateGeometry();

	// -- 6. Create constant buffers --
	CreateConstantBuffers();

	// -- 7. Create root signature --
	CreateRootSignature();

	// -- 8. Create pipeline state + shaders --
	CreatePipeline();

	// -- 9. Create command objects --
	CreateCommandObjects();

    // Show window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);

    return true;
}

// 3. Create descriptor heaps 
// For this simple app, we only need 2 non-shader visible heaps:
// - RTV  heap: g_frameCount descriptors for the back buffer RTVs (one per frame in the swap chain)
// - DSV heaps: 1 descriptor for the depth buffer DSV
// We don't need a CBV/SRV/UAV heap since we won't be using any shader resources in this demo.
void QuadDemo::CreateDescriptorHeaps()
{
    // Create DSV descriptor heap
    DescriptorHeapDesc dsvHeapDesc = {};
    dsvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::DSV;
    dsvHeapDesc.numDescriptors = 1;
    dsvHeapDesc.shaderVisible = false; // DSV heap does not need to be shader visible

    m_dsvHeap = m_device->createDescriptorHeap(dsvHeapDesc);
    m_dsvHeap->createDescriptorHeap();

    // Create RTV descriptor heap
    DescriptorHeapDesc rtvHeapDesc = {};
    rtvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::RTV;
    rtvHeapDesc.numDescriptors = g_frameCount; // One RTV for each back buffer
    rtvHeapDesc.shaderVisible = false; // RTV heap does not need to be shader visible

    m_rtvHeap = m_device->createDescriptorHeap(rtvHeapDesc);
    m_rtvHeap->createDescriptorHeap();
}

void QuadDemo::CreateSwapChainAndDepthBuffer()
{
	// We couple swap chain and depth buffer creation together since they both depend 
	// on the window size and need to be recreated together when the window is resized.
    
    // Create swap chain
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.width = WINDOW_WIDTH;
    swapChainDesc.height = WINDOW_HEIGHT;
    swapChainDesc.bufferCount = g_frameCount;
    swapChainDesc.windowHandle = m_hwnd;

    m_swapChain = m_device->createSwapChain(m_rtvHeap.get(), swapChainDesc);

    // Create depth buffer
    ResourceDesc depthDesc = {};
    depthDesc.type = ResourceDesc::ResourceType::Texture2D;
    depthDesc.width = WINDOW_WIDTH;
    depthDesc.height = WINDOW_HEIGHT;
    depthDesc.format = ResourceFormat::D24_UNORM_S8_UINT;
    depthDesc.bindFlags = ResourceBindFlags::DepthStencil;

    m_depthBuffer = m_device->createResource(depthDesc);

    // Create the DSV for the depth buffer
    DescriptorHandle dsvHandle = {};
    m_dsvHeap->getDescriptorHandle(0, &dsvHandle);
    m_depthStencilView = m_depthBuffer->getResourceView(ResourceBindFlags::DepthStencil, dsvHandle);
}

void QuadDemo::CreateGeometry()
{
    SimpleVertex quadVertices[] =
    {
        { XMFLOAT3(-1.0f, +1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 0: top-left-front
        { XMFLOAT3(+1.0f, +1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1: top-right-front
        { XMFLOAT3(+1.0f, -1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 2: bottom-right-front
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }, // 3: bottom-left-front

    };

    uint16_t quadIndices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

    const UINT vertexBufferSize = sizeof(quadVertices);
    const UINT indexBufferSize = sizeof(quadIndices);
	m_indexCount = _countof(quadIndices);

    // Create vertex buffer resource
    ResourceDesc vertexBufferDesc = {};
    vertexBufferDesc.type = ResourceDesc::ResourceType::Buffer;
    vertexBufferDesc.usage = ResourceDesc::Usage::Upload;
    vertexBufferDesc.width = vertexBufferSize;

    m_vertexBuffer = m_device->createResource(vertexBufferDesc);

    // Copy vertex data to vertex buffer
    void* vertexData = nullptr;
    if (m_vertexBuffer->map(&vertexData))
    {
        memcpy(vertexData, quadVertices, vertexBufferSize);
        m_vertexBuffer->unmap();
    }
    else
    {
        throw std::runtime_error("Failed to map vertex buffer resource.\n");
    }

    // Create vertex buffer view
    m_vertexBufferView = m_vertexBuffer->getResourceView(
        ResourceBindFlags::VertexBuffer, {}, sizeof(SimpleVertex));

    // Create index buffer resource
    ResourceDesc indexBufferDesc = {};
    indexBufferDesc.type = ResourceDesc::ResourceType::Buffer;
    indexBufferDesc.usage = ResourceDesc::Usage::Upload;
    indexBufferDesc.width = indexBufferSize;

    m_indexBuffer = m_device->createResource(indexBufferDesc);

    // Copy index data to index buffer
    void* indexData = nullptr;
    if (m_indexBuffer->map(&indexData))
    {
        memcpy(indexData, quadIndices, indexBufferSize);
        m_indexBuffer->unmap();
    }
    else
    {
        throw std::runtime_error("Failed to map index buffer resource.\n");
    }

    // Create index buffer view
    m_indexBufferView = m_indexBuffer->getResourceView(
        ResourceBindFlags::IndexBuffer, {}, sizeof(uint16_t));
}

void QuadDemo::CreateConstantBuffers()
{
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameCBs[i] = std::make_unique<UploadBuffer<FrameConstants>>(m_device.get(), 1, true);
        m_objectCBs[i] = std::make_unique<UploadBuffer<BasicObjectConstants>>(m_device.get(), 1, true);
	}
}

void QuadDemo::CreateRootSignature()
{
    // Create root signature

    // For this simple test, we have an object constant buffer (per-object data like world matrix) 
    // and a frame constant buffer (per-frame data like view/projection matrices).
    RootSignatureRangeDesc objCbv = {};
    objCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    objCbv.numParameters = 1;
    objCbv.shaderRegister = 0;

    RootSignatureRangeDesc frameCbv = {};
    frameCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    frameCbv.numParameters = 1;
    frameCbv.shaderRegister = 1;

    RootSignatureTableLayoutDesc cbvTable = {};
    cbvTable.visibility = RootSignatureTableLayoutDesc::ShaderVisibility::All;
    cbvTable.rangeDescs = { objCbv, frameCbv };

    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.tableLayoutDescs = { cbvTable };

    // Add static sampler for completeness, even though we don't use yet
    rootSigDesc.staticSamplers = {
        StaticSamplerDesc{
            .shaderRegister = 0, .filter = SamplerFilter::Point,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap
        },
        StaticSamplerDesc{
            .shaderRegister = 1, .filter = SamplerFilter::Point,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp
        },
        StaticSamplerDesc{
            .shaderRegister = 2, .filter = SamplerFilter::Linear,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap
        },
        StaticSamplerDesc{
            .shaderRegister = 3, .filter = SamplerFilter::Linear,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp
        },
        StaticSamplerDesc{
            .shaderRegister = 4, .filter = SamplerFilter::Anisotropic,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap,
            .mipLODBias = 0.0f, .maxAnisotropy = 8
        },
        StaticSamplerDesc{
            .shaderRegister = 5, .filter = SamplerFilter::Anisotropic,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp,
            .mipLODBias = 0.0f, .maxAnisotropy = 8
        }
    };

    m_rootSignature = m_device->createRootSignature(rootSigDesc);
    m_rootSignature->createRootSignature();
}

void QuadDemo::CreatePipeline()
{
    // Compile shader
    ShaderDesc shaderDesc = {};
    shaderDesc.shaderFilePath = L"Shaders\\cubeShader.hlsl";
    shaderDesc.shaderName = "CubeShader";
    shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(shaderDesc);

    // Create pipeline state
    PipelineDesc pipelineDesc = {};
    pipelineDesc.rtvFormats = { ResourceFormat::R8G8B8A8_UNORM };
    pipelineDesc.inputLayout = InputLayoutDesc::build({
        InputElementDesc::setAsPosition(0, ResourceFormat::R32G32B32_FLOAT, 0, 0),
        InputElementDesc::setAsColor(0, ResourceFormat::R32G32B32A32_FLOAT, 0, 12)
        });

    m_pipeline = m_device->createPipeline(pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());
}

void QuadDemo::CreateCommandObjects()
{
    // Create per frame command allocators
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameContexts[i].commandAllocator = m_device->createCommandAllocator();
        m_frameContexts[i].fenceValue = 0;
    }

    // Create command list (use the first frame's command allocator for now, we will reset it each frame before recording commands)
    CommandListDesc cmdListDesc = {};
    m_commandList = m_device->createCommandList(cmdListDesc);
    m_commandList->createCommandList(m_frameContexts[0].commandAllocator.Get());
}

void QuadDemo::UpdateConstantBuffers()
{
    // Rotate the cube slowly around Y axis
    m_rotationAngle += 0.01f;

	// Object constant (b0) - World matrix
	XMMATRIX worldMatrix = XMMatrixRotationY(m_rotationAngle);    

    // Object: world matrix with rotation
    BasicObjectConstants objConstants = {};
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(worldMatrix));

	// Frame constant (b1) - ViewProj matrix + eye position
    XMVECTOR eyePos = XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
    XMVECTOR lookAt = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, lookAt, up);

    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);
    XMMATRIX viewProj = view * proj;

    // Frame: identity viewproj (renders in NDC space directly)
    FrameConstants frameConstants = {};
    XMStoreFloat4x4(&frameConstants.ViewProj, XMMatrixTranspose(viewProj));

	// Copy data to the current back buffer's constant buffers
    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
    m_objectCBs[backBufferIndex]->CopyData(0, objConstants);
    m_frameCBs[backBufferIndex]->CopyData(0, frameConstants);
}

void QuadDemo::Run()
{
    MSG msg = {};
    bool running = true;

    while (running)
    {
        // Process all pending Windows messages
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }
        if (!running)
            break;

        // Get the current back buffer index from the swap chain
        UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
        // Wait for GPU to finish with the resources from the previous frame
        FrameContext& currentFrameContext = m_frameContexts[backBufferIndex];
        m_device->waitForFence(currentFrameContext.fenceValue);

		// Update constant buffers with current frame's data
		UpdateConstantBuffers();

        // Record commands
		// Retrieve current back buffer resource and RTV for render pass setup
        ResourceDx12* currentBackBuffer = m_swapChain->getCurrentBackBuffer();
        ResourceView currentRtView = m_swapChain->getCurrentRTView();

        // Build render pass descriptor for current frame
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        RenderPassDesc renderPassDesc = RenderPassDesc::buildAsSingleRenderTarget(
            currentRtView,
            currentBackBuffer->getNativeResource(),
            m_depthStencilView,
            WINDOW_WIDTH, WINDOW_HEIGHT,
            clearColor);
        renderPassDesc.debugName= "Cube Render Pass";

        // Test command list recording
        m_commandList->begin(currentFrameContext.commandAllocator.Get());
        m_commandList->beginRenderPass(renderPassDesc);

        {
			// Bind root signature and pipeline state
            m_commandList->setGraphicsRootSignature(m_rootSignature.get());
            m_commandList->setPipeline(m_pipeline.get());

			// Bind constant buffers to root parameters (descriptor tables or root descriptors 
            // depending on how we set up the root signature)
			m_commandList->setConstantBufferView(
                0, 
                m_objectCBs[backBufferIndex]->getResource()->GetGPUVirtualAddress());
			m_commandList->setConstantBufferView(
                1, 
				m_frameCBs[backBufferIndex]->getResource()->GetGPUVirtualAddress());

			// Bind geometry
            m_commandList->setVertexBuffer(0, m_vertexBufferView);
			m_commandList->setIndexBuffer(m_indexBufferView);

            m_commandList->drawIndexedInstanced(m_indexCount, 1, 0, 0,0);
        }

        m_commandList->endRenderPass();

        m_commandList->end();

        // Execute command list
        m_device->executeCommandList(m_commandList.get());
        // Present the frame
        m_swapChain->present(true);

        // Signal and increment the fence value for the current frame
        currentFrameContext.fenceValue = m_device->getNextFenceValue();
        m_device->signalFence(currentFrameContext.fenceValue);
    }
}

void QuadDemo::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down QuadDemo and releasing resources.\n");
}

LRESULT QuadDemo::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (m_device->getNativeDevice() != nullptr && wParam != SIZE_MINIMIZED)
        {
            // Wait for GPU to finish with resources before resizing
            for (UINT i = 0; i < g_frameCount; i++)
            {
                m_device->waitForFence(m_frameContexts[i].fenceValue);
            }

            // TODO: Move this to a separate method since we will need to call it from other places (e.g., when changing display modes)
            UINT newWidth = LOWORD(lParam);
            UINT newHeight = HIWORD(lParam);

            // Update global window size variables (used for viewport/scissor rect setup in command list recording, etc.)
            WINDOW_WIDTH = newWidth;
            WINDOW_HEIGHT = newHeight;

            m_swapChain->resize(newWidth, newHeight);

            // Recreate depth buffer at new size
            ResourceDesc depthDesc = {};
            depthDesc.type = ResourceDesc::ResourceType::Texture2D;
            depthDesc.width = newWidth;
            depthDesc.height = newHeight;
            depthDesc.format = ResourceFormat::D24_UNORM_S8_UINT;
            depthDesc.bindFlags = ResourceBindFlags::DepthStencil;

            m_depthBuffer = m_device->createResource(depthDesc);

            DescriptorHandle dsvHandle = {};
            m_dsvHeap->getDescriptorHandle(0, &dsvHandle);
            m_depthStencilView = m_depthBuffer->getResourceView(ResourceBindFlags::DepthStencil, dsvHandle);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool QuadDemo::CreateAppWindow()
{
    // Implementation for creating application window
    WNDCLASSEXW wc = {
            sizeof(wc), CS_CLASSDC, StaticWndProc, 0L, 0L,
            GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"ImGui Example", nullptr
    };

    ::RegisterClassExW(&wc);

    m_hwnd = ::CreateWindowW(
        wc.lpszClassName, L"Raphael Engine - Quad Demo", WS_OVERLAPPEDWINDOW,
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, wc.hInstance, this
    );

    return m_hwnd != nullptr;
}

void QuadDemo::DestroyAppWindow()
{
    // Implementation for destroying application window
    if (m_hwnd)
    {
        ::DestroyWindow(m_hwnd);
        ::UnregisterClassW(L"Raphael Engine", GetModuleHandle(nullptr));
        m_hwnd = nullptr;
    }
}

LRESULT WINAPI QuadDemo::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Retrieve the QuadDemo instance from window user data
    QuadDemo* app = nullptr;
    
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        app = static_cast<QuadDemo*>(cs->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<QuadDemo*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (app)
        return app->HandleMessage(hWnd, msg, wParam, lParam);

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
