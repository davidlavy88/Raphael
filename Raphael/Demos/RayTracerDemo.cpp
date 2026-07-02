#include "RayTracerDemo.h"
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
bool RayTracerDemo::Initialize(WindowInfo windowInfo)
{
    // Store the window handle for input processing (sad thing)
    m_windowHandle = windowInfo.hWnd;

    // -- 2. Create device --
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

    // -- 3. Create descriptor heaps --
    CreateDescriptorHeaps();

    // -- 3b. Initialize ImGui --
    if (!m_imguiLoader.Initialize(windowInfo.hWnd, m_device.get(), m_srvHeap.get(), g_frameCount))
        return false;

    // -- 4. Create swap chain and depth buffer --
    CreateSwapChainAndDepthBuffer(windowInfo);

    // -- 5. Create geometry resources --
    CreateGeometry();

    // -- 5b. Load glTF model and create triangle structured buffer --
    LoadModelAndCreateTriangleBuffer();

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
// For this simple app, we only need:
// - RTV  heap: g_frameCount descriptors for the back buffer RTVs (one per frame in the swap chain)
void RayTracerDemo::CreateDescriptorHeaps()
{
    // Create RTV descriptor heap
    DescriptorHeapDesc rtvHeapDesc = {};
    rtvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::RTV;
    rtvHeapDesc.numDescriptors = g_frameCount; // One RTV for each back buffer
    rtvHeapDesc.shaderVisible = false; // RTV heap does not need to be shader visible

    m_rtvHeap = m_device->createDescriptorHeap(rtvHeapDesc);
    m_rtvHeap->createDescriptorHeap();

    // Create SRV descriptor heap (shader-visible) for ImGui's font texture.
    // RayTracer binds its own resources via root descriptors, so this heap is
    // dedicated to ImGui.
    DescriptorHeapDesc srvHeapDesc = {};
    srvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::CBV_SRV_UAV;
    srvHeapDesc.numDescriptors = 1;
    srvHeapDesc.shaderVisible = true;

    m_srvHeap = m_device->createDescriptorHeap(srvHeapDesc);
    m_srvHeap->createDescriptorHeap();
}

void RayTracerDemo::CreateSwapChainAndDepthBuffer(WindowInfo windowInfo)
{
    // Create swap chain
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.width = windowInfo.width;
    swapChainDesc.height = windowInfo.height;
    swapChainDesc.bufferCount = g_frameCount;
    swapChainDesc.windowHandle = windowInfo.hWnd;

    m_swapChain = m_device->createSwapChain(m_rtvHeap.get(), swapChainDesc);
}

void RayTracerDemo::CreateGeometry()
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

void RayTracerDemo::LoadModelAndCreateTriangleBuffer()
{
    // Load the glTF model using the existing loader
    GltfLoader loader;
    Mesh mesh;
    if (!loader.LoadFromFile("Models/portal_gun/scene.gltf", mesh))
    {
        throw std::runtime_error("Failed to load glTF model for ray tracing");
    }

    // Extract triangles from the mesh's vertex and index buffers.
    // Important: glTF primitive indices are local to each primitive.
    // We must apply each submesh's vertex/index offsets when building triangles.
    const auto& vertices = mesh.m_vertices;
    const auto& indices = mesh.m_indices16;

    std::vector<GPUTriangle> triangles;
    triangles.reserve(indices.size() / 3);

    for (const auto& [submeshName, submesh] : mesh.m_drawMeshes)
    {
        const size_t indexStart = static_cast<size_t>(submesh.indexBufferOffset);
        const size_t indexEnd = indexStart + static_cast<size_t>(submesh.indexCount);
        const uint32_t baseVertex = submesh.vertexBufferOffset;

        for (size_t i = indexStart; i + 2 < indexEnd; i += 3)
        {
            const uint32_t i0 = baseVertex + static_cast<uint32_t>(indices[i + 0]);
            const uint32_t i1 = baseVertex + static_cast<uint32_t>(indices[i + 1]);
            const uint32_t i2 = baseVertex + static_cast<uint32_t>(indices[i + 2]);

            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            {
                continue;
            }

            GPUTriangle tri;
            tri.v0 = vertices[i0].Position;
            tri.v1 = vertices[i1].Position;
            tri.v2 = vertices[i2].Position;

            // Compute face normal: cross(v1-v0, v2-v0), normalized
            XMVECTOR e0 = XMVectorSubtract(XMLoadFloat3(&tri.v1), XMLoadFloat3(&tri.v0));
            XMVECTOR e1 = XMVectorSubtract(XMLoadFloat3(&tri.v2), XMLoadFloat3(&tri.v0));
            XMVECTOR normal = XMVector3Normalize(XMVector3Cross(e0, e1));
            XMStoreFloat3(&tri.normal, normal);

            triangles.push_back(tri);
        }
    }

    m_triangleCount = static_cast<UINT>(triangles.size());

    OutputDebugStringA(("Loaded " + std::to_string(m_triangleCount) + " triangles for ray tracing\n").c_str());

    // Create GPU buffer (Upload heap so the shader can read it directly via root SRV)
    const UINT bufferSize = static_cast<UINT>(triangles.size() * sizeof(GPUTriangle));

    ResourceDesc triBufferDesc = {};
    triBufferDesc.type = ResourceDesc::ResourceType::Buffer;
    triBufferDesc.usage = ResourceDesc::Usage::Upload;
    triBufferDesc.width = bufferSize;

    m_triangleBuffer = m_device->createResource(triBufferDesc);

    // Copy triangle data to the GPU buffer
    void* mappedData = nullptr;
    if (m_triangleBuffer->map(&mappedData))
    {
        memcpy(mappedData, triangles.data(), bufferSize);
        m_triangleBuffer->unmap();
    }
    else
    {
        throw std::runtime_error("Failed to map triangle buffer");
    }
}

void RayTracerDemo::CreateConstantBuffers()
{
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_sceneCBs[i] = std::make_unique<UploadBuffer<RayTracingSceneConstants>>(m_device.get(), 1, true);
    }
}

