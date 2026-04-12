#include "CommandList.h"
#include "DeviceDx12.h"
#include "PipelineDx12.h"
#include "DescriptorHeapDx12.h"
#include "RootSignatureDx12.h"

namespace raphael
{
    CommandList::CommandList(DeviceDx12* device, const CommandListDesc& desc)
        : m_device(device), m_desc(desc), m_isRecording(false)
    {    
    }

    void CommandList::createCommandList(ID3D12CommandAllocator* allocator)
    {
        // Create command list
        if (FAILED(m_device->getNativeDevice()->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator,
            nullptr,
            IID_PPV_ARGS(&m_commandList))))
        {
            throw std::runtime_error("Failed to create command list");
        }

        // Command lists are created in the recording state, close it
        if (FAILED(m_commandList->Close()))
        {
            throw std::runtime_error("Failed to close command list after creation");
        }
    }

    void CommandList::begin(ID3D12CommandAllocator* allocator)
    {
        if (m_isRecording)
        {
            throw std::runtime_error("Command list is already recording");
        }
        // Reset command allocator and command list to start recording
        if (FAILED(allocator->Reset()))
        {
            throw std::runtime_error("Failed to reset command allocator");
        }
        if (FAILED(m_commandList->Reset(allocator, nullptr)))
        {
            throw std::runtime_error("Failed to reset command list");
        }
        m_isRecording = true;
    }

    void CommandList::end()
    {
        if (!m_isRecording)
        {
            throw std::runtime_error("Command list is not recording");
        }

        if (FAILED(m_commandList->Close()))
        {
            throw std::runtime_error("Failed to close command list");
        }

        m_isRecording = false;
    }

    void CommandList::setPipeline(PipelineDx12* pipeline)
    {
        ID3D12PipelineState* pipelineState = pipeline->getNativePipelineState();
        m_commandList->SetPipelineState(pipelineState);
    }

    void CommandList::setDescriptorHeaps(DescriptorHeapDx12* heap, uint32_t count)
    {
        ID3D12DescriptorHeap* descriptorHeap = heap->getNativeHeap();
        m_commandList->SetDescriptorHeaps(count, &descriptorHeap);
    }

    void CommandList::setGraphicsRootSignature(RootSignatureDx12* rootSignature)
    {
        m_commandList->SetGraphicsRootSignature(rootSignature->getNativeDevice());
    }

    void CommandList::setViewports(const D3D12_VIEWPORT* viewports, uint32_t numViewports)
    {
        m_commandList->RSSetViewports(numViewports, viewports);
    }

    void CommandList::setScissorRects(const D3D12_RECT* rects, uint32_t numRects)
    {
        m_commandList->RSSetScissorRects(numRects, rects);
    }

    void CommandList::resourceBarrier(const D3D12_RESOURCE_BARRIER* barriers, uint32_t numBarriers)
    {
        m_commandList->ResourceBarrier(numBarriers, barriers);
    }

    void CommandList::clearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const FLOAT clearColor[4])
    {
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    }

    void CommandList::clearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, float depth, UINT8 stencil)
    {
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
    }

    void CommandList::setRenderTargets(uint32_t numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
    {
        m_commandList->OMSetRenderTargets(numRenderTargets, rtvHandles, true, &dsvHandle);
    }

    void CommandList::setVertexBuffer(UINT slot, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferView)
    {
        m_commandList->IASetVertexBuffers(slot, 1, vertexBufferView);
    }

    void CommandList::drawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
    {
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    }

    void CommandList::beginRenderPass(const RenderPassDesc& renderPassDesc)
    {
        if (m_isInRenderPass)
        {
            throw std::runtime_error("Already in a render pass");
        }

        m_currentRenderPassDesc = renderPassDesc;
        m_isInRenderPass = true;

        // Transition render targets to the appropriate states for rendering: PRESENT -> RENDER_TARGET
        for (UINT i = 0; i < renderPassDesc.numRenderTargets; ++i)
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                renderPassDesc.renderTargetResources[i],
                D3D12_RESOURCE_STATE_PRESENT, 
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            m_commandList->ResourceBarrier(1, &barrier);
        }

        // Set render targets and clear them based on the render pass description
        // Set viewport and scissor rect to cover the entire render target
        D3D12_VIEWPORT viewport = {};
        viewport.Height = static_cast<FLOAT>(renderPassDesc.viewportHeight);
        viewport.Width = static_cast<FLOAT>(renderPassDesc.viewportWidth);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_commandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorRect = {};
        scissorRect.right = renderPassDesc.viewportWidth;
        scissorRect.bottom = renderPassDesc.viewportHeight;
        m_commandList->RSSetScissorRects(1, &scissorRect);

        // Clear render targets and depth stencil if specified
        for (UINT i = 0; i < renderPassDesc.numRenderTargets; ++i)
        {
            m_commandList->ClearRenderTargetView(
                renderPassDesc.rtvHandles[i], 
                renderPassDesc.clearColor, 
                0, 
                nullptr);
        }
        if (renderPassDesc.hasDepthStencil)
        {
            m_commandList->ClearDepthStencilView(
                renderPassDesc.dsvHandle, 
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
                renderPassDesc.clearDepth, 
                renderPassDesc.clearStencil, 
                0, 
                nullptr);
        }

        // Set render targets for the output merger stage
        m_commandList->OMSetRenderTargets(
            renderPassDesc.numRenderTargets, 
            renderPassDesc.rtvHandles, 
            true, 
            renderPassDesc.hasDepthStencil ? &renderPassDesc.dsvHandle : nullptr);
    }

    void CommandList::endRenderPass()
    {
        if (!m_isInRenderPass)
        {
            throw std::runtime_error("Not currently in a render pass");
        }

        // Transition render targets back to PRESENT state
        for (UINT i = 0; i < m_currentRenderPassDesc.numRenderTargets; ++i)
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_currentRenderPassDesc.renderTargetResources[i],
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            );
            m_commandList->ResourceBarrier(1, &barrier);
        }

        m_isInRenderPass = false;
        // Clear to avoid dangling pointers (TODO: Should be fixed)
        m_currentRenderPassDesc = {};
    }
} // namespace raphael
