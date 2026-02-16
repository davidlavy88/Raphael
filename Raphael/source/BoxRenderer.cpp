#include "BoxRenderer.h"
#include "GPUStructs.h"
#include "Material.h"
#include "DDSTextureLoader/DDSTextureLoader.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION // optional
#include "tinygltf/tiny_gltf.h"

bool BoxRenderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
{
    if (!Renderer::Initialize(device, swapChain, hwnd))
        return false;

    // Initialize Poisson disk distribution
    m_poissonDisk = std::make_unique<PoissonDiskDistribution>(m_spawnRadius, m_minExtent, m_maxExtent, m_singleBoxPosition);

    // Reset command list for initialization
    device.GetCommandList()->Reset(device.GetCurrentCommandAllocator().Get(), nullptr);

    LoadTextures(device);
    BuildRootSignature(device);
    BuildDescriptorHeaps(device);
    BuildShadersAndInputLayout();
    BuildBoxGeometry(device);
    BuildMaterials();
    BuildLights();
    BuildRenderItems();
    BuildFrameContexts(device);
    BuildPSO(device);

    // Initialize camera position, look and up vectors
    m_camera->SetPosition(XMVectorSet(0.0f, 0.0f, -20.0f, 1.0f));
    m_camera->SetLook(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
    m_camera->SetUp(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    // ADD: Initialize matrices
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    float aspect = static_cast<float>(clientRect.right) / clientRect.bottom;

    // Initialize world matrix (identity)
    XMStoreFloat4x4(&m_world, XMMatrixIdentity());

    // Initialize camera projection matrix
    m_camera->SetProjectionMatrix(0.25f * XM_PI, aspect, 1.0f, 1000.0f);

    // Execute the initialization commands
    device.GetCommandList()->Close();
    ID3D12CommandList* cmdLists[] = { device.GetCommandList().Get()};
    device.GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

    // Wait until initialization is complete
    device.WaitForGpu();

    return true;
}

void BoxRenderer::Shutdown()
{
    m_boxGeo.reset();
    m_pso.Reset();
    m_rootSignature.Reset();
    m_cbvHeap.Reset();
    m_boxTexture.reset();

    Renderer::Shutdown();
}

void BoxRenderer::BuildBoxGeometry(D3D12Device& device)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Load the glTF model from file
    if (!loader.LoadASCIIFromFile(&gltfModel, &err, &warn, "Models/sora/scene.gltf"))
    {
        throw std::runtime_error("Failed to load glTF model: " + err);
    }
    // TODO: Add warning handling

    m_boxGeo = std::make_unique<MeshGeometry>();
    m_boxGeo->Name = "gltfMesh";

    std::vector<VertexShaderInput> totalVertices;
    std::vector<std::uint16_t> totalIndices;
    
    size_t meshIndex = 0;
    UINT vertexOffset = 0;
    UINT indexOffset = 0;

    for (const tinygltf::Mesh& mesh : gltfModel.meshes)
    {
        // Process the loaded model data and create vertex/index buffers
        // Step 1: Get meshes
        OutputDebugStringA(("Loading mesh: " + mesh.name + "\n").c_str());

        size_t primitiveIndex = 0;
    	size_t primitiveCount = mesh.primitives.size();
        // Step 2: Get primitives from the mesh
        for (const tinygltf::Primitive& primitive : mesh.primitives)
        {
            std::string primitiveName = mesh.name + "_primitive_" + std::to_string(primitiveIndex);
            OutputDebugStringA(("Loading primitive: " + primitiveName + "\n").c_str());

            // Step 3: Extract vertex positions
            std::vector<VertexShaderInput> vertices;

            // Find the POSITION attribute
            auto positionAttrIt = primitive.attributes.find("POSITION");
            if (positionAttrIt == primitive.attributes.end())
            {
                throw std::runtime_error("Mesh primitive does not contain POSITION attribute");
            }

            int positionAccessorIndex = positionAttrIt->second;
            // This accessor describes how to read the position data (e.g., type, count, etc.)
            const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
            // This buffer contains the actual binary data for the positions
            const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionBufferView.buffer];

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
            const tinygltf::Accessor& normalAccessor = gltfModel.accessors[normalAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
            // This buffer contains the actual binary data for the normals
            const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalBufferView.buffer];

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
            const tinygltf::Accessor& textureAccessor = gltfModel.accessors[textureAccessorIndex];
            // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
            const tinygltf::BufferView& textureBufferView = gltfModel.bufferViews[textureAccessor.bufferView];
            // This buffer contains the actual binary data for the textures
            const tinygltf::Buffer& textureBuffer = gltfModel.buffers[textureBufferView.buffer];

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
                const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
                // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
                // This buffer contains the actual binary data for the indices
                const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];

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

            // Step 8: Create MeshGeometry object and upload vertex/index data to GPU
            // TODO: This should be refactored later
            SubmeshGeometry submesh;
            submesh.IndexCount = (UINT)indices.size();
            submesh.StartIndexLocation = indexOffset;
            submesh.BaseVertexLocation = vertexOffset ;

            m_boxGeo->DrawArgs[primitiveName] = submesh;

            vertexOffset += vertices.size();
            indexOffset += indices.size();

            totalVertices.insert(totalVertices.end(), vertices.begin(), vertices.end());
            totalIndices.insert(totalIndices.end(), indices.begin(), indices.end());
            primitiveIndex++;
        }
        meshIndex++;
    }

    const UINT vbByteSize = static_cast<UINT>(totalVertices.size() * sizeof(VertexShaderInput));
    const UINT ibByteSize = static_cast<UINT>(totalIndices.size() * sizeof(std::uint16_t));

    D3DCreateBlob(vbByteSize, &m_boxGeo->VertexBufferCPU);
    CopyMemory(m_boxGeo->VertexBufferCPU->GetBufferPointer(), totalVertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &m_boxGeo->IndexBufferCPU);
    CopyMemory(m_boxGeo->IndexBufferCPU->GetBufferPointer(), totalIndices.data(), ibByteSize);

    m_boxGeo->VertexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), totalVertices.data(), vbByteSize, m_boxGeo->VertexBufferUploader);

    m_boxGeo->IndexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), totalIndices.data(), ibByteSize, m_boxGeo->IndexBufferUploader);

    m_boxGeo->VertexByteStride = sizeof(VertexShaderInput);
    m_boxGeo->VertexBufferByteSize = vbByteSize;
    m_boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_boxGeo->IndexBufferByteSize = ibByteSize;

    OutputDebugStringA("glTF model loaded successfully!\n");
}

