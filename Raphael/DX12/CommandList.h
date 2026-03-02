#pragma once
#include "ObjectDescriptors.h"
#include "../header/D3D12CommonHeaders.h"

namespace raphael::graphics
{
    class DeviceDx12;
    class IResource;

    class CommandList
    {
    public:
        CommandList(DeviceDx12* device, const CommandListDesc& desc);
        const CommandListDesc& getDesc() const { return m_desc; }

        void begin();
        void end();
        // void copyResource(IResource* dst, IResource* src); // record full resource GPU to GPU copy
        // void copyBufferRegion(IResource* dst, UINT64 dstOffset, IResource* src, UINT64 srcOffset, UINT64 numBytes);

        ID3D12GraphicsCommandList* getNativeCommandList() const { return m_commandList.Get(); }
        ID3D12CommandAllocator* getCommandAllocator() const { return m_commandAllocator.Get(); }

    private:
        CommandListDesc m_desc;
        DeviceDx12* m_device;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        bool m_isRecording; // Track recording state

    };
} // namespace raphael::graphics
