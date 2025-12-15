#pragma once
#include "D3D12CommonHeaders.h"
#include "Device.h"

// SwapChain wrapper class
class SwapChain
{
public:
    bool Initialize(HWND hwnd, D3D12Device& device);
    void Shutdown();
    void Resize(UINT width, UINT height, D3D12Device& device);
    HRESULT Present(bool vsync = true);
    UINT GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const { return m_rtvHandles[index]; }
    bool IsOccluded() const { return m_occluded; }
    HANDLE GetWaitableObject() const { return m_waitableObject; }

private:
    void CreateRenderTargetViews(D3D12Device& device);
    void CleanupRenderTargetViews();
    void CreateDepthStencilView(D3D12Device& device, UINT width = 0, UINT height = 0);
    void CleanupDepthStencilView();

private:
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12Resource* m_backBuffers[NUM_BACK_BUFFERS] = {};
    ID3D12Resource* m_depthStencilBuffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[NUM_BACK_BUFFERS] = {};
    bool m_tearingSupport = false;
    bool m_occluded = false;
    HANDLE m_waitableObject = nullptr;
};
