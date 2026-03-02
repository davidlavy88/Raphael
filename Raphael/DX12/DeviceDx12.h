#pragma once
#include "Interfaces.h"
#include "../header/D3D12CommonHeaders.h"

namespace raphael::graphics
{
    class DeviceDx12 : public IDevice
    {
    public:
        DeviceDx12(const DeviceDesc& desc);
        ~DeviceDx12();

        // IDevice interface implementation
        const DeviceDesc& getDesc() const override { return m_desc; }
        std::unique_ptr<IResource> createResource(const ResourceDesc& desc) override;
        std::unique_ptr<CommandList> createCommandList(const CommandListDesc& desc) override;
        void executeCommandList(CommandList* commandList) override;
        void waitForGpu() override;

        // DX12 specific methods
        ID3D12Device* getNativeDevice() const { return m_nativeDevice.Get(); }
        ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue.Get(); }

    private:
        void initializeDevice(const DeviceDesc& desc);
        void createCommandQueue();
        void createFence();

    private:
        DeviceDesc m_desc;
        ComPtr<ID3D12Device> m_nativeDevice;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<ID3D12Fence> m_fence = nullptr;
        HANDLE m_fenceEvent = nullptr;
        UINT64 m_fenceLastSignaled = 0;

    };

    inline std::unique_ptr<IDevice> CreateDevice(const DeviceDesc& desc)
    {
        return std::make_unique<DeviceDx12>(desc);
    }

} // namespace raphael::graphics
