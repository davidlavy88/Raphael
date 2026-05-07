#pragma once
#include "Vertex.h"
#include <string>
#include <unordered_map>

namespace raphael
{
    struct Submesh
    {
        std::uint32_t vertexBufferOffset = 0;
        std::uint32_t indexBufferOffset = 0;
        std::uint32_t indexCount = 0;
    };

    class Mesh
    {
    public:

        using uint16 = std::uint16_t;
        using uint32 = std::uint32_t;

        Mesh() = default;

        const std::vector<uint16>& GetIndices16() const;

        void GenerateIndices16();

    public:
        std::string m_meshName;
        std::vector<Vertex> m_vertices;
        std::vector<uint32> m_indices32;		
        std::vector<uint16> m_indices16;

        // A MeshGeometry may store multiple geometries in one vertex/index buffer.
        // Use this container to define the Submesh geometries so we can draw
        // the Submeshes individually.
        std::unordered_map<std::string, Submesh> m_drawMeshes;
    };
}
