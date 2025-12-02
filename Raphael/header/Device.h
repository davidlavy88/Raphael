#pragma once
#include "D3D12CommonHeaders.h"
#include "DescriptorHeapAllocator.h"

static constexpr int NUM_FRAMES_IN_FLIGHT = 2;
static constexpr int SRV_HEAP_SIZE = 64;

// Frame context structure
struct FrameContext
{
    ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;
    UINT64 FenceValue = 0;
};

// D3D12 Device wrapper class
class D3D12Device
{
public:
    bool Initialize();

    void Shutdown();

    ComPtr<ID3D12Device> GetDevice() const { return m_device; }
    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return m_commandQueue; }
    ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return m_commandList; }
    ComPtr<ID3D12DescriptorHeap> GetRTVHeap() const { return m_rtvHeap; }
    ComPtr<ID3D12DescriptorHeap> GetDSVHeap() const { return m_dsvHeap; }
    ComPtr<ID3D12DescriptorHeap> GetSRVHeap() const { return m_srvHeap; }
    DescriptorHeapAllocator& GetSRVAllocator() { return m_srvAllocator; }
    DXGI_FORMAT GetBackBufferFormat() const { return mBackBufferFormat; }
    DXGI_FORMAT GetDepthStencilFormat() const { return mDepthStencilFormat; }

    FrameContext* WaitForNextFrame();

    void SignalAndIncrementFence(FrameContext* frameContext);

    void WaitForGpu();

    void IncrementFrameIndex() { m_frameIndex++; }

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    bool m_appPaused = false;          // Is the application paused?

private:
    bool CreateDescriptorHeaps();

private:
    ComPtr<ID3D12Device> m_device = nullptr;
    ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_commandList = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;
    DescriptorHeapAllocator m_srvAllocator;
    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceLastSignaled = 0;
    UINT m_frameIndex = 0;
    FrameContext m_frameContexts[NUM_FRAMES_IN_FLIGHT];

    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // TODO: To be implemented if needed
    //// Set true to use 4X MSAA (§4.1.8).  The default is false.
    //bool      m4xMsaaState = false;    // 4X MSAA enabled
    //UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA
};
