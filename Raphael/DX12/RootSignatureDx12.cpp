#include "RootSignatureDx12.h"
#include "DeviceDx12.h"

namespace raphael
{
    RootSignatureDx12::RootSignatureDx12(DeviceDx12* device, const RootSignatureDesc& desc)
        : m_device(device), m_desc(desc)
    {
        createRootSignature();
    }

    void RootSignatureDx12::createRootSignature()
    {
        // Each table in the RootSignatureDesc corresponds to one or more root parameters in the root signature, 
        // depending on the number of ranges in the table. 
        // We need to keep descriptor ranges alive until serialization is complete,
        // so we store them in a parallel vector of vectors.
        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
        // Store descriptor ranges for each table to ensure they remain valid while creating root parameters
        std::vector<std::vector<CD3DX12_DESCRIPTOR_RANGE>> descriptorRangesPerTable;

        descriptorRangesPerTable.reserve(m_desc.tableLayoutDescs.size());

        // Convert RootSignatureTableLayoutDescs to CD3DX12_ROOT_PARAMETERs and store them in m_rootParameters
        for (const auto& tableLayoutDesc : m_desc.tableLayoutDescs)
        {
            size_t numRanges = tableLayoutDesc.rangeDescs.size();
            size_t rangeIndex = 0;
            rootParameters.reserve(rootParameters.size() + numRanges);
            for (const auto& rangeDesc : tableLayoutDesc.rangeDescs)
            {
                // For CBV root descriptors (single descriptor), use inline root descriptor
                    // which avoids needing a descriptor heap. This matches the existing behavior.
                if (rangeDesc.type == RootSignatureRangeDesc::RangeType::ConstantBufferView && rangeDesc.numParameters == 1)
                {
                    CD3DX12_ROOT_PARAMETER rootParam = {};
                    rootParam.InitAsConstantBufferView(rangeDesc.shaderRegister, rangeDesc.registerSpace, D3D12_SHADER_VISIBILITY_ALL);
                    rootParameters.push_back(rootParam);
                }
                else
                {
                    // Use a descriptor table for multi-descriptor ranges or SRV/UAV/Sampler
                    descriptorRangesPerTable.emplace_back(1);
                    auto& descRange = descriptorRangesPerTable.back();
                    descRange[0].Init(
                        convertRangeType(rangeDesc.type),
                        rangeDesc.numParameters,
                        rangeDesc.shaderRegister,
                        rangeDesc.registerSpace);

                    CD3DX12_ROOT_PARAMETER rootParam = {};
                    rootParam.InitAsDescriptorTable(
                        static_cast<UINT>(descRange.size()),
                        descRange.data(),
                        D3D12_SHADER_VISIBILITY_ALL);
                    rootParameters.push_back(rootParam);
                }
            }
        }

        // Convert static samplers
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> d3dSamplers;
        d3dSamplers.reserve(m_desc.staticSamplers.size());
        for (const auto& sampler : m_desc.staticSamplers)
        {
            d3dSamplers.emplace_back(
                sampler.shaderRegister,
                convertFilter(sampler.filter),
                convertAddressMode(sampler.addressU),
                convertAddressMode(sampler.addressV),
                convertAddressMode(sampler.addressW),
                sampler.mipLODBias,
                sampler.maxAnisotropy);
        }

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
            static_cast<UINT>(rootParameters.size()),
            rootParameters.data(),
            static_cast<UINT>(d3dSamplers.size()),
            d3dSamplers.empty() ? nullptr : d3dSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        ComPtr<ID3DBlob> serializedRootSig;
        ComPtr<ID3DBlob> errorBlob;
        if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob)))
        {
            std::string errorMsg = "Failed to serialize root signature: ";
            if (errorBlob)
            {
                errorMsg += static_cast<const char*>(errorBlob->GetBufferPointer());
            }
            throw std::runtime_error(errorMsg);
        }

        if (FAILED(m_device->getNativeDevice()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
        {
            throw std::runtime_error("Failed to create root signature");
        }
    }

    D3D12_DESCRIPTOR_RANGE_TYPE RootSignatureDx12::convertRangeType(RootSignatureRangeDesc::RangeType type)
    {
        switch (type)
        {
        case RootSignatureRangeDesc::RangeType::ShaderResourceView:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case RootSignatureRangeDesc::RangeType::UnorderedAccessView:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case RootSignatureRangeDesc::RangeType::ConstantBufferView:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case RootSignatureRangeDesc::RangeType::Sampler:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        default:
            throw std::runtime_error("Unsupported range type");
        }
    }

    D3D12_SHADER_VISIBILITY RootSignatureDx12::convertShaderStageVisibility(RootSignatureTableLayoutDesc::ShaderVisibility visibility)
    {
        switch (visibility)
        {
            case RootSignatureTableLayoutDesc::ShaderVisibility::Vertex: 
                return D3D12_SHADER_VISIBILITY_VERTEX;
            case RootSignatureTableLayoutDesc::ShaderVisibility::Pixel:  
                return D3D12_SHADER_VISIBILITY_PIXEL;
            case RootSignatureTableLayoutDesc::ShaderVisibility::All:    
                return D3D12_SHADER_VISIBILITY_ALL;
            default: 
                return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_FILTER RootSignatureDx12::convertFilter(SamplerFilter filter)
    {
        switch (filter)
        {
            case SamplerFilter::Point:       
                return D3D12_FILTER_MIN_MAG_MIP_POINT;
            case SamplerFilter::Linear:      
                return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            case SamplerFilter::Anisotropic: 
                return D3D12_FILTER_ANISOTROPIC;
            default: 
                return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
    }

    D3D12_TEXTURE_ADDRESS_MODE RootSignatureDx12::convertAddressMode(SamplerAddressMode mode)
    {
        switch (mode)
        {
            case SamplerAddressMode::Wrap:   
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            case SamplerAddressMode::Clamp:  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case SamplerAddressMode::Mirror: 
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case SamplerAddressMode::Border: 
                return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            default: 
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }
}