#pragma once
#include "ObjectDescriptors.h"
#include <imgui.h>

namespace raphael
{
    class DeviceDx12;
    class ResourceDx12;
    class DescriptorHeapDx12;

    class SwapChainDx12
    {
    public:
        SwapChainDx12(DeviceDx12* device, DescriptorHeapDx12* rtvHeap, const SwapChainDesc& desc);
        ~SwapChainDx12();

        void createSwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue);
        bool present(bool vsync);
        void resize(UINT width, UINT height);
        void createRenderTargetViews(DescriptorHeapDx12* rtvHeap);

        ResourceDx12* getCurrentBackBuffer() const { return m_backBuffers[m_currentBackBufferIndex].get(); }
        D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTVHandle() const { return m_rtvHandles[m_currentBackBufferIndex]; }
        UINT getCurrentBackBufferIndex() const { return m_currentBackBufferIndex; }
        ResourceFormat getFormat() const { return m_desc.format; }

    private:
        DeviceDx12* m_device = nullptr;
        DescriptorHeapDx12* m_rtvHeap = nullptr;
        SwapChainDesc m_desc = {};
        ComPtr<IDXGISwapChain4> m_swapChain;
        ComPtr<IDXGIFactory4> m_factory;
        // TODO: Check if this approach is not the best: Using unique_ptr and ResourceDx12 instead of raw ID3D12Resource* to manage the lifetime of back buffer resources
        std::array<std::unique_ptr<ResourceDx12>, NUM_BACK_BUFFERS> m_backBuffers;
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, NUM_BACK_BUFFERS> m_rtvHandles;
        UINT m_currentBackBufferIndex = 0;
        HANDLE m_waitableObject = nullptr;
    };
}