void BoxRenderer::LoadTextures(D3D12Device& device)
{
    auto woodCrateTexture = std::make_unique<Texture>("woodCrateTex", L"Textures/WoodCrate01.dds");
    if (FAILED(DirectX::CreateDDSTextureFromFile12(device.GetDevice().Get(),
        device.GetCommandList().Get(),
        woodCrateTexture->Filename.c_str(),
        woodCrateTexture->Resource,
        woodCrateTexture->UploadHeap)))
    {
        throw std::runtime_error("Failed to load texture " + std::string(woodCrateTexture->Filename.begin(), woodCrateTexture->Filename.end()));
    }

    m_boxTexture = std::move(woodCrateTexture);
}

void BoxRenderer::BuildRootSignature(D3D12Device& device)
{
    CD3DX12_DESCRIPTOR_RANGE textureTable;
    textureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Implementation for creating root signature 
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Create root parameters to bind the descriptor table & constant buffer views to the pipeline
    // As a performance tip, order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsConstantBufferView(2);
    slotRootParameter[3].InitAsDescriptorTable(1, &textureTable, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf())))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    if (FAILED(device.GetDevice()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
    {
        throw std::runtime_error("Failed to create root signature");
    }
}

void BoxRenderer::BuildDescriptorHeaps(D3D12Device& device)
{
    // Create shader resource view for the texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_boxTexture->Resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = m_boxTexture->Resource->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    // Allocate descriptor from the SRV heap
    device.GetSRVAllocator().Alloc(&m_boxTexture->SrvCpuHandle, &m_boxTexture->SrvGpuHandle);

    // Create the SRV
    device.GetDevice()->CreateShaderResourceView(
        m_boxTexture->Resource.Get(),
        &srvDesc,
        m_boxTexture->SrvCpuHandle);
}

void BoxRenderer::BuildShadersAndInputLayout()
{
    // Implementation for compiling shaders and defining input layout
    m_vsByteCode = D3D12Util::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    m_psByteCode = D3D12Util::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

/*
void BoxRenderer::BuildBoxGeometry(D3D12Device& device)
{
    // How to Visualize UV Mapping for a Cube
    //           [Top]
    //            (2)
    //
    //    [Left][Front][Right][Back]
    //    (4)     (0)      (5)      (1)
    //
    //          [Bottom]
    //            (3)
    

    // Implementation for creating box geometry
    std::array<VertexShaderInput, 24> vertices =
    {
        // Fill in the front face vertex data.
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) }),

        // Fill in the back face vertex data.
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) }),

        // Fill in the bottom face vertex data.
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }),

        // Fill in the top face vertex data.
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }),

        // Fill in the left face vertex data.
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }),

        // Fill in the right face vertex data.
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }),
        VertexShaderInput({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 5, 6,
        4, 6, 7,

        // left face
        8, 9, 10,
        8, 10, 11,

        // right face
        12, 13, 14,
        12, 14, 15,

        // top face
        16, 17, 18,
        16, 18, 19,

        // bottom face
        20, 21, 22,
        20, 22, 23
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexShaderInput);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    // Needs finishing setting up vertex and index buffers
    m_boxGeo = std::make_unique<MeshGeometry>();
    m_boxGeo->Name = "boxGeo";

    D3DCreateBlob(vbByteSize, &m_boxGeo->VertexBufferCPU);
    CopyMemory(m_boxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &m_boxGeo->IndexBufferCPU);
    CopyMemory(m_boxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    m_boxGeo->VertexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), vertices.data(), vbByteSize, m_boxGeo->VertexBufferUploader);

    m_boxGeo->IndexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), indices.data(), ibByteSize, m_boxGeo->IndexBufferUploader);

    m_boxGeo->VertexByteStride = sizeof(VertexShaderInput);
    m_boxGeo->VertexBufferByteSize = vbByteSize;
    m_boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_boxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    m_boxGeo->DrawArgs["box"] = submesh;
}
*/

void BoxRenderer::BuildPSO(D3D12Device& device)
{
    // Implementation for creating pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
    /*rsDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rsDesc.CullMode = D3D12_CULL_MODE_FRONT;*/
    psoDesc.RasterizerState = rsDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = device.GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = device.GetDepthStencilFormat();

    if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso))))
        throw std::runtime_error("Failed to create pipeline state object");
}

