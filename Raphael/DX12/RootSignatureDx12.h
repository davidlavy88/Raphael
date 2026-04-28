#pragma once
#include "ObjectDescriptors.h"

namespace raphael
{
    class DeviceDx12;

    class RootSignatureDx12
    {
    public:
        RootSignatureDx12(DeviceDx12* device, const RootSignatureDesc& desc);
        ~RootSignatureDx12() = default;
                   

        ID3D12RootSignature* getNativeDevice() const { return m_rootSignature.Get(); }

        void createRootSignature();

    private:
        static D3D12_DESCRIPTOR_RANGE_TYPE convertRangeType(RootSignatureRangeDesc::RangeType type);
        static D3D12_SHADER_VISIBILITY convertShaderStageVisibility(RootSignatureTableLayoutDesc::ShaderVisibility visibility);
        static D3D12_FILTER convertFilter(SamplerFilter filter);
        static D3D12_TEXTURE_ADDRESS_MODE convertAddressMode(SamplerAddressMode mode);


    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        DeviceDx12* m_device = nullptr;
        RootSignatureDesc m_desc = {};

    };
}
