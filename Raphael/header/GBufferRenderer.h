#pragma once
#include "Renderer.h"
#include "UploadBuffer.h"
#include "D3D12Util.h"
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "Texture.h"
#include "tinygltf/tiny_gltf.h"

static constexpr int GBUFFER_NUM_RENDER_TARGETS = 3;

class GBufferRenderer : public Renderer
{
    enum GBufferRenderTarget
    {
        GBUFFER_RENDER_TARGET_ALBEDO = 0,
        GBUFFER_RENDER_TARGET_NORMAL = 1,
        GBUFFER_RENDER_TARGET_DEPTH = 2
    };

public:
    GBufferRenderer() = default;
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd) override;
    void Shutdown();
    void Render(const ImVec4& clearColor) override;
    void Update(float deltaTime); // Same as BoxRenderer 
    
    // Build functions
    void BuildRootSignature(D3D12Device& device);
    void BuildDescriptorHeaps(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildGeometry(D3D12Device& device); // Same as BoxRenderer
    void BuildPSO(D3D12Device& device);
    void BuildRenderItems();
    void BuildFrameContexts(D3D12Device& device);
    void BuildMaterials(); // Same as BoxRenderer, but with different textures and materials
    void BuildLights(); // Same as BoxRenderer

    // Load model
    void LoadTextures(D3D12Device& device); // Same as BoxRenderer

private:
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers(); // Same as BoxRenderer

    // Create the G-buffer render resources and their RTV/SRV descriptor heaps
    void CreateGBufferResources(D3D12Device& device, UINT width, UINT height);

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<MeshGeometry> m_modelGeo = nullptr;
    std::unique_ptr<tinygltf::Model> m_gltfModel = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_modelMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_modelTextures;
    std::vector<std::unique_ptr<Light>> m_lights;

    // Create a resource for each texture (Albedo, Normal and depth)
    ComPtr<ID3D12Resource> m_gbufferRenderTargets[GBUFFER_NUM_RENDER_TARGETS];
    // RTV descriptor heap for the G-buffer render targets
    ComPtr<ID3D12DescriptorHeap> m_gbufferRTVHeap;
    // SRV CPU/GPU descriptor handles for lighting pass can sample the G-buffer render targets
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_gbufferSRVCPUHandles[GBUFFER_NUM_RENDER_TARGETS];
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_gbufferSRVGPUHandles[GBUFFER_NUM_RENDER_TARGETS];

    DirectX::XMVECTOR m_singleBoxPosition = { 0.0f, 0.0f, 0.0f, 0.0f };

};
