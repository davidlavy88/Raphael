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
#include "ImGuiLoader.h"
#include "Window.h"

using namespace raphael;

static constexpr uint32_t g_frameCount = 2;

class BoxDemo : public IDemo
{
public:
    bool Initialize(WindowInfo windowInfo) override;
    void Shutdown() override;
    void Render() override;
    void Resize(unsigned int width, unsigned int height) override;
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    std::unique_ptr<DescriptorHeapDx12> m_dsvHeap;
    std::unique_ptr<DescriptorHeapDx12> m_rtvHeap;
    std::unique_ptr<DescriptorHeapDx12> m_srvHeap;
    std::unique_ptr<ResourceDx12> m_depthBuffer;

    // Geometry resources
    std::unique_ptr<ResourceDx12> m_vertexBuffer;
    std::unique_ptr<ResourceDx12> m_indexBuffer;
    ResourceView m_vertexBufferView = {};
    ResourceView m_indexBufferView = {};
    UINT m_indexCount = 0;

    // Constant buffers (one per frame for double buffering)
    std::array<std::unique_ptr<UploadBuffer<FrameConstants>>, g_frameCount> m_frameCBs;
    std::array<std::unique_ptr<UploadBuffer<BasicObjectConstants>>, g_frameCount> m_objectCBs;

    // Pipeline resources
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;

    // Render state
    ResourceView m_depthStencilView = {};
    // Per-frame resources for double buffering
    std::array<FrameContext, g_frameCount> m_frameContexts;

    // Camera and transform state
    float m_rotationAngle = 0.0f;

    // ImGui support
    ImGuiLoader m_imguiLoader;
};
