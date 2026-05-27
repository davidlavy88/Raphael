#pragma once
#include "Mesh.h"
#include <string>
#include <memory>

namespace raphael
{
    class GltfLoader
    {
    public:
        GltfLoader() = default;
        // Load a glTF file and create a Mesh object
        bool LoadFromFile(const std::string& filename, Mesh& outmesh);
		
        size_t numTexturesLoaded = 0;
    };
}