void RayTracerDemo::CreateRootSignature()
{
    // Create root signature
    // Root parameter 0: CBV at b0 (scene constants - inline root descriptor)
    // Root parameter 1: SRV at t0 (triangle structured buffer - inline root descriptor)

    RootSignatureRangeDesc frameCbv = {};
    frameCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    frameCbv.numParameters = 1;
    frameCbv.shaderRegister = 0;

    RootSignatureRangeDesc triangleSrv = {};
    triangleSrv.type = RootSignatureRangeDesc::RangeType::ShaderResourceView;
    triangleSrv.numParameters = 1;
    triangleSrv.shaderRegister = 0;
    triangleSrv.useRootDescriptor = true; // Structured buffer -> bind as inline root SRV (t0)

    RootSignatureTableLayoutDesc table = {};
    table.visibility = RootSignatureTableLayoutDesc::ShaderVisibility::All;
    table.rangeDescs = { frameCbv, triangleSrv };

    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.tableLayoutDescs = { table };

    m_rootSignature = m_device->createRootSignature(rootSigDesc);
    m_rootSignature->createRootSignature();
}

void RayTracerDemo::CreatePipeline()
{
    // Compile shader
    ShaderDesc shaderDesc = {};
    shaderDesc.shaderFilePath = L"Shaders\\simpleGpuRt.hlsl";
    shaderDesc.shaderName = "RayTracerDemo";
    shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(shaderDesc);

    // Create pipeline state
    PipelineDesc pipelineDesc = {};
    pipelineDesc.rtvFormats = { ResourceFormat::R8G8B8A8_UNORM_SRGB };
	pipelineDesc.dsvFormat = ResourceFormat::Unknown; // No depth buffer
    pipelineDesc.inputLayout = InputLayoutDesc::build({
        InputElementDesc::setAsPosition(0, ResourceFormat::R32G32B32_FLOAT, 0, 0)
        });

    m_pipeline = m_device->createPipeline(pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());
}

void RayTracerDemo::CreateCommandObjects()
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

