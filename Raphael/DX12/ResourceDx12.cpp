#include "ResourceDx12.h"
#include "DeviceDx12.h"

namespace raphael
{
    ResourceDx12::ResourceDx12(DeviceDx12* device, const ResourceDesc& desc)
        : m_device(device), m_desc(desc)
    {
        switch (desc.type)
        {
        case ResourceDesc::ResourceType::Buffer:
            createBuffer(desc);
            break;
        case ResourceDesc::ResourceType::Texture2D:
            createTexture2D(desc);
            break;
        default:
            throw std::runtime_error("Unsupported resource type");
        }
    }

    void ResourceDx12::createBuffer(const ResourceDesc& desc)
    {
        CD3DX12_HEAP_PROPERTIES heapProp;
        D3D12_RESOURCE_STATES initialState;

        // Determine heap properties based on usage
        switch (desc.usage)
        {
        case ResourceDesc::Usage::Default:
            heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            initialState = D3D12_RESOURCE_STATE_COMMON; // Default state for buffers
            break;
        case ResourceDesc::Usage::Upload:
            heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            initialState = D3D12_RESOURCE_STATE_GENERIC_READ; // Upload buffers are typically in this state
            break;
        }

        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.width);
        if (FAILED(m_device->getNativeDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&m_resource)
        )))
        {
            throw std::runtime_error("Failed to create buffer resource");
        }
    }

    void ResourceDx12::createTexture2D(const ResourceDesc& desc)
    {
        CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            desc.format,
            desc.width,
            desc.height
        ); // Default values for other fields like MipLevels, SampleDesc, etc. can be set as needed. For now, no mipmaps

        if (FAILED(m_device->getNativeDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_resource)
        )))
        {
            throw std::runtime_error("Failed to create texture resource");
        }
    }

    void ResourceDx12::initAsCbv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_resource->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(m_desc.width); // Width, for buffer, is the size in bytes
        m_device->getNativeDevice()->CreateConstantBufferView(&cbvDesc, handle);
    }

    void ResourceDx12::initAsSrv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        // For simplicity, we will create a basic SRV for a buffer or texture. In a real implementation, you would want to allow for more customization.
        if (m_desc.type == ResourceDesc::ResourceType::Buffer)
        {
            // Not implemented yet. For now, textures only
        }
        else if (m_desc.type == ResourceDesc::ResourceType::Texture2D)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1; // Assuming no mipmaps for simplicity
            srvDesc.Format = m_desc.format;
            m_device->getNativeDevice()->CreateShaderResourceView(m_resource.Get(), &srvDesc, handle);
        }
    }

    void ResourceDx12::initAsRtv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        // TODO: Implement RTV creation based on resource type and format
    }

    void ResourceDx12::initAsDsv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        // TODO: Implement DSV creation based on resource type and format. Note that only textures with depth formats can be used as DSVs, so you may want to add validation here.
    }
} // namespace raphael
