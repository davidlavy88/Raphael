#include "GltfDemo.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "TextureLoader/DDSTextureLoader.h"
#include "TextureLoader/WICTextureLoader12.h"
#include "GPUStructs.h"


using namespace raphael;

void GltfImGui::Display()
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
bool GltfDemo::Initialize(WindowInfo windowInfo)
{
    // Init COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Store the window handle for input processing (sad thing)
    m_windowHandle = windowInfo.hWnd;

    // -- 2. Create device --
    DeviceDesc deviceDesc = {};
    deviceDesc.enableDebugLayer = true;
    m_device = std::make_unique<DeviceDx12>(deviceDesc);

    m_gltfModel = std::make_unique<tinygltf::Model>();

    CreateGltfModel();

    // -- 3. Create descriptor heaps --
    CreateDescriptorHeaps();

    // -- 4. Initialize ImGui --
    if (!m_imguiLoader.Initialize(windowInfo.hWnd, m_device.get(), m_textureSrvHeap.get(), g_frameCount))
        return false;

    // -- 4. Create swap chain and depth buffer --
    CreateSwapChainAndDepthBuffer(windowInfo);

    // -- 5. Create command objects --
    CreateCommandObjects();

    // -- 6. Create geometry resources --
    CreateGeometry();

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

void GltfDemo::CreateGltfModel()
{
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Load the glTF model from file
    if (!loader.LoadASCIIFromFile(m_gltfModel.get(), &err, &warn, "Models/sora/scene.gltf"))
    {
        throw std::runtime_error("Failed to load glTF model: " + err);
    }
}

// 3. Create descriptor heaps 
// For this simple app, we only need 2 non-shader visible heaps and 1 shader visible heap:
// - RTV  heap: g_frameCount descriptors for the back buffer RTVs (one per frame in the swap chain)
// - DSV heaps: 1 descriptor for the depth buffer DSV
// - CBV/SRV/UAV heap: 1 descriptor for the texture SRV
void GltfDemo::CreateDescriptorHeaps()
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
	textureSrvHeapDesc.numDescriptors = static_cast<UINT>(m_gltfModel->textures.size() + 2); 
    textureSrvHeapDesc.shaderVisible = true; // This heap needs to be shader visible since we'll bind the texture SRV to the pipeline

    m_textureSrvHeap = m_device->createDescriptorHeap(textureSrvHeapDesc);
    m_textureSrvHeap->createDescriptorHeap();
}

