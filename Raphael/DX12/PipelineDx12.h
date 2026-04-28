#pragma once
#include "Interfaces.h"
#include "ShaderDx12.h"
#include "RootSignatureDx12.h"

namespace raphael
{
    class DeviceDx12;

    class PipelineDx12 : public IPipeline
    {
    public:
        PipelineDx12(DeviceDx12* device, const PipelineDesc& desc);
        ~PipelineDx12() = default;

        // IPipeline interface implementation
        const PipelineDesc& getDesc() const override { return m_desc; }

        // DX12 specific methods
        ID3D12PipelineState* getNativePipelineState() const { return m_pipelineState.Get(); }
        ID3D12RootSignature* getRootSignature() const { return m_rootSignature->getNativeDevice(); }

        void createPipelineState(ShaderDx12* shader, RootSignatureDx12* rootSignature);

    private:
        PipelineDesc m_desc = {};
        DeviceDx12* m_device = nullptr;
        ShaderDx12* m_shaderDx12 = nullptr;
        RootSignatureDx12* m_rootSignature = nullptr;
        ComPtr<ID3D12PipelineState> m_pipelineState;
        // TODO: Extend input layout as a class for multiple layouts
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    };
}
