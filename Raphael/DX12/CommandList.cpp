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
        // Create command allocator
        if (FAILED(m_device->getNativeDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocator))))
        {
            throw std::runtime_error("Failed to create command allocator");
        }

        // Create command list
        if (FAILED(m_device->getNativeDevice()->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocator.Get(),
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

    void CommandList::begin()
    {
        if (m_isRecording)
        {
            throw std::runtime_error("Command list is already recording");
        }
        // Reset command allocator and command list to start recording
        if (FAILED(m_commandAllocator->Reset()))
        {
            throw std::runtime_error("Failed to reset command allocator");
        }
        if (FAILED(m_commandList->Reset(m_commandAllocator.Get(), nullptr)))
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
} // namespace raphael
