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
                    
        void addRootParameter(const CD3DX12_ROOT_PARAMETER& rootParam);
        void createRootSignature();

        ID3D12RootSignature* getNativeDevice() const { return m_rootSignature.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        std::vector<CD3DX12_ROOT_PARAMETER> m_rootParameters;
        DeviceDx12* m_device = nullptr;
        RootSignatureDesc m_desc = {};

    };
}
