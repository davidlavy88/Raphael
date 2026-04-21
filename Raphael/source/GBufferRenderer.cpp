#include "GBufferRenderer.h"
#include "Material.h"
#include "TextureLoader/DDSTextureLoader.h"
#include "TextureLoader/WICTextureLoader12.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "tinygltf/tiny_gltf.h"

bool GBufferRenderer::Initialize(D3D12Device& device, SwapChain& swapChain, HWND hwnd)
{
    if (!Renderer::Initialize(device, swapChain, hwnd))
        return false;

    // Init COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Initialize the gltf model container
    m_gltfModel = std::make_unique<tinygltf::Model>();

    // Reset command list for initialization
    device.GetCommandList()->Reset(device.GetCurrentCommandAllocator().Get(), nullptr);

    // Build pipeline in dependency order
    BuildRootSignature(device);
    BuildGeometry(device);        // loads the battlecruiser glTF
    LoadTextures(device);         // uploads textures to GPU
    BuildDescriptorHeaps(device); // creates SRVs for textures
    BuildShadersAndInputLayout();
    BuildMaterials();
    BuildLights();
    // BuildRenderItems();
    BuildFrameContexts(device);
    BuildPSO(device);

    // ADD: Initialize matrices
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    UINT width = static_cast<UINT>(clientRect.right - clientRect.left);
    UINT height = static_cast<UINT>(clientRect.bottom - clientRect.top);
    CreateGBufferResources(device, width, height);

    // Initialize camera position, look and up vectors
    m_camera->SetPosition(XMVectorSet(0.0f, 0.0f, -20.0f, 1.0f));
    m_camera->SetLook(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
    m_camera->SetUp(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    float aspect = static_cast<float>(clientRect.right) / clientRect.bottom;

    // Initialize world matrix (identity)
    XMStoreFloat4x4(&m_world, XMMatrixIdentity());

    // Initialize camera projection matrix
    m_camera->SetProjectionMatrix(0.25f * XM_PI, aspect, 1.0f, 1000.0f);

    // Execute the initialization commands
    device.GetCommandList()->Close();
    ID3D12CommandList* cmdLists[] = { device.GetCommandList().Get() };
    device.GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

    // Wait until initialization is complete
    device.WaitForGpu();

    return true;
}

void GBufferRenderer::Render(const ImVec4& clearColor)
{
    ImGui::Render();

    FrameContext* frameContext = m_device->GetCurrentFrameContext();
    UINT backBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    frameContext->CommandAllocator->Reset();

    ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList().Get();
    cmdList->Reset(frameContext->CommandAllocator.Get(), m_pso.Get());

    // Set viewport and scissor rect
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

    // Geometry pass: Render scene geometry to G-buffer render targets
    // Build RTV handles for all 3 G-buffer render targets
    UINT rtvDescriptorSize = m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE gbufferRTVHandles[GBUFFER_NUM_RENDER_TARGETS];
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapStart(m_gbufferRTVHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < GBUFFER_NUM_RENDER_TARGETS; ++i) {
        gbufferRTVHandles[i] = rtvHeapStart;
        gbufferRTVHandles[i].ptr += i * rtvDescriptorSize;
    }

    // Clear all G-Buffer render targets with the SAME clear value
    // used in CreateGBufferResources (must match for optimal performance)
    const float clearColorArray[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int i = 0; i < GBUFFER_NUM_RENDER_TARGETS; ++i) {
        cmdList->ClearRenderTargetView(gbufferRTVHandles[i], clearColorArray, 0, nullptr);
    }
    // Clear depth buffer
    cmdList->ClearDepthStencilView(m_device->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Set render target and depth stencil 
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_device->DepthStencilView();
    cmdList->OMSetRenderTargets(3, gbufferRTVHandles, FALSE, &dsvHandle);

    // Set descriptor heaps for texture sampling
    ID3D12DescriptorHeap* srvHeap = m_device->GetSRVHeap().Get();
    cmdList->SetDescriptorHeaps(1, &srvHeap);

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetGraphicsRootConstantBufferView(2, frameContext->PassCB->Resource()->GetGPUVirtualAddress());

    // Bind object and material constants
    cmdList->SetGraphicsRootConstantBufferView(0, frameContext->ObjectCB->Resource()->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, frameContext->MaterialCB->Resource()->GetGPUVirtualAddress());

    // Set vertex/index buffers and topology
    D3D12_VERTEX_BUFFER_VIEW vbView = m_modelGeo->VertexBufferView();
    D3D12_INDEX_BUFFER_VIEW ibView = m_modelGeo->IndexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);
    cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw call
    auto texture_it = m_modelTextures.begin();
    for (const auto& [name, submesh] : m_modelGeo->DrawArgs)
    {
        // TODO: Bind texture (reuse the last texture if we have more primitives than textures)
        // Note: In a real implementation, you would want to have a more robust way of associating textures with primitives (e.g., using material IDs from the glTF model), but for this example we will just bind textures in order and reuse the last one if we run out.
        if (texture_it != m_modelTextures.end())
        {
            cmdList->SetGraphicsRootDescriptorTable(3, texture_it->second->SrvGpuHandle);
            ++texture_it;
        }

        cmdList->DrawIndexedInstanced(
            submesh.IndexCount,
            1, 
            submesh.StartIndexLocation, 
            submesh.BaseVertexLocation, 
            0);
    }

	// For now we copy albedo in back buffer to see the result of the geometry pass, 
    // but later I do real implementation for a separate lighting pass that samples 
    // from the G-buffer render targets to compute the final shaded image.
    // Transition G-Buffer albedo: RENDER_TARGET -> COPY_SOURCE
    // Transition back buffer:     PRESENT       -> COPY_DEST
    D3D12_RESOURCE_BARRIER barriers[2];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_gbufferRenderTargets[GBUFFER_RENDER_TARGET_ALBEDO].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_swapChain->GetBackBuffer(backBufferIdx),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(2, barriers);

    // Copy the albedo G-Buffer content to the back buffer so we can see the result
    cmdList->CopyResource(m_swapChain->GetBackBuffer(backBufferIdx), m_gbufferRenderTargets[GBUFFER_RENDER_TARGET_ALBEDO].Get());

    // Transition albedo back to RENDER_TARGET for next frame
    // Transition back buffer to RENDER_TARGET so ImGui can draw on top
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_gbufferRenderTargets[GBUFFER_RENDER_TARGET_ALBEDO].Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_swapChain->GetBackBuffer(backBufferIdx),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(2, barriers);

    // Render ImGui on top of the back buffer
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = m_swapChain->GetRTVHandle(backBufferIdx);
    cmdList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    // Transition back to present
    // Transition back buffer to PRESENT
    D3D12_RESOURCE_BARRIER presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_swapChain->GetBackBuffer(backBufferIdx),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    cmdList->ResourceBarrier(1, &presentBarrier);
    cmdList->Close();

    // Execute commands
    ID3D12CommandList* cmdLists[] = { cmdList };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
    m_device->SignalAndIncrementFence(frameContext);
}

void GBufferRenderer::BuildGeometry(D3D12Device& device)
{
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Load the glTF model from file
    if (!loader.LoadASCIIFromFile(m_gltfModel.get(), &err, &warn, "Models/battlecruiser_sc2/scene.gltf"))
    {
        throw std::runtime_error("Failed to load glTF model: " + err);
    }
    // TODO: Add warning handling

    m_modelGeo = std::make_unique<MeshGeometry>();
    m_modelGeo->Name = "gltfMesh";

    std::vector<VertexShaderInput> totalVertices;
    std::vector<std::uint16_t> totalIndices;

    size_t meshIndex = 0;
    UINT vertexOffset = 0;
    UINT indexOffset = 0;

    for (const tinygltf::Mesh& mesh : m_gltfModel->meshes)
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

            // Step 8: Create MeshGeometry object and upload vertex/index data to GPU
            // TODO: This should be refactored later
            SubmeshGeometry submesh;
            submesh.IndexCount = (UINT)indices.size();
            submesh.StartIndexLocation = indexOffset;
            submesh.BaseVertexLocation = vertexOffset;

            m_modelGeo->DrawArgs[primitiveName] = submesh;

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

    D3DCreateBlob(vbByteSize, &m_modelGeo->VertexBufferCPU);
    CopyMemory(m_modelGeo->VertexBufferCPU->GetBufferPointer(), totalVertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &m_modelGeo->IndexBufferCPU);
    CopyMemory(m_modelGeo->IndexBufferCPU->GetBufferPointer(), totalIndices.data(), ibByteSize);

    m_modelGeo->VertexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), totalVertices.data(), vbByteSize, m_modelGeo->VertexBufferUploader);

    m_modelGeo->IndexBufferGPU = D3D12Util::CreateDefaultBuffer(device.GetDevice().Get(),
        device.GetCommandList().Get(), totalIndices.data(), ibByteSize, m_modelGeo->IndexBufferUploader);

    m_modelGeo->VertexByteStride = sizeof(VertexShaderInput);
    m_modelGeo->VertexBufferByteSize = vbByteSize;
    m_modelGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_modelGeo->IndexBufferByteSize = ibByteSize;

    OutputDebugStringA("glTF model loaded successfully!\n");
}

