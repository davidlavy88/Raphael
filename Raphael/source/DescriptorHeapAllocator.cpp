#include "DescriptorHeapAllocator.h"

// Descriptor heap allocator class
void DescriptorHeapAllocator::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
    IM_ASSERT(m_heap == nullptr && m_freeIndices.empty());

    m_heap = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    m_heapType = desc.Type;
    m_heapStartCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_heapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_handleIncrement = device->GetDescriptorHandleIncrementSize(m_heapType);

    m_freeIndices.reserve(static_cast<int>(desc.NumDescriptors));
    for (int n = desc.NumDescriptors; n > 0; n--)
        m_freeIndices.push_back(n - 1);
}

void DescriptorHeapAllocator::Shutdown()
{
    m_heap = nullptr;
    m_freeIndices.clear();
}

void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    IM_ASSERT(m_freeIndices.Size > 0);
    int idx = m_freeIndices.back();
    m_freeIndices.pop_back();
    outCpuHandle->ptr = m_heapStartCpu.ptr + (idx * m_handleIncrement);
    outGpuHandle->ptr = m_heapStartGpu.ptr + (idx * m_handleIncrement);
}

void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
    int cpuIdx = static_cast<int>((cpuHandle.ptr - m_heapStartCpu.ptr) / m_handleIncrement);
    int gpuIdx = static_cast<int>((gpuHandle.ptr - m_heapStartGpu.ptr) / m_handleIncrement);
    IM_ASSERT(cpuIdx == gpuIdx);
    m_freeIndices.push_back(cpuIdx);
}