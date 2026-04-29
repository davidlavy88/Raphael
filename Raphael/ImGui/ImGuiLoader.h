#pragma once
#include "D3D12CommonHeaders.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

namespace raphael
{
    class DeviceDx12;
    class DescriptorHeapDx12;
    class CommandList;

    class ImGuiLoader
    {
    public:
        bool Initialize(HWND hwnd, DeviceDx12* device, DescriptorHeapDx12* srvHeap, int numBackBuffers);
        void NewFrame();
        void Display();
        void Render(CommandList* cmdList);
        void Shutdown();

        // Returns true if ImGui handled the message (caller should return early)
        bool HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        static void SrvDescriptorAllocFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);
        static void SrvDescriptorFreeFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

        DescriptorHeapDx12* m_srvHeap = nullptr;
    };
}
