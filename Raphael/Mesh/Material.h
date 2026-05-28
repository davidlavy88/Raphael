#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

namespace raphael
{
    struct Material
    {
        std::string name;
        std::string albedoTexturePath;
        std::string normalTexturePath;
        std::string roughnessTexturePath;
        std::string metallicTexturePath;

        XMFLOAT4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
		XMFLOAT2 uvTiling = { 1.0f, 1.0f };
    };

    class MaterialRepository
    {
    public:
        MaterialRepository() = default;

        void AddMaterial(const std::string& name, const Material& material)
        {
            m_materials[name] = material;
        }

        const Material* GetMaterial(const std::string& name) const
        {
            auto it = m_materials.find(name);
            return (it != m_materials.end()) ? &it->second : nullptr;
        }

        std::vector<std::string> GetAllTexturePaths() const
        {
            std::vector<std::string> paths;
            for (const auto& [name, mat] : m_materials)
            {
                if (!mat.albedoTexturePath.empty())    paths.push_back(mat.albedoTexturePath);
                if (!mat.normalTexturePath.empty())     paths.push_back(mat.normalTexturePath);
                if (!mat.roughnessTexturePath.empty())  paths.push_back(mat.roughnessTexturePath);
                if (!mat.metallicTexturePath.empty())   paths.push_back(mat.metallicTexturePath);
            }
            std::sort(paths.begin(), paths.end());
            // paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
            return paths;
        }

    private:
        std::unordered_map<std::string, Material> m_materials;
    };

    // A RenderItem ties together a mesh region, a material, and a world transform.
    // This is what the render loop iterates over.
    struct RenderItem
    {
        std::string meshName;     // Key into the mesh collection
        std::string submeshName;  // Key into Mesh::m_drawMeshes
        std::string materialName; // Key into MaterialRepository

        XMFLOAT4X4 world = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
    };
}
