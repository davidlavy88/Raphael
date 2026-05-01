#include "ImGuiLoader.h"
#include "DeviceDx12.h"
#include "DescriptorHeapDx12.h"
#include "CommandList.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace raphael
{
    // -------------- ImGui setup and rendering methods --------------
    bool ImGuiLoader::Initialize(HWND hwnd, DeviceDx12* device, DescriptorHeapDx12* srvHeap, int numBackBuffers)
    {
        m_srvHeap = srvHeap;

        // Make process DPI aware and obtain main monitor scale
        ImGui_ImplWin32_EnableDpiAwareness();

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.UserData = this;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);

        ImGui_ImplDX12_InitInfo initInfo = {};
        initInfo.Device = device->getNativeDevice();
        initInfo.CommandQueue = device->getCommandQueue();
        initInfo.NumFramesInFlight = numBackBuffers;
        initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        initInfo.SrvDescriptorHeap = srvHeap->getNativeHeap();
        initInfo.SrvDescriptorAllocFn = SrvDescriptorAllocFn;
        initInfo.SrvDescriptorFreeFn = SrvDescriptorFreeFn;

        ImGui_ImplDX12_Init(&initInfo);

        return true;
    }

    void ImGuiLoader::SrvDescriptorAllocFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        ImGuiLoader* loader = static_cast<ImGuiLoader*>(ImGui::GetIO().UserData);
        DescriptorHandle handle = {};
        loader->m_srvHeap->AllocateHeap(&handle);
        *out_cpu_handle = handle.cpuHandle;
        *out_gpu_handle = handle.gpuHandle;
    }

    void ImGuiLoader::SrvDescriptorFreeFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
        DescriptorHandle handle = {};
        handle.cpuHandle = cpu_handle;
        handle.gpuHandle = gpu_handle;
        ImGuiLoader* loader = static_cast<ImGuiLoader*>(ImGui::GetIO().UserData);
        loader->m_srvHeap->FreeHeap(handle);
    }

    void ImGuiLoader::NewFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLoader::Display()
    {
		// User-defined method to populate ImGui UI elements. 
        // This will be called every frame after NewFrame() and before Render().
    }

    void ImGuiLoader::Render(CommandList* cmdList)
    {
        ImGui::Render();
        // Render ImGui
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList->getNativeCommandList());
    }

    void ImGuiLoader::Shutdown()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();
    }

    // -------------- Input handling methods --------------
    bool ImGuiLoader::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    bool ImGuiLoader::IsKeyDown(ImGuiKey key) const
    {
        return ImGui::IsKeyDown(key);
    }

    bool ImGuiLoader::IsKeyPressed(ImGuiKey key, bool repeat) const
    {
        return ImGui::IsKeyPressed(key, repeat);
    }

    bool ImGuiLoader::IsMouseDown(ImGuiMouseButton button) const
    {
        return ImGui::IsMouseDown(button);
    }

    bool ImGuiLoader::IsMouseClicked(ImGuiMouseButton button) const
    {
        return ImGui::IsMouseClicked(button);
    }

    ImVec2 ImGuiLoader::GetMousePos() const
    {
        return ImGui::GetMousePos();
    }

    ImVec2 ImGuiLoader::GetMouseDelta() const
    {
        return ImGui::GetIO().MouseDelta;
    }

    float ImGuiLoader::GetMouseWheel() const
    {
        return ImGui::GetIO().MouseWheel;
    }

    bool ImGuiLoader::WantsCaptureMouse() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiLoader::WantsCaptureKeyboard() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }
}