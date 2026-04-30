#include "BoxDemo.h"
#include "GPUStructs.h"

using namespace raphael;

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
bool BoxDemo::Initialize(WindowInfo windowInfo)
{
    // -- 2. Create device --
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

    // -- 3. Create descriptor heaps --
    CreateDescriptorHeaps();

    // -- Initialize ImGui --
    if (!m_imguiLoader.Initialize(windowInfo.hWnd, m_device.get(), m_srvHeap.get(), g_frameCount))
        return false;

    // -- 4. Create swap chain and depth buffer --
    CreateSwapChainAndDepthBuffer(windowInfo);

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

    return true;
}

// 3. Create descriptor heaps 
// For this simple app, we only need 2 non-shader visible heaps:
// - RTV  heap: g_frameCount descriptors for the back buffer RTVs (one per frame in the swap chain)
// - DSV heaps: 1 descriptor for the depth buffer DSV
// We don't need a CBV/SRV/UAV heap since we won't be using any shader resources in this demo.
void BoxDemo::CreateDescriptorHeaps()
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

    // Create SRV descriptor heap
    DescriptorHeapDesc srvHeapDesc = {};
    srvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::CBV_SRV_UAV;
    srvHeapDesc.numDescriptors = 1; // We will only have one texture SRV in this demo
    srvHeapDesc.shaderVisible = true; // This heap needs to be shader visible since we'll bind the texture SRV to the pipeline

    m_srvHeap = m_device->createDescriptorHeap(srvHeapDesc);
    m_srvHeap->createDescriptorHeap();
}

void BoxDemo::CreateSwapChainAndDepthBuffer(WindowInfo windowInfo)
{
    // We couple swap chain and depth buffer creation together since they both depend 
    // on the window size and need to be recreated together when the window is resized.

    // Create swap chain
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.width = windowInfo.width;
    swapChainDesc.height = windowInfo.height;
    swapChainDesc.bufferCount = g_frameCount;
    swapChainDesc.windowHandle = windowInfo.hWnd;

    m_swapChain = m_device->createSwapChain(m_rtvHeap.get(), swapChainDesc);

    // Create depth buffer
    ResourceDesc depthDesc = {};
    depthDesc.type = ResourceDesc::ResourceType::Texture2D;
    depthDesc.width = windowInfo.width;
    depthDesc.height = windowInfo.height;
    depthDesc.format = ResourceFormat::D24_UNORM_S8_UINT;
    depthDesc.bindFlags = ResourceBindFlags::DepthStencil;

    m_depthBuffer = m_device->createResource(depthDesc);

    // Create the DSV for the depth buffer
    DescriptorHandle dsvHandle = {};
    m_dsvHeap->getDescriptorHandle(0, &dsvHandle);
    m_depthStencilView = m_depthBuffer->getResourceView(ResourceBindFlags::DepthStencil, dsvHandle);
}

void BoxDemo::CreateGeometry()
{
    SimpleVertex quadVertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) },
        { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) },
        { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) },
        { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) },
        { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) },
        { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) },
        { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) },
        { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta)}
    };

    uint16_t quadIndices[] =
    {
        // front face
        0, 1, 2,
        0, 2, 3,
        // back face
        4, 6, 5,
        4, 7, 6,
        // left face
        4, 5, 1,
        4, 1, 0,
        // right face
        3, 2, 6,
        3, 6, 7,
        // top face
        1, 5, 6,
        1, 6, 2,
        // bottom face
        4, 0, 3,
        4, 3, 7
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

void BoxDemo::CreateConstantBuffers()
{
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameCBs[i] = std::make_unique<UploadBuffer<FrameConstants>>(m_device.get(), 1, true);
        m_objectCBs[i] = std::make_unique<UploadBuffer<BasicObjectConstants>>(m_device.get(), 1, true);
    }
}

void BoxDemo::CreateRootSignature()
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

void BoxDemo::CreatePipeline()
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

void BoxDemo::CreateCommandObjects()
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

void BoxDemo::UpdateConstantBuffers()
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

void BoxDemo::Render()
{
    // Get the current back buffer index from the swap chain
    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
    // Wait for GPU to finish with the resources from the previous frame
    FrameContext& currentFrameContext = m_frameContexts[backBufferIndex];
    m_device->waitForFence(currentFrameContext.fenceValue);

    // Update constant buffers with current frame's data
    UpdateConstantBuffers();
        
    // Start ImGui frame
    m_imguiLoader.NewFrame();
    m_imguiLoader.Display();

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
    renderPassDesc.debugName = "Cube Render Pass";

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

        m_commandList->drawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Render ImGui (needs SRV descriptor heap set since ImGui uses a font texture)
        m_commandList->setDescriptorHeaps(m_srvHeap.get(), 1);
        m_imguiLoader.Render(m_commandList.get());
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

void BoxDemo::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    // Shutdown ImGui
    m_imguiLoader.Shutdown();

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down BoxDemo and releasing resources.\n");
}

void BoxDemo::Resize(unsigned int width, unsigned int height)
{
    if (m_device->getNativeDevice() != nullptr)
    {
        // Wait for GPU to finish with resources before resizing
        for (UINT i = 0; i < g_frameCount; i++)
        {
            m_device->waitForFence(m_frameContexts[i].fenceValue);
        }

        // TODO: Move this to a separate method since we will need to call it from other places (e.g., when changing display modes)
        UINT newWidth = width;
        UINT newHeight = height;

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
}
