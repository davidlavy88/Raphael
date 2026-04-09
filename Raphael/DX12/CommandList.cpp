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
} // namespace raphael
