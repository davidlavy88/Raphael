#pragma once
#include "ObjectDescriptors.h"

namespace raphael
{
    class DeviceDx12;

    struct DescriptorHandle
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    };

    class DescriptorHeapDx12
    {
    public:
        DescriptorHeapDx12(DeviceDx12* device, const DescriptorHeapDesc& desc);
        ~DescriptorHeapDx12();

        void createDescriptorHeap();
        void getDescriptorHandle(UINT index, DescriptorHandle* outHandle) const;
        ID3D12DescriptorHeap* getNativeHeap() const { return m_heap.Get(); }

        void AllocateHeap(DescriptorHandle* outHandle);
        void FreeHeap(const DescriptorHandle& handle);


        const DescriptorHeapDesc& getDesc() const { return m_desc; }
    private:
        DescriptorHeapDesc m_desc = {};
        DeviceDx12* m_device = nullptr;
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_DESCRIPTOR_HEAP_TYPE m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        DescriptorHandle m_descriptorHandle = {};
        UINT m_descriptorSize = 0;
        std::vector<UINT> m_freeIndices; // Track available descriptor indices for allocation
    };
}
