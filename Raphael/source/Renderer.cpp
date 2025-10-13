#include "Renderer.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// Descriptor heap allocator class
void DescriptorHeapAllocator::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
    IM_ASSERT(m_heap == nullptr && m_freeIndices.empty());

    m_heap = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    m_heapType = desc.Type;
    m_heapStartCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_heapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_handleIncrement = device->GetDescriptorHandleIncrementSize(m_heapType);

    m_freeIndices.reserve(static_cast<int>(desc.NumDescriptors));
    for (int n = desc.NumDescriptors; n > 0; n--)
        m_freeIndices.push_back(n - 1);
}

void DescriptorHeapAllocator::Shutdown()
{
    m_heap = nullptr;
    m_freeIndices.clear();
}

void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    IM_ASSERT(m_freeIndices.Size > 0);
    int idx = m_freeIndices.back();
    m_freeIndices.pop_back();
    outCpuHandle->ptr = m_heapStartCpu.ptr + (idx * m_handleIncrement);
    outGpuHandle->ptr = m_heapStartGpu.ptr + (idx * m_handleIncrement);
}

void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
    int cpuIdx = static_cast<int>((cpuHandle.ptr - m_heapStartCpu.ptr) / m_handleIncrement);
    int gpuIdx = static_cast<int>((gpuHandle.ptr - m_heapStartGpu.ptr) / m_handleIncrement);
    IM_ASSERT(cpuIdx == gpuIdx);
    m_freeIndices.push_back(cpuIdx);
}

// D3D12 Device wrapper class
bool D3D12Device::Initialize()
{
    // Enable debug layer
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
    }
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_device))))
        return false;

    // Setup debug interface to break on errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (debug != nullptr)
    {
        ID3D12InfoQueue* infoQueue = nullptr;
        m_device->QueryInterface(IID_PPV_ARGS(&infoQueue));
        if (infoQueue)
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            infoQueue->Release();
        }
        debug->Release();
    }
#endif

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 1;
    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))))
        return false;

    // Create descriptor heaps
    if (!CreateDescriptorHeaps())
        return false;

    // Create command allocators for each frame
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
    {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContexts[i].CommandAllocator))))
            return false;
    }

    // Create command list
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_frameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_commandList))))
        return false;

    if (FAILED(m_commandList->Close()))
        return false;

    // Create fence
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
        return false;

    return true;
}

void D3D12Device::Shutdown()
{
    WaitForGpu();

    for (auto& frameContext : m_frameContexts)
        frameContext.~FrameContext();

    if (m_commandList) { m_commandList->Release(); m_commandList = nullptr; }
    if (m_commandQueue) { m_commandQueue->Release(); m_commandQueue = nullptr; }
    if (m_rtvHeap) { m_rtvHeap->Release(); m_rtvHeap = nullptr; }
    if (m_srvHeap) { m_srvHeap->Release(); m_srvHeap = nullptr; }
    if (m_fence) { m_fence->Release(); m_fence = nullptr; }
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* debug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
    {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        debug->Release();
    }
#endif
}

FrameContext* D3D12Device::WaitForNextFrame()
{
    FrameContext* frameContext = &m_frameContexts[m_frameIndex % NUM_FRAMES_IN_FLIGHT];
    if (m_fence->GetCompletedValue() < frameContext->FenceValue)
    {
        m_fence->SetEventOnCompletion(frameContext->FenceValue, m_fenceEvent);
        ::WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    return frameContext;
}

void D3D12Device::SignalAndIncrementFence(FrameContext* frameContext)
{
    m_commandQueue->Signal(m_fence, ++m_fenceLastSignaled);
    frameContext->FenceValue = m_fenceLastSignaled;
}

void D3D12Device::WaitForGpu()
{
    m_commandQueue->Signal(m_fence, ++m_fenceLastSignaled);
    m_fence->SetEventOnCompletion(m_fenceLastSignaled, m_fenceEvent);
    ::WaitForSingleObject(m_fenceEvent, INFINITE);
}

bool D3D12Device::CreateDescriptorHeaps()
{
    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = NUM_BACK_BUFFERS;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 1;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    // Create SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = SRV_HEAP_SIZE;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    m_srvAllocator.Initialize(m_device, m_srvHeap);
    return true;
}

// SwapChain wrapper class
bool SwapChain::Initialize(HWND hwnd, D3D12Device& device)
{
    // Setup swap chain description
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.Stereo = FALSE;

    // Create DXGI factory
    IDXGIFactory5* dxgiFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))))
        return false;

    // Check for tearing support
    BOOL allowTearing = FALSE;
    dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    m_tearingSupport = (allowTearing == TRUE);
    if (m_tearingSupport)
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    // Create swap chain
    IDXGISwapChain1* swapChain1 = nullptr;
    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(device.GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1)))
    {
        dxgiFactory->Release();
        return false;
    }

    if (FAILED(swapChain1->QueryInterface(IID_PPV_ARGS(&m_swapChain))))
    {
        swapChain1->Release();
        dxgiFactory->Release();
        return false;
    }

    if (m_tearingSupport)
        dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    swapChain1->Release();
    dxgiFactory->Release();

    m_swapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
    m_waitableObject = m_swapChain->GetFrameLatencyWaitableObject();

    // Create render target views
    CreateRenderTargetViews(device);
    return true;
}

