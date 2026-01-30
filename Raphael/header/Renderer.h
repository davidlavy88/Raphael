#pragma once

#include "Device.h"
#include "SwapChain.h"
#include "Camera.h"

class Renderer
{
public:
    virtual bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd);
    void Shutdown();
    void NewFrame();
    virtual void Update(float deltaTime) {}
    virtual void Render(const ImVec4& clearColor);
    virtual void RenderUI() {}

    // Convenience overrides for handling mouse input.
    virtual void ImGuiOnMouseMove(ImGuiMouseButton button, float x, float y);
    virtual void ImGuiOnMouseDown(ImGuiMouseButton button, float x, float y);

    Camera* GetCamera() const { return m_camera; }

protected:
    // TODO: Use smart pointers - These need to be properly deleted in Shutdown
    D3D12Device* m_device = nullptr;
    SwapChain* m_swapChain = nullptr;
    Camera* m_camera = nullptr;

    XMFLOAT4X4 mWorld;
    

    POINT mLastMousePos;
};
