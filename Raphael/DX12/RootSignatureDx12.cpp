#include "RootSignatureDx12.h"
#include "DeviceDx12.h"

namespace raphael
{
    RootSignatureDx12::RootSignatureDx12(DeviceDx12* device, const RootSignatureDesc& desc)
        : m_device(device), m_desc(desc)
    {
        // Convert RootSignatureTableLayoutDescs to CD3DX12_ROOT_PARAMETERs and store them in m_rootParameters
        for (const auto& tableLayoutDesc : desc.tableLayoutDescs)
        {
            size_t numRanges = tableLayoutDesc.rangeDescs.size();
            size_t rangeIndex = 0;
            m_rootParameters.reserve(m_rootParameters.size() + numRanges);
            for (const auto& rangeDesc : tableLayoutDesc.rangeDescs)
            {
                switch (rangeDesc.type)
                {
                    case RootSignatureRangeDesc::RangeType::ShaderResourceView:
                    {
                        CD3DX12_ROOT_PARAMETER rootParam = {};
                        CD3DX12_DESCRIPTOR_RANGE descriptorRange = {};
                        descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(rangeDesc.numParameters), rangeDesc.shaderRegister);
                        rootParam.InitAsDescriptorTable(1, &descriptorRange, /*TODO*/static_cast<D3D12_SHADER_VISIBILITY>(tableLayoutDesc.visibility));
                        m_rootParameters.push_back(rootParam);
                        break;
                    }
                    case RootSignatureRangeDesc::RangeType::UnorderedAccessView:
                        // descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;
                    case RootSignatureRangeDesc::RangeType::ConstantBufferView:
                    {
                        CD3DX12_ROOT_PARAMETER rootParam = {};
                        rootParam.InitAsConstantBufferView(rangeDesc.shaderRegister);
                        m_rootParameters.push_back(rootParam);
                        break;
                    }
                    case RootSignatureRangeDesc::RangeType::Sampler:
                        // descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                        break;
                    default:
                        throw std::runtime_error("Unknown root signature range type");
                }
            }
        }
    }

    void RootSignatureDx12::createRootSignature()
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
            static_cast<UINT>(m_rootParameters.size()),
            m_rootParameters.data(),
            0, nullptr,
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
}