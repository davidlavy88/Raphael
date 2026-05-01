#pragma once
#include "IDemo.h"
#include "DeviceDx12.h"
#include "ResourceDx12.h"
#include "CommandList.h"
#include "ShaderDx12.h"
#include "RootSignatureDx12.h"
#include "DescriptorHeapDx12.h"
#include "PipelineDx12.h"
#include "SwapChainDx12.h"
#include "FrameContext.h"
#include "UploadBufferDx12.h"
#include "GPUStructs.h"
#include "Window.h"

using namespace raphael;

static constexpr uint32_t g_frameCount = 2;

class RayTracerDemo : public IDemo
{
public:
    bool Initialize(WindowInfo windowInfo) override;
    void Shutdown() override;
    void Render() override;
    void Resize(unsigned int width, unsigned int height) override;

private:
    // ---- Initialization helpers (one per logical step) ----
    void CreateDescriptorHeaps();
    void CreateSwapChainAndDepthBuffer(WindowInfo windowInfo);
    void CreateGeometry();
    void CreateConstantBuffers();
    void CreateRootSignature();
    void CreatePipeline();
    void CreateCommandObjects();

    // ---- Per-frame helpers ----
    void UpdateConstantBuffers();

private:
    // Core DX12 components
    std::unique_ptr<DeviceDx12> m_device;
    std::unique_ptr<SwapChainDx12> m_swapChain;
    std::unique_ptr<CommandList> m_commandList;
    std::unique_ptr<DescriptorHeapDx12> m_rtvHeap;

    // Geometry resources
    std::unique_ptr<ResourceDx12> m_vertexBuffer;
    std::unique_ptr<ResourceDx12> m_indexBuffer;
    ResourceView m_vertexBufferView = {};
    ResourceView m_indexBufferView = {};
    UINT m_indexCount = 0;

    // Constant buffers (one per frame for double buffering)
    std::array<std::unique_ptr<UploadBuffer<RayTracingSceneConstants>>, g_frameCount> m_sceneCBs;

    // Pipeline resources
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;

    // Render state
    // Per-frame resources for double buffering
    std::array<FrameContext, g_frameCount> m_frameContexts;

    // Transform state
    float m_time = 0.0f;

    // Window handle
    HWND m_windowHandle = nullptr;
};