void RayTracerDemo::UpdateConstantBuffers()
{
    // Update time
    m_time += 0.01f;

	// Scene constants for ray tracing shader
    RayTracingSceneConstants sceneConstants;

    // Build a fixed camera; we invert its view-projection below to generate primary rays
    XMVECTOR eyePos = XMVectorSet(0.0f, 0.0f, -15.0f, 1.0f);
    XMVECTOR lookAt = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, lookAt, up);

    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);
    XMMATRIX viewProj = view * proj;

    XMMATRIX cubeTranslation = XMMatrixTranslation(-5.0f, 1.5f, 0.0f);
    XMFLOAT4 cubeMin = XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f);
    XMFLOAT4 cubeMax = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    // Transform cube corners by cube translation matrix
    XMVECTOR cubeMinVec = XMLoadFloat4(&cubeMin);
    XMVECTOR cubeMaxVec = XMLoadFloat4(&cubeMax);
    XMVECTOR transformedCubeMin = XMVector3Transform(cubeMinVec, cubeTranslation);
    XMVECTOR transformedCubeMax = XMVector3Transform(cubeMaxVec, cubeTranslation);


    // Compute inverse view-projection matrix for ray generation
    XMMATRIX viewProjInverse = XMMatrixInverse(nullptr, viewProj);

    // Initialize scene parameters
    XMStoreFloat4(&sceneConstants.CameraPos, eyePos);                 // Update camera position
    sceneConstants.Sphere = XMFLOAT4(1.2 * sinf(m_time), 0.0f, 2.8f * cosf(m_time), 1.0f);        // Sphere moving in elliptical path
    sceneConstants.Plane = XMFLOAT4(0.0f, 1.0f, 0.0f, 2.0f);         // Plane with normal (0,1,0), distance 2
    sceneConstants.LightPos = XMFLOAT4(3.0f, 3.0f, -3.0f, 1.0f);         // Light position
    sceneConstants.SphereColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);       // Red sphere
    sceneConstants.PlaneColor = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);        // Green plane
    XMStoreFloat4(&sceneConstants.CubeMin, transformedCubeMin);
    XMStoreFloat4(&sceneConstants.CubeMax, transformedCubeMax);
    sceneConstants.CubeColor = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);       // Blue cube
    sceneConstants.NumTriangles = m_triangleCount;

    XMStoreFloat4x4(&sceneConstants.InvViewProj, XMMatrixTranspose(viewProjInverse));

    // Copy data to the current back buffer's constant buffers
    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
    m_sceneCBs[backBufferIndex]->CopyData(0, sceneConstants);
}

void RayTracerDemo::Render()
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
    DrawDemoSwitcher();

    // Record commands
    // Retrieve current back buffer resource and RTV for render pass setup
    ResourceDx12* currentBackBuffer = m_swapChain->getCurrentBackBuffer();
    ResourceView currentRtView = m_swapChain->getCurrentRTView();

    // Build render pass descriptor for current frame
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ResourceView dsvHandle = {};
    RenderPassDesc renderPassDesc = RenderPassDesc::buildAsSingleRenderTarget(
        currentRtView,
        dsvHandle,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        clearColor,
        false);
    renderPassDesc.debugName = "Cube Render Pass";

    // Test command list recording
    m_commandList->begin(currentFrameContext.commandAllocator.Get());
    m_commandList->beginRenderPass(renderPassDesc);

    // Transition back buffer to render target state for rendering
    m_commandList->transitionResource(
        currentBackBuffer,
        ResourceBindFlags::RenderTarget);

    m_commandList->clearAndSetRenderTargets(renderPassDesc);

    {
        // Bind root signature and pipeline state
        m_commandList->setGraphicsRootSignature(m_rootSignature.get());
        m_commandList->setPipeline(m_pipeline.get());

        // Bind constant buffers to root parameters (descriptor tables or root descriptors 
        // depending on how we set up the root signature)
        m_commandList->setConstantBufferView(
            0,
            m_sceneCBs[backBufferIndex]->getResource()->GetGPUVirtualAddress());

        // Bind triangle structured buffer as SRV (root parameter 1)
        m_commandList->setShaderResourceView(
            1,
            m_triangleBuffer->getNativeResource()->GetGPUVirtualAddress());

        // Bind geometry
        m_commandList->setVertexBuffer(0, m_vertexBufferView);
        m_commandList->setIndexBuffer(m_indexBufferView);

        m_commandList->drawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Render ImGui (needs SRV descriptor heap set since ImGui uses a font texture)
        m_commandList->setDescriptorHeaps(m_srvHeap.get(), 1);
        m_imguiLoader.Render(m_commandList.get());
    }

    m_commandList->endRenderPass();

    // Transition back buffer to present state for presentation
    m_commandList->transitionResource(
        currentBackBuffer,
        ResourceBindFlags::Present);

    m_commandList->end();

    // Execute command list
    m_device->executeCommandList(m_commandList.get());
    // Present the frame
    m_swapChain->present(true);

    // Signal and increment the fence value for the current frame
    currentFrameContext.fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(currentFrameContext.fenceValue);
}

void RayTracerDemo::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    m_imguiLoader.Shutdown();

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down RayTracerDemo and releasing resources.\n");
}

void RayTracerDemo::Resize(unsigned int width, unsigned int height)
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
    }
}