void SwapChain::Shutdown()
{
    if (m_swapChain)
    {
        m_swapChain->SetFullscreenState(false, nullptr);
        m_swapChain->Release();
        m_swapChain = nullptr;
    }

    if (m_waitableObject)
    {
        CloseHandle(m_waitableObject);
        m_waitableObject = nullptr;
    }

    CleanupRenderTargetViews();
}

void SwapChain::Resize(UINT width, UINT height, D3D12Device& device)
{
    CleanupRenderTargetViews();
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swapChain->GetDesc1(&desc);
    HRESULT result = m_swapChain->ResizeBuffers(0, width, height, desc.Format, desc.Flags);
    assert(SUCCEEDED(result) && "Failed to resize swapchain.");
    CreateRenderTargetViews(device);
}

HRESULT SwapChain::Present(bool vsync)
{
    UINT syncInterval = vsync ? 1 : 0;
    UINT presentFlags = (!vsync && m_tearingSupport) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
    m_occluded = (hr == DXGI_STATUS_OCCLUDED);
    return hr;
}

void SwapChain::CreateRenderTargetViews(D3D12Device& device)
{
    SIZE_T rtvDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = device.GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* backBuffer = nullptr;
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
        device.GetDevice()->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
        m_backBuffers[i] = backBuffer;
        m_rtvHandles[i] = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }
}

void SwapChain::CleanupRenderTargetViews()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (m_backBuffers[i])
        {
            m_backBuffers[i]->Release();
            m_backBuffers[i] = nullptr;
        }
    }
}

// Renderer class that handles rendering operations
bool Renderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
{
    m_device = &device;
    m_swapChain = &swapChain;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = device.GetDevice();
    initInfo.CommandQueue = device.GetCommandQueue();
    initInfo.NumFramesInFlight = NUM_FRAMES_IN_FLIGHT;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = device.GetSRVHeap();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
        {
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Alloc(out_cpu_handle, out_gpu_handle);
        };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
        {
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Free(cpu_handle, gpu_handle);
        };

    // Store this pointer for callbacks
    ImGui::GetIO().UserData = this;

    ImGui_ImplDX12_Init(&initInfo);

    return true;
}

void Renderer::Shutdown()
{
    if (m_device)
        m_device->WaitForGpu();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Renderer::NewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::Render(const ImVec4& clearColor)
{
    ImGui::Render();

    FrameContext* frameContext = m_device->WaitForNextFrame();
    UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    frameContext->CommandAllocator->Reset();

    ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList();
    cmdList->Reset(frameContext->CommandAllocator, nullptr);

    // Transition to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_swapChain->GetBackBuffer(backBufferIdx);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &barrier);

    // Clear and set render target
    const float clearColorArray[4] = {
        clearColor.x * clearColor.w,
        clearColor.y * clearColor.w,
        clearColor.z * clearColor.w,
        clearColor.w
    };
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(backBufferIdx);
    cmdList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    ID3D12DescriptorHeap* srvHeap = m_device->GetSRVHeap();
    cmdList->SetDescriptorHeaps(1, &srvHeap);

    // Render ImGui
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    // Transition back to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // Execute commands
    ID3D12CommandList* cmdLists[] = { cmdList };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
    m_device->SignalAndIncrementFence(frameContext);
}
