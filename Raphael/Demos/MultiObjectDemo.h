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
#include "Mesh/Mesh.h"
#include "Mesh/Material.h"
#include "Components/Camera.h"

#include "tinygltf/tiny_gltf.h"

using namespace raphael;

class MultiObjectImGui : public ImGuiLoader
{
public:
    void Display() override;

    bool wireframe = false;
};

class MultiObjectDemo : public IDemo
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
    void SetupScene();
    void CreateTexture();
    void CreateDummyTexture();
    void CreateConstantBuffers();
    void CreateRootSignature();
    void CreatePipeline();
    void CreateCommandObjects();

    // ---- Per-frame helpers ----
    void UpdateConstantBuffers();

    // ---- Process input ----
    void ProcessInput();

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
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;

    // Pipeline resources
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;

    PipelineDesc m_pipelineDesc = {};
    ShaderDesc m_shaderDesc = {};

    // Render state
    ResourceView m_depthStencilView = {};
    // Per-frame resources for double buffering
    std::array<FrameContext, g_frameCount> m_frameContexts;

    // Scene data (meshes, materials, render items)
    MaterialRepository m_materialRepo;
    std::unordered_map<std::string, Mesh> m_meshes;
    std::vector<RenderItem> m_renderItems;

    // Camera and transform state
    float m_rotationAngle = 0.0f;

    // ImGui support
    MultiObjectImGui m_imguiLoader;

    // Camera
    Camera m_camera;

    // Window handle
    HWND m_windowHandle = nullptr;
};
