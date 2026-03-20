#pragma once
#include "DeviceDx12.h"
#include "ResourceDx12.h"
#include "CommandList.h"
#include "ShaderDx12.h"
#include "RootSignatureDx12.h"
#include "DescriptorHeapDx12.h"
#include "PipelineDx12.h"

using namespace raphael;

class TestApp
{
public:
    bool Initialize();
    void Shutdown();
    void Run();

private:
    std::unique_ptr<DeviceDx12> m_device;
    std::unique_ptr<ResourceDx12> m_bufferResource;
    std::unique_ptr<DescriptorHeapDx12> m_descriptorHeap;
    std::unique_ptr<ShaderDx12> m_shader;
    std::unique_ptr<CommandList> m_commandList;
    std::unique_ptr<RootSignatureDx12> m_rootSignature;
    std::unique_ptr<PipelineDx12> m_pipeline;
};
