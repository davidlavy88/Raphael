#include "GltfLoader.h"
#include "tinygltf/tiny_gltf.h"
#include <Windows.h>
#include <stdexcept>

namespace raphael
{
    bool GltfLoader::LoadFromFile(const std::string& filename, Mesh& outmesh)
    {        
        std::unique_ptr<tinygltf::Model> gltfModel = std::make_unique<tinygltf::Model>();
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = loader.LoadASCIIFromFile(gltfModel.get(), &err, &warn, filename);
        if (!warn.empty())
        {
            OutputDebugStringA(("Warning: " + warn + "\n").c_str());
        }
        if (!err.empty())
        {
            OutputDebugStringA(("Error: " + err + "\n").c_str());
            return false;
        }
        if (!ret)
        {
            throw std::runtime_error("Failed to load glTF file: " + filename);
            return false;
        }

        std::uint32_t vertexOffset = 0;
        std::uint32_t indexOffset = 0;

        size_t meshIndex = 0;

        for (const tinygltf::Mesh& mesh : gltfModel->meshes)
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

                std::vector<Vertex> vertices;

                // Find the POSITION attribute
                auto positionAttrIt = primitive.attributes.find("POSITION");
                if (positionAttrIt == primitive.attributes.end())
                {
                    throw std::runtime_error("Mesh primitive does not contain POSITION attribute");
                }

                int positionAccessorIndex = positionAttrIt->second;
                // This accessor describes how to read the position data (e.g., type, count, etc.)
                const tinygltf::Accessor& positionAccessor = gltfModel->accessors[positionAccessorIndex];
                // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                const tinygltf::BufferView& positionBufferView = gltfModel->bufferViews[positionAccessor.bufferView];
                // This buffer contains the actual binary data for the positions
                const tinygltf::Buffer& positionBuffer = gltfModel->buffers[positionBufferView.buffer];
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
                const tinygltf::Accessor& normalAccessor = gltfModel->accessors[normalAccessorIndex];
                // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                const tinygltf::BufferView& normalBufferView = gltfModel->bufferViews[normalAccessor.bufferView];
                // This buffer contains the actual binary data for the normals
                const tinygltf::Buffer& normalBuffer = gltfModel->buffers[normalBufferView.buffer];

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
                const tinygltf::Accessor& textureAccessor = gltfModel->accessors[textureAccessorIndex];
                // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                const tinygltf::BufferView& textureBufferView = gltfModel->bufferViews[textureAccessor.bufferView];
                // This buffer contains the actual binary data for the textures
                const tinygltf::Buffer& textureBuffer = gltfModel->buffers[textureBufferView.buffer];

                // Calculate the pointer to the position data
                const float* textureData = reinterpret_cast<const float*>(&textureBuffer.data[textureBufferView.byteOffset + textureAccessor.byteOffset]);

                // Step 6: Create vertex array
                vertices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    vertices[i].Position = XMFLOAT3(positionData[i * 3], positionData[i * 3 + 1], positionData[i * 3 + 2]);

                    // TODO: Set normals and texture coordinates if available
                    vertices[i].Normal = XMFLOAT3(normalData[i * 3], normalData[i * 3 + 1], normalData[i * 3 + 2]);
                    vertices[i].TexC = XMFLOAT2(textureData[i * 2], textureData[i * 2 + 1]);
                }

                // Step 7: Extract indices
                std::vector<std::uint16_t> indices;

                if (primitive.indices >= 0)
                {
                    // This accessor describes how to read the index data (e.g., type, count, etc.)
                    const tinygltf::Accessor& indexAccessor = gltfModel->accessors[primitive.indices];
                    // This buffer view describes the layout of the buffer (e.g., byte offset, stride, etc.)
                    const tinygltf::BufferView& indexBufferView = gltfModel->bufferViews[indexAccessor.bufferView];
                    // This buffer contains the actual binary data for the indices
                    const tinygltf::Buffer& indexBuffer = gltfModel->buffers[indexBufferView.buffer];
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
                // std::unique_ptr<Mesh> meshData = std::make_unique<Mesh>();
                Submesh meshData = {};
                meshData.vertexBufferOffset = vertexOffset;
                meshData.indexBufferOffset = indexOffset;
                meshData.indexCount = static_cast<std::uint32_t>(indices.size());
                outmesh.m_drawMeshes[primitiveName] = meshData;

                // Debug: print material name for this index
                if (primitive.material >= 0 && primitive.material < static_cast<int>(gltfModel->materials.size()))
                {
                    const tinygltf::Material& mat = gltfModel->materials[primitive.material];
                    OutputDebugStringA(("Primitive: " + primitiveName + " -> material[" + std::to_string(primitive.material) + "] = \"" + mat.name + "\"\n").c_str());
                }

                outmesh.m_vertices.insert(outmesh.m_vertices.end(), vertices.begin(), vertices.end());
                outmesh.m_indices16.insert(outmesh.m_indices16.end(), indices.begin(), indices.end());
                vertexOffset += vertexCount;
                indexOffset += indices.size();
                primitiveIndex++;
            }
        }
        return true;
    }
}
