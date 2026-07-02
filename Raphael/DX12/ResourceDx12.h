#pragma once
#include "ObjectDescriptors.h"
#include "Interfaces.h"
#include "DescriptorHeapDx12.h"

namespace raphael
{
    class DeviceDx12;

    class ResourceDx12 : public IResource
    {
    public:
        ResourceDx12(DeviceDx12* device, const ResourceDesc& desc);
        ResourceDx12(DeviceDx12* device, ID3D12Resource* resource);
        ResourceDx12(DeviceDx12* device, ComPtr<ID3D12Resource> resource);
        ~ResourceDx12() = default;

        // IResource interface
        const ResourceDesc& getDesc() const override { return m_desc; }
        void setDesc(const ResourceDesc& desc) { m_desc = desc; }

        // DX12 specific methods
        ID3D12Resource* getNativeResource() const { return m_resource.Get(); }
        bool map(void** data);
        void unmap();

        ResourceView getResourceView(ResourceBindFlags viewType, DescriptorHandle descriptorHandle = {}, UINT strideInBytes = 0, DXGI_FORMAT rtvFormatOverride = DXGI_FORMAT_UNKNOWN);
        ResourceBindFlags getResourceBindFlags() const { return m_desc.bindFlags; }

        void initAsCbv(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        void initAsSrv(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        void initAsRtv(D3D12_CPU_DESCRIPTOR_HANDLE handle, DXGI_FORMAT formatOverride = DXGI_FORMAT_UNKNOWN);
        void initAsDsv(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    private:
        void createBuffer(const ResourceDesc& desc);
        void createTexture2D(const ResourceDesc& desc);

    private:
        DeviceDx12* m_device = nullptr;
        ResourceDesc m_desc = {};
        ComPtr<ID3D12Resource> m_resource;
    };
} // namespace raphael
