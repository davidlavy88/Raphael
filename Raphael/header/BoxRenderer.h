#pragma once
#include "Renderer.h"
#include "UploadBuffer.h"
#include "D3D12Util.h"
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "Texture.h"
#include "PoissonDiskDistribution.h"
#include "tinygltf/tiny_gltf.h"

// Config constants
static constexpr int MAX_NUM_BOXES = 100;

class BoxRenderer : public Renderer
{
public:
    BoxRenderer() = default;
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd) override;
    void Shutdown();
    void Render(const ImVec4& clearColor) override;
    void Update(float deltaTime);

    // Build functions
    void LoadTextures(D3D12Device& device);
    void BuildRootSignature(D3D12Device& device);
    void BuildDescriptorHeaps(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry(D3D12Device& device);
    void BuildPSO(D3D12Device& device);
    void BuildRenderItems();
    void BuildFrameContexts(D3D12Device& device);
    void BuildMaterials();
    void BuildLights();
    
    // UI control
    void RenderUI();
    
    // Switching modes
    void SwitchRenderMode(bool usePoissonDisk);

    ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<MeshGeometry> m_boxGeo = nullptr;
    std::unique_ptr<tinygltf::Model> m_gltfModel = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_boxMaterial;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_boxTexture;
    std::vector<std::unique_ptr<Light>> m_lights;

    int m_passCbvOffset = 0;

    float m_spawnRate = 50.0f;
    float m_deltaTimeLastSpawn = 0.0f;
    float m_spawnRadius = 10.0f;

    DirectX::XMVECTOR m_minExtent = { -100.0f, -100.0f, -100.0f, 1.0f };
    DirectX::XMVECTOR m_maxExtent = { 100.0f,  100.0f, 100.0f, 1.0f };

    std::unique_ptr<PoissonDiskDistribution> m_poissonDisk;
    
    // Rendering mode toggle
    bool m_usePoissonDisk = false;
    DirectX::XMVECTOR m_singleBoxPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
};
