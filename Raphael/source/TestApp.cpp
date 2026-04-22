#include "TestApp.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "GPUStructs.h"


using namespace raphael;

int WINDOW_WIDTH = 1280;
int WINDOW_HEIGHT = 720;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool TestApp::Initialize()
{
    // Create application window
    // TODO: Make Window its own class
    if (!CreateAppWindow())
        return false;
    
    // Create device
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;

    // TODO: CreateDevice should be called from m_device 
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

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

    // Create swap chain
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.width = WINDOW_WIDTH;
    swapChainDesc.height = WINDOW_HEIGHT;
    swapChainDesc.bufferCount = g_frameCount;
    swapChainDesc.windowHandle = m_hwnd;

    m_swapChain = m_device->createSwapChain(m_rtvHeap.get(), swapChainDesc);

    // Create depth buffer resource
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

    // Define triangle vertex data
    SimpleVertex triangleVertices[] =
    {
        { XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }
    };
    const UINT vertexBufferSize = sizeof(triangleVertices);

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
        memcpy(vertexData, triangleVertices, vertexBufferSize);
        m_vertexBuffer->unmap();
    }
     else
    {
        OutputDebugStringA("Failed to map vertex buffer resource.\n");
        return false;
    }

    // Create vertex buffer view
    m_vertexBufferView = m_vertexBuffer->getResourceView(ResourceBindFlags::VertexBuffer, {}, sizeof(SimpleVertex));

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

    // Create buffer resource
    ResourceDesc bufferDesc = {};
    bufferDesc.type = ResourceDesc::ResourceType::Buffer;
    bufferDesc.usage = ResourceDesc::Usage::Upload;
    bufferDesc.width = 1024;

    m_bufferResource = m_device->createResource(bufferDesc);

    // Compile shader
    ShaderDesc shaderDesc = {};
    shaderDesc.shaderFilePath = L"Shaders\\testShader.hlsl";
    shaderDesc.shaderName = "TestShader";
    shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(shaderDesc);

    // Create root signature
    RootSignatureRangeDesc rangeDesc = {};
    rangeDesc.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    rangeDesc.numParameters = 1;
    rangeDesc.shaderRegister = 0;

    RootSignatureTableLayoutDesc tableLayoutDesc = {};
    tableLayoutDesc.rangeDescs.push_back(rangeDesc);
    tableLayoutDesc.visibility = RootSignatureTableLayoutDesc::ShaderVisibility::All;

    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.tableLayoutDescs.push_back(tableLayoutDesc);

    m_rootSignature = std::make_unique<RootSignatureDx12>(m_device.get(), rootSigDesc);
    m_rootSignature->createRootSignature();

    // Create pipeline state
    PipelineDesc pipelineDesc = {};
    pipelineDesc.inputLayout = InputLayoutDesc::build({
        InputElementDesc::setAsPosition(0, ResourceFormat::R32G32B32_FLOAT, 0, 0),
        InputElementDesc::setAsColor(0, ResourceFormat::R32G32B32A32_FLOAT, 0, 12)
        });

    m_pipeline = m_device->createPipeline(pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());

    // Show window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);

    return true;
}

void TestApp::Run()
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
        renderPassDesc.debugName= "Main Render Pass";

        // Test command list recording
        m_commandList->begin(currentFrameContext.commandAllocator.Get());

        m_commandList->beginRenderPass(renderPassDesc);

        // Draw
        m_commandList->setGraphicsRootSignature(m_rootSignature.get());
        m_commandList->setPipeline(m_pipeline.get());
        m_commandList->setVertexBuffer(0, m_vertexBufferView);
        m_commandList->drawInstanced(3, 1, 0, 0);

        m_commandList->endRenderPass();

        m_commandList->end();

        // Execute command list
        m_device->executeCommandList(m_commandList.get());
        // Present the frame
        m_swapChain->present(true);

        // Signal and increment the fence value for the current frame
        currentFrameContext.fenceValue = m_device->getNextFenceValue();
        m_device->signalFence(currentFrameContext.fenceValue);

        //// Move to the next frame index
        //m_frameIndex = (m_frameIndex + 1) % g_frameCount;
    }
}

void TestApp::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down TestApp and releasing resources.\n");
}

LRESULT TestApp::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

bool TestApp::CreateAppWindow()
{
    // Implementation for creating application window
    WNDCLASSEXW wc = {
            sizeof(wc), CS_CLASSDC, StaticWndProc, 0L, 0L,
            GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"ImGui Example", nullptr
    };

    ::RegisterClassExW(&wc);

    m_hwnd = ::CreateWindowW(
        wc.lpszClassName, L"Dear ImGui DirectX12 Example", WS_OVERLAPPEDWINDOW,
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, wc.hInstance, this
    );

    return m_hwnd != nullptr;
}

void TestApp::DestroyAppWindow()
{
    // Implementation for destroying application window
    if (m_hwnd)
    {
        ::DestroyWindow(m_hwnd);
        ::UnregisterClassW(L"Raphael Engine", GetModuleHandle(nullptr));
        m_hwnd = nullptr;
    }
}

LRESULT WINAPI TestApp::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Retrieve the TestApp instance from window user data
    TestApp* app = nullptr;
    
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        app = static_cast<TestApp*>(cs->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<TestApp*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (app)
        return app->HandleMessage(hWnd, msg, wParam, lParam);

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