// 4. Create swap chain and depth buffer
void GltfDemo::CreateSwapChainAndDepthBuffer(WindowInfo windowInfo)
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
void GltfDemo::CreateCommandObjects()
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
void GltfDemo::CreateGeometry()
{
    // TODO: Add warning handling

    // Step 3: Extract vertex positions
    std::vector<VertexWithTexCoord> totalVertices;
    std::vector<std::uint16_t> totalIndices;

    UINT vertexOffset = 0;
    UINT indexOffset = 0;

    for (const tinygltf::Mesh& mesh : m_gltfModel->meshes)
    {
        // Process the loaded model data and create vertex/index buffers
        // Step 1: Get meshes
        OutputDebugStringA(("Loading mesh: " + mesh.name + "\n").c_str());

        size_t primitiveIndex = 0;
        size_t primitiveCount = mesh.primitives.size();
        for (const tinygltf::Primitive& primitive : mesh.primitives)
        {
            std::string primitiveName = mesh.name + "_primitive_" + std::to_string(primitiveIndex);
            OutputDebugStringA(("Loading primitive: " + primitiveName + "\n").c_str());

            std::vector<VertexWithTexCoord> vertices;

            // Find the POSITION attribute
            auto positionAttrIt = primitive.attributes.find("POSITION");
            if (positionAttrIt == primitive.attributes.end())
            {
                throw std::runtime_error("Mesh primitive does not contain POSITION attribute");
            }

            int positionAccessorIndex = positionAttrIt->second;
            // This accessor describes how to read the position data (e.g., type, count, etc.)
            const tinygltf::Accessor& positionAccessor = m_gltfModel->accessors[positionAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& positionBufferView = m_gltfModel->bufferViews[positionAccessor.bufferView];
            // This buffer contains the actual binary data for the positions
            const tinygltf::Buffer& positionBuffer = m_gltfModel->buffers[positionBufferView.buffer];
            // Calculate the pointer to the position data
            const float* positionData = reinterpret_cast<const float*>(&positionBuffer.data[positionBufferView.byteOffset + positionAccessor.byteOffset]);

            size_t vertexCount = positionAccessor.count;
            OutputDebugStringA(("Vertex count: " + std::to_string(vertexCount) + "\n").c_str());

            // Step 4: Extract vertex normals (if available)
            auto normalAttrIt = primitive.attributes.find("NORMAL");
            if (normalAttrIt == primitive.attributes.end())
            {
                throw std::runtime_error("Mesh primitive does not contain POSITION attribute");
            }

            int normalAccessorIndex = normalAttrIt->second;
            // This accessor describes how to read the normal data (e.g., type, count, etc.)
            const tinygltf::Accessor& normalAccessor = m_gltfModel->accessors[normalAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& normalBufferView = m_gltfModel->bufferViews[normalAccessor.bufferView];
            // This buffer contains the actual binary data for the normals
            const tinygltf::Buffer& normalBuffer = m_gltfModel->buffers[normalBufferView.buffer];

            // Calculate the pointer to the normal data
            const float* normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]);

            // Step 5: Extract texture coordinates (if available)
            auto texCoordAttrIt = primitive.attributes.find("TEXCOORD_0");
            if (texCoordAttrIt == primitive.attributes.end())
            {
                throw std::runtime_error("Mesh primitive does not contain POSITION attribute");
            }

            int textureAccessorIndex = texCoordAttrIt->second;
            // This accessor describes how to read the texture data (e.g., type, count, etc.)
            const tinygltf::Accessor& textureAccessor = m_gltfModel->accessors[textureAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& textureBufferView = m_gltfModel->bufferViews[textureAccessor.bufferView];
            // This buffer contains the actual binary data for the textures
            const tinygltf::Buffer& textureBuffer = m_gltfModel->buffers[textureBufferView.buffer];

            // Calculate the pointer to the position data
            const float* textureData = reinterpret_cast<const float*>(&textureBuffer.data[textureBufferView.byteOffset + textureAccessor.byteOffset]);

            // Step 6: Create vertex array
            vertices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].Pos = XMFLOAT3(positionData[i * 3], positionData[i * 3 + 1], positionData[i * 3 + 2]);

                // TODO: Set normals and texture coordinates if available
                vertices[i].Normal = XMFLOAT3(normalData[i * 3], normalData[i * 3 + 1], normalData[i * 3 + 2]);
                vertices[i].TexC = XMFLOAT2(textureData[i * 2], textureData[i * 2 + 1]);
            }

            // Step 7: Extract indices
            std::vector<std::uint16_t> indices;

            if (primitive.indices >= 0)
            {
                // This accessor describes how to read the index data (e.g., type, count, etc.)
                const tinygltf::Accessor& indexAccessor = m_gltfModel->accessors[primitive.indices];
                // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                const tinygltf::BufferView& indexBufferView = m_gltfModel->bufferViews[indexAccessor.bufferView];
                // This buffer contains the actual binary data for the indices
                const tinygltf::Buffer& indexBuffer = m_gltfModel->buffers[indexBufferView.buffer];
                size_t indexCount = indexAccessor.count;
                OutputDebugStringA(("Index count: " + std::to_string(indexCount) + "\n").c_str());

                indices.resize(indexCount);

                // Calculate a pointer to the index data 
                // glTF uses a hierarchical data structure:
                // Buffer : Contains all binary data
                // BufferView : Describes a slice of that buffer(like "indices start at byte 1000")
                // Accessor : Describes how to interpret that view(like "skip 20 more bytes, then read as uint16")
                // 
                // Visual example of indexBuffer.data layout:
                // 
                // |------------------| <- Start of buffer
                // |   ...            |
                // |------------------| <- indexBufferView.byteOffset (Buffer view starts here)
                // |   ...            |
                // |------------------| <- indexAccessor.byteOffset (Accessor offset within view)
                // |   ...            |
                // |   Index Data     | <- Actual index data starts here
                // |   ...            |
                // |------------------| <- End of buffer    


                // gltf supports different index types (e.g. unsigned byte, unsigned short, unsigned int). Need to handle them accordingly
                // we use void* to cast to the correct type based on indexAccessor.componentType
                const void* indexDataPtr = &indexBuffer.data[
                    indexBufferView.byteOffset + // How many bytes into the buffer the view starts
                        indexAccessor.byteOffset // Additional offset within the view to reach index data
                ];

                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const std::uint16_t* indexData = reinterpret_cast<const std::uint16_t*>(indexDataPtr);
                    for (size_t i = 0; i < indexCount; ++i)
                    {
                        indices[i] = indexData[i];
                    }
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    const std::uint32_t* indexData = reinterpret_cast<const std::uint32_t*>(indexDataPtr);
                    for (size_t i = 0; i < indexCount; ++i)
                    {
                        indices[i] = static_cast<std::uint16_t>(indexData[i]);
                    }
                }
                else
                {
                    throw std::runtime_error("Unsupported index component type in glTF model");
                }
            }
            else
            {
                throw std::runtime_error("Mesh primitive does not contain indices");
            }

            // Append this primitive's vertices and indices to the total vertex/index arrays
            MeshData meshData = {};
            meshData.vertexBufferOffset = vertexOffset;
            meshData.indexBufferOffset = indexOffset;
            meshData.indexCount = indices.size();

            m_meshes.push_back(meshData);
            totalVertices.insert(totalVertices.end(), vertices.begin(), vertices.end());
            totalIndices.insert(totalIndices.end(), indices.begin(), indices.end());
            vertexOffset += vertexCount;
            indexOffset += indices.size();
            primitiveIndex++;
        }
    }

    // Step 8: Create MeshGeometry object and upload vertex/index data to GPU
    const UINT vertexBufferSize = static_cast<UINT>(totalVertices.size() * sizeof(VertexWithTexCoord));
    const UINT indexBufferSize = static_cast<UINT>(totalIndices.size() * sizeof(std::uint16_t));
    m_indexCount = totalIndices.size();

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
        ResourceBindFlags::VertexBuffer, {}, sizeof(VertexWithTexCoord));

    // Create index buffer view
    m_indexBufferView = m_indexBuffer->getResourceView(
        ResourceBindFlags::IndexBuffer, {}, sizeof(uint16_t));

    OutputDebugStringA("glTF model loaded successfully!\n");
}

