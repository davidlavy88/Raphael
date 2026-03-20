#pragma once
#include "ObjectDescriptors.h"
#include "Interfaces.h"

namespace raphael
{
    class DeviceDx12;

    class ResourceDx12 : public IResource
    {
    public:
        ResourceDx12(DeviceDx12* device, const ResourceDesc& desc);
        ~ResourceDx12() = default;

        // IResource interface
        const ResourceDesc& getDesc() const override { return m_desc; }

        // DX12 specific methods
        ID3D12Resource* getNativeResource() const { return m_resource.Get(); }

    private:
        void createBuffer(const ResourceDesc& desc);
        void createTexture2D(const ResourceDesc& desc);

        void initAsCbv(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        void initAsSrv(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        void initAsRtv(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        void initAsDsv(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    private:
        DeviceDx12* m_device = nullptr;
        ResourceDesc m_desc = {};
        ComPtr<ID3D12Resource> m_resource;
    };
} // namespace raphael
