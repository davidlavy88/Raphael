#include "DeviceDx12.h"
#include "ResourceDx12.h"

namespace raphael
{
    DeviceDx12::DeviceDx12(const DeviceDesc& desc)
        : m_desc(desc)
    {
        initializeDevice(desc);
        createCommandQueue();
        createFence();
    }

    DeviceDx12::~DeviceDx12()
    {
        waitForGpu();
        
        // Release fence event
        if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    }

    void DeviceDx12::initializeDevice(const DeviceDesc& desc)
    {
        if (m_desc.enableDebugLayer)
        {
            ID3D12Debug* debug = nullptr;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                debug->EnableDebugLayer();
            }
            debug->Release();
        }

        // Create device
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&m_nativeDevice))))
        {
            throw std::runtime_error("Failed to create D3D12 device");
        }

        // Setup debug interface to break on errors
        if (m_desc.enableDebugLayer)
        {
            ID3D12InfoQueue* infoQueue = nullptr;
            m_nativeDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));
            if (infoQueue)
            {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
                infoQueue->Release();
            }
        }
    }

    void DeviceDx12::createCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 1;
        if (FAILED(m_nativeDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))))
        {
            throw std::runtime_error("Failed to create command queue");
        }
    }

    void DeviceDx12::createFence()
    {
        if (FAILED(m_nativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        {
            throw std::runtime_error("Failed to create fence");
        }

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        if (m_fenceEvent == nullptr)
        {
            throw std::runtime_error("Failed to create fence event");
        }

        m_fenceLastSignaled = 0;
    }

    std::unique_ptr<IResource> DeviceDx12::createResource(const ResourceDesc& desc)
    {
        // *outResource = new ResourceDx12(this, desc);
        return std::make_unique<ResourceDx12>(this, desc);
    }

    std::unique_ptr<CommandList> DeviceDx12::createCommandList(const CommandListDesc& desc)
    {
        //*outCommandList = new CommandList(this, desc);
        return std::make_unique<CommandList>(this, desc);
    }

    void DeviceDx12::executeCommandList(CommandList* commandList)
    {
        ID3D12CommandList* cmdLists[] = { commandList->getNativeCommandList() };
        getCommandQueue()->ExecuteCommandLists(1, cmdLists);
    }

    void DeviceDx12::waitForGpu()
    {
        m_commandQueue->Signal(m_fence.Get(), ++m_fenceLastSignaled);
        if (m_fence->GetCompletedValue() < m_fenceLastSignaled)
        {
            m_fence->SetEventOnCompletion(m_fenceLastSignaled, m_fenceEvent);
            ::WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
} // namespace raphael