void BoxRenderer::BuildRenderItems()
{
    // Generate the boxes to be rendered
    while (m_poissonDisk->HasActiveSamples() && m_poissonDisk->GetSampleCount() < MAX_NUM_BOXES)
    {
        m_poissonDisk->SpawnNewSamples(10);
    }
}

void BoxRenderer::BuildFrameContexts(D3D12Device& device)
{
    // Create frame contexts based on current mode
    int numObjects = m_usePoissonDisk ? static_cast<int>(m_poissonDisk->GetSampleCount()) : 1;
    device.CreateFrameContexts(1, numObjects);
}

void BoxRenderer::BuildMaterials()
{
    std::unique_ptr<Material> boxMat = std::make_unique<Material>("woodCrate");
    boxMat->DiffuseAlbedo = XMFLOAT4(Colors::White);
    boxMat->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    boxMat->Roughness = 0.9f;

    m_boxMaterial = std::move(boxMat);
}

void BoxRenderer::BuildLights()
{
    auto light = std::make_unique<Light>("Directional 0");
    light->Type = LightType::Directional;
    light->Color = { 0.8f, 0.8f, 0.8f };
    // light->FalloffStart = 1.0f;
    light->Direction = { 0.57735f, -0.57735f, 0.57735f };
    // light->FalloffEnd = 10.0f;
    // light->Position = {0.0f, 0.0f, 0.0f};
    // light->SpotLightIntensity = 64.0f;    
    m_lights.push_back(std::move(light));

    light = std::make_unique<Light>("Directional 1");
    light->Type = LightType::Directional;
    light->Color = { 0.3f, 0.3f, 0.3f };
    light->Direction = { -0.57735f, -0.57735f, 0.57735f };
    m_lights.push_back(std::move(light));

    light = std::make_unique<Light>("Directional 2");
    light->Type = LightType::Directional;
    light->Color = { 0.15f, 0.15f, 0.15f };
    light->Direction = { 0.0f, -0.707f, -0.707f };
    m_lights.push_back(std::move(light));
}