void GBufferRenderer::LoadTextures(D3D12Device& device)
{
    size_t i = 0;
    for (const tinygltf::Texture& texture : m_gltfModel->textures)
    {
        if (texture.source < 0 || texture.source >= m_gltfModel->images.size())
        {
            throw std::runtime_error("Texture source index out of bounds in gltf model");
        }

        const tinygltf::Image& image = m_gltfModel->images[texture.source];

        // Upload image to GPU as a texture resource
        std::string texturePath = "Models/battlecruiser_sc2/" + image.uri;
        std::wstring imageUri{ texturePath.begin(), texturePath.end() };

        std::string textureName = image.uri;

        std::unique_ptr<Texture> gltfTexture = std::make_unique<Texture>(textureName, imageUri);

        // WIC loader needs these additional outputs
        std::unique_ptr<uint8_t[]> decodedData;
        D3D12_SUBRESOURCE_DATA subresource = {};

        // Load the texture from file
        HRESULT hr = DirectX::LoadWICTextureFromFile(
            device.GetDevice().Get(),
            imageUri.c_str(),
            gltfTexture->Resource.GetAddressOf(),  // Creates the texture resource
            decodedData,                            // Stores decoded pixel data
            subresource);                           // Contains upload info

        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to load texture " + std::string(imageUri.begin(), imageUri.end()));
        }

        // Now manually upload the texture data to GPU
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(
            gltfTexture->Resource.Get(), 0, 1);

        // Create upload heap
        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        hr = device.GetDevice()->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&gltfTexture->UploadHeap));

        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create upload heap for texture");
        }

        // Copy data to the intermediate upload heap and schedule a copy 
        // from the upload heap to the texture
        UpdateSubresources(
            device.GetCommandList().Get(),
            gltfTexture->Resource.Get(),
            gltfTexture->UploadHeap.Get(),
            0, 0, 1, &subresource);

        // Transition texture to PIXEL_SHADER_RESOURCE state
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            gltfTexture->Resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        device.GetCommandList()->ResourceBarrier(1, &barrier);

        m_modelTextures[textureName] = std::move(gltfTexture);
    }
}

