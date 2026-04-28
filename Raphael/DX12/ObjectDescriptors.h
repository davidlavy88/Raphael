#pragma once
#include "Constants.h"
#include "D3D12CommonHeaders.h"

namespace raphael
{

    // Device initialization
    struct DeviceDesc {
        const char* applicationName = nullptr;
        bool enableDebugLayer = false;
    };

    struct ResourceDesc {
        enum class ResourceType {
            Buffer,
            Texture2D
        };

        enum class Usage {
            Default,
            Upload
        };

        ResourceType type = ResourceType::Buffer;
        Usage usage = Usage::Default;
        UINT64 width = 0; // For buffers, this is the byte size. For textures, this is the width in pixels.
        UINT height = 0; // Only used for textures, ignored for buffers
        ResourceFormat format = ResourceFormat::Unknown; // TODO: make this generic not tied to DXGI
        ResourceBindFlags bindFlags = ResourceBindFlags::None;
    };

    struct ResourceView {
        ResourceViewType type = ResourceViewType::Unknown;
        ResourceFormat format = ResourceFormat::Unknown;

        // Descriptor handles (used  for RTV/DSV/SRV/UAV)
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};

        // Buffer view data (used for vertex/index buffer views)
        D3D12_GPU_VIRTUAL_ADDRESS bufferLocation = 0;
        UINT sizeInBytes = 0;
        UINT strideInBytes = 0; // VBV stride or Index buffer format size (e.g., 2 for R16_UINT, 4 for R32_UINT)

        // Convert to D3D12_VERTEX_BUFFER_VIEW to use in CommandList
        D3D12_VERTEX_BUFFER_VIEW toVertexBufferView() const
        {
            if (type != ResourceViewType::VertexBuffer)
            {
                throw std::runtime_error("ResourceView is not of type VertexBuffer");
            }

            D3D12_VERTEX_BUFFER_VIEW vbv = {};
            vbv.BufferLocation = bufferLocation;
            vbv.SizeInBytes = sizeInBytes;
            vbv.StrideInBytes = strideInBytes;
            return vbv;
        }

        D3D12_INDEX_BUFFER_VIEW toIndexBufferView() const
        {
            if (type != ResourceViewType::IndexBuffer)
            {
                throw std::runtime_error("ResourceView is not of type IndexBuffer");
            }
            D3D12_INDEX_BUFFER_VIEW ibv = {};
            ibv.BufferLocation = bufferLocation;
            ibv.SizeInBytes = sizeInBytes;
            // strideInBytes: 2 = R16_UINT, 4 = R32_UINT
            ibv.Format = (strideInBytes == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT; // Determine index format based on stride
            return ibv;
        }
    };

    struct InputElementDesc {
        InputElementSemantic semanticName = InputElementSemantic::Position;
        UINT semanticIndex = 0;
        ResourceFormat format = ResourceFormat::R32G32B32_FLOAT;
        UINT inputSlot = 0;
        UINT alignedByteOffset = 0;

        // Builders
        static InputElementDesc setAsPosition(UINT semanticIndex, ResourceFormat format, UINT inputSlot, UINT alignedByteOffset)
        {
            return { InputElementSemantic::Position, semanticIndex, format, inputSlot, alignedByteOffset };
        }

        static InputElementDesc setAsColor(UINT semanticIndex, ResourceFormat format, UINT inputSlot, UINT alignedByteOffset)
        {
            return { InputElementSemantic::Color, semanticIndex, format, inputSlot, alignedByteOffset };
        }

        static InputElementDesc setAsNormal(UINT semanticIndex, ResourceFormat format, UINT inputSlot, UINT alignedByteOffset)
        {
            return { InputElementSemantic::Normal, semanticIndex, format, inputSlot, alignedByteOffset };
        }

        static InputElementDesc setAsTexCoord(UINT semanticIndex, ResourceFormat format, UINT inputSlot, UINT alignedByteOffset)
        {
            return { InputElementSemantic::TexCoord, semanticIndex, format, inputSlot, alignedByteOffset };
        }
    };

    struct InputLayoutDesc {
        std::vector<InputElementDesc> elements;

        // Build from a C-style array without specifying the size (deduced from the array)
        template<size_t N>
        static InputLayoutDesc build(const InputElementDesc(&elems)[N])
        {
            InputLayoutDesc desc;
            desc.elements.assign(elems, elems + N);
            return desc;
        }
    };

    struct CommandListDesc {
        const char* debugName = nullptr;
        CommandListType type = CommandListType::Direct;
    };

