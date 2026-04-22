#pragma once
#include "ObjectDescriptors.h"
#include "DescriptorHeapDx12.h"

namespace raphael
{
    class DeviceDx12;

    class RootSignatureTableDx12
    {
        public:
        RootSignatureTableDx12(DeviceDx12* device, DescriptorHeapDx12* heap, const RootSignatureTableDesc& desc);
        ~RootSignatureTableDx12() = default;

        const RootSignatureTableDesc& getDesc() const { return m_desc; }

        UINT getTableIndex() const { return m_desc.tableIndex; }

        D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle() const { return m_gpuHandle; }

    private:
        DeviceDx12* m_device = nullptr;
        DescriptorHeapDx12* m_descriptorHeap = nullptr;
        RootSignatureTableDesc m_desc = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle = {};
    };
}
