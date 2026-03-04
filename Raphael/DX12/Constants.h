#pragma once

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
} // namespace raphael
