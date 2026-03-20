#include "DescriptorHeapDx12.h"
#include "DeviceDx12.h"

namespace raphael
{
    DescriptorHeapDx12::DescriptorHeapDx12(DeviceDx12* device, const DescriptorHeapDesc& desc)
        :m_device(device), m_desc(desc)
    {
        switch (m_desc.type)
        {
        case DescriptorHeapDesc::DescriptorHeapType::CBV_SRV_UAV:
            m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        case DescriptorHeapDesc::DescriptorHeapType::RTV:
            m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            break;
        case DescriptorHeapDesc::DescriptorHeapType::DSV:
            m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            break;
        case DescriptorHeapDesc::DescriptorHeapType::Sampler:
            m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            break;
        }

        m_freeIndices.reserve(m_desc.numDescriptors);
        for (UINT i = m_desc.numDescriptors; i > 0; i--)
        {
            m_freeIndices.push_back(i);
        }

    }
    DescriptorHeapDx12::~DescriptorHeapDx12()
    {
        m_freeIndices.clear();
    }

    void DescriptorHeapDx12::createDescriptorHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = m_desc.numDescriptors;
        desc.Type = m_heapType;
        desc.Flags = m_desc.shaderVisible ? 
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(m_device->getNativeDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap))))
        {
            throw std::runtime_error("Failed to create descriptor heap");
        }

        m_descriptorSize = m_device->getNativeDevice()->GetDescriptorHandleIncrementSize(m_heapType);
        m_descriptorHandle.cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_descriptorHandle.gpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
    }

    // TODO: Handle RTV vs DSV since DSV only has one handle vs RTV which can have multiple
    void DescriptorHeapDx12::getDescriptorHandle(UINT index, DescriptorHandle* outHandle) const
    {
        if (index >= m_desc.numDescriptors)
        {
            throw std::out_of_range("Descriptor index out of range");
        }
        outHandle->cpuHandle.ptr = m_descriptorHandle.cpuHandle.ptr + (index * m_descriptorSize);
    }

    void DescriptorHeapDx12::AllocateHeap(DescriptorHandle* outHandle)
    {
        assert(!m_freeIndices.empty() && "No more descriptors available in the heap!");
        UINT index = m_freeIndices.back();
        m_freeIndices.pop_back();
        outHandle->cpuHandle.ptr = m_descriptorHandle.cpuHandle.ptr + (index * m_descriptorSize);
        outHandle->gpuHandle.ptr = m_descriptorHandle.gpuHandle.ptr + (index * m_descriptorSize);
    }

    void DescriptorHeapDx12::FreeHeap(const DescriptorHandle& handle)
    {
        UINT cpuIndex = static_cast<UINT>((handle.cpuHandle.ptr - m_descriptorHandle.cpuHandle.ptr) / m_descriptorSize);
        UINT gpuIndex = static_cast<UINT>((handle.gpuHandle.ptr - m_descriptorHandle.gpuHandle.ptr) / m_descriptorSize);
        assert(cpuIndex == gpuIndex && "CPU and GPU descriptor indices do not match!");
        m_freeIndices.push_back(cpuIndex);
    }
}
