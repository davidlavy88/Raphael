#include "SwapChain.h"

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
    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(device.GetCommandQueue().Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1)))
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

    RECT rcWnd;
    GetWindowRect(hwnd, &rcWnd);
    UINT width = rcWnd.right - rcWnd.left;
    UINT height = rcWnd.bottom - rcWnd.top;
    // Create render target views
    CreateRenderTargetViews(device);
    // Create depth stencil view
    FrameContext* frameContext = device.WaitForNextFrame();
    frameContext->CommandAllocator->Reset();
    device.GetCommandList()->Reset(frameContext->CommandAllocator.Get(), nullptr);

    CreateDepthStencilView(device, width, height);

    device.GetCommandList()->Close();
    ID3D12CommandList* const commandLists[] = { device.GetCommandList().Get()};
    device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    device.WaitForGpu();

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

    // Release back buffers
    for (int i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (m_backBuffers[i])
        {
            m_backBuffers[i]->Release();
            m_backBuffers[i] = nullptr;
        }
    }

    // Release depth stencil buffer
    if (m_depthStencilBuffer)
    {
        m_depthStencilBuffer->Release();
        m_depthStencilBuffer = nullptr;
    }

    CleanupRenderTargetViews();
    CleanupDepthStencilView();
}

void SwapChain::Resize(UINT width, UINT height, D3D12Device& device)
{
    FrameContext* frameContext = device.WaitForNextFrame();
    frameContext->CommandAllocator->Reset();
    device.GetCommandList()->Reset(frameContext->CommandAllocator.Get(), nullptr);

    CleanupRenderTargetViews();
    CleanupDepthStencilView();
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    m_swapChain->GetDesc1(&desc);
    HRESULT result = m_swapChain->ResizeBuffers(0, width, height, desc.Format, desc.Flags);
    assert(SUCCEEDED(result) && "Failed to resize swapchain.");
    CreateRenderTargetViews(device);
    CreateDepthStencilView(device, width, height);

    device.GetCommandList()->Close();
    ID3D12CommandList* const commandLists[] = { device.GetCommandList().Get()};
    device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
    device.WaitForGpu();
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

void SwapChain::CreateDepthStencilView(D3D12Device& device, UINT width, UINT height)
{
    SIZE_T dsvDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = device.GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = width;
    depthStencilDesc.Height = height;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (device.GetDevice()->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(&m_depthStencilBuffer)) != S_OK)
    {
        throw std::runtime_error("Failed to create depth stencil buffer.");
    }

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;
    device.GetDevice()->CreateDepthStencilView(m_depthStencilBuffer, nullptr, dsvHandle);

    // Transition the resource from its initial state to be used as a depth buffer.
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer,
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    device.GetCommandList()->ResourceBarrier(1, &barrier);
}

void SwapChain::CleanupDepthStencilView()
{
    if (m_depthStencilBuffer)
    {
        m_depthStencilBuffer->Release();
        m_depthStencilBuffer = nullptr;
    }
}
