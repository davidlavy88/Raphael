#pragma once
#include <memory>
#include "IDemo.h"

namespace raphael
{
    enum class DemoType
    {
        Box,
        Quad,
        TexturedBox,
        Gltf,
        GBuffer,
        RayTracer,
        MultiObject
    };

    // Creates a demo instance for the requested type.
    // To add a demo: add a value above and a matching case in CreateDemo().
    std::unique_ptr<IDemo> CreateDemo(DemoType type);
}
