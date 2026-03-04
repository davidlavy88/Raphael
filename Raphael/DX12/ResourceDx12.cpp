#include "ResourceDx12.h"
#include "d3dx12.h"

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

    ResourceDx12::~ResourceDx12()
    {

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
        CD3DX12_HEAP_PROPERTIES heapProp;
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
} // namespace raphael
