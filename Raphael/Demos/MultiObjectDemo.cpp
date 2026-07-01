#include "MultiObjectDemo.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "TextureLoader/DDSTextureLoader.h"
#include "TextureLoader/WICTextureLoader12.h"
#include "GPUStructs.h"
#include "Mesh/GltfLoader.h"
#include "Mesh/MeshGenerator.h"


using namespace raphael;

void MultiObjectImGui::Display()
{
    ImGui::Begin("GLTF Demo");
    ImGui::Text("GLTF render");
    ImGui::Checkbox("Wireframe", &wireframe);
    ImGui::End();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Initialization process
// 1. Create application window
// 2. Create device
// 3. Create descriptor heaps (DSV, RTV, CBV/SRV/UAV if needed)
// 4. Create swap chain + depth buffer
// 5. Create command objects (command allocators, command lists)
// 6. Create geometry resources (vertex/index buffers, views)
// 7. Constant buffers (per-frame upload buffers)
// 8. Create root signature (define shader resource bindings)
// 9. Create pipeline state (compile shaders, create PSO)
// 10. Create texture resources (load texture, create SRV)
bool MultiObjectDemo::Initialize(WindowInfo windowInfo)
{
    // Init COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Store the window handle for input processing (sad thing)
    m_windowHandle = windowInfo.hWnd;

    // -- 2. Create device --
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

    m_camera.Initialize(
        XMVectorSet(0.0f, 0.5f, 5.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        0.0f,
        XM_PI,
        0.05f
    );

    float aspect = static_cast<float>(windowInfo.width) / windowInfo.height;
    m_camera.SetProjectionMatrix(0.25f * XM_PI, aspect, 1.0f, 1000.0f);

    // -- 3. Create descriptor heaps --
    CreateDescriptorHeaps();

    // -- Initialize ImGui --
    if (!m_imguiLoader.Initialize(windowInfo.hWnd, m_device.get(), m_srvHeap.get(), g_frameCount))
        return false;

    // -- 4. Create swap chain and depth buffer --
    CreateSwapChainAndDepthBuffer(windowInfo);

    // -- 5. Create command objects --
    CreateCommandObjects();

    // -- 6. Create geometry resources --
    SetupScene();

    // -- 7. Create constant buffers --
    CreateConstantBuffers();

    // -- 8. Create root signature --
    CreateRootSignature();

    // -- 9. Create pipeline state + shaders --
    CreatePipeline();

    // -- 10. Create texture resources --
    CreateTexture();
    CreateDummyTexture();

    return true;
}

// 3. Create descriptor heaps 
// For this simple app, we only need 2 non-shader visible heaps and 1 shader visible heap:
// - RTV  heap: g_frameCount descriptors for the back buffer RTVs (one per frame in the swap chain)
// - DSV heaps: 1 descriptor for the depth buffer DSV
// - CBV/SRV/UAV heap: 1 descriptor for the texture SRV
void MultiObjectDemo::CreateDescriptorHeaps()
{
    // Create DSV descriptor heap
    DescriptorHeapDesc dsvHeapDesc = {};
    dsvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::DSV;
    dsvHeapDesc.numDescriptors = 1;
    dsvHeapDesc.shaderVisible = false; // DSV heap does not need to be shader visible

    m_dsvHeap = m_device->createDescriptorHeap(dsvHeapDesc);
    m_dsvHeap->createDescriptorHeap();

    // Create RTV descriptor heap
    DescriptorHeapDesc rtvHeapDesc = {};
    rtvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::RTV;
    rtvHeapDesc.numDescriptors = g_frameCount; // One RTV for each back buffer
    rtvHeapDesc.shaderVisible = false; // RTV heap does not need to be shader visible

    m_rtvHeap = m_device->createDescriptorHeap(rtvHeapDesc);
    m_rtvHeap->createDescriptorHeap();

    DescriptorHeapDesc textureSrvHeapDesc = {};
    textureSrvHeapDesc.type = DescriptorHeapDesc::DescriptorHeapType::CBV_SRV_UAV;
    // One SRV for each texture in the model + 1 for ImGui font texture + 1 for dummy white texture
    textureSrvHeapDesc.numDescriptors = 64; 
    textureSrvHeapDesc.shaderVisible = true; // This heap needs to be shader visible since we'll bind the texture SRV to the pipeline

    m_srvHeap = m_device->createDescriptorHeap(textureSrvHeapDesc);
    m_srvHeap->createDescriptorHeap();
}

// 4. Create swap chain and depth buffer
void MultiObjectDemo::CreateSwapChainAndDepthBuffer(WindowInfo windowInfo)
{
    // We couple swap chain and depth buffer creation together since they both depend 
    // on the window size and need to be recreated together when the window is resized.

    // Create swap chain
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.width = windowInfo.width;
    swapChainDesc.height = windowInfo.height;
    swapChainDesc.bufferCount = g_frameCount;
    swapChainDesc.windowHandle = windowInfo.hWnd;

    m_swapChain = m_device->createSwapChain(m_rtvHeap.get(), swapChainDesc);

    // Create depth buffer
    ResourceDesc depthDesc = {};
    depthDesc.type = ResourceDesc::ResourceType::Texture2D;
    depthDesc.width = windowInfo.width;
    depthDesc.height = windowInfo.height;
    depthDesc.format = ResourceFormat::D24_UNORM_S8_UINT;
    depthDesc.bindFlags = ResourceBindFlags::DepthStencil;

    m_depthBuffer = m_device->createResource(depthDesc);

    // Create the DSV for the depth buffer
    DescriptorHandle dsvHandle = {};
    m_dsvHeap->AllocateHeap(&dsvHandle);
    m_depthStencilView = m_depthBuffer->getResourceView(ResourceBindFlags::DepthStencil, dsvHandle);
}

// 5. Create command allocators and command list
// Each frame in the swap chain gets its own command allocator, 
// which we will reset at the beginning of each frame when we record commands for that frame. 
// We only need one command list since we will execute it and wait for it to finish 
// before recording commands for the next frame.
void MultiObjectDemo::CreateCommandObjects()
{
    // Create per frame command allocators
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameContexts[i].commandAllocator = m_device->createCommandAllocator();
        m_frameContexts[i].fenceValue = 0;
    }

    // Create command list (use the first frame's command allocator for now, we will reset it each frame before recording commands)
    CommandListDesc cmdListDesc = {};
    m_commandList = m_device->createCommandList(cmdListDesc);
    m_commandList->createCommandList(m_frameContexts[0].commandAllocator.Get());
}

// 6. Create geometry resources (vertex/index buffers, views)
void MultiObjectDemo::SetupScene()
{
    // --- Materials (shared across all objects) ---
    Material soraMaterial0 = { .name = "soraMaterial0",
        .albedoTexturePath = "Models/sora/textures/material_0_diffuse.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("soraMaterial0", soraMaterial0);
    Material soraMaterial1 = { .name = "soraMaterial1",
        .albedoTexturePath = "Models/sora/textures/material_1_diffuse.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("soraMaterial1", soraMaterial1);
    Material soraMaterial2 = { .name = "soraMaterial2",
        .albedoTexturePath = "Models/sora/textures/material_2_diffuse.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("soraMaterial2", soraMaterial2);
    Material soraMaterial3 = { .name = "soraMaterial3",
        .albedoTexturePath = "Models/sora/textures/material_3_diffuse.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("soraMaterial3", soraMaterial3);
    Material soraMaterial4 = { .name = "soraMaterial4",
        .albedoTexturePath = "Models/sora/textures/material_4_diffuse.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("soraMaterial4", soraMaterial4);

    Material portalGunMaterial = { .name = "portalGunMaterial0",
        .albedoTexturePath = "Models/portal_gun/textures/PORTAL_GUN_baseColor.png",
        .roughnessFactor = 1.0f };
    m_materialRepo.AddMaterial("portalGunMaterial", portalGunMaterial);

    Material shinyMetal = { .name = "shinyMetal",
        .baseColorFactor = { 0.8f, 0.8f, 0.85f, 1.0f },
        .metallicFactor = 1.0f,
        .roughnessFactor = 0.2f };
    m_materialRepo.AddMaterial("shinyMetal", shinyMetal);

    Material redPlastic = { .name = "redPlastic",
        .baseColorFactor = { 0.9f, 0.1f, 0.1f, 1.0f },
        .roughnessFactor = 0.6f };
    m_materialRepo.AddMaterial("redPlastic", redPlastic);

    Material stoneFloor = { .name = "stoneFloor",
        .albedoTexturePath = "Textures/stone.dds",
        .roughnessFactor = 1.0f,
        .uvTiling = { 8.0f, 8.0f } };
    m_materialRepo.AddMaterial("stoneFloor", stoneFloor);

    // --- Meshes ---
    // glTF model: loaded via GltfLoader, produces submeshes automatically
    GltfLoader loader;
    loader.LoadFromFile("Models/sora/scene.gltf", m_meshes["sora"]);

    // Another glTF model
    GltfLoader loader2;
    loader2.LoadFromFile("Models/portal_gun/scene.gltf", m_meshes["portal_gun"]);

    // Procedural shapes
    m_meshes["sphere"] = MeshGenerator::CreateSphere(0.5f, 20, 20);
    m_meshes["sphere"].GenerateIndices16();
    m_meshes["sphere"].m_drawMeshes["whole"] = {
        .indexCount = static_cast<uint32_t>(m_meshes["sphere"].GetIndices16().size()) };

    m_meshes["cube"] = MeshGenerator::CreateBox(1.0f, 1.0f, 1.0f, 0);
    m_meshes["cube"].GenerateIndices16();
    m_meshes["cube"].m_drawMeshes["whole"] = {
        .indexCount = static_cast<uint32_t>(m_meshes["cube"].GetIndices16().size())};

    m_meshes["floor"] = MeshGenerator::CreateGrid(20.0f, 20.0f, 8, 8);
    m_meshes["floor"].GenerateIndices16();
    m_meshes["floor"].m_drawMeshes["whole"] = {
        .indexCount = static_cast<uint32_t>(m_meshes["floor"].GetIndices16().size()) };

    // --- Render items (what to draw, with what material, where) ---
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    // Sora model submeshes - each primitive gets its own render item
    // The sora model has 5 materials mapped to its primitives
    std::vector<std::string> soraMaterials = {
        "soraMaterial0", "soraMaterial1", "soraMaterial2", "soraMaterial3", "soraMaterial4"
    };
    size_t soraMaterialIdx = 0;
    for (const auto& [submeshName, submesh] : m_meshes["sora"].m_drawMeshes)
    {
        std::string matName = soraMaterials[soraMaterialIdx % soraMaterials.size()];
        m_renderItems.push_back({ "sora", submeshName, matName, identity });
        soraMaterialIdx++;
    }

    // Portal gun placed to the right - all submeshes share the same material
    XMFLOAT4X4 portalGunWorld;
    XMStoreFloat4x4(&portalGunWorld, XMMatrixTranslation(6.5f, 2.0f, 0.0f) * XMMatrixScaling(0.25f, 0.25f, 0.25f));
    for (const auto& [submeshName, submesh] : m_meshes["portal_gun"].m_drawMeshes)
    {
        m_renderItems.push_back({ "portal_gun", submeshName, "portalGunMaterial", portalGunWorld });
    }

    // Decorative sphere on the left
    XMFLOAT4X4 sphereWorld;
    XMStoreFloat4x4(&sphereWorld, XMMatrixTranslation(-3.0f, 0.5f, 0.0f));
    m_renderItems.push_back({ "sphere", "whole", "shinyMetal", sphereWorld });

    // Red cube behind
    XMFLOAT4X4 cubeWorld;
    XMStoreFloat4x4(&cubeWorld, XMMatrixTranslation(-1.2f, 0.5f, 0.0f));
    m_renderItems.push_back({ "cube", "whole", "redPlastic", cubeWorld });

    // Floor
    XMFLOAT4X4 floorWorld;
    XMStoreFloat4x4(&floorWorld, XMMatrixTranslation(0.0f, -0.01f, 0.0f));
    m_renderItems.push_back({ "floor", "whole", "stoneFloor", floorWorld });

    // Combine every mesh's vertices and indices into single buffers, then upload to the GPU
    std::vector<Vertex> totalVertices;
    std::vector<std::uint16_t> totalIndices;

    for (auto& [meshName, mesh] : m_meshes)
    {
        uint32_t baseVertex = static_cast<uint32_t>(totalVertices.size());
        uint32_t baseIndex = static_cast<uint32_t>(totalIndices.size());

        // Update submesh offsets to be relative to the combined buffer
        for (auto& [submeshName, submesh] : mesh.m_drawMeshes)
        {
            submesh.vertexBufferOffset += baseVertex;
            submesh.indexBufferOffset += baseIndex;
        }

        totalVertices.insert(totalVertices.end(), mesh.m_vertices.begin(), mesh.m_vertices.end());
        totalIndices.insert(totalIndices.end(), mesh.m_indices16.begin(), mesh.m_indices16.end());
    }

    const UINT vertexBufferSize = static_cast<UINT>(totalVertices.size() * sizeof(Vertex));
    const UINT indexBufferSize = static_cast<UINT>(totalIndices.size() * sizeof(std::uint16_t));
    m_indexCount = static_cast<UINT>(totalIndices.size());

    // Create default vertex buffer resource
    ResourceDesc vertexBufferDesc = {};
    vertexBufferDesc.type = ResourceDesc::ResourceType::Buffer;
    vertexBufferDesc.usage = ResourceDesc::Usage::Default;
    vertexBufferDesc.width = vertexBufferSize;

    m_vertexBuffer = m_device->createResource(vertexBufferDesc);

    // Create upload vertex buffer resource
    ResourceDesc vertexUploadDesc = vertexBufferDesc;
    vertexUploadDesc.usage = ResourceDesc::Usage::Upload;
    std::unique_ptr<ResourceDx12> vertexUploadBuffer = m_device->createResource(vertexUploadDesc);

    // Copy vertex data to vertex buffer
    void* vertexData = nullptr;
    if (vertexUploadBuffer->map(&vertexData))
    {
        memcpy(vertexData, totalVertices.data(), vertexBufferSize);
        vertexUploadBuffer->unmap();
    }
    else
    {
        throw std::runtime_error("Failed to map vertex buffer resource.\n");
    }

    // Create index buffer resource
    ResourceDesc indexBufferDesc = {};
    indexBufferDesc.type = ResourceDesc::ResourceType::Buffer;
    indexBufferDesc.usage = ResourceDesc::Usage::Default;
    indexBufferDesc.width = indexBufferSize;

    m_indexBuffer = m_device->createResource(indexBufferDesc);

    // Create upload index buffer resource
    ResourceDesc indexUploadDesc = indexBufferDesc;
    indexUploadDesc.usage = ResourceDesc::Usage::Upload;
    std::unique_ptr<ResourceDx12> indexUploadBuffer = m_device->createResource(indexUploadDesc);

    // Copy index data to index buffer
    void* indexData = nullptr;
    if (indexUploadBuffer->map(&indexData))
    {
        memcpy(indexData, totalIndices.data(), indexBufferSize);
        indexUploadBuffer->unmap();
    }
    else
    {
        throw std::runtime_error("Failed to map index buffer resource.\n");
    }

    // Copy data from upload buffers to default buffers using command list
    // (since default buffers are not CPU accessible)
    m_commandList->begin(m_frameContexts[0].commandAllocator.Get());
    m_commandList->copyResource(m_vertexBuffer.get(), vertexUploadBuffer.get(), totalVertices.data(), vertexBufferSize);
    m_commandList->copyResource(m_indexBuffer.get(), indexUploadBuffer.get(), totalIndices.data(), indexBufferSize);
    m_commandList->end();
    m_device->executeCommandList(m_commandList.get());

    // Wait for GPU to finish copying before we release the upload buffers
    UINT64 fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(fenceValue);
    m_device->waitForFence(fenceValue);

    // Create vertex buffer view
    m_vertexBufferView = m_vertexBuffer->getResourceView(
        ResourceBindFlags::VertexBuffer, {}, sizeof(Vertex));

    // Create index buffer view
    m_indexBufferView = m_indexBuffer->getResourceView(
        ResourceBindFlags::IndexBuffer, {}, sizeof(uint16_t));
}

// 7. Create constant buffers (per-frame upload buffers)
// Each frame gets its own constant buffers to avoid GPU/CPU synchronization issues.
// We have three constant buffers: per-object data (world matrix), per-frame data
// (view/projection matrices, eye position), and per-object material parameters.
// Layout matches MaterialAndTexturedShader.hlsl's FrameConstants and BasicObjectConstants structs.
void MultiObjectDemo::CreateConstantBuffers()
{
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameContexts[i].resources = std::make_unique<FrameResources>();
        m_frameContexts[i].resources->frameCB = std::make_unique<UploadBuffer<FrameConstants>>(m_device.get(), 1, true);
        m_frameContexts[i].resources->objectCB = std::make_unique<UploadBuffer<BasicObjectConstants>>(m_device.get(), static_cast<UINT>(m_renderItems.size()), true);
        m_frameContexts[i].resources->materialCB = std::make_unique<UploadBuffer<MaterialConstants>>(m_device.get(), static_cast<UINT>(m_renderItems.size()), true);
    }
}

// 8. Create root signature
// The root signature defines how shader resources are bound to the pipeline.
// Root parameter  0: inline CBV at b0 for per-object constants (world matrix)  
// Root parameter  1: inline CBV at b1 for per-frame constants (view/projection matrices, eye position)
// Root parameter  2: descriptor table with 1 SRV for the texture (t0)
void MultiObjectDemo::CreateRootSignature()
{
    // Create root signature

    // For this simple test, we have an object constant buffer (per-object data like world matrix) 
    // and a frame constant buffer (per-frame data like view/projection matrices).
    RootSignatureRangeDesc objCbv = {};
    objCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    objCbv.numParameters = 1;
    objCbv.shaderRegister = 0;

    RootSignatureRangeDesc frameCbv = {};
    frameCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    frameCbv.numParameters = 1;
    frameCbv.shaderRegister = 1;

    RootSignatureRangeDesc materialCbv = {};
    materialCbv.type = RootSignatureRangeDesc::RangeType::ConstantBufferView;
    materialCbv.numParameters = 1;
    materialCbv.shaderRegister = 2;

    RootSignatureRangeDesc textureSrv = {};
    textureSrv.type = RootSignatureRangeDesc::RangeType::ShaderResourceView;
    textureSrv.numParameters = 1;
    textureSrv.shaderRegister = 0;

    RootSignatureTableLayoutDesc cbvTable = {};
    cbvTable.visibility = RootSignatureTableLayoutDesc::ShaderVisibility::All;
    cbvTable.rangeDescs = { objCbv, frameCbv, materialCbv, textureSrv };

    RootSignatureDesc rootSigDesc = {};
    rootSigDesc.tableLayoutDescs = { cbvTable };

    // Add static sampler for completeness, even though we don't use yet
    rootSigDesc.staticSamplers = {
        StaticSamplerDesc{
            .shaderRegister = 0, .filter = SamplerFilter::Point,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap
        },
        StaticSamplerDesc{
            .shaderRegister = 1, .filter = SamplerFilter::Point,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp
        },
        StaticSamplerDesc{
            .shaderRegister = 2, .filter = SamplerFilter::Linear,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap
        },
        StaticSamplerDesc{
            .shaderRegister = 3, .filter = SamplerFilter::Linear,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp
        },
        StaticSamplerDesc{
            .shaderRegister = 4, .filter = SamplerFilter::Anisotropic,
            .addressU = SamplerAddressMode::Wrap, .addressV = SamplerAddressMode::Wrap, .addressW = SamplerAddressMode::Wrap,
            .mipLODBias = 0.0f, .maxAnisotropy = 8
        },
        StaticSamplerDesc{
            .shaderRegister = 5, .filter = SamplerFilter::Anisotropic,
            .addressU = SamplerAddressMode::Clamp, .addressV = SamplerAddressMode::Clamp, .addressW = SamplerAddressMode::Clamp,
            .mipLODBias = 0.0f, .maxAnisotropy = 8
        }
    };

    m_rootSignature = m_device->createRootSignature(rootSigDesc);
    m_rootSignature->createRootSignature();
}

// 9. Create pipeline state and compile shaders
void MultiObjectDemo::CreatePipeline()
{
    // Compile shader
    m_shaderDesc.shaderFilePath = L"Shaders\\MaterialAndTexturedShader.hlsl";
    m_shaderDesc.shaderName = "CubeTexturedShader";
    m_shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(m_shaderDesc);

    // Create pipeline state
    m_pipelineDesc.rtvFormats = { ResourceFormat::R8G8B8A8_UNORM };
    m_pipelineDesc.inputLayout = InputLayoutDesc::build({
        InputElementDesc::setAsPosition(0, ResourceFormat::R32G32B32_FLOAT, 0, 0),
        InputElementDesc::setAsNormal(0, ResourceFormat::R32G32B32_FLOAT, 0, 12),
        InputElementDesc::setAsTexCoord(0, ResourceFormat::R32G32_FLOAT, 0, 36),
        });

    m_pipeline = m_device->createPipeline(m_pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());
}

// 10. Create texture resources
// Load every texture referenced by the scene's materials, picking the DDS or WIC loader
// based on the file extension. Each loader creates the texture and its upload resource and
// records the copy commands to get the data onto the GPU.
void MultiObjectDemo::CreateTexture()
{   
    // Reset the command list to record texture upload commands
    m_commandList->begin(m_frameContexts[0].commandAllocator.Get());

    // Load all textures referenced by materials in the repository
    std::vector<std::string> texturePaths = m_materialRepo.GetAllTexturePaths();
    for (const std::string& texturePath : texturePaths)
    {
        // Skip if already loaded
        if (m_textures.find(texturePath) != m_textures.end())
            continue;

        auto texture = std::make_unique<Texture>();
        texture->Initialize(m_srvHeap, m_device.get());
        // Check if texture is a DDS file or a WIC-supported format based on file extension
        if (texturePath.substr(texturePath.find_last_of('.')) == ".dds")
            texture->LoadTextureFromDDSFile(texturePath, m_device.get(), m_commandList.get());
        else if (texturePath.substr(texturePath.find_last_of('.')) == ".png" || texturePath.substr(texturePath.find_last_of('.')) == ".jpg")
            texture->LoadTextureFromWICFile(texturePath, m_device.get(), m_commandList.get());
        else
            throw std::runtime_error("Unsupported texture format: " + texturePath);
        m_textures[texturePath] = std::move(texture);
    }

    // Close and execute the command list to perform the texture upload
    m_commandList->end();
    m_device->executeCommandList(m_commandList.get());

    // Wait for the GPU to finish uploading the texture before proceeding
    UINT64 fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(fenceValue);
    m_device->waitForFence(fenceValue);
}

void MultiObjectDemo::CreateDummyTexture()
{
    m_commandList->begin(m_frameContexts[0].commandAllocator.Get());

    auto dummyTexture = std::make_unique<Texture>();
    dummyTexture->Initialize(m_srvHeap, m_device.get());
    // Create a simple 1x1 white texture for wireframe mode
    dummyTexture->CreateDummyTexture(m_device.get(), m_commandList.get());

    m_textures["dummy"] = std::move(dummyTexture);

    m_commandList->end();
    m_device->executeCommandList(m_commandList.get());

    UINT64 fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(fenceValue);
    m_device->waitForFence(fenceValue);
}

void MultiObjectDemo::UpdateConstantBuffers()
{
    // Rotate all objects slowly around the Y axis
    // m_rotationAngle += 0.01f;

    // Object constant (b0) - World matrix
    XMMATRIX rotation = XMMatrixRotationY(m_rotationAngle);

    // Update pass constants
    m_camera.UpdateViewMatrix();

    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();

    // Upload per-object world matrices and material constants
    for (size_t i = 0; i < m_renderItems.size(); i++)
    {
        XMMATRIX itemWorld = XMLoadFloat4x4(&m_renderItems[i].world);
        // Apply rotation to all objects (or selectively)
        XMMATRIX finalWorld = itemWorld * rotation;

        BasicObjectConstants objConstants = {};
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(finalWorld));

        m_frameContexts[backBufferIndex].resources->objectCB->CopyData(static_cast<int>(i), objConstants);

        // Upload material constants
        const Material* material = m_materialRepo.GetMaterial(m_renderItems[i].materialName);
        MaterialConstants matConstants = {};
        if (material)
        {
            matConstants.DiffuseAlbedo = material->baseColorFactor;
            matConstants.Metallic = material->metallicFactor;
            matConstants.Roughness = material->roughnessFactor;
            matConstants.UVTiling = material->uvTiling;
        }
        m_frameContexts[backBufferIndex].resources->materialCB->CopyData(static_cast<int>(i), matConstants);
    }


    // Frame: store the transposed view-projection matrix
    FrameConstants frameConstants = {};
    XMStoreFloat4x4(&frameConstants.ViewProj, XMMatrixTranspose(m_camera.GetViewProjectionMatrix()));

    // Copy data to the current back buffer's constant buffers
    m_frameContexts[backBufferIndex].resources->frameCB->CopyData(0, frameConstants);
}

void MultiObjectDemo::Render()
{
    // Get the current back buffer index from the swap chain
    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
    // Wait for GPU to finish with the resources from the previous frame
    FrameContext& currentFrameContext = m_frameContexts[backBufferIndex];
    m_device->waitForFence(currentFrameContext.fenceValue);

    // Update constant buffers with current frame's data
    UpdateConstantBuffers();

    // Start ImGui frame
    m_imguiLoader.NewFrame();
    m_imguiLoader.Display();
    DrawDemoSwitcher();

    // Process input (after starting ImGui frame so that we can query ImGui input capture state)
    ProcessInput();

    // Record commands
    // Retrieve current back buffer resource and RTV for render pass setup
    ResourceDx12* currentBackBuffer = m_swapChain->getCurrentBackBuffer();
    ResourceView currentRtView = m_swapChain->getCurrentRTView();

    // Build render pass descriptor for current frame
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    RenderPassDesc renderPassDesc = RenderPassDesc::buildAsSingleRenderTarget(
        currentRtView,
        m_depthStencilView,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        clearColor);
    renderPassDesc.debugName = "Textured Box Render Pass";

    // Test command list recording
    m_commandList->begin(currentFrameContext.commandAllocator.Get());
    m_commandList->beginRenderPass(renderPassDesc);

    // Transition back buffer to render target state for rendering
    m_commandList->transitionResource(
        currentBackBuffer,
        ResourceBindFlags::RenderTarget);

    m_commandList->clearAndSetRenderTargets(renderPassDesc);

    {
        // Set descriptor heaps (for the texture shader resource descriptor heaps)
        m_commandList->setDescriptorHeaps(m_srvHeap.get(), 1);

        // Bind root signature and pipeline state
        m_commandList->setGraphicsRootSignature(m_rootSignature.get());
        m_commandList->setPipeline(m_pipeline.get());

        // Bind constant buffers to root parameters (descriptor tables or root descriptors 
        // depending on how we set up the root signature)
        m_commandList->setConstantBufferView(
            0,
            m_frameContexts[backBufferIndex].resources->objectCB->getResource()->GetGPUVirtualAddress());
        m_commandList->setConstantBufferView(
            1,
            m_frameContexts[backBufferIndex].resources->frameCB->getResource()->GetGPUVirtualAddress());
        m_commandList->setConstantBufferView(
            2,
            m_frameContexts[backBufferIndex].resources->materialCB->getResource()->GetGPUVirtualAddress());

        // Bind geometry
        m_commandList->setVertexBuffer(0, m_vertexBufferView);
        m_commandList->setIndexBuffer(m_indexBufferView);

        // Draw all render items
        for (size_t i = 0; i < m_renderItems.size(); i++)
        {
            const auto& item = m_renderItems[i];
            const Mesh& mesh = m_meshes[item.meshName];
            const auto submeshIt = mesh.m_drawMeshes.find(item.submeshName);
            if (submeshIt == mesh.m_drawMeshes.end())
                continue;

            const Submesh& submesh = submeshIt->second;
            const Material* material = m_materialRepo.GetMaterial(item.materialName);

            // Bind per-object constant buffer at the correct offset
            D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = m_frameContexts[backBufferIndex].resources->objectCB->getResource()->GetGPUVirtualAddress()
                + i * m_frameContexts[backBufferIndex].resources->objectCB->getElementByteSize();
            m_commandList->setConstantBufferView(0, objCBAddress);

            // Bind per-object material constant buffer at the correct offset
            D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = m_frameContexts[backBufferIndex].resources->materialCB->getResource()->GetGPUVirtualAddress()
                + i * m_frameContexts[backBufferIndex].resources->materialCB->getElementByteSize();
            m_commandList->setConstantBufferView(2, matCBAddress);

            // Bind texture
            if (m_imguiLoader.wireframe)
            {
                m_commandList->setGraphicsRootDescriptorTable(3, m_textures["dummy"]->GetResourceView().gpuHandle);
            }
            else if (material && !material->albedoTexturePath.empty() && m_textures.count(material->albedoTexturePath))
            {
                m_commandList->setGraphicsRootDescriptorTable(3, m_textures[material->albedoTexturePath]->GetResourceView().gpuHandle);
            }
            else
            {
                m_commandList->setGraphicsRootDescriptorTable(3, m_textures["dummy"]->GetResourceView().gpuHandle);
            }

            m_commandList->drawIndexedInstanced(submesh.indexCount, 1, submesh.indexBufferOffset, submesh.vertexBufferOffset, 0);
        }

        m_imguiLoader.Render(m_commandList.get());

    }

    m_commandList->endRenderPass();

    // Transition back buffer to present state for presentation
    m_commandList->transitionResource(
        currentBackBuffer,
        ResourceBindFlags::Present);

    m_commandList->end();

    // Execute command list
    m_device->executeCommandList(m_commandList.get());
    // Present the frame
    m_swapChain->present(true);

    // Signal and increment the fence value for the current frame
    currentFrameContext.fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(currentFrameContext.fenceValue);
}

void MultiObjectDemo::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    // Shutdown ImGui
    m_imguiLoader.Shutdown();

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down MultiObjectDemo and releasing resources.\n");
}

void MultiObjectDemo::Resize(unsigned int width, unsigned int height)
{
    if (m_device->getNativeDevice() != nullptr)
    {
        // Wait for GPU to finish with resources before resizing
        for (UINT i = 0; i < g_frameCount; i++)
        {
            m_device->waitForFence(m_frameContexts[i].fenceValue);
        }

        // TODO: Move this to a separate method since we will need to call it from other places (e.g., when changing display modes)
        UINT newWidth = width;
        UINT newHeight = height;

        // Update global window size variables (used for viewport/scissor rect setup in command list recording, etc.)
        WINDOW_WIDTH = newWidth;
        WINDOW_HEIGHT = newHeight;

        m_swapChain->resize(newWidth, newHeight);

        // Recreate depth buffer at new size
        ResourceDesc depthDesc = {};
        depthDesc.type = ResourceDesc::ResourceType::Texture2D;
        depthDesc.width = newWidth;
        depthDesc.height = newHeight;
        depthDesc.format = ResourceFormat::D24_UNORM_S8_UINT;
        depthDesc.bindFlags = ResourceBindFlags::DepthStencil;

        m_depthBuffer = m_device->createResource(depthDesc);

        DescriptorHandle dsvHandle = {};
        m_dsvHeap->getDescriptorHandle(0, &dsvHandle);
        m_depthStencilView = m_depthBuffer->getResourceView(ResourceBindFlags::DepthStencil, dsvHandle);
    }
}

void MultiObjectDemo::ProcessInput()
{
    // Gate input processing on whether my window is the foreground window.
    HWND foreground = ::GetForegroundWindow();
    bool windowFocused = (foreground == m_windowHandle);

    if (windowFocused && !m_imguiLoader.WantsCaptureKeyboard())
    {
        // Process camera movement input only if ImGui is not capturing the input
        if (GetAsyncKeyState('W') & 0x8000)
            m_camera.MoveForward();
        if (GetAsyncKeyState('S') & 0x8000)
            m_camera.MoveBackward();
        if (GetAsyncKeyState('A') & 0x8000)
            m_camera.MoveLeft();
        if (GetAsyncKeyState('D') & 0x8000)
            m_camera.MoveRight();
        if (GetAsyncKeyState(VK_SPACE) & 0x8000)
            m_camera.MoveUpDown(1.0f);
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
            m_camera.MoveUpDown(-1.0f);
        if (GetAsyncKeyState('R') & 0x8000)
            m_camera.Reset();
    }

    if (windowFocused && !m_imguiLoader.WantsCaptureMouse())
    {
        // Process camera rotation input only if ImGui is not capturing the input
        static POINT lastMousePos = {};
        POINT currentMousePos;
        GetCursorPos(&currentMousePos);
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        {
            if (currentMousePos.x != lastMousePos.x || currentMousePos.y != lastMousePos.y)
            {
                m_camera.SetYaw(m_camera.GetYaw() + (currentMousePos.x - lastMousePos.x) * 0.005f);
                m_camera.SetPitch(m_camera.GetPitch() + (currentMousePos.y - lastMousePos.y) * 0.005f);
            }
        }
        else if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        {
            if (currentMousePos.y != lastMousePos.y)
            {
                m_camera.MoveUpDown(static_cast<float>(currentMousePos.y - lastMousePos.y));
            }
        }
        lastMousePos = currentMousePos;
    }

    m_camera.UpdateLook();

    RasterizerFillMode newFillMode = m_imguiLoader.wireframe
        ? RasterizerFillMode::Wireframe
        : RasterizerFillMode::Solid;

    if (m_pipelineDesc.rasterizerFillMode != newFillMode)
    {
        m_pipelineDesc.rasterizerFillMode = newFillMode;

        // Wait for ALL frames to finish before destroying the old PSO
        for (UINT i = 0; i < g_frameCount; i++)
        {
            m_device->waitForFence(m_frameContexts[i].fenceValue);
        }

        // Recreate pipeline with new rasterizer state
        m_pipeline = m_device->createPipeline(m_pipelineDesc);
        m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());
    }
}
