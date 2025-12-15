#pragma once

#include "Device.h"
#include "SwapChain.h"
    bool CreateFrameContexts(int passCount, int objectCount);
    ID3D12CommandAllocator* m_commandAllocator = nullptr;

    std::vector<std::unique_ptr<FrameContext>> m_frameContexts;
    FrameContext* m_currentFrameContext = nullptr;

class Renderer
{
public:
    virtual bool Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd);
    void Shutdown();
    void NewFrame();
    virtual void Update(float deltaTime) {}
    virtual void Render(const ImVec4& clearColor);

    // Convenience overrides for handling mouse input.
    virtual void ImGuiOnMouseMove(ImGuiMouseButton button, float x, float y);
    virtual void ImGuiOnMouseDown(ImGuiMouseButton button, float x, float y);

    // Overrides for camera movement
    virtual void CameraForward();
    virtual void CameraBackward();
    virtual void CameraLeft();
    virtual void CameraRight();

    void SetCameraSpeed(float speed) { cameraSpeed = speed; }
    float GetCameraSpeed() const { return cameraSpeed; }
    float GetPitch() const { return mPitch; }
    float GetYaw() const { return mYaw; }
    void SetPos(XMVECTOR pos) { mPos = pos; }

//private:
protected:
    D3D12Device* m_device = nullptr;
    SwapChain* m_swapChain = nullptr;

    float mPitch = 0.0f;
    float mYaw = 0.0f;

    XMVECTOR mFront;
    XMVECTOR mPos;
    XMVECTOR mUp;

    float cameraSpeed = 0.05f;

    XMFLOAT4X4 mWorld;
    XMFLOAT4X4 mView;
    XMFLOAT4X4 mProj;

    POINT mLastMousePos;
};