void BoxRenderer::RenderUI()
{
    ImGui::Begin("Box Renderer Settings");
    
    bool previousMode = m_usePoissonDisk;
    ImGui::Checkbox("Use Poisson Disk Distribution", &m_usePoissonDisk);
    
    // If mode changed, rebuild frame contexts
    if (previousMode != m_usePoissonDisk)
    {
        SwitchRenderMode(m_usePoissonDisk);
    }
    
    if (m_usePoissonDisk)
    {
        ImGui::Text("Rendering Mode: Multiple Random Boxes");
        ImGui::Text("Number of boxes: %zu", m_poissonDisk->GetSampleCount());
    }
    else
    {
        ImGui::Text("Rendering Mode: Single Box");
    }
    
    ImGui::End();
}

void BoxRenderer::SwitchRenderMode(bool usePoissonDisk)
{
    // Wait for GPU to finish current work
    m_device->WaitForGpu();
    
    // Rebuild frame contexts with new object count
    int numObjects = usePoissonDisk ? static_cast<int>(m_poissonDisk->GetSampleCount()) : 1;
    m_device->CreateFrameContexts(1, numObjects);
}

void BoxRenderer::Render(const ImVec4& clearColor)
{
    ImGui::Render();

    FrameContext* frameContext = m_device->GetCurrentFrameContext();
    UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    frameContext->CommandAllocator->Reset();

    ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList().Get();
    cmdList->Reset(frameContext->CommandAllocator.Get(), m_pso.Get());

    // TODO: Record commands and the rest
    // ADD: Set viewport and scissor rect (CRITICAL!)
    RECT clientRect;
    GetClientRect(GetActiveWindow(), &clientRect);
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(clientRect.right - clientRect.left);
    viewport.Height = static_cast<float>(clientRect.bottom - clientRect.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = clientRect.right;
    scissorRect.bottom = clientRect.bottom;

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // Indicate a state transition on the resource usage.
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChain->GetBackBuffer(backBufferIdx),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);

    // Clear the back buffer and depth buffer.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(backBufferIdx);
    // Clear and set render target
    const float clearColorArray[4] = {
        clearColor.x * clearColor.w,
        clearColor.y * clearColor.w,
        clearColor.z * clearColor.w,
        clearColor.w
    };
    cmdList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
    cmdList->ClearDepthStencilView(m_device->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_device->DepthStencilView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    // Set descriptor heaps
    ID3D12DescriptorHeap* srvHeap = m_device->GetSRVHeap().Get();
    cmdList->SetDescriptorHeaps(1, &srvHeap);

    cmdList->SetGraphicsRootSignature(GetRootSignature().Get());

    auto passCbvHandle = frameContext->PassCB->Resource();
    cmdList->SetGraphicsRootConstantBufferView(2, passCbvHandle->GetGPUVirtualAddress());

    // Bind texture
    cmdList->SetGraphicsRootDescriptorTable(3, m_boxTexture->SrvGpuHandle);

    // Draw the boxes
    UINT objCbvByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCbvByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objCbvHandle = frameContext->ObjectCB->Resource();
    auto matCbvHandle = frameContext->MaterialCB->Resource();

    if (m_usePoissonDisk)
    {
        // Render Poisson disk distribution
        const auto& cubes = m_poissonDisk->GetSamples();
        for (size_t i = 0; i < cubes.size(); ++i)
        {
            // Set the object constant buffer view
            D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objCbvHandle->GetGPUVirtualAddress() + i * objCbvByteSize;
            D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCbvHandle->GetGPUVirtualAddress() + i * matCbvByteSize;
            
            cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
            cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);
            
            // Draw the box
            D3D12_VERTEX_BUFFER_VIEW vbView = m_boxGeo->VertexBufferView();
            D3D12_INDEX_BUFFER_VIEW ibView = m_boxGeo->IndexBufferView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            for (const auto& [name, submesh] : m_boxGeo->DrawArgs)
            {
                cmdList->DrawIndexedInstanced(
                    m_boxGeo->DrawArgs[name].IndexCount,
                    1, m_boxGeo->DrawArgs[name].StartIndexLocation, m_boxGeo->DrawArgs[name].BaseVertexLocation, 0);
            }
        }
    }
    else
    {
        // Render single box
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objCbvHandle->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCbvHandle->GetGPUVirtualAddress();
        
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);
        
        // Draw the box
        D3D12_VERTEX_BUFFER_VIEW vbView = m_boxGeo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibView = m_boxGeo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        cmdList->IASetIndexBuffer(&ibView);
        cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (const auto& [name, submesh] : m_boxGeo->DrawArgs)
        {
            cmdList->DrawIndexedInstanced(
                m_boxGeo->DrawArgs[name].IndexCount,
                1, m_boxGeo->DrawArgs[name].StartIndexLocation, m_boxGeo->DrawArgs[name].BaseVertexLocation, 0);
        }
    }

    // Render ImGui
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    // Transition back to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // Execute commands
    ID3D12CommandList* cmdLists[] = { cmdList };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
    m_device->SignalAndIncrementFence(frameContext);
}