// 7. Create constant buffers (per-frame upload buffers)
// Each frame gets its own constant buffers to avoid GPU/CPU synchronization issues.
// We have two constant buffers: one for per-object data (world matrix) 
// and one for per-frame data (view/projection matrices, eye position).
// Layout matches cubeTexturedShader.hlsl's FrameConstants and BasicObjectConstants structs.
void GltfDemo::CreateConstantBuffers()
{
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_frameCBs[i] = std::make_unique<UploadBuffer<FrameConstants>>(m_device.get(), 1, true);
        m_objectCBs[i] = std::make_unique<UploadBuffer<BasicObjectConstants>>(m_device.get(), 1, true);
    }
}

// 8. Create root signature
// The root signature defines how shader resources are bound to the pipeline.
// Root parameter  0: inline CBV at b0 for per-object constants (world matrix)  
// Root parameter  1: inline CBV at b1 for per-frame constants (view/projection matrices, eye position)
// Root parameter  2: descriptor table with 1 SRV for the texture (t0)
void GltfDemo::CreateRootSignature()
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

    RootSignatureRangeDesc textureSrv = {};
    textureSrv.type = RootSignatureRangeDesc::RangeType::ShaderResourceView;
    textureSrv.numParameters = 1;
    textureSrv.shaderRegister = 0;

    RootSignatureTableLayoutDesc cbvTable = {};
    cbvTable.visibility = RootSignatureTableLayoutDesc::ShaderVisibility::All;
    cbvTable.rangeDescs = { objCbv, frameCbv, textureSrv };

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
void GltfDemo::CreatePipeline()
{
    // Compile shader
    m_shaderDesc.shaderFilePath = L"Shaders\\cubeTexturedShader.hlsl";
    m_shaderDesc.shaderName = "CubeTexturedShader";
    m_shaderDesc.types = { ShaderDesc::ShaderType::Vertex, ShaderDesc::ShaderType::Pixel };

    m_shader = std::make_unique<ShaderDx12>(m_shaderDesc);

    // Create pipeline state
    m_pipelineDesc.rtvFormats = { ResourceFormat::R8G8B8A8_UNORM };
    m_pipelineDesc.inputLayout = InputLayoutDesc::build({
        InputElementDesc::setAsPosition(0, ResourceFormat::R32G32B32_FLOAT, 0, 0),
        InputElementDesc::setAsNormal(0, ResourceFormat::R32G32B32_FLOAT, 0, 12),
        InputElementDesc::setAsTexCoord(0, ResourceFormat::R32G32_FLOAT, 0, 24),
        });

    m_pipeline = m_device->createPipeline(m_pipelineDesc);
    m_pipeline->createPipelineState(m_shader.get(), m_rootSignature.get());
}

