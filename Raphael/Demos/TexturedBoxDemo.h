#pragma once
#include "IDemo.h"
#include "DX12/DeviceDx12.h"
#include "DX12/ResourceDx12.h"
#include "DX12/CommandList.h"
#include "DX12/ShaderDx12.h"
#include "DX12/RootSignatureDx12.h"
#include "DX12/DescriptorHeapDx12.h"
#include "DX12/PipelineDx12.h"
#include "DX12/SwapChainDx12.h"
#include "DX12/FrameContext.h"
#include "DX12/UploadBufferDx12.h"
#include "GPUStructs.h"
#include "ImGui/ImGuiLoader.h"
#include "Components/Window.h"
#include "Texture/Texture.h"

using namespace raphael;

class TexturedBoxImGui : public ImGuiLoader
{
public:
    void Display() override;
};

class TexturedBoxDemo : public IDemo
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
    void CreateTexture();
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
    std::shared_ptr<DescriptorHeapDx12> m_srvHeap;
    std::unique_ptr<ResourceDx12> m_depthBuffer;

    // Geometry resources
    std::unique_ptr<ResourceDx12> m_vertexBuffer;
    std::unique_ptr<ResourceDx12> m_indexBuffer;
    ResourceView m_vertexBufferView = {};
    ResourceView m_indexBufferView = {};
    UINT m_indexCount = 0;

    // Texture resources
    std::unique_ptr<Texture> m_textureData;

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
    TexturedBoxImGui m_imguiLoader;

    // Window handle
    HWND m_windowHandle = nullptr;
};
