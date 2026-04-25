#pragma once
#include <cstdint>

namespace raphael
{
    enum RenderMode
    {
        Normal,
        PoissonDisk
    };

    enum class TextureType
    {
        Albedo,
        Normal,
        Roughness
    };

    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    enum CommandListType
    {
        Direct
    };

    enum class InputElementSemantic
    {
        Position,
        Normal,
        TexCoord,
        Color
    };

    enum class ResourceFormat
    {
        Unknown ,
        R8G8B8A8_UNORM ,
        R32G32_FLOAT,
        R32G32B32_FLOAT ,
        R32G32B32A32_FLOAT ,
        D24_UNORM_S8_UINT ,
        R32_FLOAT ,
        R32_UINT 
        // TODO: Add more formats as needed
    };

    // TODO: Add validation for these flags (e.g., some combinations may not be valid, or some flags may require specific resource types)
    enum class ResourceBindFlags : uint32_t {
        None = 0,
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        ConstantBuffer = 1 << 2,
        ShaderResource = 1 << 3,
        RenderTarget = 1 << 4,
        DepthStencil = 1 << 5,
        UnorderedAccess = 1 << 6
    };

    inline ResourceBindFlags operator|(ResourceBindFlags a, ResourceBindFlags b)
    {
        return static_cast<ResourceBindFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline ResourceBindFlags operator&(ResourceBindFlags a, ResourceBindFlags b)
    {
        return static_cast<ResourceBindFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline bool hasFlag(ResourceBindFlags flags, ResourceBindFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }

    enum class ResourceViewType
    {
        Unknown,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        ShaderResource,
        RenderTarget,
        DepthStencil,
        UnorderedAccess
    };

    enum class SamplerFilter
    {
        Point,
        Linear,
        Anisotropic
    };

    enum class SamplerAddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        Border
    };
} // namespace raphael