// 10. Create texture resources
// We will load a texture from a DDS file using the DirectXTK's CreateDDSTextureFromFile12 helper function,
// which creates both the texture resource and an intermediate upload resource, 
// and records the necessary copy commands to upload the texture data to the GPU.
void GltfDemo::CreateTexture()
{   
    // Reset the command list to record texture upload commands
    m_commandList->begin(m_frameContexts[0].commandAllocator.Get());

    for (const tinygltf::Texture& texture : m_gltfModel->textures)
    {
        if (texture.source < 0 || texture.source >= m_gltfModel->images.size())
        {
            throw std::runtime_error("Texture source index out of bounds in gltf model");
        }

        const tinygltf::Image& image = m_gltfModel->images[texture.source];

        // Upload image to GPU as a texture resource
        std::string texturePath = "Models/sora/" + image.uri;
        std::wstring imageUri{ texturePath.begin(), texturePath.end() };

        // WIC loader needs these additional outputs
        std::unique_ptr<uint8_t[]> decodedData;
        D3D12_SUBRESOURCE_DATA subresource = {};

        ComPtr<ID3D12Resource> textureResource;

            // Load the texture from file
            HRESULT hr = DirectX::LoadWICTextureFromFile(
                m_device->getNativeDevice(),
                imageUri.c_str(),
                textureResource.GetAddressOf(),  // Creates the texture resource
                decodedData,                            // Stores decoded pixel data
                subresource);                           // Contains upload info

            if (FAILED(hr))
            {
                throw std::runtime_error("Failed to load texture " + std::string(imageUri.begin(), imageUri.end()));
            }

        // Query the texture resource to calculate how many bytes the staging buffer needs
        const UINT64 textureBufferSize = GetRequiredIntermediateSize(
            textureResource.Get(), 0, 1);

        // Create texture buffer resource
        ResourceDesc textureUploadDesc = {};
        textureUploadDesc.type = ResourceDesc::ResourceType::Buffer;
        textureUploadDesc.usage = ResourceDesc::Usage::Upload;
        textureUploadDesc.width = textureBufferSize;
        
        std::unique_ptr<ResourceDx12> textureUploadBuffer = m_device->createResource(textureUploadDesc);
        auto textureResourceBuffer = std::make_unique<ResourceDx12>(m_device.get(), textureResource);

        m_commandList->copyTextureResource(
            textureResourceBuffer.get(), textureUploadBuffer.get(), &subresource);
        // Wrap the native D3D12 resource in our ResourceDx12 class
        TextureData textureData = { 
            std::make_unique<ResourceDx12>(m_device.get(), textureResource), 
            std::move(textureUploadBuffer) };

        m_textures.push_back(std::move(textureData));

        DescriptorHandle srvHandle = {};
        m_textureSrvHeap->AllocateHeap(&srvHandle);
        m_textureSrvs.push_back(m_textures.back().m_textureDefaultBuffer->getResourceView(ResourceBindFlags::ShaderResource, srvHandle));
    }

    // Close and execute the command list to perform the texture upload
    m_commandList->end();
    m_device->executeCommandList(m_commandList.get());

    // Wait for the GPU to finish uploading the texture before proceeding
    UINT64 fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(fenceValue);
    m_device->waitForFence(fenceValue);
}

void GltfDemo::CreateDummyTexture()
{
    m_commandList->begin(m_frameContexts[0].commandAllocator.Get());

    static const uint32_t whitePixel = 0xFFFFFFFF;
    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = &whitePixel;
    subresource.RowPitch = sizeof(whitePixel);
    subresource.SlicePitch = sizeof(whitePixel);

    ResourceDesc textureDesc = {};
    textureDesc.type = ResourceDesc::ResourceType::Texture2D;
    textureDesc.width = 1;
    textureDesc.height = 1;
    textureDesc.mipLevels = 1;
    textureDesc.format = ResourceFormat::R8G8B8A8_UNORM;
    textureDesc.bindFlags = ResourceBindFlags::ShaderResource;

    auto whiteTextureResource = m_device->createResource(textureDesc);
    ComPtr<ID3D12Resource> nativeResource = whiteTextureResource->getNativeResource();

    const UINT64 textureBufferSize = GetRequiredIntermediateSize(nativeResource.Get(), 0, 1);

    ResourceDesc textureUploadDesc = {};
    textureUploadDesc.type = ResourceDesc::ResourceType::Buffer;
    textureUploadDesc.usage = ResourceDesc::Usage::Upload;
    textureUploadDesc.width = textureBufferSize;

    std::unique_ptr<ResourceDx12> textureUploadBuffer = m_device->createResource(textureUploadDesc);
    auto textureResourceBuffer = std::make_unique<ResourceDx12>(m_device.get(), nativeResource);

    m_commandList->copyTextureResource(textureResourceBuffer.get(), textureUploadBuffer.get(), &subresource);

    m_whiteTexture = {
        std::make_unique<ResourceDx12>(m_device.get(), nativeResource),
        std::move(textureUploadBuffer) };

    DescriptorHandle srvHandle = {};
    m_textureSrvHeap->AllocateHeap(&srvHandle);
    m_whiteTextureSrv = m_whiteTexture.m_textureDefaultBuffer->getResourceView(ResourceBindFlags::ShaderResource, srvHandle);

    m_commandList->end();
    m_device->executeCommandList(m_commandList.get());

    UINT64 fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(fenceValue);
    m_device->waitForFence(fenceValue);
}

