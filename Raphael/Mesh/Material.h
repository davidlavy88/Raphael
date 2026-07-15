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

    struct TextureLoadInfo
    {
        std::string path;
        bool        isSRGB;   // true = color data (albedo) -> sRGB view; false = linear data (normal/roughness/metallic)
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

        std::vector<TextureLoadInfo> GetAllTextures() const
        {
            std::vector<TextureLoadInfo> textures;
            for (const auto& [name, mat] : m_materials)
            {
                if (!mat.albedoTexturePath.empty())     textures.push_back({ mat.albedoTexturePath,    true  });
                if (!mat.normalTexturePath.empty())     textures.push_back({ mat.normalTexturePath,    false });
                if (!mat.roughnessTexturePath.empty())  textures.push_back({ mat.roughnessTexturePath, false });
                if (!mat.metallicTexturePath.empty())   textures.push_back({ mat.metallicTexturePath,  false });
            }
            std::sort(textures.begin(), textures.end(),
                      [](const TextureLoadInfo& a, const TextureLoadInfo& b) { return a.path < b.path; });
            return textures;
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
