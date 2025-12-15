#pragma once

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <tchar.h>
#include <cassert>
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "ShaderStructs.h"
#include "UploadBuffer.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// Config constants
static constexpr int NUM_FRAMES_IN_FLIGHT = 2;
static constexpr int NUM_BACK_BUFFERS = 2;
static constexpr int SRV_HEAP_SIZE = 64;

// Frame context structure
struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator = nullptr;
    UINT64 FenceValue = 0;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame n eeds their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    FrameContext(ID3D12Device* device, int passCount, int objectCount)
    {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&CommandAllocator))))
        {
            throw std::runtime_error("Failed to map upload buffer");
        }

        PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
        ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	}

    ~FrameContext()
    {
        if (CommandAllocator) {
            CommandAllocator->Release();
            CommandAllocator = nullptr;
        }
    }
};

// Descriptor heap allocator class
class DescriptorHeapAllocator
{
public:
    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* heap);

    void Shutdown();

    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

private:
    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGpu{};
    UINT m_handleIncrement = 0;
    ImVector<int> m_freeIndices;
};

// D3D12 Device wrapper class
class D3D12Device
{
public:
    bool Initialize();

    void Shutdown();

    ID3D12Device* GetDevice() const { return m_device; }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue; }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList; }
    ID3D12CommandAllocator* GetCurrentCommandAllocator() const { return m_commandAllocator; }
    ID3D12DescriptorHeap* GetRTVHeap() const { return m_rtvHeap; }
	ID3D12DescriptorHeap* GetDSVHeap() const { return m_dsvHeap; }
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap; }
    DescriptorHeapAllocator& GetSRVAllocator() { return m_srvAllocator; }
	DXGI_FORMAT GetBackBufferFormat() const { return mBackBufferFormat; }
	DXGI_FORMAT GetDepthStencilFormat() const { return mDepthStencilFormat; }

    FrameContext* WaitForNextFrame();
    FrameContext* GetCurrentFrameContext();
    FrameContext* GetFrameContext(int index) { return m_frameContexts[index].get(); }

    void SignalAndIncrementFence(FrameContext* frameContext);

    void WaitForGpu();

    void IncrementFrameIndex() { m_frameIndex++; }

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    bool m_appPaused = false;          // Is the application paused?

    bool CreateDescriptorHeaps();
    bool CreateFrameContexts(int passCount, int objectCount);

private:
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12CommandAllocator* m_commandAllocator = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_dsvHeap = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    DescriptorHeapAllocator m_srvAllocator;
    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceLastSignaled = 0;

    std::vector<std::unique_ptr<FrameContext>> m_frameContexts;
    FrameContext* m_currentFrameContext = nullptr;
    UINT m_frameIndex = 0;

    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// TODO: To be implemented if needed
    //// Set true to use 4X MSAA (§4.1.8).  The default is false.
    //bool      m4xMsaaState = false;    // 4X MSAA enabled
    //UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA
};

// SwapChain wrapper class
class SwapChain
{
public:
    bool Initialize(HWND hwnd, D3D12Device& device);
    void Shutdown();
    void Resize(UINT width, UINT height, D3D12Device& device);
    HRESULT Present(bool vsync = true);
    UINT GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index]; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const { return m_rtvHandles[index]; }
    bool IsOccluded() const { return m_occluded; }
    HANDLE GetWaitableObject() const { return m_waitableObject; }

private:
    void CreateRenderTargetViews(D3D12Device& device);
    void CleanupRenderTargetViews();
	void CreateDepthStencilView(D3D12Device& device, UINT width = 0, UINT height= 0);
	void CleanupDepthStencilView();

private:
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12Resource* m_backBuffers[NUM_BACK_BUFFERS] = {};
	ID3D12Resource* m_depthStencilBuffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[NUM_BACK_BUFFERS] = {};
    bool m_tearingSupport = false;
    bool m_occluded = false;
    HANDLE m_waitableObject = nullptr;
};

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
