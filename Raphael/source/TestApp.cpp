#include "TestApp.h"


using namespace raphael;

bool TestApp::Initialize()
{
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;

    m_device = CreateDevice(deviceDesc);

    // Create buffer resource
    ResourceDesc bufferDesc = {};
    bufferDesc.type = ResourceDesc::ResourceType::Buffer;
    bufferDesc.usage = ResourceDesc::Usage::Upload;
    bufferDesc.width = 1024;

    m_bufferResource;
    m_bufferResource = m_device->createResource(bufferDesc);

    // Create command list
    CommandListDesc cmdListDesc = {};
    m_commandList;
    m_commandList = m_device->createCommandList(cmdListDesc);

    return true;
}

void TestApp::Shutdown()
{
    // Cleanup resources if needed
}

void TestApp::Run()
{
    // Test command list recording
    m_commandList->begin();
    m_commandList->end();

    // Execute command list
    m_device->executeCommandList(m_commandList.get());
    m_device->waitForGpu();
}