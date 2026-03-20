#include "TestApp.h"


using namespace raphael;

bool TestApp::Initialize()
{
    // Create device
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;

    m_device = CreateDevice(deviceDesc);

    // Create buffer resource
    ResourceDesc bufferDesc = {};
    bufferDesc.type = ResourceDesc::ResourceType::Buffer;
    bufferDesc.usage = ResourceDesc::Usage::Upload;
    bufferDesc.width = 1024;

    m_bufferResource = m_device->createResource(bufferDesc);

    // Create descriptor heap (CBV_SRV_UAV, shader-visible)
    DescriptorHeapDesc heapDesc = {};
    heapDesc.type = DescriptorHeapDesc::DescriptorHeapType::CBV_SRV_UAV;
    heapDesc.numDescriptors = 10;
    heapDesc.shaderVisible = true;

    m_descriptorHeap = m_device->createDescriptorHeap(heapDesc);
    m_descriptorHeap->createDescriptorHeap();

    // Compile shader
    ShaderDesc shaderDesc = {};
    shaderDesc.shaderFilePath = L"Shaders\\testShader.hlsl";
    shaderDesc.shaderName = "TestShader";
    shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(shaderDesc);

    // Create command list
    CommandListDesc cmdListDesc = {};
    m_commandList = m_device->createCommandList(cmdListDesc);

    // Create root signature
    RootSignatureRangeDesc rangeDesc = {};
    rangeDesc.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    rangeDesc.numParameters = 1;
    rangeDesc.shaderRegister = 0;

    RootSignatureTableLayoutDesc tableLayoutDesc = {};
    tableLayoutDesc.rangeDescs.push_back(rangeDesc);
    tableLayoutDesc.visibility = RootSignatureTableLayoutDesc::ShaderStage::All;

    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.tableLayoutDescs.push_back(tableLayoutDesc);

    m_rootSignature = std::make_unique<RootSignatureDx12>(m_device.get(), rootSigDesc);
    m_rootSignature->createRootSignature();

    // Create pipeline state
    PipelineDesc pipelineDesc = {};

    m_pipeline = m_device->createPipeline(pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());

    return true;
}

void TestApp::Run()
{
    // Test command list recording
    m_commandList->begin();
    m_commandList->setDescriptorHeaps(m_descriptorHeap.get(), 1);
    m_commandList->setGraphicsRootSignature(m_rootSignature.get());
    m_commandList->setPipeline(m_pipeline.get());
    m_commandList->end();

    // Execute command list
    m_device->executeCommandList(m_commandList.get());
    m_device->waitForGpu();
}

void TestApp::Shutdown()
{
    // Cleanup resources if needed
    OutputDebugStringA("Shutting down TestApp and releasing resources.\n");
}
