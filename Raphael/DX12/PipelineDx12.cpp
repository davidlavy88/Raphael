#include "PipelineDx12.h"
#include "DeviceDx12.h"
#include "InputLayoutDx12.h"
#include "UtilDx12.h"

namespace raphael
{
    PipelineDx12::PipelineDx12(DeviceDx12* device, const PipelineDesc& desc)
        : m_device(device), m_desc(desc)
    {
        // Ensure the number of render targets matches the number of RTV formats specified
		assert(desc.numRenderTargets == desc.rtvFormats.size());
        m_inputLayout = InputLayoutDx12::convertToD3D12InputLayout(desc.inputLayout);
    }

    void PipelineDx12::createPipelineState(ShaderDx12* shader, RootSignatureDx12* rootSignature)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
        psoDesc.pRootSignature = rootSignature->getNativeDevice();
        psoDesc.VS = shader->getVertexShaderBytecode();
        psoDesc.PS = shader->getPixelShaderBytecode();
        CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
        rsDesc.FillMode = convertFillModeToD3D12(m_desc.rasterizerFillMode);
        rsDesc.CullMode = convertCullModeToD3D12(m_desc.rasterizerCullMode);
        psoDesc.RasterizerState = rsDesc;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        CD3DX12_DEPTH_STENCIL_DESC dsDesc(D3D12_DEFAULT);
        if (m_desc.dsvFormat == ResourceFormat::Unknown)
        {
            // If no depth buffer, disable depth testing
            dsDesc.DepthEnable = FALSE;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
        psoDesc.DepthStencilState = dsDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = m_desc.numRenderTargets;
		for (size_t i = 0; i < m_desc.numRenderTargets; ++i)
        {
            psoDesc.RTVFormats[i] = convertFormatToDXGI(m_desc.rtvFormats[i]);
        }
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = convertFormatToDXGI(m_desc.dsvFormat);

        if (FAILED(m_device->getNativeDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
            throw std::runtime_error("Failed to create pipeline state object");
    }
}
