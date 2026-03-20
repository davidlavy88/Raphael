#pragma once
#include "ObjectDescriptors.h"
#include "D3D12CommonHeaders.h"

namespace raphael
{
    class DeviceDx12;
    class ResourceDx12;
    class PipelineDx12;
    class DescriptorHeapDx12;
    class RootSignatureDx12;

    class CommandList
    {
    public:
        CommandList(DeviceDx12* device, const CommandListDesc& desc);
        ~CommandList() = default;
        const CommandListDesc& getDesc() const { return m_desc; }

        void begin();
        void end();
        // void copyResource(IResource* dst, IResource* src); // record full resource GPU to GPU copy
        // void copyBufferRegion(IResource* dst, UINT64 dstOffset, IResource* src, UINT64 srcOffset, UINT64 numBytes);

        ID3D12GraphicsCommandList* getNativeCommandList() const { return m_commandList.Get(); }
        ID3D12CommandAllocator* getCommandAllocator() const { return m_commandAllocator.Get(); }

        void setPipeline(PipelineDx12* pipeline);
        void setDescriptorHeaps(DescriptorHeapDx12* heap, uint32_t count);
        void setGraphicsRootSignature(RootSignatureDx12* rootSignature);
        void draw(uint32_t vertexCount, uint32_t instanceCount);

    private:
        CommandListDesc m_desc = {};
        DeviceDx12* m_device = nullptr;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        bool m_isRecording = false; // Track recording state

    };
} // namespace raphael