void GltfDemo::UpdateConstantBuffers()
{
    // Rotate the cube slowly around Y axis
    m_rotationAngle += 0.01f;

    // Object constant (b0) - World matrix
    XMMATRIX worldMatrix = XMMatrixRotationY(m_rotationAngle);

    // Object: world matrix with rotation
    BasicObjectConstants objConstants = {};
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(worldMatrix));

    // Frame constant (b1) - ViewProj matrix + eye position
    XMVECTOR eyePos = XMVectorSet(0.0f, 0.7f, -2.0f, 1.0f);
    XMVECTOR lookAt = XMVectorSet(0.0f, 0.7f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, lookAt, up);

    float aspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);
    XMMATRIX viewProj = view * proj;

    // Frame: identity viewproj (renders in NDC space directly)
    FrameConstants frameConstants = {};
    XMStoreFloat4x4(&frameConstants.ViewProj, XMMatrixTranspose(viewProj));

    // Copy data to the current back buffer's constant buffers
    UINT backBufferIndex = m_swapChain->getCurrentBackBufferIndex();
    m_objectCBs[backBufferIndex]->CopyData(0, objConstants);
    m_frameCBs[backBufferIndex]->CopyData(0, frameConstants);
}

void GltfDemo::Render()
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
        currentBackBuffer->getNativeResource(),
        m_depthStencilView,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        clearColor);
    renderPassDesc.debugName = "Textured Box Render Pass";

    // Test command list recording
    m_commandList->begin(currentFrameContext.commandAllocator.Get());
    m_commandList->beginRenderPass(renderPassDesc);

    {
        // Set descriptor heaps (for the texture shader resource descriptor heaps)
        m_commandList->setDescriptorHeaps(m_textureSrvHeap.get(), 1);

        // Bind root signature and pipeline state
        m_commandList->setGraphicsRootSignature(m_rootSignature.get());
        m_commandList->setPipeline(m_pipeline.get());

        // Bind constant buffers to root parameters (descriptor tables or root descriptors 
        // depending on how we set up the root signature)
        m_commandList->setConstantBufferView(
            0,
            m_objectCBs[backBufferIndex]->getResource()->GetGPUVirtualAddress());
        m_commandList->setConstantBufferView(
            1,
            m_frameCBs[backBufferIndex]->getResource()->GetGPUVirtualAddress());

        // Bind geometry
        m_commandList->setVertexBuffer(0, m_vertexBufferView);
        m_commandList->setIndexBuffer(m_indexBufferView);

        // TODO: Match each primitive to its corresponding texture/material for multiple meshes
        for (size_t i = 0; i < m_textureSrvs.size(); i++)
        {
            if (m_imguiLoader.wireframe)
            {
                m_commandList->setGraphicsRootDescriptorTable(2, m_whiteTextureSrv.gpuHandle);
            }
            else
            {
                m_commandList->setGraphicsRootDescriptorTable(2, m_textureSrvs[i].gpuHandle);
            }
            m_commandList->drawIndexedInstanced(m_meshes[i].indexCount, 1, m_meshes[i].indexBufferOffset, m_meshes[i].vertexBufferOffset, 0);
        }

        m_imguiLoader.Render(m_commandList.get());

    }

    m_commandList->endRenderPass();

    m_commandList->end();

    // Execute command list
    m_device->executeCommandList(m_commandList.get());
    // Present the frame
    m_swapChain->present(true);

    // Signal and increment the fence value for the current frame
    currentFrameContext.fenceValue = m_device->getNextFenceValue();
    m_device->signalFence(currentFrameContext.fenceValue);
}

void GltfDemo::Shutdown()
{
    // Ensure GPU is finished with all resources before shutting down
    for (UINT i = 0; i < g_frameCount; i++)
    {
        m_device->waitForFence(m_frameContexts[i].fenceValue);
    }

    // Shutdown ImGui
    m_imguiLoader.Shutdown();

    // Cleanup resources if needed
    OutputDebugStringA("Shutting down GltfDemo and releasing resources.\n");
}

void GltfDemo::Resize(unsigned int width, unsigned int height)
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

void GltfDemo::ProcessInput()
{
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
