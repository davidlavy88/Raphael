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

        void createCommandList(ID3D12CommandAllocator* allocator);
        void begin(ID3D12CommandAllocator* allocator);
        void end();
        void reset();
        // void copyResource(IResource* dst, IResource* src); // record full resource GPU to GPU copy
        // void copyBufferRegion(IResource* dst, UINT64 dstOffset, IResource* src, UINT64 srcOffset, UINT64 numBytes);

        ID3D12GraphicsCommandList* getNativeCommandList() const { return m_commandList.Get(); }
        // ID3D12CommandAllocator* getCommandAllocator() const { return m_commandAllocator.Get(); }

        void setPipeline(PipelineDx12* pipeline);
        void setDescriptorHeaps(DescriptorHeapDx12* heap, uint32_t count);
        void setGraphicsRootSignature(RootSignatureDx12* rootSignature);

        void setViewports(const D3D12_VIEWPORT* viewports, uint32_t numViewports);
        void setScissorRects(const D3D12_RECT* rects, uint32_t numRects);
        void resourceBarrier(const D3D12_RESOURCE_BARRIER* barriers, uint32_t numBarriers);
        void clearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const FLOAT clearColor[4]);
        void clearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, float depth, UINT8 stencil);
        void setRenderTargets(uint32_t numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
        void setVertexBuffer(UINT slot, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferView);
        void drawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation);


    private:
        CommandListDesc m_desc = {};
        DeviceDx12* m_device = nullptr;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        // ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        bool m_isRecording = false; // Track recording state

    };
} // namespace raphael
