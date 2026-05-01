#include "ResourceDx12.h"
#include "DeviceDx12.h"
#include "UtilDx12.h"

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

    ResourceDx12::ResourceDx12(DeviceDx12* device, ID3D12Resource* resource)
        : m_device(device)
    {
        // ComPtr will automatically AddRef the resource, so we can just assign it
        m_resource.Attach(resource);
        // Retrieve resource description to fill m_desc
        D3D12_RESOURCE_DESC resDesc = m_resource->GetDesc();
        m_desc.width = static_cast<uint32_t>(resDesc.Width);
        m_desc.height = resDesc.Height;
        // TODO: Assuming only swapchain use for now. Determine resource type based on dimension in the future (switch case)
        m_desc.type = ResourceDesc::ResourceType::Texture2D;
    }

    ResourceDx12::ResourceDx12(DeviceDx12* device, ComPtr<ID3D12Resource> resource)
        : m_device(device), m_resource(std::move(resource))
    {
        // Retrieve resource description to fill m_desc
        D3D12_RESOURCE_DESC resDesc = m_resource->GetDesc();
        m_desc.width = static_cast<uint32_t>(resDesc.Width);
        m_desc.height = resDesc.Height;
        m_desc.type = ResourceDesc::ResourceType::Texture2D; // TODO: Assuming only swapchain use for now. Determine resource type based on dimension in the future (switch case)
    }

    bool ResourceDx12::map(void** data)
    {
        if (FAILED(m_resource->Map(0, nullptr, data)))
        {
            return false;
        }
        return true;
    }

    void ResourceDx12::unmap()
    {
        if (m_resource != nullptr)
        {
            m_resource->Unmap(0, nullptr);
        }
    }

    ResourceView ResourceDx12::getResourceView(ResourceBindFlags viewType, DescriptorHandle descriptorHandle, UINT strideInBytes)
    {
        ResourceView view = {};

        switch (viewType)
        {
        case ResourceBindFlags::ConstantBuffer:
            view.type = ResourceViewType::ConstantBuffer;
            view.cpuHandle = descriptorHandle.cpuHandle;
            view.gpuHandle = descriptorHandle.gpuHandle;
            initAsCbv(descriptorHandle.cpuHandle);
            break;
        case ResourceBindFlags::ShaderResource:
            view.type = ResourceViewType::ShaderResource;
            view.cpuHandle = descriptorHandle.cpuHandle;
            view.gpuHandle = descriptorHandle.gpuHandle;
            initAsSrv(descriptorHandle.cpuHandle);
            break;
        case ResourceBindFlags::RenderTarget:
            view.type = ResourceViewType::RenderTarget;
            view.cpuHandle = descriptorHandle.cpuHandle;
            view.gpuHandle = descriptorHandle.gpuHandle;
            initAsRtv(descriptorHandle.cpuHandle);
            break;
        case ResourceBindFlags::DepthStencil:
            view.type = ResourceViewType::DepthStencil;
            view.cpuHandle = descriptorHandle.cpuHandle;
            view.gpuHandle = descriptorHandle.gpuHandle;
            initAsDsv(descriptorHandle.cpuHandle);
            break;
        case ResourceBindFlags::VertexBuffer:
            view.type = ResourceViewType::VertexBuffer;
            view.bufferLocation = m_resource->GetGPUVirtualAddress();
            view.sizeInBytes = static_cast<UINT>(m_desc.width);
            view.strideInBytes = strideInBytes;
            break;
        case ResourceBindFlags::IndexBuffer:
            view.type = ResourceViewType::IndexBuffer;
            view.bufferLocation = m_resource->GetGPUVirtualAddress();
            view.sizeInBytes = static_cast<UINT>(m_desc.width);
            view.strideInBytes = strideInBytes;
            break;
        default:
            throw std::runtime_error("Unsupported view type");
        }

        view.format = m_desc.format; // Set the format in the view for later use if needed
        return view;
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

        // Translate bind flags to D3D12 resource flags
        // Check for UAV support (usually needed for compute buffers or structured buffers that will be written to by shaders)
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
        if (hasFlag(desc.bindFlags, ResourceBindFlags::UnorderedAccess))
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.width, resourceFlags);
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

        // Translate bind flags to D3D12 resource flags
        D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
        if (hasFlag(desc.bindFlags, ResourceBindFlags::RenderTarget))
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (hasFlag(desc.bindFlags, ResourceBindFlags::DepthStencil))
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (hasFlag(desc.bindFlags, ResourceBindFlags::UnorderedAccess))
            resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            convertFormatToDXGI(desc.format),
            desc.width,
            desc.height,
            1,
            desc.mipLevels, 1, 0,
            resourceFlags
        ); // Default values for other fields like MipLevels, SampleDesc, etc. can be set as needed. For now, no mipmaps

        // For textures that will be used as render targets or depth stencils, we should specify an optimized clear value. 
        // This is optional but can improve performance when clearing these resources.
        D3D12_CLEAR_VALUE* pOptClear = nullptr;
        D3D12_CLEAR_VALUE optClear;
        if (hasFlag(desc.bindFlags, ResourceBindFlags::RenderTarget))
        {
            optClear.Format = convertFormatToDXGI(desc.format);
            optClear.Color[0] = 0.0f; // Clear to black
            optClear.Color[1] = 0.0f;
            optClear.Color[2] = 0.0f;
            optClear.Color[3] = 1.0f; // Full alpha
            pOptClear = &optClear;
        }
        else if (hasFlag(desc.bindFlags, ResourceBindFlags::DepthStencil))
        {
            optClear.Format = convertFormatToDXGI(desc.format);
            optClear.DepthStencil.Depth = 1.0f; // Clear depth to far plane
            optClear.DepthStencil.Stencil = 0; // Clear stencil to 0
            pOptClear = &optClear;
        }

        // Choose initial state based on bind flags. 
        // For simplicity, we will use COMMON for most resources
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        // If the resource will be used as a depth stencil, we can start it in the DEPTH_WRITE state to optimize for depth clears and writes.
        if (hasFlag(desc.bindFlags, ResourceBindFlags::DepthStencil))
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        if (FAILED(m_device->getNativeDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initialState,
            pOptClear,
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
            srvDesc.Texture2D.MipLevels = m_desc.mipLevels; // Assuming no mipmaps for simplicity
            srvDesc.Format = convertFormatToDXGI(m_desc.format);
            m_device->getNativeDevice()->CreateShaderResourceView(m_resource.Get(), &srvDesc, handle);
        }
    }

    void ResourceDx12::initAsRtv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        // TODO: Implement RTV creation based on resource type and format
        if (m_desc.type == ResourceDesc::ResourceType::Texture2D)
        {
            // nullptr as pDesc lets D3D12 infer the view from the resource desc.
            // WARNING: Use an explicit desc if typeless formats or specific mip/array targeting is needed.
            m_device->getNativeDevice()->CreateRenderTargetView(m_resource.Get(), nullptr, handle);
        }
    }

    void ResourceDx12::initAsDsv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        // TODO: Implement DSV creation based on resource type and format. Note that only textures with depth formats can be used as DSVs, so you may want to add validation here.
        // nullptr as pDesc lets D3D12 infer the view from the resource desc. 
        // WARNING: Use an explicit desc if typeless formats or specific mip/array targeting is needed.
        m_device->getNativeDevice()->CreateDepthStencilView(m_resource.Get(), nullptr, handle);

    }
} // namespace raphael
