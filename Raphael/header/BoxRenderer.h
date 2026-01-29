#pragma once
#include "Renderer.h"
#include "UploadBuffer.h"
#include "D3D12Util.h"
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "PoissonDiskDistribution.h"

// Config constants
static constexpr int MAX_NUM_BOXES = 100;

class BoxRenderer : public Renderer
{
public:
    BoxRenderer();
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd) override;
    void Shutdown();
    void Render(const ImVec4& clearColor) override;
    void Update(float deltaTime);

    void BuildRootSignature(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry(D3D12Device& device);
    void BuildPSO(D3D12Device& device);
    void BuildRenderItems();
    void BuildFrameContexts(D3D12Device& device);
    void BuildMaterials();
    void BuildLights();

    ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<MeshGeometry> m_boxGeo = nullptr;
    std::unique_ptr<Material> m_boxMaterial = nullptr;
    std::vector<std::unique_ptr<Light>> m_lights;

    int m_passCbvOffset = 0;

    float m_spawnRate = 50.0f;
    float m_deltaTimeLastSpawn = 0.0f;
    float m_spawnRadius = 10.0f;

    DirectX::XMVECTOR m_minExtent = { -100.0f, -100.0f, -100.0f, 1.0f };
    DirectX::XMVECTOR m_maxExtent = { 100.0f,  100.0f, 100.0f, 1.0f };

    std::unique_ptr<PoissonDiskDistribution> m_poissonDisk;
};
