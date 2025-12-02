#include "Renderer.h"
#include "d3dx12.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

// Renderer class that handles rendering operations
bool Renderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
{
    m_device = &device;
    m_swapChain = &swapChain;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
	// io.ConfigNavMoveSetMousePos = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = device.GetDevice().Get();
    initInfo.CommandQueue = device.GetCommandQueue().Get();
    initInfo.NumFramesInFlight = NUM_FRAMES_IN_FLIGHT;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    initInfo.SrvDescriptorHeap = device.GetSRVHeap().Get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
        {
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Alloc(out_cpu_handle, out_gpu_handle);
        };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
        {
            return static_cast<Renderer*>(ImGui::GetIO().UserData)->m_device->GetSRVAllocator().Free(cpu_handle, gpu_handle);
        };

    // Store this pointer for callbacks
    ImGui::GetIO().UserData = this;

    ImGui_ImplDX12_Init(&initInfo);

    return true;
}

void Renderer::Shutdown()
{
    if (m_device)
        m_device->WaitForGpu();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Renderer::NewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::Render(const ImVec4& clearColor)
{
    ImGui::Render();

    FrameContext* frameContext = m_device->WaitForNextFrame();
    UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    frameContext->CommandAllocator->Reset();

    ComPtr<ID3D12GraphicsCommandList> cmdList = m_device->GetCommandList();
    cmdList->Reset(frameContext->CommandAllocator.Get(), nullptr);

    // Transition to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_swapChain->GetBackBuffer(backBufferIdx);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &barrier);

    // Clear and set render target
    const float clearColorArray[4] = {
        clearColor.x * clearColor.w,
        clearColor.y * clearColor.w,
        clearColor.z * clearColor.w,
        clearColor.w
    };
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(backBufferIdx);
    cmdList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    ComPtr<ID3D12DescriptorHeap> srvHeap = m_device->GetSRVHeap();
    cmdList->SetDescriptorHeaps(1, &srvHeap);

    // Render ImGui
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList.Get());

    // Transition back to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // Execute commands
    ID3D12CommandList* cmdLists[] = { cmdList.Get()};
    m_device->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
    m_device->SignalAndIncrementFence(frameContext);
}

void Renderer::CameraForward()
{
    mPos = mPos + cameraSpeed * mFront;
}

void Renderer::CameraBackward()
{
    mPos = mPos - cameraSpeed * mFront;
}

void Renderer::CameraLeft()
{
    mPos = mPos + XMVector3Normalize(XMVector3Cross(mFront, mUp)) * cameraSpeed;
}

void Renderer::CameraRight()
{
    mPos = mPos - XMVector3Normalize(XMVector3Cross(mFront, mUp)) * cameraSpeed;
}

template<typename T>
static T Clamp(const T& x, const T& low, const T& high)
{
    return x < low ? low : (x > high ? high : x);
}

void Renderer::ImGuiOnMouseDown(ImGuiMouseButton button, float x, float y)
{
    mLastMousePos.x = static_cast<LONG>(x);
    mLastMousePos.y = static_cast<LONG>(y);
}

void Renderer::ImGuiOnMouseMove(ImGuiMouseButton button, float x, float y)
{
    if (button == ImGuiMouseButton_Left)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(4 * cameraSpeed * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(4 * cameraSpeed * static_cast<float>(y - mLastMousePos.y));

        mPitch += dy;
        mYaw += dx;

        // Make sure that when pitch is out of bounds, screen doesn't get flipped
        mPitch = Clamp(mPitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
    }
    else if (button == ImGuiMouseButton_Right)
    {
        mPos += cameraSpeed * (static_cast<float>(y - mLastMousePos.y)) * mUp;
    }

    // Calculate the new front vector
    mFront = XMVectorSet(
        cosf(mPitch) * sinf(mYaw),
        sinf(mPitch),
        cosf(mPitch) * cosf(mYaw),
        0.0f
    );

    mLastMousePos.x = static_cast<LONG>(x);
    mLastMousePos.y = static_cast<LONG>(y);
}