void BoxRenderer::Update(float deltaTime)
{
    static double totalTime = 0.0;
    totalTime += deltaTime;

    FrameContext* frameContext = m_device->WaitForNextFrame();

    // Update pass constants
    m_camera->UpdateViewMatrix();

    PassConstants passConstants;
    XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(m_camera->GetViewMatrix()));
    XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(m_camera->GetProjectionMatrix()));
    XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(m_camera->GetViewProjectionMatrix()));

    XMVECTOR eyePos = m_camera->GetPosition();
    XMStoreFloat3(&passConstants.EyePosW, eyePos);
    
    passConstants.AmbientLight = { 0.5f, 0.5f, 0.5f, 1.0f };
    passConstants.Lights[0].Direction = m_lights[0].get()->Direction;
    passConstants.Lights[0].Color = m_lights[0].get()->Color;
    passConstants.Lights[1].Direction = m_lights[1].get()->Direction;
    passConstants.Lights[1].Color = m_lights[1].get()->Color;
    passConstants.Lights[2].Direction = m_lights[2].get()->Direction;
    passConstants.Lights[2].Color = m_lights[2].get()->Color;

    frameContext->PassCB->CopyData(0, passConstants);

    // Update material constants
    MaterialConstants matConstants;
    matConstants.DiffuseAlbedo = m_boxMaterial->DiffuseAlbedo;
    matConstants.FresnelR0 = m_boxMaterial->FresnelR0;
    matConstants.Roughness = m_boxMaterial->Roughness;

    if (m_usePoissonDisk)
    {
        // Update object constants for Poisson disk distribution
        const auto& cubes = m_poissonDisk->GetSamples();
        size_t index = 0;
        for (auto& cube : cubes)
        {
            XMMATRIX world = XMLoadFloat4x4(&m_world);
            world = XMMatrixTranslation(XMVectorGetX(cube), XMVectorGetY(cube), XMVectorGetZ(cube));

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            frameContext->ObjectCB->CopyData(index, objConstants);

            // Copy material data for each cube
            frameContext->MaterialCB->CopyData(index, matConstants);

            index++;
        }
    }
    else
    {
        // Update object constants for single box
        XMMATRIX world = XMLoadFloat4x4(&m_world);
        world = XMMatrixTranslation(XMVectorGetX(m_singleBoxPosition), XMVectorGetY(m_singleBoxPosition), XMVectorGetZ(m_singleBoxPosition));

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
        frameContext->ObjectCB->CopyData(0, objConstants);

        // Copy material data for single box
        frameContext->MaterialCB->CopyData(0, matConstants);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BoxRenderer::GetStaticSamplers()
{
    // Defining all six static samplers and keep them available for the root signature

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);  // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}