void GBufferRenderer::BuildDescriptorHeaps(D3D12Device& device)
{
    // Create SRVs for each texture
    for (const auto& [name, texture] : m_modelTextures)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texture->Resource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = texture->Resource->GetDesc().MipLevels;

        device.GetSRVAllocator().Alloc(&texture->SrvCpuHandle, &texture->SrvGpuHandle);
        device.GetDevice()->CreateShaderResourceView(
            texture->Resource.Get(), 
            &srvDesc, 
            texture->SrvCpuHandle);
    }
}

void GBufferRenderer::BuildShadersAndInputLayout()
{
    // Implementation for compiling shaders and defining input layout
    m_vsByteCode = D3D12Util::CompileShader(L"Shaders\\gbuffer.hlsl", nullptr, "VS", "vs_5_0");
    m_psByteCode = D3D12Util::CompileShader(L"Shaders\\gbuffer.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void GBufferRenderer::BuildPSO(D3D12Device& device)
{
    // Implementation for creating pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
    psoDesc.RasterizerState = rsDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Multi render targets for G-buffer
    psoDesc.NumRenderTargets = GBUFFER_NUM_RENDER_TARGETS;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Albedo
    psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT; // Normal + Roughness
    psoDesc.RTVFormats[2] = DXGI_FORMAT_R32_FLOAT; // Depth

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = device.GetDepthStencilFormat();

    if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso))))
        throw std::runtime_error("Failed to create pipeline state object");
}

void GBufferRenderer::BuildRootSignature(D3D12Device& device)
{
    CD3DX12_DESCRIPTOR_RANGE textureTable;
    textureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Implementation for creating root signature 
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];
    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants
    slotRootParameter[1].InitAsConstantBufferView(1); // Material constants
    slotRootParameter[2].InitAsConstantBufferView(2); // Pass constants 
    slotRootParameter[3].InitAsDescriptorTable(1, &textureTable); // Texture SRV (diffuse map)

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

void GBufferRenderer::BuildFrameContexts(D3D12Device& device)
{
    device.CreateFrameContexts(1, 1); // 1 pass, 1 object (the model)
}

void GBufferRenderer::Shutdown()
{
    // Clean up resources
    m_modelGeo.reset();
    m_pso.Reset();
    m_rootSignature.Reset();
    m_gbufferRTVHeap.Reset();
    for (int i = 0; i < GBUFFER_NUM_RENDER_TARGETS; ++i)
    {
        m_gbufferRenderTargets[i].Reset();
    }
    for (auto& texture : m_modelTextures)
    {
        texture.second.reset();
    }

    Renderer::Shutdown();
}