    struct DescriptorHeapDesc {
        enum class DescriptorHeapType {
            CBV_SRV_UAV, // Combined heap for constant buffer views, shader resource views, and unordered access views
            RTV, // Render target view heap
            DSV, // Depth stencil view heap
            Sampler // Sampler resources heap (used in texturing)
        };

        DescriptorHeapType type = DescriptorHeapType::CBV_SRV_UAV;
        UINT numDescriptors = 0;
        bool shaderVisible = false;
        const char* debugName = nullptr;
    };

    struct PipelineDesc {
        const void* vertexShaderBytecode = nullptr;
        size_t vertexShaderSize = 0;
        const void* pixelShaderBytecode = nullptr;
        size_t pixelShaderSize = 0;
        std::vector<ResourceFormat> rtvFormats; // Array of render target formats
        UINT numRenderTargets = 1;
        ResourceFormat dsvFormat = ResourceFormat::D24_UNORM_S8_UINT; // Depth stencil format
        InputLayoutDesc inputLayout;
        // ... more shader stages, input layout, etc.
    };

    struct ShaderDesc {
        enum class ShaderType {
            Vertex,
            Pixel,
            Compute
        };

        std::string shaderName;
        std::wstring shaderFilePath;
        std::vector<ShaderType> types;
    };

    struct StaticSamplerDesc {
        UINT shaderRegister = 0;
        SamplerFilter filter = SamplerFilter::Point;
        SamplerAddressMode addressU = SamplerAddressMode::Wrap;
        SamplerAddressMode addressV = SamplerAddressMode::Wrap;
        SamplerAddressMode addressW = SamplerAddressMode::Wrap;
        float mipLODBias = 0.0f;
        UINT maxAnisotropy = 0;
        UINT registerSpace = 0;
    };

    struct RootSignatureRangeDesc {
        enum class RangeType {
            ShaderResourceView,
            UnorderedAccessView,
            ConstantBufferView,
            Sampler
        };
        RangeType type = RangeType::ConstantBufferView;
        size_t numParameters = 0; // Number of descriptors in the range 
        UINT shaderRegister = 0;
        UINT registerSpace = 0;
    };

    struct RootSignatureTableLayoutDesc {
        enum class ShaderVisibility {
            Vertex,
            Pixel,
            Compute,
            All
        };

        ShaderVisibility visibility = ShaderVisibility::Vertex;
        std::vector<RootSignatureRangeDesc> rangeDescs;
    };

    struct RootSignatureDesc {
        std::vector<RootSignatureTableLayoutDesc> tableLayoutDescs;
        std::vector<StaticSamplerDesc> staticSamplers;
        const char* debugName = nullptr;
    };

    struct RootSignatureTableDesc {
        const ResourceView* resourceViews = nullptr; // Array of resource views to bind in the table
        UINT viewCount = 0; // Number of resource views in the table
        UINT tableIndex = 0; // Root parameter index for this table
    };

    struct SwapChainDesc {
        UINT width = 0;
        UINT height = 0;
        ResourceFormat format = ResourceFormat::R8G8B8A8_UNORM; // TODO: make this generic not tied to DXGI
        UINT bufferCount = 2;
        HWND windowHandle = nullptr;
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        const char* debugName = nullptr;
    };

    struct RenderPassDesc {
        // Render targets
        ResourceView rtvHandles[8] = {}; // Support up to 8 render targets
        UINT numRenderTargets = 0;
        ID3D12Resource* renderTargetResources[8] = {}; // Corresponding resources for the render targets

        // Depth stencil
        ResourceView dsvHandle = {};
        bool hasDepthStencil = false;

        // Clear values
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float clearDepth = 1.0f;
        UINT8 clearStencil = 0;
        // Other render state (e.g., viewport, scissor rect) can be added here as needed

        // Viewport and scissor rect
        UINT viewportWidth = 0;
        UINT viewportHeight = 0;

        const char* debugName = nullptr;

        // Builder for a single render target with depth stencil
        static RenderPassDesc buildAsSingleRenderTarget(
            ResourceView& rtvHandle,
            ID3D12Resource* rtvResource,
            ResourceView& dsvHandle,
            UINT width, UINT height,
            const float clearColor[4] = nullptr,
            float clearDepth = 1.0f)
        {
            RenderPassDesc desc;
            desc.rtvHandles[0] = rtvHandle;
            desc.renderTargetResources[0] = rtvResource;
            desc.numRenderTargets = 1;
            desc.dsvHandle = dsvHandle;
            desc.hasDepthStencil = true;
            desc.viewportWidth = width;
            desc.viewportHeight = height;
            desc.clearDepth = clearDepth;
            if (clearColor)
            {
                std::copy(clearColor, clearColor + 4, desc.clearColor);
            }

            return desc;
        }

    };
} // namespace raphael
