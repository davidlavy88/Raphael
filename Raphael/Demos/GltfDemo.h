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
#include "UploadBufferDx12.h"
#include "GPUStructs.h"

#include "tinygltf/tiny_gltf.h"

using namespace raphael;

static constexpr uint32_t g_frameCount = 2;

class GltfDemo
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

    // ---- Initialization helpers (one per logical step) ----
    void CreateGltfModel();
    void CreateDescriptorHeaps();
    void CreateSwapChainAndDepthBuffer();
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
    std::unique_ptr<DescriptorHeapDx12> m_textureSrvHeap;
    std::unique_ptr<ResourceDx12> m_depthBuffer;

    // Geometry resources
    std::unique_ptr<ResourceDx12> m_vertexBuffer;
    std::unique_ptr<ResourceDx12> m_indexBuffer;
    ResourceView m_vertexBufferView = {};
    ResourceView m_indexBufferView = {};
    UINT m_indexCount = 0;

    // Texture resources
    struct TextureData {
        std::unique_ptr<ResourceDx12> m_textureDefaultBuffer;
        std::unique_ptr<ResourceDx12> m_textureUploadBuffer;
    };
    std::vector<TextureData> m_textures;

    // Constant buffers (one per frame for double buffering)
    std::array<std::unique_ptr<UploadBuffer<FrameConstants>>, g_frameCount> m_frameCBs;
    std::array<std::unique_ptr<UploadBuffer<BasicObjectConstants>>, g_frameCount> m_objectCBs;

    // Pipeline resources
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;

    // Render state
    ResourceView m_depthStencilView = {};
    std::vector<ResourceView> m_textureSrvs;
    // Per-frame resources for double buffering
    std::array<FrameContext, g_frameCount> m_frameContexts;

    // GLTF model data
    std::unique_ptr<tinygltf::Model> m_gltfModel;
    struct MeshData {
        uint32_t vertexBufferOffset = 0;
        uint32_t indexBufferOffset = 0;
        uint32_t indexCount = 0;
        int textureIndex = -1; // Index of the texture used by this mesh
    };
    std::vector<MeshData> m_meshes;

    // Camera and transform state
    float m_rotationAngle = 0.0f;

    // Window handle
    HWND m_hwnd = nullptr;
};
