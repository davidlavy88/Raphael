#pragma once
#include "Renderer.h"
#include "UploadBuffer.h"
#include "D3D12Util.h"
#include "Camera.h"

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

    void BuildDescriptorHeaps(D3D12Device& device);
    void BuildConstantBufferViews(D3D12Device& device);
    void BuildRootSignature(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry(D3D12Device& device);
    void BuildPSO(D3D12Device& device);
    void BuildRenderItems();
    void BuildFrameContexts(D3D12Device& device);

    void SpawnNewBoxes(int count);
    bool PointInExtents(const XMVECTOR& location);
    bool PointIntersectsGrid(const XMVECTOR& location);

    ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    int m_passCbvOffset = 0;

    float _spawnRate = 50.0f;
    float _deltaTimeLastSpawn = 0.0f;
    float _spawnRadius = 10.0f;

    DirectX::XMVECTOR _minExtent = { -100.0f, -100.0f, -100.0f, 1.0f };
    DirectX::XMVECTOR _maxExtent = { 100.0f,  100.0f, 100.0f, 1.0f };

    std::vector<DirectX::XMVECTOR> _cubes;
    size_t _activeIndex;

    float _cellSize;
    float _gridWidth;
    float _gridHeight;
    float _gridDepth;
    size_t _cellsNumX;
    size_t _cellsNumY;
    size_t _cellsNumZ;
    std::vector<std::vector<std::vector<int>>> _grid;
};
