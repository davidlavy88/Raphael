#pragma once
#include "D3D12CommonHeaders.h"
#include "DescriptorHeapDx12.h"
#include "ResourceDx12.h"
#include "PipelineDx12.h"
#include "SwapChainDx12.h"

namespace raphael
{

    class DeviceDx12 : public IDevice
    {
    public:
        DeviceDx12(const DeviceDesc& desc);
        ~DeviceDx12();

        // IDevice interface implementation
        const DeviceDesc& getDesc() const override { return m_desc; }
        std::unique_ptr<ResourceDx12> createResource(const ResourceDesc& desc);
        std::unique_ptr<CommandList> createCommandList(const CommandListDesc& desc);
        std::unique_ptr<PipelineDx12> createPipeline(const PipelineDesc& desc);
        std::unique_ptr<DescriptorHeapDx12> createDescriptorHeap(const DescriptorHeapDesc& desc);
        std::unique_ptr<SwapChainDx12> createSwapChain(DescriptorHeapDx12* rtvHeap, const SwapChainDesc& desc);
        void executeCommandList(CommandList* commandList);
        void waitForGpu(); // TODO: Probably don't need this method
        ComPtr<ID3D12CommandAllocator> createCommandAllocator();
        void signalFence(UINT64 value);
        void waitForFence(UINT64 value);
        UINT64 getNextFenceValue() { return ++m_fenceLastSignaled; }

        // DX12 specific methods
        ID3D12Device* getNativeDevice() const { return m_nativeDevice.Get(); }
        ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue.Get(); }

    private:
        void initializeDevice(const DeviceDesc& desc);
        void createCommandQueue();
        void createFence();

    private:
        DeviceDesc m_desc = {};
        ComPtr<ID3D12Device> m_nativeDevice;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent = nullptr;
        UINT64 m_fenceLastSignaled = 0;

    };
} // namespace raphael
