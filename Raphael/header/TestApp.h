#pragma once
#include "../DX12/DeviceDx12.h"
#include "../DX12/ResourceDx12.h"
#include "../DX12/CommandList.h"

using namespace raphael;

class TestApp
{
public:
    bool Initialize();
    void Shutdown();
    void Run();

private:
    std::unique_ptr<IDevice> m_device;
    std::unique_ptr<IResource> m_bufferResource;
    std::unique_ptr<CommandList> m_commandList;
};

