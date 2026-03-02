#pragma once
#include "Interfaces.h"
#include "DeviceDx12.h"

namespace raphael::graphics
{
    class ResourceDx12 : public IResource
    {
    public:
        ResourceDx12(DeviceDx12* device, const ResourceDesc& desc);
        ~ResourceDx12();

        // IResource interface
        const ResourceDesc& getDesc() const override { return m_desc; }

        // DX12 specific methods
        ID3D12Resource* getNativeResource() const { return m_resource.Get(); }

    private:
        void createBuffer(const ResourceDesc& desc);
        void createTexture2D(const ResourceDesc& desc);

    private:
        DeviceDx12* m_device;
        ResourceDesc m_desc;
        ComPtr<ID3D12Resource> m_resource;
    };
} // namespace raphael::graphics