void GBufferRenderer::Update(float deltaTime)
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
    // TODO: Support multiple materials if we have multiple boxes with different materials in the future
    MaterialConstants matConstants;
    matConstants.DiffuseAlbedo = XMFLOAT4(Colors::White);
    matConstants.FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    matConstants.Roughness = 0.9f;

    // Update object constants for single box
    XMMATRIX world = XMLoadFloat4x4(&m_world);
    world = XMMatrixTranslation(XMVectorGetX(m_singleBoxPosition), XMVectorGetY(m_singleBoxPosition), XMVectorGetZ(m_singleBoxPosition));

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
    frameContext->ObjectCB->CopyData(0, objConstants);

    // Copy material data for single box
    frameContext->MaterialCB->CopyData(0, matConstants);
}

void GBufferRenderer::BuildMaterials()
{
    auto texture_it = m_modelTextures.begin();
    auto material_it = m_gltfModel->materials.begin();
    for (; material_it != m_gltfModel->materials.end() && texture_it != m_modelTextures.end(); ++material_it, ++texture_it)
    {
        const tinygltf::Material& material = *material_it;
        const std::string& texture = texture_it->first;
        std::unique_ptr<Material> gltfMaterial = std::make_unique<Material>(material.name);
        gltfMaterial->DiffuseTextureName = texture;
        gltfMaterial->DiffuseAlbedo = XMFLOAT4(Colors::White);
        gltfMaterial->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
        gltfMaterial->Roughness = 0.9f;

        m_modelMaterials[material.name] = std::move(gltfMaterial);
    }
}

void GBufferRenderer::BuildLights()
{
    auto light = std::make_unique<Light>("Directional 0");
    light->Type = LightType::Directional;
    light->Color = { 0.8f, 0.8f, 0.8f };
    // light->FalloffStart = 1.0f;
    light->Direction = { 0.57735f, -0.57735f, -0.57735f };
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

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GBufferRenderer::GetStaticSamplers()
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

void GBufferRenderer::CreateGBufferResources(D3D12Device& device, UINT width, UINT height)
{
    // Using Multiple Render Targets (MRT) to create separate render targets for Albedo, Normal and Depth
    // 0 - Albedo (R8G8B8A8_UNORM) - base color of the material
    // 1 - Normal (R16G16B16A16_FLOAT) - world space normal (need float precision)
    // 2 - Depth (R32_FLOAT) - linear depth for lighting calculations
    DXGI_FORMAT formats[GBUFFER_NUM_RENDER_TARGETS] = {
        DXGI_FORMAT_R8G8B8A8_UNORM, // Albedo
        DXGI_FORMAT_R16G16B16A16_FLOAT, // Normal
        DXGI_FORMAT_R32_FLOAT // Depth
    };

    // Create RTV descriptor heap for G-buffer render targets
    // We 1 RTV for each render target (Albedo, Normal, Depth) - 3 total
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = GBUFFER_NUM_RENDER_TARGETS;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(device.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_gbufferRTVHeap))))
    {
        throw std::runtime_error("Failed to create RTV descriptor heap for G-buffer");
    }

    UINT rtvDescriptorSize = device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_gbufferRTVHeap->GetCPUDescriptorHandleForHeapStart();

    // Create each render target resource and its RTV + SRV
    for (int i = 0; i < GBUFFER_NUM_RENDER_TARGETS; ++i)
    {
        // Create the render target resource
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            formats[i],
            width,
            height,
            1, // array size
            1  // mip levels
        );
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        // Clear value for the render target (important for performance)
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = formats[i];
        clearValue.Color[0] = 0.0f; // Clear to black
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;

        if (FAILED(device.GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(&m_gbufferRenderTargets[i]))))
        {
            throw std::runtime_error("Failed to create render target resource for G-buffer");
        }

        // Create RTV for this render target
        device.GetDevice()->CreateRenderTargetView(m_gbufferRenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize; // Move to the next RTV descriptor slot

        // Create SRV for this render target (for later use in lighting pass)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = formats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // Allocate from the device SRV descriptor heap (not the RTV heap) for this SRV
        device.GetSRVAllocator().Alloc(&m_gbufferSRVCPUHandles[i], &m_gbufferSRVGPUHandles[i]);
        device.GetDevice()->CreateShaderResourceView(m_gbufferRenderTargets[i].Get(), &srvDesc, m_gbufferSRVCPUHandles[i]);
    }
}
