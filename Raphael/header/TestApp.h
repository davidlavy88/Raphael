#pragma once
#include "DeviceDx12.h"
#include "ResourceDx12.h"
#include "CommandList.h"
#include "ShaderDx12.h"
#include "RootSignatureDx12.h"
#include "DescriptorHeapDx12.h"
#include "PipelineDx12.h"
#include "SwapChainDx12.h"
#include "FrameContext.h"

using namespace raphael;

static constexpr uint32_t g_frameCount = 2;

class TestApp
{
public:
    bool Initialize();
    void Shutdown();
    void Run();
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool CreateAppWindow();
    void DestroyAppWindow();
    static LRESULT WINAPI StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    std::unique_ptr<DeviceDx12> m_device;
    std::unique_ptr<ResourceDx12> m_bufferResource;
    std::unique_ptr<ResourceDx12> m_depthBuffer;
    std::unique_ptr<ResourceDx12> m_vertexBuffer;
    std::unique_ptr<DescriptorHeapDx12> m_dsvHeap;
    std::unique_ptr<DescriptorHeapDx12> m_rtvHeap;
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<CommandList> m_commandList;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;
    std::unique_ptr<SwapChainDx12> m_swapChain;

    // Per-frame resources for double buffering
    // Using array for a fixed number of frames (e.g., double buffering)
    std::array<FrameContext, g_frameCount> m_frameContexts;
    UINT m_frameIndex = 0; // Current frame index for double buffering

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    HWND m_hwnd = nullptr;
};
