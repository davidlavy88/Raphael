#pragma once
#include "Renderer.h"
#include "UploadBuffer.h"
#include "d3dUtil.h"
/// ADD HERE /// - Include additional headers for cube rendering
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <stdexcept>
#include "d3dx12.h"
#include <array>
#include <cstdint>
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj;
};

class BoxRenderer : public Renderer
{
public:
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd) override;
    void Shutdown();
    void Render(const ImVec4& clearColor) override;
    void Update(float deltaTime);

    void BuildDescriptorHeaps(D3D12Device& device);
    void BuildConstantBuffers(D3D12Device& device);
    void BuildRootSignature(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry(D3D12Device& device);
    void BuildPSO(D3D12Device& device);

    ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<UploadBuffer<ObjectConstants>> m_objectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
};
