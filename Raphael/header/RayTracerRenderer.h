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
};

struct SceneConstants
{
    XMFLOAT4X4 InvViewProj;
	XMFLOAT4 CameraPos;
	XMFLOAT4 Sphere; // x, y, z: center, w: radius
	XMFLOAT4 Plane;  // x, y, z: normal, w: d
	XMFLOAT4 LightPos; // x, y, z: position, w: unused
	XMFLOAT4 SphereColor;
	XMFLOAT4 PlaneColor;
};

class RayTracerRenderer : public Renderer
{
public:
    bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd) override;
    void Shutdown();
    void Render(const ImVec4& clearColor) override;
    void Update();

    virtual void ImGuiOnMouseMove(ImGuiMouseButton button, float x, float y)override;
    virtual void ImGuiOnMouseDown(ImGuiMouseButton button, float x, float y)override;

    void BuildDescriptorHeaps(D3D12Device& device);
    void BuildConstantBuffers(D3D12Device& device);
    void BuildRootSignature(D3D12Device& device);
    void BuildShadersAndInputLayout();
    void BuildFullscreenGeometry(D3D12Device& device);
    void BuildPSO(D3D12Device& device);

    ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }

private:
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12PipelineState> m_pso = nullptr;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    std::unique_ptr<UploadBuffer<SceneConstants>> m_sceneCB = nullptr;

    std::unique_ptr<MeshGeometry> m_fullscreenGeo = nullptr;

    XMFLOAT4X4 mWorld;
    XMFLOAT4X4 mView;
    XMFLOAT4X4 mProj;

    POINT mLastMousePos;
};
