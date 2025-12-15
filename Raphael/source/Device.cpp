#include "Device.h"

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

    // Create command allocator
    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator))))
        return false;

    // Create command list
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList))))
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

    /*for (auto& frameContext : m_frameContexts)
		frameContext.CommandAllocator.Reset();*/

    if (m_commandList) { m_commandList.Reset(); }
    if (m_commandQueue) { m_commandQueue.Reset(); }
    if (m_rtvHeap) { m_rtvHeap.Reset(); }
    if (m_dsvHeap) { m_dsvHeap.Reset(); }
    if (m_srvHeap) { m_srvHeap.Reset(); }
    if (m_fence) { m_fence->Release(); m_fence = nullptr; }
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    if (m_device) { m_device.Reset(); }

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
    FrameContext* frameContext = m_frameContexts[m_frameIndex % NUM_FRAMES_IN_FLIGHT].get();
    if (m_fence->GetCompletedValue() < frameContext->FenceValue)
    {
        m_fence->SetEventOnCompletion(frameContext->FenceValue, m_fenceEvent);
        ::WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    return frameContext;
}

FrameContext* D3D12Device::GetCurrentFrameContext()
{
    return m_frameContexts[m_frameIndex % NUM_FRAMES_IN_FLIGHT].get();
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

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Device::DepthStencilView()const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
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

    // Create DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // Assuming one depth stencil view
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 1;
    if (FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    // Create SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = SRV_HEAP_SIZE;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    m_srvAllocator.Initialize(m_device.Get(), m_srvHeap.Get());
    return true;
}

bool D3D12Device::CreateFrameContexts(int passCount, int objectCount)
{
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
    {
        m_frameContexts.push_back(std::make_unique<FrameContext>(m_device.Get(), passCount, objectCount));
    }

    return true;
}
