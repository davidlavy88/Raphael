#pragma once

#include "D3D12CommonHeaders.h"
#include "imgui/imgui.h"

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
