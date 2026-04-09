#include "SwapChainDx12.h"
#include "DeviceDx12.h"
#include "DescriptorHeapDx12.h"
#include "UtilDx12.h"

namespace raphael
{
    SwapChainDx12::SwapChainDx12(DeviceDx12* device, DescriptorHeapDx12* rtvHeap, const SwapChainDesc& desc)
        : m_device(device), m_rtvHeap(rtvHeap), m_desc(desc)
    {
        // Setup swap chain description
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = desc.bufferCount;
        swapChainDesc.Width = desc.width;
        swapChainDesc.Height = desc.height;
        swapChainDesc.Format = convertFormatToDXGI(desc.format);
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.Stereo = FALSE;

        // Create DXGI factory
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory))))
        {
            throw std::runtime_error("Failed to create DXGI factory");
        }

        // TOD: Check for tearing support

        // Create swap chain
        IDXGISwapChain1* swapChain1 = nullptr;
        if (FAILED(m_factory->CreateSwapChainForHwnd(device->getCommandQueue(), desc.windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain1)))
        {
            throw std::runtime_error("Failed to create swap chain");
        }

        if (FAILED(swapChain1->QueryInterface(IID_PPV_ARGS(&m_swapChain))))
        {
            swapChain1->Release();
            throw std::runtime_error("Failed to query swap chain interface");
        }

        swapChain1->Release();

        m_swapChain->SetMaximumFrameLatency(desc.bufferCount);
        m_waitableObject = m_swapChain->GetFrameLatencyWaitableObject();
        // Then, before each frame, you wait on it:
        WaitForSingleObjectEx(m_waitableObject, 1000, TRUE);

        // Initialize back buffer resources and RTV handles
        createRenderTargetViews(m_rtvHeap);

        // Query the initial back buffer index
        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    SwapChainDx12::~SwapChainDx12()
    {
        // Ensure we are not in fullscreen mode before releasing resources
        m_swapChain->SetFullscreenState(FALSE, nullptr);

        if (m_waitableObject)
        {
            CloseHandle(m_waitableObject);
            m_waitableObject = nullptr;
        }
    }

    bool SwapChainDx12::present(bool vsync)
    {
        UINT syncInterval = vsync ? 1 : 0;
        UINT presentFlags = 0;
        if (FAILED(m_swapChain->Present(syncInterval, presentFlags)))
        {
            return false;
        }

        // Update the back buffer index after presenting
        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        return true;
    }

    void SwapChainDx12::resize(UINT width, UINT height)
    {
        // First, release references to back buffers and RTVs
        for (UINT i = 0; i < m_desc.bufferCount; i++)
        {
            m_backBuffers[i].reset();
            m_rtvHandles[i] = {};
        }

        // Resize the swap chain buffers
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        m_swapChain->GetDesc1(&desc);
        HRESULT result = m_swapChain->ResizeBuffers(0, width, height, convertFormatToDXGI(m_desc.format), desc.Flags);
        if (FAILED(result))
        {
            throw std::runtime_error("Failed to resize swap chain buffers");
        }

        createRenderTargetViews(m_rtvHeap);

        // Reset back buffer index after resize
        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    void SwapChainDx12::createRenderTargetViews(DescriptorHeapDx12* rtvHeap)
    {

        for (UINT i = 0; i < m_desc.bufferCount; i++)
        {
            ID3D12Resource* backBuffer = nullptr;
            if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer))))
            {
                throw std::runtime_error("Failed to get back buffer from swap chain");
            }
            // Wrap the back buffer in our ResourceDx12 class, no need to release the raw resource since ResourceDx12 will manage its lifetime
            m_backBuffers[i] = std::make_unique<ResourceDx12>(m_device, backBuffer);
            DescriptorHandle handle = {};
            rtvHeap->getDescriptorHandle(i, &handle);
            m_rtvHandles[i] = handle.cpuHandle;
            // Create RTV handle for this back buffer
            m_backBuffers[i]->initAsRtv(m_rtvHandles[i]);
        }
    }
}