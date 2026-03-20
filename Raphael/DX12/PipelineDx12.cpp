#include "PipelineDx12.h"
#include "DeviceDx12.h"
#include "InputLayoutDx12.h"

namespace raphael
{
    PipelineDx12::PipelineDx12(DeviceDx12* device, const PipelineDesc& desc)
        : m_device(device), m_desc(desc)
    {
        // TODO: For now we hardcode the input layout, but this should be extended to support multiple layouts
        InputLayoutDx12 inputLayout;
        m_inputLayout = inputLayout.getInputLayout("DefaultLayout");
    }

    void PipelineDx12::createPipelineState(ShaderDx12* shader, RootSignatureDx12* rootSignature)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
        psoDesc.pRootSignature = rootSignature->getNativeDevice();
        psoDesc.VS = { reinterpret_cast<BYTE*>(
            shader->m_shaderObjs.at(ShaderDesc::ShaderType::Vertex)->GetBufferPointer()),
            shader->m_shaderObjs.at(ShaderDesc::ShaderType::Vertex)->GetBufferSize() };
        psoDesc.PS = { reinterpret_cast<BYTE*>(
            shader->m_shaderObjs.at(ShaderDesc::ShaderType::Pixel)->GetBufferPointer()),
            shader->m_shaderObjs.at(ShaderDesc::ShaderType::Pixel)->GetBufferSize() };
        CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
        psoDesc.RasterizerState = rsDesc;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = m_desc.numRenderTargets;
        psoDesc.RTVFormats[0] = m_desc.rtvFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = m_desc.dsvFormat;

        if (FAILED(m_device->getNativeDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
            throw std::runtime_error("Failed to create pipeline state object");
    }
}
