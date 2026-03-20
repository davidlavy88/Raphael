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
        DXGI_FORMAT format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN; // TODO: make this generic not tied to DXGI
        UINT bindFlags = 0; // SRV, RTV, DSV, etc
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
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Array of render target formats
        UINT numRenderTargets = 1;
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // Depth stencil format
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

    struct RootSignatureRangeDesc {
        enum class RangeType {
            ShaderResourceView,
            UnorderedAccessView,
            ConstantBufferView,
            Sampler
        };
        RangeType type = RangeType::ConstantBufferView;
        size_t numParameters = 0;
        // For constants
        //UINT num32BitValues = 0;
        UINT shaderRegister = 0;
        UINT registerSpace = 0;

        /*RootSignatureRangeDesc(RangeType type, size_t numParameters, UINT shaderRegister, UINT registerSpace = 0) :
            type(type), 
            numParameters(numParameters), 
            shaderRegister(shaderRegister), 
            registerSpace(registerSpace) 
        {

        }*/
    };

    struct RootSignatureTableLayoutDesc {
        enum class ShaderStage {
            Vertex,
            Pixel,
            Compute,
            All
        };

        ShaderStage visibility = ShaderStage::Vertex;
        std::vector<RootSignatureRangeDesc> rangeDescs;
        std::vector<CD3DX12_DESCRIPTOR_RANGE> descriptorRanges;
    };

    struct RootSignatureDesc {
        // For simplicity, we will just define a vector of root parameters. In a real implementation, you would want to allow for more customization (e.g., static samplers, flags, etc.)
        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
        std::vector<RootSignatureTableLayoutDesc> tableLayoutDescs;
        const char* debugName = nullptr;
    };
} // namespace raphael
